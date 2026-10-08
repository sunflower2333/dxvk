// SPDX-License-Identifier: Zlib
// Actual ticket/evaluator/order/replay helpers; CS state/payload/availability modeled.
#include "../src/d3d11/d3d11_predicate_action.h"
#include "../src/d3d11/d3d11_predicate_ticket.h"
#include "../src/d3d11/d3d11_command_replay.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <vector>

static unsigned checks,actions,suppressed,chunks,bindings,submissions,fields;
#define CHECK(v) do { ++checks;if (!(v)) { std::fprintf(stderr,"predicate dispatch control failure line=%d\n",__LINE__);std::abort(); } } while (0)
using Decision=dxvk::D3D11PredicateDecision;
using Status=dxvk::DxvkGpuQueryStatus;
struct Ticket {
  dxvk::D3D11QueryTicketState state;
  dxvk::DxvkQueryData data={};
  bool available=false;
  explicit Ticket(uint64_t id) : state(id) { state.issueEnd(); }
};
using Owner=std::shared_ptr<Ticket>;
struct Query { Owner current=std::make_shared<Ticket>(0); };
using QueryRef=std::shared_ptr<Query>;
using Order=dxvk::D3D11CommandOrder<QueryRef>;
using Replay=dxvk::D3D11CommandReplay<QueryRef,Owner>;
struct Payload {
  static unsigned live;
  std::array<uint32_t,24> output{},append{};
  unsigned count=0;
  Payload() { ++live;reset(); }
  ~Payload() { --live; }
  void reset() {
    count=0;
    for (unsigned i=0;i<24;++i) output[i]=append[i]=0x7e91c523u^(i*0x1357acdfu);
  }
};
unsigned Payload::live=0;
struct Args {
  static unsigned live;
  std::array<unsigned,5> words{0,3,2,2,0};
  Args() { ++live; }
  ~Args() { --live; }
};
unsigned Args::live=0;
struct State {
  std::shared_ptr<Payload> payload;
  std::shared_ptr<Args> args;
  uint32_t shader=0;
};
static Decision evaluate(const dxvk::D3D11ReplayPredicate<QueryRef,Owner>& binding) {
  return dxvk::D3D11EvaluatePredicateAction(bool(binding.query),binding.hint,binding.value,
    [&] (bool* result) {
      return dxvk::D3D11ReadPredicateTicket(&binding.ticket->state,dxvk::D3D11PredicateKind::Occlusion,false,result,
        [&] (dxvk::DxvkQueryData& data) {
          if (!binding.ticket->available) return Status::Pending;
          data=binding.ticket->data;return Status::Available;
        });
    },[&] { ++submissions;binding.ticket->state.completeEnd();binding.ticket->available=true;return true; },
    [] { return true; },[] {});
}
static void checkPayload(const Payload& payload,bool execute,uint32_t shader) {
  CHECK(payload.count==(execute ? 17u : 5u));++fields;
  for (unsigned i=0;i<24;++i) {
    const uint32_t initial=0x7e91c523u^(i*0x1357acdfu);
    CHECK(payload.output[i]==(execute && i<12 ? shader|i : initial));++fields;
    CHECK(payload.append[i]==(execute && i>=5 && i<17 ? 0xa11ec0deu : initial));++fields;
  }
}
int main() {
  unsigned replays=0;
  for (bool indirect : {false,true}) for (bool nested : {false,true}) for (bool restore : {false,true}) {
    auto parent=std::make_shared<Query>();parent->current->state.completeEnd();parent->current->available=true;
    const dxvk::D3D11ReplayPredicate<QueryRef,Owner> apiParent{parent,parent->current,0,false};
    CHECK(evaluate(apiParent)==Decision::Suppress);
    State state;
    const unsigned count=nested ? 5 : 4;
    std::vector<std::shared_ptr<Payload>> observers;
    std::vector<std::weak_ptr<Args>> argsObservers;
    std::vector<std::function<void()>> bindCommands;
    for (unsigned i=0;i<count;++i) {
      auto resource=std::make_shared<Payload>();auto args=std::make_shared<Args>();
      observers.push_back(resource);argsObservers.push_back(args);
      // Bind chunks own the slices/views/args; dispatch captures only numbers.
      bindCommands.emplace_back([&,resource,args,i] {
        state={resource,args,i==4 ? 0xd2700000u : 0xd1700000u};
        resource->count=5;++bindings;
      });
      resource.reset();args.reset();
    }
    std::vector<std::function<void()>> innerChunks;
    Order inner;
    auto bind=[&] (unsigned index) {
      inner.addChunk(innerChunks.size());innerChunks.push_back(bindCommands[index]);
    };
    auto unconditional=[&] {
      inner.addChunk(innerChunks.size());innerChunks.emplace_back([] {});
    };
    bind(0);inner.addAction(0);
    auto query=std::make_shared<Query>();inner.addQueryBegin(query,false);unconditional();inner.addQueryEnd(query);unconditional();
    inner.addPredicate(query,0,false);bind(1);inner.addAction(1);
    inner.addPredicate(QueryRef(),-17,false);bind(2);inner.addAction(2);
    inner.addQueryBegin(query,false);unconditional();inner.addQueryEnd(query);unconditional();
    inner.addPredicate(query,0,false);bind(3);inner.addAction(3);inner.addPredicate(QueryRef(),0,false);
    std::weak_ptr<Query> releasedQuery=query;query.reset();CHECK(!releasedQuery.expired());
    Order order;std::vector<std::function<void()>> chunkCommands;
    if (nested) {
      order.addPredicate(parent,0,false);order.addChunk(0);chunkCommands.push_back(bindCommands[4]);
      order.addPredicate(QueryRef(),0,false);order.append(inner,1,0);
      chunkCommands.insert(chunkCommands.end(),innerChunks.begin(),innerChunks.end());
      order.addPredicate(restore ? parent : QueryRef(),0,false);
      order.addChunk(chunkCommands.size());chunkCommands.push_back(bindCommands[4]);order.addAction(4);
    } else { order.append(inner,0,0);chunkCommands=innerChunks; }
    // All external argument owners are gone; only bind chunks hold them.
    for (const auto& arg : argsObservers) CHECK(!arg.expired());
    uint64_t id=0;
    for (unsigned repeat=0;repeat<3;++repeat) {
      for (const auto& resource : observers) resource->reset();
      Replay replay(order,[] (const QueryRef& q) { return q->current; },[&] (const auto& operation) {
        auto ticket=std::make_shared<Ticket>(++id);ticket->data.occlusion.samplesPassed=operation.endOccurrence==2;
        operation.query->current=ticket;return ticket;
      });
      std::array<bool,5> executed{};
      for (const auto& operation : replay.operations()) {
        if (operation.recorded.type==dxvk::D3D11RecordedOperationType::Chunk) {
          CHECK(operation.recorded.chunkId<chunkCommands.size());chunkCommands[operation.recorded.chunkId]();++chunks;continue;
        }
        if (operation.recorded.type!=dxvk::D3D11RecordedOperationType::Action) continue;
        const auto index=operation.recorded.actionId;CHECK(index<count);
        CHECK(state.payload==observers[index] && state.args && state.payload->count==5);
        if (index==0) CHECK(!operation.predicate.query && operation.predicate.value==0);
        if (index==2) CHECK(!operation.predicate.query && operation.predicate.value==-17);
        if (index==4) CHECK(bool(operation.predicate.query)==restore);
        const auto decision=evaluate(operation.predicate);CHECK(decision!=Decision::Invalid);
        if (decision==Decision::Execute) {
          const std::array<unsigned,3> groups=indirect ? std::array<unsigned,3>{state.args->words[1],state.args->words[2],state.args->words[3]} : std::array<unsigned,3>{3,2,2};
          for (unsigned z=0;z<groups[2];++z) for (unsigned y=0;y<groups[1];++y) for (unsigned x=0;x<groups[0];++x) {
            const unsigned i=x+3*y+6*z;CHECK(i<24 && state.payload->count<24);
            state.payload->output[i]=state.shader|i;state.payload->append[state.payload->count++]=0xa11ec0deu;
          }
          executed[index]=true;
        } else ++suppressed;
        ++actions;
      }
      CHECK(executed[0] && !executed[1] && executed[2] && executed[3]);
      if (nested) CHECK(executed[4]==!restore);
      CHECK(evaluate(apiParent)==Decision::Suppress);
      for (unsigned i=0;i<count;++i) checkPayload(*observers[i],executed[i],i==4 ? 0xd2700000u : 0xd1700000u);
      ++replays;
    }
    inner=Order();order=Order();CHECK(releasedQuery.expired());
    observers.clear();CHECK(Payload::live==count);
    innerChunks.clear();chunkCommands.clear();bindCommands.clear();state={};
    CHECK(Payload::live==0 && Args::live==0);
    for (const auto& arg : argsObservers) CHECK(arg.expired());
  }
  CHECK(actions==108 && suppressed==30 && chunks==216 && bindings==120 && submissions==48 && replays==24 && fields==5292);
  std::printf("D3D11 predicate dispatch PASS checks=%u actions=%u suppressed=%u chunks=%u bindings=%u submissions=%u fields=%u replays=%u live_payloads=0 live_args=0 gpu_execution=0 predication_complete=0\n",
    checks,actions,suppressed,chunks,bindings,submissions,fields,replays);
}
