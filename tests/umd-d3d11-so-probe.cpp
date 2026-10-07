// SPDX-License-Identifier: MIT
#include "umd-kmt-graphics-probe.h"
#include "umd-so-oracle.h"
#include <algorithm>

namespace {
using namespace dxvk::umd::probe;
using namespace dxvk::umd::probe::graphics;

constexpr char VertexSource[] = R"(
struct V{float4 p:SV_Position;uint id:DATA0;};
V main(uint id:SV_VertexID){V v;v.p=float4(0,0,0,1);v.id=id;return v;})";
constexpr char GeometrySource[] = R"(
struct V{float4 p:SV_Position;uint id:DATA0;};struct O{float4 p:SV_Position;uint2 value:DATA0;};
[maxvertexcount(10)]void main(point V v[1],inout PointStream<O> s0,inout PointStream<O> s1,inout PointStream<O> s2,inout PointStream<O> s3){
O o;o.p=v[0].p;o.value=uint2(v[0].id,900);s0.Append(o);
for(uint j=0;j<2;++j){o.value=uint2(v[0].id+100,901+j);s1.Append(o);}
for(uint k=0;k<3;++k){o.value=uint2(v[0].id+200,902+k);s2.Append(o);}
for(uint l=0;l<4;++l){o.value=uint2(v[0].id+300,903+l);s3.Append(o);}})";
constexpr char NullVertexA[] = R"(
struct O{float4 p:SV_Position;uint4 v:DATA0;};
O main(uint id:SV_VertexID){O o;o.p=float4(0,0,0,1);
o.v=uint4(0x7fc01234u,0x80000000u,0x87654321u,id);return o;})";
constexpr char NullVertexB[] = R"(
struct O{float4 p:SV_Position;uint4 v:DATA0;};
O main(uint id:SV_VertexID){O o;o.p=float4(0,0,0,1);
o.v=uint4(0xffc04321u,0xffffffffu,0x11223344u,id+100u);return o;})";

void streamOnly(Session& s) {
  s.call([&] {
    s.api().pfnPsSetShader(s.device(), {}); s.api().pfnHsSetShader(s.device(), {}); s.api().pfnDsSetShader(s.device(), {});
    s.api().pfnIaSetInputLayout(s.device(), {});
    s.api().pfnIaSetTopology(s.device(), D3D10_DDI_PRIMITIVE_TOPOLOGY_POINTLIST);
  }, "stream-only-pipeline");
}

void capture(Session& s, Resource& buffer, const std::wstring& directory, const std::wstring& name,
    const SoCapture& expected, unsigned stream, unsigned firstVertex, bool nullGeometry = false) {
  const auto words = readBuffer(s, buffer, SoWords);
  require(words.size() == expected.size(), "SO-readback-size");
  SoCapture actual{}; std::copy(words.begin(), words.end(), actual.begin());
  writeOriginal(directory, name + L".actual.u32", actual.data(), sizeof(actual));
  writeOriginal(directory, name + L".expected.u32", expected.data(), sizeof(expected));
  unsigned mismatches = 0;
  for (unsigned word = 0; word < SoWords; ++word) {
    mismatches += actual[word] != expected[word];
    std::printf("D3D11_KMT_SO_WORD case=%ls stream=%u word=%u actual=%08x expected=%08x\n",
      name.c_str(), stream, word, actual[word], expected[word]);
  }
  const bool matches = nullGeometry ? nullSoCaptureMatches(actual, firstVertex != 0)
    : soCaptureMatches(actual, stream, firstVertex);
  require(!mismatches && matches, "SO-entire-buffer-oracle");
  std::printf("D3D11_KMT_SO_CAPTURE case=%ls stream=%u words=64 mismatches=0 null_geometry=%u\n",
    name.c_str(), stream, unsigned(nullGeometry));
}

