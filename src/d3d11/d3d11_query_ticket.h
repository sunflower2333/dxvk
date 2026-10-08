#pragma once
// SPDX-License-Identifier: Zlib
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <utility>

namespace dxvk {

  // Completion means that CS recorded this ticket's End. The backend query or
  // event can still be GPU-pending; callers must separately inspect its data.
  class D3D11QueryTicketState {
  public:
    explicit D3D11QueryTicketState(uint64_t id) : m_id(id) { }

    uint64_t id() const {
      return m_id;
    }

    void issueEnd() {
      m_endIssued.store(true, std::memory_order_release);
    }

    bool endIssued() const {
      return m_endIssued.load(std::memory_order_acquire);
    }

    void completeEnd() {
      m_endRecorded.store(true, std::memory_order_release);
    }

    bool endRecorded() const {
      return m_endRecorded.load(std::memory_order_acquire);
    }

  private:
    const uint64_t m_id;
    std::atomic<bool> m_endIssued = { false };
    std::atomic<bool> m_endRecorded = { false };
  };

  // API submission produces fresh tickets for deferred End occurrences before
  // dispatch. CS Begin peeks its corresponding ticket; CS End takes that same
  // ticket. Owning references keep an older issue's result alive on reissue.
  template<typename TicketRef>
  class D3D11DeferredQueryTickets {
  public:
    void push(TicketRef ticket) {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_tickets.push_back(std::move(ticket));
    }

    TicketRef front() const {
      std::lock_guard<std::mutex> lock(m_mutex);
      return m_tickets.empty() ? TicketRef() : m_tickets.front();
    }

    TicketRef take() {
      std::lock_guard<std::mutex> lock(m_mutex);
      if (m_tickets.empty())
        return TicketRef();
      TicketRef ticket = std::move(m_tickets.front());
      m_tickets.pop_front();
      return ticket;
    }

    std::size_t pending() const {
      std::lock_guard<std::mutex> lock(m_mutex);
      return m_tickets.size();
    }

  private:
    mutable std::mutex m_mutex;
    std::deque<TicketRef> m_tickets;
  };

}
