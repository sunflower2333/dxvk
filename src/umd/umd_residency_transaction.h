#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace dxvk::umd {

// A bounded implementation limit, not a residency capability or an inferred
// resident answer. Larger batches remain explicitly unsupported.
constexpr uint32_t residencyBatchLimit = 4096;

// The native callback receives owned handle/status arrays. No partial status
// reaches the runtime, including a callback that succeeds and then retires a
// resource, changes generation, or leaves a later result uninitialized.
template<typename Handle, typename Status, typename Result,
    typename Invoke, typename Live, typename Valid>
Result residencyTransaction(const Handle* handles, uint32_t count,
    Status* output, Invoke&& invoke, Live&& live, Valid&& valid,
    Result invalid, Result unsupported, Result failed, Result removed) {
  if (!handles || !output || !count) return invalid;
  if (count > residencyBatchLimit) return unsupported;
  std::vector<Handle> ownedHandles(handles, handles + count);
  if (std::find(ownedHandles.begin(), ownedHandles.end(), Handle{}) != ownedHandles.end())
    return invalid;
  std::vector<Status> staged(count);
  if (!live()) return removed;
  const Result result = invoke(ownedHandles.data(), staged.data(), count);
  if (!live()) return removed;
  if (result != Result{}) return result < Result{} ? result : failed;
  if (!std::all_of(staged.begin(), staged.end(), valid)) return failed;
  std::copy(staged.begin(), staged.end(), output);
  return Result{};
}

}