void rejectDeclarations(Session& s, const ShaderSource& source,
    const D3D11DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT& valid, const std::wstring& directory) {
  auto inputs = source.inputs, outputs = source.outputs;
  const D3D10DDIARG_STAGE_IO_SIGNATURES signature{inputs.data(), UINT(inputs.size()), outputs.data(), UINT(outputs.size())};
  auto original = valid; original.pShaderCode = source.tokens.data();
  const SIZE_T size = s.size([&] { return s.api().pfnCalcPrivateGeometryShaderWithStreamOutput(
    s.device(), &original, &signature); }, "negative-SO-private-size");
  for (unsigned test = 0; test < 8; ++test) {
    auto desc = original;
    std::vector<D3D11DDIARG_STREAM_OUTPUT_DECLARATION_ENTRY> entries(valid.pOutputStreamDecl,
      valid.pOutputStreamDecl + valid.NumEntries);
    std::vector<UINT> strides(valid.BufferStridesInBytes, valid.BufferStridesInBytes + valid.NumStrides);
    desc.pOutputStreamDecl = entries.data(); desc.BufferStridesInBytes = strides.data();
    if (test == 0) entries[0].RegisterMask = 0;
    if (test == 1) entries[0].OutputSlot = D3D11_SO_BUFFER_SLOT_COUNT;
    if (test == 2) entries[0].Stream = D3D11_SO_STREAM_COUNT;
    if (test == 3) entries[0].RegisterIndex = D3D11_VS_OUTPUT_REGISTER_COUNT;
    if (test == 4) entries[0].RegisterMask = 8; // DATA is an original uint2.
    if (test == 5) desc.RasterizedStream = D3D11_SO_STREAM_COUNT;
    if (test == 6) strides[1] = 13;
    if (test == 7) strides[1] = 4;
    Storage failed(size); const D3D10DDI_HSHADER handle{failed.data()};
    const HRESULT error = s.negative([&] { s.api().pfnCreateGeometryShaderWithStreamOutput(
      s.device(), &desc, handle, {}, &signature); }, E_INVALIDARG, "named-invalid-SO-declaration");
    require(error == E_INVALIDARG && failed.intact() && failed.zero(), "invalid-SO-declaration-atomic", error);
    const UINT fields[] = {test, entries[0].Stream, entries[0].OutputSlot, entries[0].RegisterIndex,
      entries[0].RegisterMask, desc.RasterizedStream, strides[1], UINT(error)};
    writeOriginal(directory, L"invalid-SO-" + std::to_wstring(test) + L".fields-u32", fields, sizeof(fields));
    std::printf("D3D11_KMT_SO_NEGATIVE case=%u hr=%08lx private_unchanged=1 guards=1\n",
      test, static_cast<unsigned long>(error));
  }
  s.transport().beginDdi(); s.check("negative-SO-device-guards");
  // The following raw captures prove the original bound GS remains usable.
}

