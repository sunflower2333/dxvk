#pragma once
// SPDX-License-Identifier: Zlib
#include <cstdint>
#include <utility>
#include <vector>

namespace dxvk {

  inline uint64_t D3D11RecordedCurrentChunkId(uint64_t chunkCount, bool empty) {
    return empty && chunkCount ? chunkCount - 1 : chunkCount;
  }

  inline uint64_t D3D11RelocatedChunkId(uint64_t chunkId, uint64_t base) {
    return chunkId + base;
  }

  enum class D3D11RecordedOperationType : uint32_t {
    Chunk,
    Predicate,
    QueryBegin,
    QueryEnd,
    Action,
  };

  // QueryRef owns its query. End occurrence IDs are local recording identities,
  // not GPU result generations; every replay still needs fresh result tickets.
  template<typename QueryRef>
  struct D3D11RecordedOperation {
    D3D11RecordedOperationType type = D3D11RecordedOperationType::Chunk;
    QueryRef query = { };
    uint64_t chunkId = 0;
    uint64_t endOccurrence = 0;
    uint64_t actionId = 0;
    int32_t predicateValue = 0;
    bool predicateHint = false;
    bool implicitBegin = false;
  };

  template<typename QueryRef>
  class D3D11CommandOrder {
  public:
    using Operation = D3D11RecordedOperation<QueryRef>;

    D3D11CommandOrder() {
      // A command list always begins with default context predication.
      addPredicate(QueryRef(), 0, false);
    }

    void addChunk(uint64_t chunkId) {
      Operation operation;
      operation.type = D3D11RecordedOperationType::Chunk;
      operation.chunkId = chunkId;
      m_operations.push_back(std::move(operation));
    }

    void addPredicate(QueryRef query, int32_t value, bool hint) {
      Operation operation;
      operation.type = D3D11RecordedOperationType::Predicate;
      operation.query = std::move(query);
      // Preserve the original BOOL even when the query is null.
      operation.predicateValue = value;
      operation.predicateHint = hint;
      m_operations.push_back(std::move(operation));
    }

    void addQueryBegin(QueryRef query, bool implicit) {
      Operation operation;
      operation.type = D3D11RecordedOperationType::QueryBegin;
      operation.query = std::move(query);
      operation.implicitBegin = implicit;
      m_operations.push_back(std::move(operation));
    }

    void addQueryEnd(QueryRef query) {
      Operation operation;
      operation.type = D3D11RecordedOperationType::QueryEnd;
      operation.query = std::move(query);
      operation.endOccurrence = ++m_endOccurrences;
      m_operations.push_back(std::move(operation));
    }

    void addAction(uint64_t actionId) {
      Operation operation;
      operation.type = D3D11RecordedOperationType::Action;
      operation.actionId = actionId;
      m_operations.push_back(std::move(operation));
    }

    void append(const D3D11CommandOrder& other, uint64_t chunkBase, uint64_t actionBase = 0) {
      const uint64_t endBase = m_endOccurrences;
      for (const auto& source : other.m_operations) {
        Operation operation = source;
        if (operation.type == D3D11RecordedOperationType::Chunk)
          operation.chunkId = D3D11RelocatedChunkId(operation.chunkId, chunkBase);
        if (operation.type == D3D11RecordedOperationType::QueryEnd)
          operation.endOccurrence += endBase;
        if (operation.type == D3D11RecordedOperationType::Action)
          operation.actionId += actionBase;
        m_operations.push_back(std::move(operation));
      }
      m_endOccurrences += other.m_endOccurrences;
    }

    const std::vector<Operation>& operations() const {
      return m_operations;
    }

    uint64_t endOccurrences() const {
      return m_endOccurrences;
    }

  private:
    std::vector<Operation> m_operations;
    uint64_t m_endOccurrences = 0;
  };

}
