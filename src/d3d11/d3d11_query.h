#pragma once

#include "../dxvk/dxvk_gpu_event.h"
#include "../dxvk/dxvk_gpu_query.h"

#include "../d3d10/d3d10_query.h"

#include "d3d11_device_child.h"
#include "d3d11_query_sequence.h"
#include "d3d11_query_ticket.h"

#include <memory>

namespace dxvk {

  struct D3D11QueryDataTicket {
    explicit D3D11QueryDataTicket(uint64_t id) : state(id) { }

    D3D11QueryTicketState state;
    std::array<Rc<DxvkQuery>, 2> query;
    std::array<Rc<DxvkEvent>, 1> event;
  };

  using D3D11QueryTicket = std::shared_ptr<D3D11QueryDataTicket>;
  
  class D3D11Query : public D3D11DeviceChild<ID3D11Query1> {
    constexpr static uint32_t MaxGpuQueries = 2;
    constexpr static uint32_t MaxGpuEvents  = 1;
  public:
    
    D3D11Query(
            D3D11Device*        device,
      const D3D11_QUERY_DESC1&  desc);
    
    ~D3D11Query();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID  riid,
            void**  ppvObject) final;
    
    UINT STDMETHODCALLTYPE GetDataSize();
    
    void STDMETHODCALLTYPE GetDesc(D3D11_QUERY_DESC* pDesc) final;

    void STDMETHODCALLTYPE GetDesc1(D3D11_QUERY_DESC1* pDesc) final;

    void Begin(DxvkContext* ctx);
    
    void End(DxvkContext* ctx);

    void Begin(DxvkContext* ctx, const D3D11QueryTicket& ticket);

    void End(DxvkContext* ctx, const D3D11QueryTicket& ticket);

    D3D11QueryTicket CaptureTicket() const {
      return m_currentTicket;
    }
    
    bool STDMETHODCALLTYPE DoBegin();

    bool STDMETHODCALLTYPE DoEnd();

    HRESULT STDMETHODCALLTYPE GetData(
            void*                             pData,
            UINT                              GetDataFlags);
    
    D3D11QueryTicket DoDeferredEnd();

    bool IsScoped() const {
      return m_desc.Query != D3D11_QUERY_EVENT
          && m_desc.Query != D3D11_QUERY_TIMESTAMP;
    }

    bool IsEvent() const {
      return m_desc.Query == D3D11_QUERY_EVENT;
    }

    bool TrackStalls() const {
      return m_desc.Query == D3D11_QUERY_EVENT
          || m_desc.Query == D3D11_QUERY_TIMESTAMP
          || m_desc.Query == D3D11_QUERY_TIMESTAMP_DISJOINT;
    }

    bool IsStalling() const {
      return m_stallFlag;
    }

    void NotifyEnd() {
      m_stallMask <<= 1;
    }

    void NotifyStall() {
      m_stallMask |= 1;
      m_stallFlag |= bit::popcnt(m_stallMask) >= 16;
    }
    
    D3D10Query* GetD3D10Iface() {
      return &m_d3d10;
    }

    static HRESULT ValidateDesc(const D3D11_QUERY_DESC1* pDesc);

    static ID3D11Predicate* AsPredicate(ID3D11Query* pQuery) {
      // ID3D11Predicate and ID3D11Query have the same vtable. This
      // saves us some headache in all query-related functions.
      return static_cast<ID3D11Predicate*>(pQuery);
    }
    
    static D3D11Query* FromPredicate(ID3D11Predicate* pPredicate) {
      return static_cast<D3D11Query*>(static_cast<ID3D11Query*>(pPredicate));
    }
    
  private:
    
    D3D11_QUERY_DESC1  m_desc;

    D3D11QuerySequence m_sequence;
    
    D3D11QueryTicket m_currentTicket;
    D3D11DeferredQueryTickets<D3D11QueryTicket> m_deferredTickets;
    uint64_t m_nextTicket = 0;

    D3D10Query m_d3d10;

    uint32_t m_stallMask = 0;
    bool     m_stallFlag = false;

    D3DDestructionNotifier m_destructionNotifier;

    UINT64 GetTimestampQueryFrequency() const;

    D3D11QueryTicket CreateTicket();
    
  };
  
}
