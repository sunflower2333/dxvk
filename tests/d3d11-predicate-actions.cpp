// SPDX-License-Identifier: Zlib
// Executes production evaluator, ticket decoder and replay order helpers.
// Backend GPU availability and copied words are modeled, not native execution.
#include "../src/d3d11/d3d11_predicate_action.h"
#include "../src/d3d11/d3d11_predicate_ticket.h"
#include "../src/d3d11/d3d11_command_replay.h"
#include "../src/d3d11/d3d11_query_sequence.h"
#include <atomic>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>
#include <vector>

static std::atomic<unsigned> checks{0};
#define CHECK(v) do { ++checks; if (!(v)) { std::fprintf(stderr,"predicate action check failed line=%d\n",__LINE__);std::abort(); } } while (0)
using Status=dxvk::DxvkGpuQueryStatus;
using Decision=dxvk::D3D11PredicateDecision;
struct Ticket {
  static std::atomic<unsigned> live;
  dxvk::D3D11QueryTicketState state;
  std::atomic<Status> status{Status::Pending};
  std::atomic<unsigned> polls{0};
  dxvk::DxvkQueryData data={};
  explicit Ticket(uint64_t id) : state(id) { ++live; }
  ~Ticket() { --live; }
};
std::atomic<unsigned> Ticket::live{0};
using Owner=std::shared_ptr<Ticket>;
struct Query {
  static unsigned live;
  Owner current;
  dxvk::D3D11QuerySequence publicSequence;
  Query() : current(std::make_shared<Ticket>(0)) { ++live; }
  ~Query() { --live; }
};
unsigned Query::live=0;
using QueryRef=std::shared_ptr<Query>;
using Order=dxvk::D3D11CommandOrder<QueryRef>;
using Replay=dxvk::D3D11CommandReplay<QueryRef,Owner>;
static Status read(const Owner& ticket,bool* value) {
  return dxvk::D3D11ReadPredicateTicket(&ticket->state,dxvk::D3D11PredicateKind::Occlusion,false,value,
    [&] (dxvk::DxvkQueryData& data) {
      ++ticket->polls;
      const auto status=ticket->status.load(std::memory_order_acquire);
      if (status==Status::Available) data=ticket->data;
      return status;
    });
}
static Decision evaluate(const Owner& ticket,bool bound,bool hint,int32_t value,
    unsigned& submissions,unsigned& yields) {
  return dxvk::D3D11EvaluatePredicateAction(bound,hint,value,
    [&] (bool* output) { return read(ticket,output); },
    [&] { ++submissions;ticket->state.completeEnd();ticket->status=Status::Available;return true; },
    [] { return true; },[&] { ++yields; });
}
int main() {
  // Equal result suppresses, all other cases execute. BOOLs are logical truth,
  // while recorded values retain the original noncanonical integer exactly.
  for (bool result : {false,true}) for (int32_t value : {0,1,-1,42}) {
    auto ticket=std::make_shared<Ticket>(1);
    ticket->state.issueEnd();ticket->state.completeEnd();
    ticket->data.occlusion.samplesPassed=result ? uint64_t(1)<<63 : 0;
    ticket->status=Status::Available;
    unsigned submissions=0,yields=0;
    const auto decision=evaluate(ticket,true,false,value,submissions,yields);
    CHECK(decision==(result==bool(value) ? Decision::Suppress : Decision::Execute));
    CHECK(ticket->polls==1 && submissions==0 && yields==0);
    CHECK(evaluate(ticket,false,false,value,submissions,yields)==Decision::Execute);
    CHECK(evaluate(ticket,true,true,value,submissions,yields)==Decision::Execute);
    CHECK(ticket->polls==1 && submissions==0 && yields==0);
  }
  {
    auto ticket=std::make_shared<Ticket>(2);ticket->state.issueEnd();
    unsigned submissions=0,yields=0;
    CHECK(evaluate(ticket,true,false,0,submissions,yields)==Decision::Suppress);
    CHECK(submissions==1 && ticket->polls==1 && yields==0);
  }
  for (auto status : {Status::Invalid,Status::Failed,static_cast<Status>(99)}) {
    unsigned polls=0,submissions=0,yields=0;
    auto decision=dxvk::D3D11EvaluatePredicateAction(true,false,0,
      [&] (bool*) { ++polls;return status; },[&] { ++submissions;return true; },
      [] { return true; },[&] { ++yields; });
    CHECK(decision==(status==Status::Invalid ? Decision::Invalid : Decision::Failed));
    CHECK(polls==1 && submissions==0 && yields==0);
  }
  for (unsigned mode=0;mode<3;++mode) {
    unsigned polls=0,submissions=0,yields=0,health=0;
    auto decision=dxvk::D3D11EvaluatePredicateAction(true,false,0,
      [&] (bool*) { ++polls;return Status::Pending; },
      [&] { ++submissions;return mode!=1; },
      [&] { return ++health <= (mode==0 ? 0u : 2u); },[&] { ++yields; });
    CHECK(decision==Decision::Failed);
    CHECK(polls==(mode==0 ? 0u : mode==1 ? 1u : 2u));
    CHECK(submissions==(mode==0 ? 0u : 1u) && yields==(mode==2 ? 1u : 0u));
  }
  // A guaranteed pending result cannot escape because of elapsed poll count.
  {
    unsigned polls=0,submissions=0,yields=0;
    auto decision=dxvk::D3D11EvaluatePredicateAction(true,false,0,
      [&] (bool* value) { ++polls;if (polls<10001) return Status::Pending;*value=true;return Status::Available; },
      [&] { ++submissions;return true; },[] { return true; },[&] { ++yields; });
    CHECK(decision==Decision::Execute && polls==10001 && submissions==1 && yields==9999);
  }

  unsigned replayActions=0,replayChunks=0,suppressed=0,submissions=0;
  {
    auto query=std::make_shared<Query>();
    Order inner;
    inner.addQueryBegin(query,false);inner.addChunk(0);inner.addQueryEnd(query);inner.addChunk(1);
    inner.addPredicate(query,0,false);inner.addAction(0);
    inner.addPredicate(QueryRef(),42,false);inner.addChunk(2);
    inner.addQueryBegin(query,false);inner.addChunk(3);inner.addQueryEnd(query);inner.addChunk(4);
    inner.addPredicate(query,0,false);inner.addAction(1);inner.addPredicate(QueryRef(),0,false);
    Order outer;outer.addChunk(0);outer.append(inner,1,0);outer.addChunk(6);outer.append(inner,7,2);
    CHECK(outer.endOccurrences()==4);
    query.reset(); // Ordered operations are actual shared ownership pins.
    CHECK(Query::live==1);
    std::vector<Owner> retained;
    for (unsigned replayIndex=0;replayIndex<1000;++replayIndex) {
      uint64_t generation=uint64_t(replayIndex)*4;
      Replay replay(outer,[] (const QueryRef& q) { return q->current; },
        [&] (const auto& operation) {
          auto ticket=std::make_shared<Ticket>(++generation);ticket->state.issueEnd();
          // First End false, following End true. Prefix accounting is still
          // all-End-up-front; the evaluator must read each exact result.
          ticket->data.occlusion.samplesPassed=operation.endOccurrence%2==0;
          operation.query->publicSequence.deferEnd();operation.query->current=ticket;
          return ticket;
        });
      std::array<uint32_t,4> words{{0xb5ad09ce,0xb5ad09ce,0xb5ad09ce,0xb5ad09ce}};
      unsigned actionIndex=0,chunkIndex=0;
      for (const auto& operation : replay.operations()) {
        if (operation.recorded.type==dxvk::D3D11RecordedOperationType::Chunk) {
          CHECK(operation.recorded.chunkId==chunkIndex++);++replayChunks;
        }
        if (operation.recorded.type!=dxvk::D3D11RecordedOperationType::Action) continue;
        CHECK(operation.recorded.actionId==actionIndex++);
        unsigned yielded=0;
        auto decision=evaluate(operation.predicate.ticket,bool(operation.predicate.query),
          operation.predicate.hint,operation.predicate.value,submissions,yielded);
        CHECK(yielded==0 && operation.predicate.query->publicSequence.read().pending==4);
        if (decision==Decision::Execute) words[operation.recorded.actionId]=0x8759ec31;
        else { CHECK(decision==Decision::Suppress);++suppressed; }
        ++replayActions;
      }
      CHECK(words[0]==0xb5ad09ce && words[1]==0x8759ec31 && words[2]==0xb5ad09ce && words[3]==0x8759ec31);
      CHECK(actionIndex==4 && chunkIndex==12);
      // Completion of public prefixes stays a separate existing mechanism.
      for (const auto& end : replay.endTickets()) end.query->publicSequence.completeEnd();
      if (replayIndex%32==0) retained.push_back(replay.endTickets().front().ticket);
      for (const auto& ticket : retained) {
        unsigned count=0,yielded=0;
        CHECK(evaluate(ticket,true,false,0,count,yielded)==Decision::Suppress && count==0 && yielded==0);
      }
    }
    CHECK(retained.size()==32 && replayActions==4000 && replayChunks==12000 && suppressed==2000 && submissions==4000);
  }
  CHECK(Query::live==0 && Ticket::live==0);
  // Actual release/acquire End markers plus evaluator are driven by a distinct
  // completion thread. No unsubmitted worker command is responsible for polls.
  {
    auto ticket=std::make_shared<Ticket>(5000);ticket->state.issueEnd();
    ticket->data.occlusion.samplesPassed=1;
    std::atomic<bool> submitted{false};unsigned polls=0,submission=0;
    std::thread completion([&] {
      while (!submitted.load(std::memory_order_acquire)) std::this_thread::yield();
      ticket->state.completeEnd();ticket->status.store(Status::Available,std::memory_order_release);
    });
    auto decision=dxvk::D3D11EvaluatePredicateAction(true,false,1,
      [&] (bool* value) { ++polls;return read(ticket,value); },
      [&] { ++submission;submitted.store(true,std::memory_order_release);return true; },
      [] { return true; },[] { std::this_thread::yield(); });
    completion.join();CHECK(decision==Decision::Suppress && submission==1 && polls>=2);
  }
  CHECK(Ticket::live==0);
  std::printf("D3D11 predicate actions PASS checks=%u actions=%u suppressed=%u chunks=%u replays=1000 retained=32 live_queries=%u live_tickets=%u gpu_execution=0 predication_complete=0\n",
    checks.load(),replayActions,suppressed,replayChunks,Query::live,Ticket::live.load());
}
