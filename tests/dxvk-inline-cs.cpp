// Actual CS chunk/queue bodies with controlled device status and command peers.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "../src/util/util_likely.h"
#include "../src/util/rc/util_rc_ptr.h"
#include "../src/util/util_flags.h"

#define CACHE_LINE_SIZE 64
constexpr int VK_SUCCESS=0, VK_ERROR_DEVICE_LOST=-4;
static unsigned checks=0;
#define CHECK(x) do { ++checks; if(!(x)) throw std::runtime_error(#x); } while(0)
namespace dxvk {
using mutex=std::mutex;
using condition_variable=std::condition_variable;
using high_resolution_clock=std::chrono::steady_clock;
using thread=std::thread;
template<typename T> T align(T value,T alignment) {return (value+alignment-1)&~(alignment-1);}
struct RcObject {
  std::atomic<unsigned> refs{0};
  unsigned incRef(){return ++refs;}
  unsigned decRef(){return --refs;}
};
struct DxvkError:std::runtime_error {
  using std::runtime_error::runtime_error;
  std::string message() const {return what();}
};
struct Logger {static void err(const std::string&) {}}
;
namespace env {void setThreadName(const char*) {}}
enum class DxvkStatCounter {CsChunkCount,CsSyncCount,CsSyncTicks,CsIdleTicks};
struct VkDebugUtilsLabelEXT {const char* pLabelName;};
struct DxvkSubmitStatus {int result=1;};
enum class DxvkContextFeature {DescriptorHeap,DescriptorBuffer,DebugUtils};
enum class DxvkCmdBuffer {ExecBuffer};
struct DxvkCommandList:RcObject {
  std::atomic<int>* status;
  bool recording=false;
  explicit DxvkCommandList(std::atomic<int>* s):status(s){}
  void init(){if(*status!=VK_SUCCESS)throw DxvkError("cannot begin on lost device");recording=true;}
  void finalize(){CHECK(recording);recording=false;}
  void waitFence(unsigned,uint64_t){CHECK(recording);}
  void signalFence(unsigned,uint64_t){CHECK(recording);}
  void setDescriptorSyncHandle(unsigned){CHECK(recording);}
  void cmdInsertDebugUtilsLabel(DxvkCmdBuffer,const VkDebugUtilsLabelEXT&){CHECK(recording);}
};
struct DxvkLatencyTracker:RcObject {
  unsigned ended=0;
  void notifyCsRenderEnd(uint64_t){++ended;}
};
struct DxvkDevice:RcObject {
  bool inlineMode=true;
  std::atomic<int> status{VK_SUCCESS};
  bool failSubmit=false;
  unsigned created=0,submitted=0;
  bool disableExtraThreads()const{return inlineMode;}
  int getDeviceStatus()const{return status.load();}
  void addStatCtr(DxvkStatCounter,uint64_t){}
  Rc<DxvkCommandList> createCommandList(){++created;return new DxvkCommandList(&status);}
  void submitCommandList(const Rc<DxvkCommandList>& c,const Rc<DxvkLatencyTracker>&,uint64_t,DxvkSubmitStatus* s){
    CHECK(!c->recording);++submitted;if(failSubmit)status=VK_ERROR_DEVICE_LOST;
    if(s)s->result=status;
  }
};
struct DxvkContext:RcObject {
  Rc<DxvkDevice> m_device;
  Rc<DxvkCommandList> m_cmd;
  Rc<DxvkLatencyTracker> m_latencyTracker;
  uint64_t m_latencyFrameId=0,m_submitWaitId=0,m_trackingId=0,m_submitLastId=0;
  unsigned m_trackingFence=1,begun=0,ended=0,zeroFreed=0;
  bool m_endLatencyTracking=false;
  std::pair<unsigned,unsigned> m_framesToCapture{0,0};
  struct Features {
    bool any(DxvkContextFeature,DxvkContextFeature){return false;}
    bool test(DxvkContextFeature){return false;}
  }m_features;
  struct Worker {unsigned getSyncHandle(){return 0;}}m_descriptorWorker;
  struct Resolves {void cleanup(uint64_t){}}m_implicitResolves;
  explicit DxvkContext(const Rc<DxvkDevice>& d):m_device(d){}
  void addStatCtr(DxvkStatCounter,uint64_t){}
  void beginFrameCapture(){}
  void beginCurrentCommands(){CHECK(m_cmd && m_cmd->recording);++begun;}
  void endCurrentCommands(){CHECK(m_cmd && m_cmd->recording);++ended;}
  void relocateQueuedResources(){}
  void freeZeroBuffer(){++zeroFreed;}
  void beginRecording(const Rc<DxvkCommandList>&);
  Rc<DxvkCommandList> endRecording(const VkDebugUtilsLabelEXT*);
  void flushCommandList(const VkDebugUtilsLabelEXT*,DxvkSubmitStatus*);
};
}
#include "context_under_test.h"
#include "dxvk_cs_under_test.h"
#include "dxvk_cs_under_test.cpp"
using namespace dxvk;

