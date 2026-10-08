#include "d3d11_cmdlist.h"
#include "d3d11_device.h"
#include "d3d11_buffer.h"
#include "d3d11_texture.h"
#include "d3d11_command_replay.h"
#include <cassert>

namespace dxvk {
    
  D3D11CommandList::D3D11CommandList(
          D3D11Device*  pDevice,
          UINT          ContextFlags)
  : D3D11DeviceChild<ID3D11CommandList>(pDevice),
    m_contextFlags(ContextFlags), m_destructionNotifier(this) { }
  
  
  D3D11CommandList::~D3D11CommandList() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11CommandList::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11CommandList)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (riid == __uuidof(ID3DDestructionNotifier)) {
      *ppvObject = ref(&m_destructionNotifier);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D11CommandList), riid)) {
      Logger::warn("D3D11CommandList::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }
  
  
  UINT STDMETHODCALLTYPE D3D11CommandList::GetContextFlags() {
    return m_contextFlags;
  }
  
  
  void D3D11CommandList::AddQuery(D3D11Query* pQuery) {
    m_queries.emplace_back(pQuery);
    m_order.addQueryEnd(Com<D3D11Query, false>(pQuery));
  }


  void D3D11CommandList::AddQueryBegin(D3D11Query* pQuery, bool Implicit) {
    m_order.addQueryBegin(Com<D3D11Query, false>(pQuery), Implicit);
  }


  void D3D11CommandList::AddPredication(D3D11Query* pQuery, BOOL Value, bool Hint) {
    m_order.addPredicate(Com<D3D11Query, false>(pQuery), Value, Hint);
  }


  uint64_t D3D11CommandList::AddChunk(DxvkCsChunkRef&& Chunk, uint64_t Cost) {
    m_chunks.emplace_back(std::move(Chunk), Cost);
    m_order.addChunk(m_chunks.size() - 1);
    return m_chunks.size() - 1;
  }
  
  
  uint64_t D3D11CommandList::AddCommandList(
          D3D11CommandList*   pCommandList) {
    // This will be the chunk ID of the first chunk
    // added, for the purpose of resource tracking.
    uint64_t baseChunkId = m_chunks.size();
    
    for (const auto& chunk : pCommandList->m_chunks)
      m_chunks.push_back(chunk);

    for (const auto& query : pCommandList->m_queries)
      m_queries.push_back(query);

    m_order.append(pCommandList->m_order, baseChunkId);

    for (const auto& resource : pCommandList->m_resources) {
      TrackedResource entry = resource;
      entry.chunkId = D3D11RelocatedChunkId(entry.chunkId, baseChunkId);

      m_resources.push_back(std::move(entry));
    }

    // Return ID of the last chunk added. The command list
    // added can never be empty, so do not handle zero.
    return m_chunks.size() - 1;
  }


  void D3D11CommandList::EmitToCsThread(
    const D3D11ChunkDispatchProc& DispatchProc) {
    std::size_t endIndex = 0;
    const D3D11CommandReplay<Com<D3D11Query, false>, D3D11QueryTicket> replay(m_order,
      [] (const Com<D3D11Query, false>& query) {
        return query->CaptureTicket();
      },
      [&] (const D3D11RecordedOperation<Com<D3D11Query, false>>& operation) {
        assert(endIndex < m_queries.size());
        assert(operation.query == m_queries[endIndex]);
        assert(operation.endOccurrence == endIndex + 1);
        return m_queries[endIndex++]->DoDeferredEnd();
      });
    assert(endIndex == m_queries.size());

    // Public readiness remains all-End-up-front. Each API replay owns its
    // ordered ticket bindings; this view does not evaluate or suppress work.
    size_t j = 0;
    for (const auto& replayOperation : replay.operations()) {
      const auto& operation = replayOperation.recorded;
      if (operation.type != D3D11RecordedOperationType::Chunk)
        continue;
      const size_t i = size_t(operation.chunkId);
      // If there are resources to track for the current chunk,
      // use a strong flush hint to dispatch GPU work quickly.
      GpuFlushType flushType = GpuFlushType::ImplicitWeakHint;

      if (j < m_resources.size() && m_resources[j].chunkId == i)
        flushType = GpuFlushType::ImplicitStrongHint;

      // Dispatch the chunk and capture its sequence number
      uint64_t seq = DispatchProc(DxvkCsChunkRef(m_chunks[i].chunk), m_chunks[i].cost, flushType);

      // Track resource sequence numbers for the added chunk
      while (j < m_resources.size() && m_resources[j].chunkId == i)
        TrackResourceSequenceNumber(m_resources[j++].ref, seq);
    }
  }
  
  
  void D3D11CommandList::TrackResourceUsage(
          ID3D11Resource*     pResource,
          D3D11_RESOURCE_DIMENSION ResourceType,
          UINT                Subresource,
          uint64_t            ChunkId) {
    TrackedResource entry;
    entry.ref = D3D11ResourceRef(pResource, Subresource, ResourceType);
    entry.chunkId = ChunkId;

    m_resources.push_back(std::move(entry));
  }


  void D3D11CommandList::TrackResourceSequenceNumber(
    const D3D11ResourceRef&   Resource,
          uint64_t            Seq) {
    ID3D11Resource* iface = Resource.Get();

    switch (Resource.GetType()) {
      case D3D11_RESOURCE_DIMENSION_UNKNOWN:
        break;

      case D3D11_RESOURCE_DIMENSION_BUFFER: {
        auto impl = static_cast<D3D11Buffer*>(iface);
        impl->TrackSequenceNumber(Seq);
      } break;

      case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
        auto impl = static_cast<D3D11Texture1D*>(iface)->GetCommonTexture();
        impl->TrackSequenceNumber(Resource.GetSubresource(), Seq);
      } break;

      case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
        auto impl = static_cast<D3D11Texture2D*>(iface)->GetCommonTexture();
        impl->TrackSequenceNumber(Resource.GetSubresource(), Seq);
      } break;

      case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
        auto impl = static_cast<D3D11Texture3D*>(iface)->GetCommonTexture();
        impl->TrackSequenceNumber(Resource.GetSubresource(), Seq);
      } break;
    }
  }
  
}
