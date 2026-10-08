// SPDX-License-Identifier: Zlib
// Actual readiness/decode helper and backend data types; GPU polling is modeled.
#include "../src/d3d11/d3d11_predicate_ticket.h"
#include "../src/d3d11/d3d11_query_sequence.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

static std::atomic<unsigned> checks{0};
#define CHECK(v) do { checks.fetch_add(1,std::memory_order_relaxed);if (!(v)) { std::fprintf(stderr,"predicate ticket check failed line=%d\n",__LINE__);std::abort(); } } while (0)
using Status=dxvk::DxvkGpuQueryStatus;
using Kind=dxvk::D3D11PredicateKind;
using State=dxvk::D3D11QueryTicketState;
static_assert(sizeof(dxvk::DxvkQueryData)==88);
static_assert(sizeof(Status)==4);
static_assert(unsigned(Status::Invalid)==0 && unsigned(Status::Pending)==1 && unsigned(Status::Available)==2 && unsigned(Status::Failed)==3);
struct Ticket {
  static std::atomic<unsigned> live;
  State state;
  dxvk::DxvkQueryData data = { };
  std::atomic<Status> status{Status::Pending};
  std::atomic<unsigned> polls{0};
  explicit Ticket(uint64_t id) : state(id) { live.fetch_add(1); }
  ~Ticket() { live.fetch_sub(1); }
};
std::atomic<unsigned> Ticket::live{0};
using Owner=std::shared_ptr<Ticket>;
static Status read(const Owner& ticket,bool& result,bool hint=false) {
  return dxvk::D3D11ReadPredicateTicket(&ticket->state,Kind::Occlusion,hint,&result,
    [&] (dxvk::DxvkQueryData& data) {
      ticket->polls.fetch_add(1,std::memory_order_relaxed);
      const auto status=ticket->status.load(std::memory_order_acquire);
      if (status==Status::Available) data=ticket->data;
      return status;
    });
}
int main() {
  State state(1);
  unsigned polls=0;
  dxvk::DxvkQueryData data = { };
  Status backend=Status::Available;
  auto poll=[&] (dxvk::DxvkQueryData& output) { ++polls;output=data;return backend; };
  for (bool canary : {false,true}) {
    bool value=canary;
    CHECK(dxvk::D3D11ReadPredicateTicket(nullptr,Kind::Occlusion,false,&value,poll)==Status::Invalid && value==canary && polls==0);
    CHECK(dxvk::D3D11ReadPredicateTicket(&state,Kind::Occlusion,false,&value,poll)==Status::Invalid && value==canary && polls==0);
    CHECK(dxvk::D3D11ReadPredicateTicket(&state,Kind::Invalid,false,&value,poll)==Status::Invalid && value==canary && polls==0);
    CHECK(dxvk::D3D11ReadPredicateTicket(&state,static_cast<Kind>(99),false,&value,poll)==Status::Invalid && value==canary && polls==0);
  }
  state.issueEnd();CHECK(state.endIssued() && !state.endRecorded());
  for (bool canary : {false,true}) {
    bool value=canary;
    CHECK(dxvk::D3D11ReadPredicateTicket(&state,Kind::Occlusion,false,&value,poll)==Status::Pending && value==canary && polls==0);
    CHECK(dxvk::D3D11ReadPredicateTicket(&state,Kind::Occlusion,true,&value,poll)==Status::Invalid && value==canary && polls==0);
  }
  State unissued(2);unissued.completeEnd();
  bool value=true;
  CHECK(dxvk::D3D11ReadPredicateTicket(&unissued,Kind::Occlusion,false,&value,poll)==Status::Invalid && value && polls==0);
  state.completeEnd();CHECK(state.endIssued() && state.endRecorded());
  for (auto status : {Status::Invalid,Status::Pending,Status::Failed,static_cast<Status>(99)}) {
    backend=status;
    for (bool canary : {false,true}) {
      value=canary;const auto before=polls;
      CHECK(dxvk::D3D11ReadPredicateTicket(&state,Kind::Occlusion,false,&value,poll)==status && value==canary && polls==before+1);
    }
  }
  backend=Status::Available;
  const uint64_t max=std::numeric_limits<uint64_t>::max();
  const uint64_t samples[]={0,1,2,0xffffffffu,uint64_t(1)<<32,uint64_t(1)<<63,max};
  for (auto sample : samples) {
    data.occlusion.samplesPassed=sample;value=sample==0;const auto before=polls;
    CHECK(dxvk::D3D11ReadPredicateTicket(&state,Kind::Occlusion,false,&value,poll)==Status::Available && value==(sample!=0) && polls==before+1);
  }
  struct Pair { uint64_t written,needed; };
  const Pair overflow[]={{0,0},{0,1},{1,0},{1,1},{max,max},{max-1,max},{max,max-1},{uint64_t(1)<<63,0},{0,uint64_t(1)<<63},{uint64_t(1)<<32,0xffffffffu}};
  for (const auto& pair : overflow) {
    data.xfbStream.primitivesWritten=pair.written;data.xfbStream.primitivesNeeded=pair.needed;
    const bool expected=pair.needed>pair.written;value=!expected;const auto before=polls;
    CHECK(dxvk::D3D11ReadPredicateTicket(&state,Kind::StreamOverflow,false,&value,poll)==Status::Available && value==expected && polls==before+1);
  }
  for (Kind kind : {Kind::Occlusion,Kind::StreamOverflow}) {
    for (bool canary : {false,true}) {
      value=canary;const auto before=polls;
      CHECK(dxvk::D3D11ReadPredicateTicket(&state,kind,true,&value,poll)==Status::Invalid && value==canary && polls==before);
      CHECK(dxvk::D3D11ReadPredicateTicket(&state,Kind::Invalid,false,&value,poll)==Status::Invalid && value==canary && polls==before);
    }
    const auto before=polls;
    CHECK(dxvk::D3D11ReadPredicateTicket(&state,kind,false,nullptr,poll)==Status::Available && polls==before+1);
  }

  // Exact old result may be read while later public prefixed Ends are pending.
  {
    dxvk::D3D11QuerySequence sequence;
    auto older=std::make_shared<Ticket>(3),latest=std::make_shared<Ticket>(4);
    older->state.issueEnd();latest->state.issueEnd();
    for (unsigned end=0;end<4096;++end) sequence.deferEnd();
    older->state.completeEnd();sequence.completeEnd();
    older->data.occlusion.samplesPassed=0;older->status.store(Status::Available,std::memory_order_release);
    value=true;CHECK(read(older,value)==Status::Available && !value && sequence.read().pending==4095);
    value=false;CHECK(read(latest,value)==Status::Pending && !value && latest->polls.load()==0);
    latest->state.completeEnd();latest->data.occlusion.samplesPassed=1;latest->status.store(Status::Available,std::memory_order_release);
    value=false;CHECK(read(latest,value)==Status::Available && value);
    value=true;CHECK(read(older,value)==Status::Available && !value);
    const auto before=older->polls.load();value=true;
    CHECK(read(older,value,true)==Status::Invalid && value && older->polls.load()==before);
  }
  CHECK(Ticket::live.load()==0);

  constexpr unsigned issues=100000,retained=128;
  std::atomic<unsigned> observations{0};
  {
    dxvk::D3D11DeferredQueryTickets<Owner> cs,gpu;
    std::vector<Owner> older;
    for (unsigned id=1;id<=retained;++id) {
      auto ticket=std::make_shared<Ticket>(id);ticket->state.issueEnd();older.push_back(ticket);cs.push(ticket);
    }
    std::atomic<bool> done{false};
    std::thread csThread([&] {
      for (unsigned id=1;id<=issues;) {
        auto ticket=cs.take();if (!ticket) { std::this_thread::yield();continue; }
        CHECK(ticket->state.id()==id && ticket->state.endIssued() && !ticket->state.endRecorded());
        ticket->state.completeEnd();gpu.push(ticket);++id;
      }
    });
    std::thread gpuThread([&] {
      for (unsigned id=1;id<=issues;) {
        auto ticket=gpu.take();if (!ticket) { std::this_thread::yield();continue; }
        CHECK(ticket->state.id()==id && ticket->state.endRecorded());
        ticket->data.occlusion.samplesPassed=(id&1) ? std::numeric_limits<uint64_t>::max() : 0;
        ticket->status.store(Status::Available,std::memory_order_release);++id;
      }
    });
    std::thread reader([&] {
      while (!done.load(std::memory_order_acquire)) {
        for (const auto& ticket : older) {
          bool output=true;
          const auto status=read(ticket,output);
          if (status==Status::Available) {
            if (output!=bool(ticket->state.id()&1)) std::abort();
            observations.fetch_add(1,std::memory_order_relaxed);
          } else if (status!=Status::Pending || !output) std::abort();
          const auto before=ticket->polls.load();output=false;
          if (read(ticket,output,true)!=Status::Invalid || output || ticket->polls.load()!=before) std::abort();
        }
      }
    });
    Owner current;
    for (unsigned id=retained+1;id<=issues;++id) {
      current=std::make_shared<Ticket>(id);current->state.issueEnd();cs.push(current);
    }
    csThread.join();gpuThread.join();done.store(true,std::memory_order_release);reader.join();
    CHECK(cs.pending()==0 && gpu.pending()==0 && current->state.id()==issues);
    value=true;CHECK(read(current,value)==Status::Available && !value);
    for (const auto& ticket : older) {
      value=false;CHECK(read(ticket,value)==Status::Available && value==bool(ticket->state.id()&1));
    }
    current.reset();older.clear();
  }
  CHECK(Ticket::live.load()==0);
  std::printf("D3D11 predicate ticket PASS checks=%u issues=%u prefix=4096 retained=%u observations=%u live_tickets=%u actual_backend_data_types=1 gpu_execution=0 predication_admission=0\n",checks.load(),issues,retained,observations.load(),Ticket::live.load());
}
