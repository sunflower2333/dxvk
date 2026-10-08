// SPDX-License-Identifier: Zlib
// Actual production replay/order/ownership helpers; API/backend objects modeled.
#include "../src/d3d11/d3d11_command_replay.h"
#include "../src/d3d11/d3d11_query_ticket.h"
#include "../src/d3d11/d3d11_query_sequence.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>

static std::atomic<unsigned> checks{0};
#define CHECK(v) do { checks.fetch_add(1,std::memory_order_relaxed);if (!(v)) std::abort(); } while (0)
struct Ticket {
  static std::atomic<unsigned> live;
  dxvk::D3D11QueryTicketState state;
  explicit Ticket(uint64_t id) : state(id) { live.fetch_add(1); }
  ~Ticket() { live.fetch_sub(1); }
};
std::atomic<unsigned> Ticket::live{0};
using TicketRef=std::shared_ptr<Ticket>;
struct Query {
  static std::atomic<unsigned> live;
  TicketRef current;
  dxvk::D3D11DeferredQueryTickets<TicketRef> fifo;
  dxvk::D3D11QuerySequence sequence;
  explicit Query(uint64_t id) : current(std::make_shared<Ticket>(id)) { live.fetch_add(1); }
  ~Query() { live.fetch_sub(1); }
};
std::atomic<unsigned> Query::live{0};
using QueryRef=std::shared_ptr<Query>;
using Order=dxvk::D3D11CommandOrder<QueryRef>;
using Replay=dxvk::D3D11CommandReplay<QueryRef,TicketRef>;
using Type=dxvk::D3D11RecordedOperationType;
static uint64_t nextTicket=100;
static Replay replay(const Order& order,unsigned expectedQueries) {
  unsigned captures=0,ends=0;
  return Replay(order,[&] (const QueryRef& query) {
    CHECK(ends==0);++captures;return query->current;
  },[&] (const dxvk::D3D11RecordedOperation<QueryRef>& operation) {
    CHECK(captures==expectedQueries && operation.endOccurrence==ends+1);
    ++ends;auto ticket=std::make_shared<Ticket>(nextTicket++);
    operation.query->fifo.push(ticket);operation.query->sequence.deferEnd();
    operation.query->current=ticket;return ticket;
  });
}
static void complete(const Replay& execution) {
  for (const auto& end : execution.endTickets()) {
    auto begin=end.query->fifo.front(),finish=end.query->fifo.take();
    CHECK(begin==end.ticket && finish==end.ticket && !end.ticket->state.endRecorded());
    finish->state.completeEnd();end.query->sequence.completeEnd();
    CHECK(end.ticket->state.endRecorded());
  }
}
int main() {
  {
    Order empty;auto execution=replay(empty,0);
    CHECK(execution.initialTickets().empty() && execution.endTickets().empty());
    CHECK(execution.operations().size()==1 && !execution.operations()[0].predicate.query);
    CHECK(!execution.ticketForEnd(0) && !execution.ticketForEnd(1));
  }
  {
    auto a=std::make_shared<Query>(11),b=std::make_shared<Query>(21);
    Order order;
    order.addPredicate(a,0,false);order.addChunk(0);
    order.addPredicate({},7,false);order.addQueryBegin(a,false);order.addChunk(1);order.addQueryEnd(a);
    order.addPredicate(a,1,false);order.addChunk(2);
    order.addPredicate({},37,false);order.addQueryBegin(b,true);order.addQueryEnd(b);
    order.addPredicate(b,-5,true);order.addChunk(3);
    order.addPredicate({},0,false);order.addQueryBegin(a,false);order.addQueryEnd(a);
    order.addPredicate(a,0,false);order.addChunk(4);order.addPredicate({},99,false);
    const auto firstId=nextTicket;auto execution=replay(order,2);
    CHECK(execution.initialTickets().size()==2 && execution.endTickets().size()==3);
    CHECK(execution.initialTickets()[0].ticket->state.id()==11 && execution.initialTickets()[1].ticket->state.id()==21);
    CHECK(a->current->state.id()==firstId+2 && b->current->state.id()==firstId+1);
    CHECK(a->sequence.read().pending==2 && b->sequence.read().pending==1);
    unsigned chunk=0,begin=0,end=0;
    for (const auto& operation : execution.operations()) {
      const auto& recorded=operation.recorded;
      CHECK(recorded.type==order.operations()[&operation-execution.operations().data()].type);
      if (recorded.type==Type::Chunk) {
        CHECK(recorded.chunkId==chunk);
        const auto& predicate=operation.predicate;
        if (chunk==0) CHECK(predicate.query==a && predicate.ticket->state.id()==11 && predicate.value==0 && !predicate.hint);
        if (chunk==1) CHECK(!predicate.query && !predicate.ticket && predicate.value==7);
        if (chunk==2) CHECK(predicate.query==a && predicate.ticket==execution.ticketForEnd(1) && predicate.value==1);
        if (chunk==3) CHECK(predicate.query==b && predicate.ticket==execution.ticketForEnd(2) && predicate.value==-5 && predicate.hint);
        if (chunk==4) CHECK(predicate.query==a && predicate.ticket==execution.ticketForEnd(3) && predicate.value==0);
        ++chunk;
      }
      if (recorded.type==Type::QueryBegin) {
        CHECK(operation.queryTicket==execution.ticketForEnd(++begin));
        CHECK(recorded.implicitBegin==(begin==2));
      }
      if (recorded.type==Type::QueryEnd) {
        CHECK(recorded.endOccurrence==++end && operation.queryTicket==execution.ticketForEnd(end));
        CHECK(!operation.queryTicket->state.endRecorded());
      }
    }
    CHECK(chunk==5 && begin==3 && end==3);
    CHECK(!execution.operations().back().predicate.query && execution.operations().back().predicate.value==99);
    CHECK(!execution.ticketForEnd(0) && !execution.ticketForEnd(4));
    complete(execution);
    CHECK(a->sequence.current(a->sequence.read()) && b->sequence.current(b->sequence.read()));
    CHECK(execution.operations()[2].predicate.ticket->state.id()==11);
  }
  CHECK(Query::live.load()==0 && Ticket::live.load()==0);

  // Release the recording and app owners while an execution and an extracted
  // predicate binding own the query/result. Dropping each owner retires exactly.
  {
    std::weak_ptr<Query> queryWeak;
    std::weak_ptr<Ticket> initialWeak,endWeak;
    std::unique_ptr<Replay> execution;
    {
      auto query=std::make_shared<Query>(31);queryWeak=query;initialWeak=query->current;
      Order order;order.addPredicate(query,0,false);order.addChunk(0);
      order.addPredicate({},0,false);order.addQueryBegin(query,false);order.addQueryEnd(query);
      order.addPredicate(query,1,false);order.addChunk(1);
      execution=std::make_unique<Replay>(replay(order,1));endWeak=execution->ticketForEnd(1);
    }
    CHECK(!queryWeak.expired() && !initialWeak.expired() && !endWeak.expired());
    complete(*execution);
    auto binding=execution->operations().back().predicate;
    execution.reset();CHECK(!queryWeak.expired() && initialWeak.expired() && !endWeak.expired());
    binding={};CHECK(queryWeak.expired() && endWeak.expired());
  }
  CHECK(Query::live.load()==0 && Ticket::live.load()==0);

  constexpr unsigned repeated=4096;
  unsigned nestedChunks=0,nestedEnds=0;
  {
    auto query=std::make_shared<Query>(41),other=std::make_shared<Query>(51);
    Order child;child.addQueryBegin(query,false);child.addQueryEnd(query);
    child.addPredicate(query,1,false);child.addChunk(0);child.addPredicate({},23,false);
    Order parent;parent.addChunk(0);parent.append(child,1);
    parent.addQueryBegin(query,true);parent.addQueryEnd(query);parent.addPredicate(query,0,false);parent.addChunk(2);
    parent.append(child,3);
    Order nested;nested.addQueryBegin(other,false);nested.addQueryEnd(other);nested.addChunk(0);nested.append(parent,1);
    auto execution=replay(nested,2);
    for (const auto& operation : execution.operations()) {
      if (operation.recorded.type==Type::QueryEnd) CHECK(operation.recorded.endOccurrence==++nestedEnds && operation.queryTicket==execution.ticketForEnd(nestedEnds));
      if (operation.recorded.type==Type::Chunk) CHECK(operation.recorded.chunkId==nestedChunks++);
    }
    CHECK(nestedEnds==4 && nestedChunks==5);complete(execution);
    Order many;
    for (unsigned index=0;index<repeated;++index) many.append(child,index);
    auto large=replay(many,1);CHECK(large.endTickets().size()==repeated);
    unsigned ends=0,chunks=0;
    for (const auto& operation : large.operations()) {
      if (operation.recorded.type==Type::QueryEnd) CHECK(operation.recorded.endOccurrence==++ends && operation.queryTicket==large.ticketForEnd(ends));
      if (operation.recorded.type==Type::Chunk) CHECK(operation.recorded.chunkId==chunks++ && operation.predicate.ticket==large.ticketForEnd(chunks));
    }
    CHECK(ends==repeated && chunks==repeated);complete(large);
  }
  CHECK(Query::live.load()==0 && Ticket::live.load()==0);

  constexpr unsigned concurrentReplays=1000,endsPerReplay=17,retained=32;
  {
    auto query=std::make_shared<Query>(61);
    Order order;
    for (unsigned end=0;end<endsPerReplay;++end) {
      order.addPredicate({},int32_t(end),false);order.addQueryBegin(query,false);order.addQueryEnd(query);
      order.addPredicate(query,int32_t(end&1),end==2);order.addChunk(end);
    }
    using Owner=std::shared_ptr<const Replay>;
    dxvk::D3D11DeferredQueryTickets<Owner> executions;
    std::vector<Owner> older;
    std::thread cs([&] {
      for (unsigned index=0;index<concurrentReplays;) {
        auto execution=executions.take();if (!execution) { std::this_thread::yield();continue; }
        CHECK(execution->endTickets().size()==endsPerReplay);
        complete(*execution);
        for (const auto& operation : execution->operations()) {
          if (operation.recorded.type==Type::Chunk) CHECK(operation.predicate.ticket==execution->ticketForEnd(operation.recorded.chunkId+1));
        }
        ++index;
      }
    });
    uint64_t previousFinal=61;
    for (unsigned index=0;index<concurrentReplays;++index) {
      auto execution=std::make_shared<const Replay>(replay(order,1));
      CHECK(execution->initialTickets()[0].ticket->state.id()==previousFinal);
      previousFinal=execution->ticketForEnd(endsPerReplay)->state.id();
      for (unsigned end=1;end<=endsPerReplay;++end) {
        CHECK(execution->ticketForEnd(end)->state.id()==previousFinal-endsPerReplay+end);
        if (index && index<=older.size()) CHECK(execution->ticketForEnd(end)!=older[index-1]->ticketForEnd(end));
      }
      if (index<retained) older.push_back(execution);
      executions.push(execution);
    }
    cs.join();CHECK(executions.pending()==0 && query->fifo.pending()==0);
    for (const auto& execution : older) {
      for (unsigned end=1;end<=endsPerReplay;++end) CHECK(execution->ticketForEnd(end)->state.endRecorded());
    }
    CHECK(query->sequence.current(query->sequence.read()));
    older.clear();query.reset();
  }
  CHECK(Query::live.load()==0 && Ticket::live.load()==0);
  std::printf("D3D11 command replay PASS checks=%u nested_chunks=%u nested_ends=%u repeated_ends=%u replays=%u ends_per_replay=%u retained=%u live_queries=%u live_tickets=%u gpu_execution=0 predication_admission=0\n",checks.load(),nestedChunks,nestedEnds,repeated,concurrentReplays,endsPerReplay,retained,Query::live.load(),Ticket::live.load());
}