void fourStreams(Session& s, const std::wstring& directory) {
  const auto vertexSource = compileShader(directory, L"four-stream-vs", VertexSource, "vs_5_0", 1);
  const auto geometrySource = compileShader(directory, L"four-stream-gs", GeometrySource, "gs_5_0", 2);
  std::array<D3D11DDIARG_STREAM_OUTPUT_DECLARATION_ENTRY, 5> entries{};
  std::array<UINT, 4> strides{};
  for (unsigned stream = 0; stream < SoStreams; ++stream) {
    entries[stream] = {stream, stream, geometrySource.outputRegister("DATA", stream), 3};
    strides[stream] = SoStrideWords[stream] * sizeof(UINT);
  }
  // Deliberately discontiguous stream0 grouping, matching the original DDI
  // conversion control. Only stream-local order controls its skipped DWORD.
  entries[4] = {0, 0, D3D10_SO_DDI_REGISTER_INDEX_DENOTING_GAP, 1};
  D3D11DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT desc{};
  desc.pOutputStreamDecl = entries.data(); desc.NumEntries = UINT(entries.size());
  desc.BufferStridesInBytes = strides.data(); desc.NumStrides = UINT(strides.size());
  desc.RasterizedStream = D3D11_SO_NO_RASTERIZED_STREAM;
  writeOriginal(directory, L"four-stream-DDI-declaration.raw", entries.data(), sizeof(entries));
  writeOriginal(directory, L"four-stream-strides.u32", strides.data(), sizeof(strides));
  Shader vertex(s, vertexSource, 1), geometry(s, geometrySource, 2, &desc);
  vertex.bind(); geometry.bind(); streamOnly(s);
  rejectDeclarations(s, geometrySource, desc, directory);
  SoCapture poison{}; poison.fill(SoPoison);
  D3D10_DDIARG_SUBRESOURCE_UP data{poison.data(), sizeof(poison), sizeof(poison)};
  std::array<D3D10DDI_MIPINFO, 4> shapes{};
  std::array<std::unique_ptr<Resource>, 4> buffers;
  std::array<D3D10DDI_HRESOURCE, 4> handles{};
  const UINT offsets[] = {0, 0, 0, 0};
  for (unsigned stream = 0; stream < SoStreams; ++stream) {
    buffers[stream] = std::make_unique<Resource>(s, bufferDescription(shapes[stream], sizeof(poison), D3D10_DDI_BIND_STREAM_OUTPUT, &data));
    handles[stream] = buffers[stream]->handle;
  }
  std::array<std::unique_ptr<Query>, 4> statistics, overflow;
  const D3D10DDI_QUERY statisticsTypes[] = {D3D11DDI_QUERY_STREAMOUTPUTSTATS_STREAM0, D3D11DDI_QUERY_STREAMOUTPUTSTATS_STREAM1,
    D3D11DDI_QUERY_STREAMOUTPUTSTATS_STREAM2, D3D11DDI_QUERY_STREAMOUTPUTSTATS_STREAM3};
  const D3D10DDI_QUERY overflowTypes[] = {D3D11DDI_QUERY_STREAMOVERFLOWPREDICATE_STREAM0, D3D11DDI_QUERY_STREAMOVERFLOWPREDICATE_STREAM1,
    D3D11DDI_QUERY_STREAMOVERFLOWPREDICATE_STREAM2, D3D11DDI_QUERY_STREAMOVERFLOWPREDICATE_STREAM3};
  for (unsigned stream = 0; stream < SoStreams; ++stream) {
    statistics[stream] = std::make_unique<Query>(s, statisticsTypes[stream]);
    overflow[stream] = std::make_unique<Query>(s, overflowTypes[stream]);
  }
  Query aggregate(s, D3D10DDI_QUERY_STREAMOVERFLOWPREDICATE);
  for (unsigned draw = 0; draw < 2; ++draw) {
    const unsigned first = draw ? 17 : 0;
    s.call([&] { s.api().pfnSoSetTargets(s.device(), 4, 0, handles.data(), offsets); }, "bind-four-stream-targets");
    for (unsigned stream = 0; stream < SoStreams; ++stream) { statistics[stream]->begin(); overflow[stream]->begin(); }
    aggregate.begin(); s.call([&] { s.api().pfnDraw(s.device(), 3, first); }, "draw-four-streams"); aggregate.end();
    for (unsigned stream = 0; stream < SoStreams; ++stream) { statistics[stream]->end(); overflow[stream]->end(); }
    for (unsigned stream = 0; stream < SoStreams; ++stream) {
      const auto stats = statistics[stream]->result<D3D10_DDI_QUERY_DATA_SO_STATISTICS>();
      const auto over = overflow[stream]->result<BOOL>();
      require(stats.NumPrimitivesWritten == 3 * (stream + 1) && stats.PrimitivesStorageNeeded == stats.NumPrimitivesWritten
        && over == FALSE, "four-stream-statistics");
      const UINT64 raw[] = {stats.NumPrimitivesWritten, stats.PrimitivesStorageNeeded, UINT64(over)};
      writeOriginal(directory, L"draw-" + std::to_wstring(draw) + L"-stream-" + std::to_wstring(stream) + L".query-u64", raw, sizeof(raw));
      capture(s, *buffers[stream], directory, L"draw-" + std::to_wstring(draw) + L"-stream-" + std::to_wstring(stream),
        expectedSoCapture(stream, first), stream, first);
    }
    const BOOL aggregateOverflow = aggregate.result<BOOL>();
    writeOriginal(directory, L"draw-" + std::to_wstring(draw) + L"-aggregate-overflow.u32", &aggregateOverflow, sizeof(aggregateOverflow));
    require(aggregateOverflow == FALSE, "four-stream-aggregate-no-overflow");
  }
  D3D10DDI_MIPINFO tinyShape{};
  Resource tiny(s, bufferDescription(tinyShape, 24, D3D10_DDI_BIND_STREAM_OUTPUT, &data));
  auto limited = handles; limited[2] = tiny.handle;
  s.call([&] { s.api().pfnSoSetTargets(s.device(), 4, 0, limited.data(), offsets); }, "bind-stream2-overflow-target");
  for (unsigned stream = 0; stream < SoStreams; ++stream) { statistics[stream]->begin(); overflow[stream]->begin(); }
  aggregate.begin(); s.call([&] { s.api().pfnDraw(s.device(), 3, 0); }, "draw-stream2-overflow"); aggregate.end();
  for (unsigned stream = 0; stream < SoStreams; ++stream) { statistics[stream]->end(); overflow[stream]->end(); }
  for (unsigned stream = 0; stream < SoStreams; ++stream) {
    const auto stats = statistics[stream]->result<D3D10_DDI_QUERY_DATA_SO_STATISTICS>();
    const auto over = overflow[stream]->result<BOOL>();
    require(stats.NumPrimitivesWritten == (stream == 2 ? 1 : 3 * (stream + 1))
      && stats.PrimitivesStorageNeeded == 3 * (stream + 1) && over == (stream == 2 ? TRUE : FALSE), "isolated-stream2-overflow");
    const UINT64 raw[] = {stats.NumPrimitivesWritten, stats.PrimitivesStorageNeeded, UINT64(over)};
    writeOriginal(directory, L"overflow-stream-" + std::to_wstring(stream) + L".query-u64", raw, sizeof(raw));
  }
  const BOOL aggregateOverflow = aggregate.result<BOOL>();
  writeOriginal(directory, L"overflow-aggregate-overflow.u32", &aggregateOverflow, sizeof(aggregateOverflow));
  require(aggregateOverflow == TRUE, "aggregate-stream2-overflow");
  s.call([&] { s.api().pfnSoSetTargets(s.device(), 0, 4, nullptr, nullptr); s.api().pfnGsSetShader(s.device(), {}); }, "unbind-four-streams");
  std::printf("D3D11_KMT_SO_FOUR_STREAMS_PASS draws=3 raw_words=512 overflow_stream=2\n");
}