struct Owner {unsigned* released; explicit Owner(unsigned* p):released(p){} ~Owner(){++*released;}};
struct Harness {
  Rc<DxvkDevice> device=new DxvkDevice;
  Rc<DxvkContext> context=new DxvkContext(device);
  DxvkCsChunkPool pool;
  DxvkCsThread cs{device,context};
  DxvkCsChunkRef chunk(bool single=true) {
    return {pool.allocChunk(single?DxvkCsChunkFlags(DxvkCsChunkFlag::SingleUse):DxvkCsChunkFlags()),&pool};
  }
};

enum class D3D11CmdType {None,Data};
using D3D9CmdType=D3D11CmdType;
enum class GpuFlushType {ImplicitWeakHint};
struct Frontend {
  Harness h;
  static constexpr bool IsDeferred=false;
  DxvkCsChunkRef m_csChunk=h.chunk();
  DxvkCsDataBlock* m_csData=nullptr;
  D3D11CmdType m_csDataType=D3D11CmdType::None;
  Frontend* GetTypedContext(){return this;}
  DxvkCsChunkRef AllocCsChunk(){return h.chunk();}
  void EmitCsChunk(DxvkCsChunkRef&& c){h.cs.dispatchChunk(std::move(c));}
  void ConsiderFlush(GpuFlushType){}
#include "frontend_under_test.h"
};
struct Frontend9:Frontend {
#include "frontend9_under_test.h"
};
struct InitParent {
  Harness h;
  InitParent* GetContext(){return this;}
  DxvkCsChunkRef AllocCsChunk(DxvkCsChunkFlags flags={DxvkCsChunkFlag::SingleUse}){return h.chunk();}
  void InjectCsChunk(DxvkCsQueue q,DxvkCsChunkRef&& c,bool sync){h.cs.injectChunk(q,std::move(c),sync);}
  void InjectCsChunk(DxvkCsChunkRef&& c,bool sync){InjectCsChunk(DxvkCsQueue::HighPriority,std::move(c),sync);}
};
struct D3D11Initializer {
  InitParent* m_parent;
  DxvkCsChunkRef m_csChunk;
  explicit D3D11Initializer(InitParent* p):m_parent(p),m_csChunk(p->AllocCsChunk()){}
  void FlushCsChunkLocked();
};
struct D3D9Initializer:D3D11Initializer {
  using D3D11Initializer::D3D11Initializer;
  void FlushCsChunkLocked();
};
#include "initializer11_under_test.h"
#include "initializer9_under_test.h"

void healthy() {
  Harness h;unsigned run=0,released=0;auto caller=std::this_thread::get_id();
  for(unsigned i=0;i<3;i++) {
    auto c=h.chunk();
    c->push([o=std::make_shared<Owner>(&released),&run,caller](DxvkContext*){CHECK(std::this_thread::get_id()==caller);++run;});
    auto seq=h.cs.dispatchChunk(std::move(c));CHECK(!c);CHECK(seq==i+1);h.cs.synchronize(seq);
  }
  CHECK(run==3);CHECK(released==3);CHECK(h.cs.lastSequenceNumber()==3);
  auto c=h.chunk(false);c->push([&run](DxvkContext*){++run;});
  auto replay=c;h.cs.dispatchChunk(std::move(replay));replay=c;h.cs.dispatchChunk(std::move(replay));
  CHECK(run==5);CHECK(!c->empty());
}

void removed() {
  Harness h;unsigned run=0,released=0;h.device->status=VK_ERROR_DEVICE_LOST;
  auto command=[&](DxvkContext*){++run;};
  auto c=h.chunk();c->push(command);auto seq=h.cs.dispatchChunk(std::move(c));
  CHECK(!c);CHECK(seq==1);h.cs.synchronize(seq);
  for(auto queue:{DxvkCsQueue::Ordered,DxvkCsQueue::HighPriority}) {
    for(bool sync:{false,true}) {
      c=h.chunk();c->push([o=std::make_shared<Owner>(&released),&run](DxvkContext*){++run;});
      h.cs.injectChunk(queue,std::move(c),sync);CHECK(!c);
    }
  }
  CHECK(run==0);CHECK(released==4);
  c=h.chunk(false);c->push(command);auto replay=c;h.cs.dispatchChunk(std::move(replay));
  CHECK(run==0);CHECK(!c->empty());
}

void throwing() {
  Harness h;unsigned run=0,released=0;auto c=h.chunk();
  for(unsigned i=0;i<3;i++) {
    c->push([o=std::make_shared<Owner>(&released),i,&run](DxvkContext*) {
      ++run;if(i==1)throw DxvkError("injected recording failure");
    });
  }
  // A retained reference makes checking partial consumption independent of
  // pool recycling; the old code double-releases the first captured owner.
  auto retained=c;
  bool caught=false;
  try{h.cs.dispatchChunk(std::move(c));}catch(const DxvkError&){caught=true;}
  CHECK(caught);CHECK(run==2);CHECK(!c);CHECK(retained->empty());CHECK(released==3);
  CHECK(h.cs.lastSequenceNumber()==1);h.cs.synchronize(1);
  retained={};CHECK(released==3);
}

