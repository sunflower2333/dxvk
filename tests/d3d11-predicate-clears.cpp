// SPDX-License-Identifier: Zlib
// Actual evaluator/ticket/order/replay helpers; RGBA GPU clears are modeled.
#include "../src/d3d11/d3d11_predicate_action.h"
#include "../src/d3d11/d3d11_predicate_ticket.h"
#include "../src/d3d11/d3d11_command_replay.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <vector>

static unsigned checks,actions,suppressed,submissions,pixels,chunks;
#define CHECK(v) do { ++checks;if (!(v)) { std::fprintf(stderr,"predicate clear check failed line=%d\n",__LINE__);std::abort(); } } while (0)
using Status=dxvk::DxvkGpuQueryStatus;
using Decision=dxvk::D3D11PredicateDecision;
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
struct Attachment {
  static unsigned live;
  std::array<uint32_t,256> image{};
  std::array<uint32_t,256> shadow{};
  Attachment() { ++live;reset(); }
  ~Attachment() { --live; }
  void reset() {
    for (unsigned i=0;i<image.size();++i) image[i]=shadow[i]=0x28a9f573u^(i*0x1357acdfu);
  }
};
unsigned Attachment::live=0;
static Decision evaluate(const dxvk::D3D11ReplayPredicate<QueryRef,Owner>& predicate) {
  return dxvk::D3D11EvaluatePredicateAction(bool(predicate.query),predicate.hint,predicate.value,
    [&] (bool* result) {
      return dxvk::D3D11ReadPredicateTicket(&predicate.ticket->state,dxvk::D3D11PredicateKind::Occlusion,false,result,
        [&] (dxvk::DxvkQueryData& data) {
          if (!predicate.ticket->available) return Status::Pending;
          data=predicate.ticket->data;return Status::Available;
        });
    },[&] { ++submissions;predicate.ticket->state.completeEnd();predicate.ticket->available=true;return true; },
    [] { return true; },[] {});
}
int main() {
  for (bool nested : {false,true}) for (bool restore : {false,true}) {
    auto parent=std::make_shared<Query>();parent->current->state.completeEnd();parent->current->available=true;
    const dxvk::D3D11ReplayPredicate<QueryRef,Owner> immediateBinding{parent,parent->current,0,false};
    CHECK(evaluate(immediateBinding)==Decision::Suppress);
    auto query=std::make_shared<Query>();
    Order inner;inner.addAction(0); // default NULL/FALSE before any own binding
    inner.addQueryBegin(query,false);inner.addChunk(0);inner.addQueryEnd(query);inner.addChunk(1);
    inner.addPredicate(query,0,false);inner.addAction(1);
    inner.addPredicate(QueryRef(),-17,false);inner.addAction(2);
    inner.addQueryBegin(query,false);inner.addChunk(2);inner.addQueryEnd(query);inner.addChunk(3);
    inner.addPredicate(query,0,false);inner.addAction(3);inner.addPredicate(QueryRef(),0,false);
    Order order;
    if (nested) {
      order.addPredicate(parent,0,false);
      order.addPredicate(QueryRef(),0,false); // ExecuteCommandList reset
      order.append(inner,0,0);
      order.addPredicate(restore ? parent : QueryRef(),0,false);
      order.addAction(4);
    } else {
      order.append(inner,0,0);
    }
    std::vector<std::shared_ptr<Attachment>> observers;
    std::vector<std::function<void()>> commands;
    const unsigned count=nested ? 5 : 4;
    for (unsigned i=0;i<count;++i) {
      auto resource=std::make_shared<Attachment>();observers.push_back(resource);
      // Model separate view/shadow owners retained by the recorded action.
      auto view=resource,shadow=resource;
      commands.emplace_back([view,shadow,i] {
        const uint32_t color=i==2 ? 0xff00ff00u : i==0 ? 0xffff0000u : 0xff0000ffu;
        view->image.fill(color);shadow->shadow.fill(color);
      });
      resource.reset();view.reset();shadow.reset();
    }
    query.reset();
    uint64_t id=0;
    for (unsigned replayIndex=0;replayIndex<250;++replayIndex) {
      for (const auto& observer : observers) observer->reset();
      Replay replay(order,[] (const QueryRef& q) { return q->current; },
        [&] (const auto& operation) {
          auto result=std::make_shared<Ticket>(++id);
          result->data.occlusion.samplesPassed=operation.endOccurrence==2;
          operation.query->current=result;return result;
        });
      std::array<bool,5> executed{};
      for (const auto& operation : replay.operations()) {
        if (operation.recorded.type==dxvk::D3D11RecordedOperationType::Chunk) { ++chunks;continue; }
        if (operation.recorded.type!=dxvk::D3D11RecordedOperationType::Action) continue;
        const auto index=operation.recorded.actionId;
        CHECK(index<count);
        if (index==0) CHECK(!operation.predicate.query && operation.predicate.value==0);
        if (index==2) CHECK(!operation.predicate.query && operation.predicate.value==-17);
        if (index==4) CHECK(bool(operation.predicate.query)==restore);
        const auto decision=evaluate(operation.predicate);
        CHECK(decision==Decision::Execute || decision==Decision::Suppress);
        if (decision==Decision::Execute) { commands[index]();executed[index]=true; }
        else ++suppressed;
        ++actions;
      }
      CHECK(executed[0] && !executed[1] && executed[2] && executed[3]);
      if (nested) CHECK(executed[4]==!restore);
      CHECK(evaluate(immediateBinding)==Decision::Suppress); // parent unchanged
      for (unsigned i=0;i<count;++i) for (unsigned x=0;x<256;++x) {
        const uint32_t initial=0x28a9f573u^(x*0x1357acdfu);
        const uint32_t color=i==2 ? 0xff00ff00u : i==0 ? 0xffff0000u : 0xff0000ffu;
        CHECK(observers[i]->image[x]==(executed[i] ? color : initial));
        CHECK(observers[i]->shadow[x]==observers[i]->image[x]);++pixels;
      }
    }
    observers.clear();CHECK(Attachment::live==count);commands.clear();CHECK(Attachment::live==0);
  }
  CHECK(actions==4500 && suppressed==1250 && chunks==4000 && submissions==2000 && pixels==1152000);
  CHECK(Attachment::live==0);
  std::printf("D3D11 predicate clears PASS checks=%u actions=%u suppressed=%u chunks=%u submissions=%u pixels=%u replays=1000 default_null=1 explicit_null=1 nested_restore=2 attachment_owners=0 gpu_execution=0 predication_complete=0\n",
    checks,actions,suppressed,chunks,submissions,pixels);
}