void nullGeometry(Session& s, const std::wstring& directory) {
  const auto sourceA = compileShader(directory, L"null-GS-vs-A", NullVertexA, "vs_5_0", 1);
  const auto sourceB = compileShader(directory, L"null-GS-vs-B", NullVertexB, "vs_5_0", 1);
  const D3D11DDIARG_STREAM_OUTPUT_DECLARATION_ENTRY entries[] = {
    {0, 0, sourceA.outputRegister("SV_Position"), 15}, {0, 0, sourceA.outputRegister("DATA"), 15}};
  const UINT stride = 32, offset = 0;
  D3D11DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT desc{};
  desc.pOutputStreamDecl = entries; desc.NumEntries = UINT(std::size(entries));
  desc.BufferStridesInBytes = &stride; desc.NumStrides = 1; desc.RasterizedStream = D3D11_SO_NO_RASTERIZED_STREAM;
  writeOriginal(directory, L"null-GS-DDI-declaration.raw", entries, sizeof(entries));
  Shader vertexA(s, sourceA, 1), vertexB(s, sourceB, 1), passthrough(s, sourceA, 2, &desc, true);
  passthrough.bind(); streamOnly(s);
  SoCapture poison{}; poison.fill(SoPoison);
  D3D10_DDIARG_SUBRESOURCE_UP data{poison.data(), sizeof(poison), sizeof(poison)};
  D3D10DDI_MIPINFO shape{};
  Resource captured(s, bufferDescription(shape, sizeof(poison), D3D10_DDI_BIND_STREAM_OUTPUT, &data));
  for (unsigned draw = 0; draw < 3; ++draw) {
    (draw == 1 ? vertexB : vertexA).bind();
    s.call([&] { s.api().pfnSoSetTargets(s.device(), 1, 3, &captured.handle, &offset); }, "bind-null-GS-capture");
    s.call([&] { s.api().pfnDraw(s.device(), 3, 0); }, "draw-null-GS-capture");
    capture(s, captured, directory, L"null-GS-draw-" + std::to_wstring(draw), expectedNullSoCapture(draw == 1), 0, draw == 1, true);
  }
  s.call([&] { s.api().pfnSoSetTargets(s.device(), 0, 4, nullptr, nullptr); s.api().pfnGsSetShader(s.device(), {}); }, "unbind-null-GS");
  std::printf("D3D11_KMT_SO_NULL_GEOMETRY_PASS draws=3 raw_words=192\n");
}
} // namespace

