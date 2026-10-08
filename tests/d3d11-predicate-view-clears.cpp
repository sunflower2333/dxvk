// SPDX-License-Identifier: Zlib
// Actual predicate/order/replay helpers; view payloads and GPU completion modeled.
#include "../src/d3d11/d3d11_predicate_action.h"
#include "../src/d3d11/d3d11_predicate_ticket.h"
#include "../src/d3d11/d3d11_command_replay.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <vector>

static unsigned checks,actions,suppressed,chunks,submissions,fields;
#define CHECK(v) do { ++checks;if (!(v)) { std::fprintf(stderr,"predicate view clear failure line=%d\n",__LINE__);std::abort(); } } while (0)
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
  std::array<uint32_t,16> depth{};
  std::array<uint8_t,16> stencil{};
  std::array<std::array<uint32_t,64>,5> uav{};
  Payload() { ++live;reset(); }
  ~Payload() { --live; }
  void reset() {
    depth.fill(0x3f400000u);stencil.fill(0x3cu);
    for (auto& target : uav) for (unsigned i=0;i<target.size();++i) target[i]=0x7e91c523u^(i*0x1357acdfu);
  }
};
unsigned Payload::live=0;
static constexpr std::array<uint32_t,4> integer{0xdeadbeefu,0x01234567u,0x89abcdefu,0xa55a5aa5u};
static constexpr std::array<uint32_t,4> floating{0x3e800000u,0xc0000000u,0x40400000u,0x3f800000u};
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
static void checkPayload(const Payload& payload,bool executed,unsigned aspects) {
  for (unsigned i=0;i<16;++i) {
    CHECK(payload.depth[i]==(executed && (aspects&1) ? 0x3e800000u : 0x3f400000u));
    CHECK(payload.stencil[i]==(executed && (aspects&2) ? 0xa7u : 0x3cu));fields+=2;
  }
  for (unsigned kind=0;kind<5;++kind) for (unsigned i=0;i<64;++i) {
    const uint32_t expected=kind==0 ? integer[0] : kind<3 ? integer[i%4] : floating[i%4];
    CHECK(payload.uav[kind][i]==(executed ? expected : 0x7e91c523u^(i*0x1357acdfu)));++fields;
  }
}
int main() {
  unsigned replays=0;
  for (bool nested : {false,true}) for (bool restore : {false,true}) {
    auto parent=std::make_shared<Query>();parent->current->state.completeEnd();parent->current->available=true;
    const dxvk::D3D11ReplayPredicate<QueryRef,Owner> parentBinding{parent,parent->current,0,false};
    CHECK(evaluate(parentBinding)==Decision::Suppress);
    auto query=std::make_shared<Query>();
    Order inner;inner.addAction(0); // List default NULL/FALSE.
    inner.addQueryBegin(query,false);inner.addChunk(0);inner.addQueryEnd(query);inner.addChunk(1);
    inner.addPredicate(query,0,false);inner.addAction(1);
    inner.addPredicate(QueryRef(),-17,false);inner.addAction(2);
    inner.addQueryBegin(query,false);inner.addChunk(2);inner.addQueryEnd(query);inner.addChunk(3);
    inner.addPredicate(query,0,false);inner.addAction(3);inner.addPredicate(QueryRef(),0,false);
    Order order;
    if (nested) {
      order.addPredicate(parent,0,false);order.addPredicate(QueryRef(),0,false);
      order.append(inner,0,0);order.addPredicate(restore ? parent : QueryRef(),0,false);order.addAction(4);
    } else order.append(inner,0,0);
    const unsigned count=nested ? 5 : 4;
    std::vector<std::shared_ptr<Payload>> observers;
    std::vector<std::function<void()>> commands;
    for (unsigned i=0;i<count;++i) {
      auto resource=std::make_shared<Payload>();observers.push_back(resource);
      auto image=resource,buffer=resource,attachment=resource;
      commands.emplace_back([image,buffer,attachment,i] {
        const unsigned aspects=i==0 ? 1 : i==2 ? 2 : 3;
        if (aspects&1) attachment->depth.fill(0x3e800000u);
        if (aspects&2) attachment->stencil.fill(0xa7u);
        for (unsigned kind=0;kind<5;++kind) for (unsigned word=0;word<64;++word) {
          auto& target=kind==0 || kind==1 || kind==3 ? buffer->uav[kind] : image->uav[kind];
          target[word]=kind==0 ? integer[0] : kind<3 ? integer[word%4] : floating[word%4];
        }
      });
      resource.reset();image.reset();buffer.reset();attachment.reset();
    }
    std::weak_ptr<Query> releasedQuery=query;query.reset();CHECK(!releasedQuery.expired());
    uint64_t id=0;
    for (unsigned repeat=0;repeat<3;++repeat) {
      for (const auto& observer : observers) observer->reset();
      Replay replay(order,[] (const QueryRef& q) { return q->current; },[&] (const auto& operation) {
        auto ticket=std::make_shared<Ticket>(++id);ticket->data.occlusion.samplesPassed=operation.endOccurrence==2;
        operation.query->current=ticket;return ticket;
      });
      std::array<bool,5> executed{};
      for (const auto& operation : replay.operations()) {
        if (operation.recorded.type==dxvk::D3D11RecordedOperationType::Chunk) { ++chunks;continue; }
        if (operation.recorded.type!=dxvk::D3D11RecordedOperationType::Action) continue;
        const auto index=operation.recorded.actionId;CHECK(index<count);
        if (index==0) CHECK(!operation.predicate.query && operation.predicate.value==0);
        if (index==2) CHECK(!operation.predicate.query && operation.predicate.value==-17);
        if (index==4) CHECK(bool(operation.predicate.query)==restore);
        const auto decision=evaluate(operation.predicate);CHECK(decision!=Decision::Invalid);
        if (decision==Decision::Execute) { commands[index]();executed[index]=true; }
        else ++suppressed;
        ++actions;
      }
      CHECK(executed[0] && !executed[1] && executed[2] && executed[3]);
      if (nested) CHECK(executed[4]==!restore);
      CHECK(evaluate(parentBinding)==Decision::Suppress);
      for (unsigned i=0;i<count;++i) checkPayload(*observers[i],executed[i],i==0 ? 1 : i==2 ? 2 : 3);
      ++replays;
    }
    inner=Order();order=Order();CHECK(releasedQuery.expired());
    observers.clear();CHECK(Payload::live==count);commands.clear();CHECK(Payload::live==0);
  }
  CHECK(actions==54 && suppressed==15 && chunks==48 && submissions==24 && replays==12 && fields==19008);
  CHECK(Payload::live==0);
  std::printf("D3D11 predicate view clears PASS checks=%u actions=%u suppressed=%u chunks=%u submissions=%u fields=%u replays=%u modeled_owners=0 gpu_execution=0 predication_complete=0\n",
    checks,actions,suppressed,chunks,submissions,fields,replays);
}
