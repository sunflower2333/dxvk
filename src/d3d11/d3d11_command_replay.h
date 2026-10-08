#pragma once
// SPDX-License-Identifier: Zlib
#include "d3d11_command_order.h"
#include <cstddef>

namespace dxvk {

  template<typename QueryRef, typename TicketRef>
  struct D3D11ReplayQueryTicket {
    QueryRef query = { };
    TicketRef ticket = { };
    uint64_t endOccurrence = 0;
  };

  template<typename QueryRef, typename TicketRef>
  struct D3D11ReplayPredicate {
    QueryRef query = { };
    TicketRef ticket = { };
    int32_t value = 0;
    bool hint = false;
  };

  template<typename QueryRef, typename TicketRef>
  struct D3D11ReplayOperation {
    D3D11RecordedOperation<QueryRef> recorded;
    TicketRef queryTicket = { };
    D3D11ReplayPredicate<QueryRef, TicketRef> predicate;
  };

  // One API replay owns its initial snapshots and newly issued End tickets.
  // The view binds metadata only; it neither waits nor suppresses CS chunks.
  template<typename QueryRef, typename TicketRef>
  class D3D11CommandReplay {
  public:
    using QueryTicket = D3D11ReplayQueryTicket<QueryRef, TicketRef>;
    using Operation = D3D11ReplayOperation<QueryRef, TicketRef>;

    template<typename Capture, typename Issue>
    D3D11CommandReplay(const D3D11CommandOrder<QueryRef>& order,
            Capture capture, Issue issue) {
      // Snapshot every referenced query before issuing any End. Issue replaces
      // the public current owner, including queries first used as predicates.
      for (const auto& operation : order.operations()) {
        if (operation.query && !find(m_initial, operation.query))
          m_initial.push_back({ operation.query, capture(operation.query), 0 });
      }

      m_ends.reserve(order.endOccurrences());
      m_operations.reserve(order.operations().size());
      for (const auto& operation : order.operations())
        m_operations.push_back({ operation, { }, { } });
      auto current = m_initial;
      auto next = m_initial;
      for (auto& entry : next)
        entry.ticket = { };

      for (const auto& operation : order.operations()) {
        if (operation.type == D3D11RecordedOperationType::QueryEnd)
          m_ends.push_back({ operation.query, issue(operation), operation.endOccurrence });
      }

      // Begin uses its next accepted End ticket, matching the backend FIFO.
      // FinalizeQueries supplies an End for every accepted recorded Begin.
      std::size_t endIndex = m_ends.size();
      for (auto i = m_operations.rbegin(); i != m_operations.rend(); ++i) {
        auto& operation = i->recorded;
        if (operation.type == D3D11RecordedOperationType::QueryEnd) {
          i->queryTicket = m_ends[--endIndex].ticket;
          find(next, operation.query)->ticket = i->queryTicket;
        } else if (operation.type == D3D11RecordedOperationType::QueryBegin) {
          i->queryTicket = find(next, operation.query)->ticket;
        }
      }

      D3D11ReplayPredicate<QueryRef, TicketRef> predicate;
      for (auto& operation : m_operations) {
        const auto& recorded = operation.recorded;
        if (recorded.type == D3D11RecordedOperationType::QueryEnd) {
          find(current, recorded.query)->ticket = operation.queryTicket;
        } else if (recorded.type == D3D11RecordedOperationType::Predicate) {
          predicate.query = recorded.query;
          predicate.ticket = recorded.query ? find(current, recorded.query)->ticket : TicketRef();
          predicate.value = recorded.predicateValue;
          predicate.hint = recorded.predicateHint;
        }
        // Owning copies preserve the selected result when a later End updates
        // the query's replay/current public owner. Null BOOL remains exact.
        operation.predicate = predicate;
      }
    }

    const std::vector<QueryTicket>& initialTickets() const {
      return m_initial;
    }

    const std::vector<QueryTicket>& endTickets() const {
      return m_ends;
    }

    const std::vector<Operation>& operations() const {
      return m_operations;
    }

    TicketRef ticketForEnd(uint64_t occurrence) const {
      if (!occurrence || occurrence > m_ends.size())
        return TicketRef();
      const auto& entry = m_ends[std::size_t(occurrence - 1)];
      return entry.endOccurrence == occurrence ? entry.ticket : TicketRef();
    }

  private:
    std::vector<QueryTicket> m_initial;
    std::vector<QueryTicket> m_ends;
    std::vector<Operation> m_operations;

    static QueryTicket* find(std::vector<QueryTicket>& entries, const QueryRef& query) {
      for (auto& entry : entries) {
        if (entry.query == query)
          return &entry;
      }
      return nullptr;
    }
  };

}
