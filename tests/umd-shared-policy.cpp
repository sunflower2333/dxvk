// SPDX-License-Identifier: MIT
#include "../src/umd/umd_shared_policy.h"
#include <cstdio>
#include <cstdlib>

using namespace dxvk::umd;
static unsigned checks;
// Test verdicts must not depend on NDEBUG or an installed graphics runtime.
static void check(bool condition, unsigned line) {
  checks++;
  if (!condition) { std::fprintf(stderr, "FAIL shared policy line=%u\n", line); std::exit(1); }
}
#define CHECK(x) check(!!(x), __LINE__)

// A surface starts owing a read: its cache has never seen the allocation, and
// the pixels there belong to whoever created it.
static void freshSurfaceOwesARead() {
  SharedState state;
  CHECK(sharedNeedsRefresh(state, 1));
  // Epoch zero is not a live epoch, but even if a caller passes it the initial
  // state must not read as already current.
  CHECK(state.refreshed == 0);
}

// The whole point of the epoch: many draws binding one surface read it once.
static void repeatedReadsInOneEpochCostOne() {
  SharedState state;
  CHECK(sharedNeedsRefresh(state, 7));
  sharedRefreshed(state, 7);
  for (unsigned draw = 0; draw < 100; draw++) CHECK(!sharedNeedsRefresh(state, 7));
  // ...and exactly once more after the boundary.
  const uint64_t next = nextSharedEpoch(7);
  CHECK(next == 8);
  CHECK(sharedNeedsRefresh(state, next));
}

// Unpublished local writes are newer than the allocation, so a read must not
// go back to it and silently discard them.
static void dirtyCacheIsNotOverwrittenByARead() {
  SharedState state;
  sharedRefreshed(state, 3);
  sharedWroteRegion(state);
  CHECK(state.dirty);
  CHECK(!sharedNeedsRefresh(state, 3));
  // Still true across a boundary: the epoch changed, but our writes did not
  // stop being the newest thing anyone has.
  CHECK(!sharedNeedsRefresh(state, nextSharedEpoch(3)));
  sharedPublished(state, 4);
  CHECK(!state.dirty);
  CHECK(!sharedNeedsRefresh(state, 4));
  CHECK(sharedNeedsRefresh(state, 5));
}

// A write covering every pixel makes the cache authoritative with no read.
static void wholeWriteSkipsTheRead() {
  SharedState state;
  CHECK(sharedNeedsRefresh(state, 9));
  sharedWroteWhole(state, 9);
  CHECK(state.dirty);
  CHECK(!sharedNeedsRefresh(state, 9));
  // A partial write in the same epoch does not undo that.
  sharedWroteRegion(state);
  CHECK(state.dirty);
  CHECK(!sharedNeedsRefresh(state, 9));
}

// The rotation trap. A flip chain re-pairs buffers with each other's
// allocations and moves no pixels; a surface that keeps its memo across that
// shows the frame belonging to the allocation it used to own.
static void rotationForcesARead() {
  SharedState state;
  sharedRefreshed(state, 11);
  CHECK(!sharedNeedsRefresh(state, 11));
  sharedInvalidate(state);
  // Same epoch, and it must still read: the allocation underneath changed,
  // which is not something an epoch boundary can express.
  CHECK(sharedNeedsRefresh(state, 11));
}

// Invalidation discards a dirty cache instead of carrying it across the
// re-pairing, so a rotation cannot deliver this frame's pixels into whichever
// buffer it handed over. Callers publish first; that is the contract.
static void rotationDiscardsUnpublishedWrites() {
  SharedState state;
  sharedRefreshed(state, 13);
  sharedWroteWhole(state, 13);
  CHECK(state.dirty);
  sharedInvalidate(state);
  CHECK(!state.dirty);
  CHECK(sharedNeedsRefresh(state, 13));
}

// A published surface that is then rotated still owes a read: publishing made
// cache and allocation agree, but the rotation changed which allocation.
static void publishThenRotateStillReads() {
  SharedState state;
  sharedWroteWhole(state, 17);
  sharedPublished(state, 17);
  CHECK(!sharedNeedsRefresh(state, 17));
  sharedInvalidate(state);
  CHECK(sharedNeedsRefresh(state, 17));
}

// Epochs only ever move forward, so a stale memo can never be mistaken for a
// current one by wrapping onto it.
static void epochsAdvance() {
  uint64_t epoch = 1;
  for (unsigned i = 0; i < 1000; i++) {
    const uint64_t next = nextSharedEpoch(epoch);
    CHECK(next > epoch);
    epoch = next;
  }
  SharedState state;
  sharedRefreshed(state, epoch);
  CHECK(!sharedNeedsRefresh(state, epoch));
  CHECK(sharedNeedsRefresh(state, nextSharedEpoch(epoch)));
}

int main() {
  freshSurfaceOwesARead();
  repeatedReadsInOneEpochCostOne();
  dirtyCacheIsNotOverwrittenByARead();
  wholeWriteSkipsTheRead();
  rotationForcesARead();
  rotationDiscardsUnpublishedWrites();
  publishThenRotateStillReads();
  epochsAdvance();
  std::printf("PASS shared policy: %u checks; no graphics runtime\n", checks);
}
