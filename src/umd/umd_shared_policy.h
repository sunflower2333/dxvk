#pragma once
// SPDX-License-Identifier: MIT
#include <cstdint>

namespace dxvk::umd {

// When a shared surface's cache has to be re-read, and when it owes the other
// process a write. Separated from the transfer itself so the decisions can be
// tested without a D3D device or a kernel allocation behind them; the copies
// live in umd_shared_surface.h.
//
// The whole policy exists because the obvious rule -- read the allocation
// before every use -- is what the Mesa driver does, and it costs 17.35 ms p50
// at 3040x1904 against a 6.06 ms frame budget, because a surface sampled by
// several draws is read once per draw.
struct SharedState {
  // Epoch whose contents the cache holds. Zero means never read, which no live
  // epoch equals, so a surface starts out owing a read.
  uint64_t refreshed = 0;
  // The cache holds writes the allocation has not seen.
  bool dirty = false;
};

// An ownership epoch is the boundary at which another process's writes may
// become visible, and the granularity a read is memoized against. Flush and
// Present open a new one; nothing else does.
inline uint64_t nextSharedEpoch(uint64_t epoch) { return epoch + 1; }

// Does a read of this surface have to go back to the allocation?
//
// No when the cache was already read this epoch, and no when the cache is
// dirty -- unpublished local writes are strictly newer than the allocation, so
// reading it back would discard them. That second case is why dirty is checked
// here and not only at publish time.
inline bool sharedNeedsRefresh(const SharedState& state, uint64_t epoch) {
  return state.refreshed != epoch && !state.dirty;
}

// Record a completed read.
inline void sharedRefreshed(SharedState& state, uint64_t epoch) {
  state.refreshed = epoch;
}

// A write that defines every pixel: a full-view clear, a whole-resource copy,
// a box-less update. The cache becomes authoritative with no read at all,
// which is the one case this policy is cheaper than reading before every use
// rather than merely later.
inline void sharedWroteWhole(SharedState& state, uint64_t epoch) {
  state.dirty = true;
  state.refreshed = epoch;
}

// A write that leaves some pixels as the other process last wrote them: a
// region copy, a boxed update, or any draw, since blending, a partial viewport
// and a scissor all preserve what was underneath. The caller must refresh
// first; this only records the result.
inline void sharedWroteRegion(SharedState& state) { state.dirty = true; }

// Record a completed publish. Cache and allocation now agree, so this epoch
// owes no read.
inline void sharedPublished(SharedState& state, uint64_t epoch) {
  state.dirty = false;
  state.refreshed = epoch;
}

// Drop the memoized read, forcing the next one back to the allocation.
//
// RotateResourceIdentities is why this exists. A flip chain rotates allocation
// handles between its buffers and moves no pixels, so afterwards each resource
// is paired with an allocation it has never read while its cache still holds
// the pixels of the one it used to own. Leaving the memo alone there shows the
// previous frame in the rotated buffers, drifting with the flip cadence --
// which is a live bug in the Mesa driver on this stack, not a hypothetical.
//
// It discards a dirty cache rather than carrying it across the re-pairing, so
// callers must publish before they rotate. Keeping the dirty bit would deliver
// this frame's pixels into whichever buffer the rotation handed over, one
// publish later. Losing an unpublished write is recoverable; writing into
// another frame's buffer is not.
inline void sharedInvalidate(SharedState& state) {
  state.refreshed = 0;
  state.dirty = false;
}

}
