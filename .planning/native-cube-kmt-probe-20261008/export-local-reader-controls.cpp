// SPDX-License-Identifier: MIT
// Synthetic arithmetic controls only. No Windows API, shader, core or GPU.
#include "../../tests/umd-cube-probe-oracle.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace oracle = dxvk::umd::probe::cube;
std::filesystem::path root;
void save(const std::string& name, const uint32_t* words, size_t count) {
  std::ofstream output(root / name, std::ios::binary);
  output.write(reinterpret_cast<const char*>(words), std::streamsize(count * 4));
  if (!output) throw std::runtime_error("synthetic control export failed");
}
int main(int argc, char** argv) {
  if (argc != 2 || !std::filesystem::create_directory(argv[1])) return 2;
  root = argv[1];
  const oracle::Shape shapes[] = {{7, 3, 12}, {7, 3, 12}, {8, 4, 12}, {8, 4, 12}, {8, 4, 12}, {7, 3, 6}};
  const oracle::Words resources[] = {oracle::initial(shapes[0]), oracle::transferred(),
    oracle::generated(6, UINT32_MAX), oracle::generated(0, 2), oracle::generated(6, 1), oracle::initial(shapes[5])};
  for (unsigned index = 0; index < 6; ++index) {
    const auto shape = shapes[index];
    for (unsigned face = 0; face < shape.faces; ++face) for (unsigned mip = 0; mip < shape.levels; ++mip) {
      const auto& words = resources[index][face * shape.levels + mip];
      const auto stem = "cube-readback-" + std::to_string(index) + "-face-" + std::to_string(face) + "-mip-" + std::to_string(mip);
      save(stem + ".actual.u32", words.data(), words.size()); save(stem + ".expected.u32", words.data(), words.size());
      const unsigned edge = oracle::extent(shape, mip);
      const uint32_t fields[] = {index, shape.edge, shape.levels, shape.faces, face, mip, edge, edge * 4 + 12, (edge * 4 + 12) * edge};
      save(stem + ".shape-pitches.u32", fields, std::size(fields));
    }
  }
  std::vector<std::array<unsigned, 5>> requests;
  for (const auto& range : {std::array<unsigned, 4>{0, 1, 0, 3}, {6, 1, 1, 2}, {0, 2, 0, 3}})
    for (unsigned mip = 0; mip < range[3]; ++mip) requests.push_back({0, range[0], range[1], range[2], mip});
  for (const auto& range : {std::array<unsigned, 3>{2, 6, 3}, {3, 0, 2}, {4, 6, 1}})
    for (unsigned mip = 0; mip < range[2]; ++mip) requests.push_back({range[0], range[1], 1, 1, mip});
  for (unsigned mip = 0; mip < 3; ++mip) requests.push_back({5, 0, 1, 0, mip});
  for (unsigned index = 0; index < requests.size(); ++index) {
    const auto r = requests[index]; const auto shape = shapes[r[0]];
    auto words = oracle::sampled(shape, resources[r[0]], r[1], r[2], r[3], r[4]);
    const uint32_t count = uint32_t(words.size()); words.resize(64, 0xcdcdcdcdu);
    const auto stem = "cube-sampled-" + std::to_string(index);
    save(stem + ".actual.u32", words.data(), words.size()); save(stem + ".expected.u32", words.data(), words.size());
    const uint32_t fields[] = {index, shape.edge, shape.levels, shape.faces, r[1], r[2], r[3], r[4], 64, count};
    save(stem + ".shape-relative-mip.u32", fields, std::size(fields));
  }
}
