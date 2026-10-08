// SPDX-License-Identifier: Zlib
// Actual production metadata helper; no GPU execution or predication admission.
#include "../src/d3d11/d3d11_command_order.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

static unsigned checks;
#define CHECK(value) do { ++checks; if (!(value)) std::abort(); } while (0)
struct Query {
  static unsigned live;
  const unsigned id;
  explicit Query(unsigned value) : id(value) { ++live; }
  ~Query() { --live; }
};
unsigned Query::live;
using Reference=std::shared_ptr<Query>;
using Order=dxvk::D3D11CommandOrder<Reference>;
using Type=dxvk::D3D11RecordedOperationType;
using dxvk::D3D11RecordedCurrentChunkId;
using dxvk::D3D11RelocatedChunkId;

static std::string trace(const Order& order) {
  std::string result;
  for (const auto& operation : order.operations()) {
    if (!result.empty()) result+='|';
    const auto id=operation.query ? operation.query->id : 0;
    if (operation.type==Type::Chunk) result+="C"+std::to_string(operation.chunkId);
    if (operation.type==Type::Predicate) result+="P"+std::to_string(id)+":"+std::to_string(operation.predicateValue)+":"+std::to_string(operation.predicateHint);
    if (operation.type==Type::QueryBegin) result+="B"+std::to_string(id)+":"+std::to_string(operation.implicitBegin);
    if (operation.type==Type::QueryEnd) result+="E"+std::to_string(id)+":"+std::to_string(operation.endOccurrence);
  }
  return result;
}
static Order child(const Reference& a,const Reference& b) {
  Order result;
  result.addQueryBegin(a,false); result.addChunk(0); result.addChunk(1);
  result.addQueryEnd(a); result.addChunk(2);
  result.addPredicate(a,-7,false); result.addChunk(3);
  result.addPredicate({},17,false);
  result.addQueryBegin(a,true); result.addChunk(4);
  result.addQueryEnd(a); result.addChunk(5);
  result.addQueryBegin(b,false); result.addChunk(6);
  result.addQueryEnd(b); result.addChunk(7);
  result.addPredicate(b,1,true); result.addChunk(8);
  result.addPredicate({},0,false); result.addChunk(9);
  return result;
}
int main() {
  CHECK(D3D11RecordedCurrentChunkId(0,true)==0);
  // The first pending chunk is zero, not one: resource tracking must identify
  // the chunk that AddChunk will actually record.
  CHECK(D3D11RecordedCurrentChunkId(0,false)==0);
  CHECK(D3D11RecordedCurrentChunkId(1,true)==0);
  CHECK(D3D11RecordedCurrentChunkId(1,false)==1);
  CHECK(D3D11RecordedCurrentChunkId(26,true)==25);
  CHECK(D3D11RecordedCurrentChunkId(26,false)==26);
  Order defaults;
  CHECK(trace(defaults)=="P0:0:0" && defaults.endOccurrences()==0);
  defaults.addPredicate({},0x12345678,false);
  CHECK(trace(defaults)=="P0:0:0|P0:305419896:0");
  auto a=std::make_shared<Query>(1),b=std::make_shared<Query>(2);
  std::weak_ptr<Query> weakA=a,weakB=b;
  Order recorded=child(a,b);
  const std::string expected="P0:0:0|B1:0|C0|C1|E1:1|C2|P1:-7:0|C3|P0:17:0|B1:1|C4|E1:2|C5|B2:0|C6|E2:3|C7|P2:1:1|C8|P0:0:0|C9";
  CHECK(trace(recorded)==expected && recorded.endOccurrences()==3);
  Order outer;
  outer.addPredicate(b,33,true); outer.addChunk(0);
  outer.addPredicate({},0,false); outer.addChunk(1);
  outer.append(recorded,2);
  // Restore TRUE rebinds the saved exact predicate; the next nested execution
  // resets execution state without mutating the saved API state.
  outer.addPredicate(b,33,true); outer.addChunk(12);
  outer.addPredicate({},0,false); outer.addChunk(13);
  outer.append(recorded,14); outer.addChunk(24);
  const std::string expectedOuter="P0:0:0|P2:33:1|C0|P0:0:0|C1|P0:0:0|B1:0|C2|C3|E1:1|C4|P1:-7:0|C5|P0:17:0|B1:1|C6|E1:2|C7|B2:0|C8|E2:3|C9|P2:1:1|C10|P0:0:0|C11|P2:33:1|C12|P0:0:0|C13|P0:0:0|B1:0|C14|C15|E1:4|C16|P1:-7:0|C17|P0:17:0|B1:1|C18|E1:5|C19|B2:0|C20|E2:6|C21|P2:1:1|C22|P0:0:0|C23|C24";
  CHECK(trace(outer)==expectedOuter && outer.endOccurrences()==6);
  Order nested;
  nested.addQueryBegin(b,false); nested.addChunk(0);
  nested.addQueryEnd(b); nested.addChunk(1);
  nested.append(outer,2);
  unsigned chunks=0,ends=0;
  for (const auto& operation : nested.operations()) {
    if (operation.type==Type::Chunk) CHECK(operation.chunkId==chunks++);
    if (operation.type==Type::QueryEnd) CHECK(operation.endOccurrence==++ends);
  }
  CHECK(chunks==27 && ends==7 && nested.endOccurrences()==7);
  // Chunk/resource IDs use the same relocation helper, preserving ownership
  // and actual CS sequence assignment rather than operation-vector positions.
  constexpr std::array<unsigned,4> resources{{0,3,5,9}};
  for (const auto id : resources) {
    CHECK(D3D11RelocatedChunkId(id,2)==id+2);
    CHECK(D3D11RelocatedChunkId(id,14)==id+14);
    CHECK(D3D11RelocatedChunkId(D3D11RelocatedChunkId(id,14),2)==id+16);
  }
  a.reset();b.reset();recorded=Order();
  CHECK(!weakA.expired() && !weakB.expired() && Query::live==2);
  for (unsigned replay=0;replay<32;++replay) {
    CHECK(trace(outer)==expectedOuter);
    CHECK(nested.endOccurrences()==7);
  }
  Order repeated;
  // Each insertion gives recorded Ends distinct occurrence IDs while replay
  // of the immutable outer/nested stream above leaves recorded IDs unchanged.
  auto childA=weakA.lock(),childB=weakB.lock();
  auto pattern=child(childA,childB);
  constexpr unsigned repeats=4096;
  constexpr std::array<Type,21> types{{Type::Predicate,Type::QueryBegin,Type::Chunk,Type::Chunk,Type::QueryEnd,Type::Chunk,Type::Predicate,Type::Chunk,Type::Predicate,Type::QueryBegin,Type::Chunk,Type::QueryEnd,Type::Chunk,Type::QueryBegin,Type::Chunk,Type::QueryEnd,Type::Chunk,Type::Predicate,Type::Chunk,Type::Predicate,Type::Chunk}};
  for (unsigned insertion=0;insertion<repeats;++insertion) {
    const uint64_t base=uint64_t(insertion)*10;
    CHECK(D3D11RecordedCurrentChunkId(base,false)==base);
    repeated.append(pattern,base);
    CHECK(D3D11RecordedCurrentChunkId(base+10,true)==base+9);
    for (unsigned index=0;index<types.size();++index) {
      const auto& operation=repeated.operations()[1+insertion*types.size()+index];
      CHECK(operation.type==types[index]);
      if (operation.type==Type::Predicate && index==8)
        CHECK(!operation.query && operation.predicateValue==17 && !operation.predicateHint);
      if (operation.type==Type::Predicate && index==17)
        CHECK(operation.query->id==2 && operation.predicateValue==1 && operation.predicateHint);
    }
  }
  chunks=0;ends=0;
  for (const auto& operation : repeated.operations()) {
    if (operation.type==Type::Chunk) CHECK(operation.chunkId==chunks++);
    if (operation.type==Type::QueryEnd) CHECK(operation.endOccurrence==++ends);
  }
  CHECK(chunks==repeats*10 && ends==repeats*3 && repeated.endOccurrences()==repeats*3);
  childA.reset();childB.reset();pattern=Order();outer=Order();nested=Order();
  CHECK(!weakA.expired() && !weakB.expired());
  repeated=Order(); CHECK(weakA.expired() && weakB.expired() && Query::live==0);
  std::printf("D3D11 command order PASS checks=%u nested_chunks=27 nested_ends=7 repeats=%u repeated_chunks=%u repeated_ends=%u live_queries=%u gpu_execution=0 predication_admission=0\n",checks,repeats,chunks,ends,Query::live);
}