int wmain(int argc, WCHAR** argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  LUID luid{};
  if (argc != 3 || !parseLuid(argv[1], luid)) {
    std::fprintf(stderr, "usage: dxvk-umd-d3d11-so-probe <16 hex LUID bytes> <fresh output directory>\n"); return 2;
  }
  KmtComputeTransport transport;
  HRESULT hr = S_OK;
  bool completed = false;
  unsigned acceptedErrors = 0;
  std::printf("D3D11_KMT_SO_SELECTED luid="); printLuid(luid); std::printf("\n");
  try {
    WCHAR path[32768]{};
    const HMODULE core = GetModuleHandleW(VIOGPU_DXVK_UMD_FILE);
    const DWORD length = core ? GetModuleFileNameW(core, path, DWORD(std::size(path))) : 0;
    require(length && length < std::size(path), "original-core-module");
    std::printf("D3D11_KMT_SO_CORE path=%ls\n", path);
    const std::wstring directory = argv[2];
    const BOOL created = CreateDirectoryW(directory.c_str(), nullptr);
    require(created, "fresh-original-directory", created ? S_OK : HRESULT_FROM_WIN32(GetLastError()));
    hr = transport.open(luid); require(hr == S_OK, "real-kmt-open", hr);
    Session session(transport); session.create();
    try { fourStreams(session, directory); nullGeometry(session, directory); }
    catch (...) { acceptedErrors = session.acceptedErrors(); throw; }
    acceptedErrors = session.acceptedErrors();
    hr = session.close(); require(hr == S_OK, "typed11-owned-teardown", hr);
    require(transport.balanced(acceptedErrors), "actual-kernel-ownership-balance");
    completed = true;
  } catch (const Failure& failure) {
    hr = failure.result; std::printf("D3D11_KMT_SO_FAILURE stage=%s hr=%08lx\n", failure.stage, static_cast<unsigned long>(hr));
  } catch (const std::bad_alloc&) { hr = E_OUTOFMEMORY; }
  catch (...) { hr = E_FAIL; }
  transport.printCounts();
  const bool balanced = completed && transport.balanced(acceptedErrors);
  const HRESULT closed = transport.close();
  std::printf("D3D11_KMT_SO_RAW_CLOSE hr=%08lx\n", static_cast<unsigned long>(closed));
  if (hr == S_OK && closed != S_OK) hr = closed;
  const bool passed = completed && balanced && hr == S_OK;
  std::printf("D3D11_KMT_SO_%s streams=4 draws=6 words=704 negatives=8 expected_core_errors=%u guards=%u balanced=%u hr=%08lx ordinary_runtime_admission=0\n",
    passed ? "PASS" : "FAIL", acceptedErrors, unsigned(completed), unsigned(balanced), static_cast<unsigned long>(hr));
  return passed ? 0 : 1;
}