void reentrantOwner() {
  Harness h;unsigned run=0;auto source=h.chunk();
  source->push([&](DxvkContext*) {CHECK(!source);++run;});
  CHECK(h.cs.dispatchChunk(std::move(source))==1);CHECK(run==1);CHECK(!source);
}

template<typename F> void frontend() {
  F f;unsigned run=0;
  auto active=f.m_csChunk.operator->();
  f.EmitCs([&](DxvkContext*) {
    CHECK(f.m_csChunk.operator->()!=active);
    f.EmitCs([&](DxvkContext*){++run;});
    ++run;
  });
  f.FlushCsChunk();CHECK(run==1);CHECK(!f.m_csChunk->empty());
  f.FlushCsChunk();CHECK(run==2);CHECK(f.m_csChunk->empty());
  struct Large {
    std::array<char,4096> padding{};
    unsigned* run;
    void operator()(DxvkContext*){++*run;}
  };
  for(unsigned i=0;i<20;i++)f.EmitCs(Large{{},&run});
  f.FlushCsChunk();CHECK(run==22);
  struct Data {unsigned index;char padding[1000];};
  for(unsigned i=0;i<20;i++) {
    f.template EmitCsCmd<Data>(D3D11CmdType::Data,4,[i,&run](DxvkContext*,Data* data,size_t count) {
      CHECK(count==4);for(unsigned j=0;j<count;j++)CHECK(data[j].index==i+j);++run;
    });
    auto data=static_cast<Data*>(f.m_csData->first());
    for(unsigned j=0;j<4;j++)new(data+j)Data{i+j,{}};
  }
  f.FlushCsChunk();CHECK(run==42);
}

template<typename I> void initializer() {
  InitParent p;I init(&p);unsigned run=0;
  auto active=init.m_csChunk.operator->();
  init.m_csChunk->push([&](DxvkContext*){
    CHECK(init.m_csChunk && init.m_csChunk.operator->()!=active);++run;
  });
  init.FlushCsChunkLocked();CHECK(run==1);CHECK(init.m_csChunk);
  init.m_csChunk->push([](DxvkContext*){throw DxvkError("initializer recording failure");});
  bool caught=false;
  try{init.FlushCsChunkLocked();}catch(const DxvkError&){caught=true;}
  CHECK(caught);CHECK(init.m_csChunk && init.m_csChunk->empty());
}

void context(bool lost) {
  Harness h;unsigned run=0,released=0;
  h.context->beginRecording(h.device->createCommandList());
  h.context->m_submitWaitId=1;
  h.device->failSubmit=lost;
  DxvkSubmitStatus status;
  auto c=h.chunk();
  c->push([o=std::make_shared<Owner>(&released),&run](DxvkContext*){++run;});
  c->push([o=std::make_shared<Owner>(&released),&status](DxvkContext* ctx){ctx->flushCommandList(nullptr,&status);});
  c->push([o=std::make_shared<Owner>(&released),&run](DxvkContext* ctx){CHECK(ctx->m_cmd && ctx->m_cmd->recording);++run;});
  bool caught=false;
  try{h.cs.dispatchChunk(std::move(c));}catch(const DxvkError&){caught=true;}
  // Device loss is reported through the actual submission/device status.
  // It must abort recording without escaping through a COM destructor.
  CHECK(!caught);CHECK(run==(lost?1u:2u));CHECK(released==3);CHECK(!c);
  CHECK(status.result==(lost?VK_ERROR_DEVICE_LOST:VK_SUCCESS));
  CHECK(h.device->submitted==1);CHECK(h.device->created==(lost?1u:2u));
  CHECK(h.context->begun==(lost?1u:2u));CHECK(h.context->ended==1);
  CHECK(bool(h.context->m_cmd)==!lost);
  h.cs.synchronize(1);
  if(lost) {
    for(auto q:{DxvkCsQueue::Ordered,DxvkCsQueue::HighPriority}) {
      c=h.chunk();c->push([&run](DxvkContext*){++run;});
      h.cs.injectChunk(q,std::move(c),true);CHECK(!c);
    }
    CHECK(run==1);CHECK(h.device->created==1);CHECK(h.device->submitted==1);
  }
}

int main(int argc,char** argv) {
  try {
    std::string which=argc>1?argv[1]:"healthy";
    if(which=="healthy")healthy();else if(which=="removed")removed();
    else if(which=="throwing")throwing();else if(which=="ownership")reentrantOwner();
    else if(which=="frontend")frontend<Frontend>();
    else if(which=="frontend9")frontend<Frontend9>();
    else if(which=="initializer"){initializer<D3D11Initializer>();initializer<D3D9Initializer>();}
    else if(which=="context-healthy")context(false);
    else if(which=="context-lost")context(true);
    else throw std::runtime_error("unknown case");
    std::cout<<"PASS "<<which<<" checks="<<checks<<'\n';return 0;
  }catch(const std::exception& e){std::cerr<<"FAIL checks="<<checks<<" "<<e.what()<<'\n';return 1;}
}
