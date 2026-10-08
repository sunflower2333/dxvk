#pragma once
// SPDX-License-Identifier: Zlib
#include <atomic>
#include <cstdint>

namespace dxvk {

  enum class D3D11QueryPhase : uint32_t {
    Initial,
    Begun,
    Ended,
  };

  struct D3D11QueryReadState {
    uint64_t generation;
    uint32_t pending;
    D3D11QueryPhase phase;
    bool stable;
  };

  // API issue transitions are serialized by the immediate context lock.
  // CS completion may run concurrently. A zero pending count alone does not
  // identify the issue whose result a reader obtained: reissue can return it
  // to zero again while the reader is inspecting the old GPU query data.
  class D3D11QuerySequence {
  public:
    bool begin(bool scoped) {
      if (!scoped || m_phase.load() == D3D11QueryPhase::Begun)
        return false;
      m_phase.store(D3D11QueryPhase::Begun);
      m_generation.fetch_add(1);
      return true;
    }

    bool end(bool scoped) {
      bool begun = m_phase.load() == D3D11QueryPhase::Begun || !scoped;
      deferEnd();
      return begun;
    }

    void deferEnd() {
      // Make readiness false before publishing a new generation. Otherwise a
      // reader could pair the new generation with an old zero pending count.
      m_pending.fetch_add(1);
      m_generation.fetch_add(1);
      m_phase.store(D3D11QueryPhase::Ended);
    }

    void completeEnd() {
      m_pending.fetch_sub(1);
    }

    D3D11QueryReadState read() const {
      D3D11QueryReadState result;
      result.generation = m_generation.load();
      result.phase = m_phase.load();
      result.pending = m_pending.load();
      result.stable = result.generation == m_generation.load();
      return result;
    }

    bool current(const D3D11QueryReadState& result) const {
      if (!result.stable || result.phase != D3D11QueryPhase::Ended || result.pending)
        return false;
      auto now = read();
      return now.stable && now.phase == D3D11QueryPhase::Ended && !now.pending
          && now.generation == result.generation;
    }

  private:
    std::atomic<D3D11QueryPhase> m_phase = { D3D11QueryPhase::Initial };
    std::atomic<uint32_t> m_pending = { 0u };
    std::atomic<uint64_t> m_generation = { 0u };
  };

}
