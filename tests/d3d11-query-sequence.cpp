// SPDX-License-Identifier: Zlib
// Production query issue state only; this is not a GPU predication test.
#include "../src/d3d11/d3d11_query_sequence.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <thread>

static unsigned checks;
#define CHECK(value) do { ++checks; if (!(value)) std::abort(); } while (0)
using dxvk::D3D11QueryPhase;
using dxvk::D3D11QuerySequence;

int main() {
  D3D11QuerySequence query;
  auto initial = query.read();
  CHECK(initial.stable && initial.generation == 0 && !initial.pending && initial.phase == D3D11QueryPhase::Initial);
  CHECK(!query.current(initial));
  CHECK(!query.begin(false) && query.read().generation == 0);
  CHECK(query.begin(true));
  CHECK(!query.begin(true));
  CHECK(query.read().phase == D3D11QueryPhase::Begun);
  CHECK(query.end(true));
  auto first = query.read();
  CHECK(first.generation == 2 && first.pending == 1 && first.phase == D3D11QueryPhase::Ended);
  CHECK(!query.current(first));
  query.completeEnd(); first = query.read(); CHECK(query.current(first));
  // The pending count returns to the same zero after reissue. Its generation
  // must reject the old ticket, regardless of the Boolean value to publish.
  for (unsigned value : {0u, 1u}) {
    CHECK(query.begin(true)); CHECK(!query.current(first)); CHECK(query.end(true));
    CHECK(query.read().pending == 1 && !query.current(first));
    query.completeEnd(); const auto current = query.read();
    CHECK(!current.pending && query.current(current) && !query.current(first));
    unsigned output = 0x6du;
    if (query.current(first)) output = value;
    CHECK(output == 0x6du);
    if (query.current(current)) output = value;
    CHECK(output == value);
  }
  D3D11QuerySequence implicit;
  CHECK(!implicit.end(true)); CHECK(implicit.read().pending == 1);
  implicit.completeEnd(); CHECK(implicit.current(implicit.read()));
  D3D11QuerySequence event;
  CHECK(event.end(false)); CHECK(event.read().pending == 1);
  event.completeEnd(); CHECK(event.current(event.read()));
  // Retain the command-list's existing all-End-up-front pending policy. This
  // foundation deliberately does not claim intermediate replay can wait yet.
  D3D11QuerySequence deferred;
  constexpr unsigned repeats = 4096;
  for (unsigned i = 0; i < repeats; ++i) deferred.deferEnd();
  const auto last = deferred.read();
  CHECK(last.generation == repeats && last.pending == repeats && !deferred.current(last));
  for (unsigned i = 1; i < repeats; ++i) {
    deferred.completeEnd(); CHECK(!deferred.current(deferred.read()));
  }
  deferred.completeEnd(); CHECK(deferred.current(deferred.read()));
  CHECK(!deferred.current(last)); // Captured while pending, never a ready ticket.
  // Actual production atomic state with serialized API issues, independent CS
  // completion and concurrent readers. A consumer never completes an End
  // before the API producer has issued it.
  D3D11QuerySequence concurrent;
  constexpr unsigned issues = 100000;
  std::atomic<unsigned> emitted{0}, completed{0}, observations{0};
  std::atomic<bool> finished{false};
  std::thread cs([&] {
    unsigned seen = 0;
    while (seen != issues) {
      if (seen < emitted.load(std::memory_order_acquire)) {
        concurrent.completeEnd(); ++seen; completed.store(seen, std::memory_order_release);
      } else std::this_thread::yield();
    }
  });
  std::thread reader([&] {
    while (!finished.load(std::memory_order_acquire)) {
      const auto ticket = concurrent.read();
      if (concurrent.current(ticket)) observations.fetch_add(1, std::memory_order_relaxed);
    }
  });
  for (unsigned i = 0; i < issues; ++i) {
    CHECK(concurrent.begin(true)); CHECK(concurrent.end(true));
    emitted.store(i + 1, std::memory_order_release);
  }
  cs.join(); finished.store(true, std::memory_order_release); reader.join();
  const auto final = concurrent.read();
  CHECK(completed.load() == issues && final.pending == 0 && final.generation == issues * 2);
  CHECK(concurrent.current(final) && final.phase == D3D11QueryPhase::Ended);
  std::printf("D3D11 query sequence PASS checks=%u issues=%u deferred=%u observations=%u predication_admission=0 gpu_execution=0\n",
    checks, issues, repeats, observations.load());
}
