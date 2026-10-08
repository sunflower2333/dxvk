// SPDX-License-Identifier: MIT
#include "../src/umd/umd_residency_transaction.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

static unsigned checks;
#define CHECK(value) do { ++checks; if (!(value)) { \
  std::fprintf(stderr, "residency transaction line=%d: %s\n", __LINE__, #value); std::abort(); \
} } while (0)

using Word = uint32_t;
using Result = int32_t;
constexpr Result invalid = -1, unsupported = -2, failed = -3, removed = -4;
constexpr Word sentinel = 0xfeed1234;

template<typename Invoke, typename Live>
static Result query(const Word* handles, uint32_t count, Word* output, Invoke&& invoke, Live&& live) {
  return dxvk::umd::residencyTransaction(handles, count, output,
    std::forward<Invoke>(invoke), std::forward<Live>(live),
    [](Word status) { return status >= 1 && status <= 3; }, invalid, unsupported, failed, removed);
}

int main() {
  for (uint32_t count = 1; count <= 64; ++count) {
    std::vector<Word> handles(count), output(count, sentinel);
    for (uint32_t i = 0; i < count; ++i) handles[i] = 1000 + i;
    unsigned calls = 0;
    auto invoke = [&](const Word* owned, Word* statuses, uint32_t size) {
      CHECK(size == count && owned != handles.data() && statuses != output.data()); ++calls;
      for (uint32_t i = 0; i < size; ++i) {
        CHECK(owned[i] == 1000 + i);
        statuses[i] = i % 3 + 1;
        handles[i] = 0; // Caller input reclamation cannot redefine this batch.
        CHECK(owned[i] == 1000 + i);
      }
      return Result(0);
    };
    CHECK(query(handles.data(), count, output.data(), invoke, [] { return true; }) == 0);
    CHECK(calls == 1);
    for (uint32_t i = 0; i < count; ++i) CHECK(output[i] == i % 3 + 1);
    std::fill(handles.begin(), handles.end(), 123);
    for (uint32_t bad = 0; bad < count; ++bad) {
      std::fill(output.begin(), output.end(), sentinel);
      CHECK(query(handles.data(), count, output.data(), [&](const Word*, Word* statuses, uint32_t size) {
        for (uint32_t i = 0; i < size; ++i) if (i != bad) statuses[i] = 1;
        return Result(0); // Unwritten later statuses must not publish earlier ones.
      }, [] { return true; }) == failed);
      for (Word value : output) CHECK(value == sentinel);
    }
  }
  std::array<Word, 3> handles{3, 2, 3}, output{sentinel, sentinel, sentinel};
  for (const Result callbackResult : {Result(-7), Result(1)}) {
    CHECK(query(handles.data(), 3, output.data(), [&](const Word*, Word* statuses, uint32_t) {
      statuses[0] = 1; statuses[1] = 2; statuses[2] = 3; return callbackResult;
    }, [] { return true; }) == (callbackResult < 0 ? callbackResult : failed));
    for (Word value : output) CHECK(value == sentinel);
  }
  for (const Word invalidStatus : {Word(0), Word(4), std::numeric_limits<Word>::max()}) {
    CHECK(query(handles.data(), 3, output.data(), [&](const Word*, Word* statuses, uint32_t) {
      statuses[0] = 1; statuses[1] = invalidStatus; statuses[2] = 3; return Result(0);
    }, [] { return true; }) == failed);
    for (Word value : output) CHECK(value == sentinel);
  }
  bool live = true;
  CHECK(query(handles.data(), 3, output.data(), [&](const Word*, Word* statuses, uint32_t) {
    statuses[0] = 1; statuses[1] = 2; statuses[2] = 3; live = false; return Result(0);
  }, [&] { return live; }) == removed);
  for (Word value : output) CHECK(value == sentinel);
  unsigned calls = 0;
  auto shouldNotCall = [&](const Word*, Word*, uint32_t) { ++calls; return Result(0); };
  CHECK(query(handles.data(), 3, output.data(), shouldNotCall, [] { return false; }) == removed);
  CHECK(query(nullptr, 3, output.data(), shouldNotCall, [] { return true; }) == invalid);
  CHECK(query(handles.data(), 3, nullptr, shouldNotCall, [] { return true; }) == invalid);
  CHECK(query(handles.data(), 0, output.data(), shouldNotCall, [] { return true; }) == invalid);
  CHECK(query(handles.data(), dxvk::umd::residencyBatchLimit + 1, output.data(),
    shouldNotCall, [] { return true; }) == unsupported);
  handles[2] = 0;
  CHECK(query(handles.data(), 3, output.data(), shouldNotCall, [] { return true; }) == invalid);
  CHECK(calls == 0);
  // An aliased caller input/output remains defined because the callback reads
  // the owned handles until every status has been validated.
  handles = {71, 72, 73};
  CHECK(query(handles.data(), 3, handles.data(), [&](const Word* owned, Word* statuses, uint32_t) {
    for (unsigned i = 0; i < 3; ++i) { CHECK(owned[i] == 71 + i); statuses[i] = 3 - i; }
    return Result(0);
  }, [] { return true; }) == 0);
  CHECK((handles == std::array<Word, 3>{3, 2, 1}));
  std::vector<Word> maximum(dxvk::umd::residencyBatchLimit, 99), maximumOutput(maximum.size(), sentinel);
  CHECK(query(maximum.data(), uint32_t(maximum.size()), maximumOutput.data(),
    [](const Word*, Word* statuses, uint32_t count) {
      std::fill(statuses, statuses + count, 2); return Result(0);
    }, [] { return true; }) == 0);
  for (Word value : maximumOutput) CHECK(value == 2);
  std::printf("DXGI residency transaction PASS checks=%u\n", checks);
}
