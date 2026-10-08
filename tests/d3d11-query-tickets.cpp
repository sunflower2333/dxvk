// SPDX-License-Identifier: Zlib
// Production ticket completion/FIFO controls; backend payload is a host model.
#include "../src/d3d11/d3d11_query_ticket.h"
#include "../src/d3d11/d3d11_query_sequence.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>
#include <vector>

static std::atomic<unsigned> checks{0};
#define CHECK(value) do { checks.fetch_add(1,std::memory_order_relaxed); if (!(value)) std::abort(); } while (0)
struct Ticket {
  static std::atomic<unsigned> live;
  dxvk::D3D11QueryTicketState state;
  const unsigned expected;
  unsigned payload=0x65;
  std::atomic<bool> gpuAvailable{false};
  explicit Ticket(uint64_t id) : state(id),expected(unsigned(id&1)) { live.fetch_add(1); }
  ~Ticket() { live.fetch_sub(1); }
};
std::atomic<unsigned> Ticket::live{0};
using Reference=std::shared_ptr<Ticket>;
using Queue=dxvk::D3D11DeferredQueryTickets<Reference>;
static bool read(const Reference& ticket,unsigned& result) {
  if (!ticket->state.endRecorded() || !ticket->gpuAvailable.load(std::memory_order_acquire)) return false;
  result=ticket->payload;
  return true;
}
static void signal(const Reference& ticket) {
  ticket->payload=ticket->expected;
  ticket->gpuAvailable.store(true,std::memory_order_release);
}
int main() {
  Queue empty;
  CHECK(!empty.front() && !empty.take() && empty.pending()==0);
  auto first=std::make_shared<Ticket>(2);
  unsigned output=0x75;
  CHECK(first->state.id()==2 && !first->state.endRecorded());
  CHECK(!read(first,output) && output==0x75);
  first->state.completeEnd();
  CHECK(first->state.endRecorded());
  // Recording End is not GPU availability and must not publish payload yet.
  CHECK(!read(first,output) && output==0x75);
  signal(first); CHECK(read(first,output) && output==0);
  auto next=std::make_shared<Ticket>(3);
  CHECK(!next->state.endRecorded() && first->state.endRecorded());
  next->state.completeEnd();signal(next);
  CHECK(read(next,output) && output==1);
  CHECK(read(first,output) && output==0 && first->state.id()==2);
  std::weak_ptr<Ticket> weakFirst=first,weakNext=next;
  empty.push(first);empty.push(next);first.reset();next.reset();
  CHECK(!weakFirst.expired() && !weakNext.expired() && empty.pending()==2);
  auto begin=empty.front(),end=empty.take();
  CHECK(begin==end && begin->state.id()==2 && empty.pending()==1);
  begin.reset();end.reset();CHECK(weakFirst.expired());
  end=empty.take();CHECK(end->state.id()==3 && empty.pending()==0);
  CHECK(!weakNext.expired());end.reset();CHECK(weakNext.expired());
  CHECK(Ticket::live.load()==0);

  // Retain the public all-End-up-front policy while proving that the first
  // owned ticket can be CS/GPU-ready independently of future End tickets.
  dxvk::D3D11QuerySequence sequence;
  Queue deferred;
  Reference oldest,latest;
  constexpr unsigned prefixed=4096;
  for (unsigned id=1;id<=prefixed;++id) {
    auto ticket=std::make_shared<Ticket>(id);
    if (id==1) oldest=ticket;
    latest=ticket;deferred.push(ticket);sequence.deferEnd();
  }
  CHECK(sequence.read().pending==prefixed && deferred.pending()==prefixed);
  for (unsigned id=1;id<=prefixed;++id) {
    auto ticket=deferred.front();auto ended=deferred.take();
    CHECK(ticket==ended && ticket->state.id()==id && !ticket->state.endRecorded());
    ended->state.completeEnd();sequence.completeEnd();signal(ended);
    CHECK(ended->state.endRecorded());
    if (id==1) {
      output=0x75;CHECK(read(oldest,output) && output==1);
      CHECK(sequence.read().pending==prefixed-1 && !sequence.current(sequence.read()));
      CHECK(!latest->state.endRecorded());
    }
  }
  CHECK(sequence.current(sequence.read()) && deferred.pending()==0);
  CHECK(read(oldest,output) && output==1 && oldest->state.id()==1);
  CHECK(read(latest,output) && output==0 && latest->state.id()==prefixed);
  oldest.reset();latest.reset();CHECK(Ticket::live.load()==0);

  // Fresh tickets for repeated/nested recorded occurrences. Old owners retain
  // their results while a new replay uses different IDs and data objects.
  constexpr unsigned replayEnds=7,replays=128;
  uint64_t issued=0;
  std::vector<Reference> retained;
  for (unsigned replay=0;replay<replays;++replay) {
    for (unsigned endIndex=0;endIndex<replayEnds;++endIndex) {
      auto ticket=std::make_shared<Ticket>(++issued);deferred.push(ticket);
      if (endIndex==0) retained.push_back(ticket);
    }
    for (unsigned endIndex=0;endIndex<replayEnds;++endIndex) {
      auto ticket=deferred.take();
      CHECK(ticket->state.id()==uint64_t(replay)*replayEnds+endIndex+1);
      ticket->state.completeEnd();signal(ticket);
    }
    for (unsigned prior=0;prior<=replay;++prior) {
      output=0x75;CHECK(read(retained[prior],output));
      CHECK(retained[prior]->state.id()==uint64_t(prior)*replayEnds+1 && output==((prior*replayEnds+1)&1));
    }
  }
  CHECK(deferred.pending()==0 && issued==replayEnds*replays);
  retained.clear();CHECK(Ticket::live.load()==0);

  Queue cs,gpu;
  constexpr unsigned issues=100000,held=128;
  for (unsigned id=1;id<=held;++id) {
    auto ticket=std::make_shared<Ticket>(id);retained.push_back(ticket);cs.push(ticket);
  }
  std::atomic<unsigned> csCompleted{0},gpuCompleted{0},observations{0};
  std::atomic<bool> finished{false};
  std::thread csThread([&] {
    for (unsigned id=1;id<=issues;) {
      auto begun=cs.front();
      if (!begun) { std::this_thread::yield();continue; }
      auto ended=cs.take();
      CHECK(begun==ended && ended->state.id()==id && !ended->state.endRecorded());
      ended->state.completeEnd();CHECK(ended->state.endRecorded());
      gpu.push(ended);csCompleted.store(id,std::memory_order_release);++id;
    }
  });
  std::thread gpuThread([&] {
    for (unsigned id=1;id<=issues;) {
      auto ticket=gpu.take();
      if (!ticket) { std::this_thread::yield();continue; }
      CHECK(ticket->state.endRecorded() && ticket->state.id()==id);
      signal(ticket);gpuCompleted.store(id,std::memory_order_release);++id;
    }
  });
  std::thread reader([&] {
    while (!finished.load(std::memory_order_acquire)) {
      for (const auto& ticket : retained) {
        unsigned value=0x75;
        if (read(ticket,value)) {
          if (value!=ticket->expected || ticket->state.id()>held) std::abort();
          observations.fetch_add(1,std::memory_order_relaxed);
        }
      }
    }
  });
  Reference current;
  for (unsigned id=held+1;id<=issues;++id) {
    current=std::make_shared<Ticket>(id);cs.push(current);
  }
  csThread.join();gpuThread.join();finished.store(true,std::memory_order_release);reader.join();
  CHECK(csCompleted.load()==issues && gpuCompleted.load()==issues && cs.pending()==0 && gpu.pending()==0);
  CHECK(current->state.id()==issues && read(current,output) && output==0);
  for (unsigned id=1;id<=held;++id) {
    CHECK(retained[id-1]->state.id()==id && read(retained[id-1],output) && output==(id&1));
  }
  current.reset();retained.clear();CHECK(Ticket::live.load()==0);
  std::printf("D3D11 query tickets PASS checks=%u issues=%u prefix=%u replays=%u replay_ends=%u retained=%u observations=%u live_tickets=%u gpu_execution=0 predication_admission=0\n",checks.load(),issues,prefixed,replays,replayEnds,held,observations.load(),Ticket::live.load());
}
