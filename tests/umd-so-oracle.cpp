// SPDX-License-Identifier: MIT
#include "umd-so-oracle.h"
#include "umd-kmt-callback-policy.h"
#include <cstdio>
#include <cstdlib>

using namespace dxvk::umd::probe;
static unsigned checks;
static void check(bool value) {
  ++checks;
  if (!value) { std::fprintf(stderr, "SO oracle failed check=%u\n", checks); std::abort(); }
}

int main() {
  unsigned callbackChecks = 0;
  const int32_t invalid = int32_t(0x80070057u), pending = int32_t(0x887a000au);
  for (unsigned accepted : {0u, 1u, 7u, 1234u, UINT32_MAX - 1}) {
    for (int32_t expected : {invalid, pending}) {
      unsigned next = 0x12345678u;
      check(admitExpectedCoreCallback(accepted, accepted + 1, expected, expected, next)
        && next == accepted + 1); ++callbackChecks;
      for (unsigned raw : {accepted, accepted + 2}) {
        next = 0x12345678u;
        check(!admitExpectedCoreCallback(accepted, raw, expected, expected, next)
          && next == 0x12345678u); ++callbackChecks;
      }
      for (int32_t observed : {int32_t(0), expected == pending ? invalid : pending}) {
        next = 0x12345678u;
        check(!admitExpectedCoreCallback(accepted, accepted + 1, expected, observed, next)
          && next == 0x12345678u); ++callbackChecks;
      }
    }
  }
  for (int32_t unexpected : {int32_t(0), int32_t(1), invalid, pending}) {
    unsigned next = 0x12345678u;
    check(!admitExpectedCoreCallback(UINT32_MAX, 0, unexpected, unexpected, next)
      && next == 0x12345678u); ++callbackChecks;
  }
  unsigned next = 0x12345678u;
  check(!admitExpectedCoreCallback(0, 1, 0, 0, next) && next == 0x12345678u); ++callbackChecks;
  unsigned bitFlips = 0;
  for (unsigned first : {0u, 17u}) for (unsigned stream = 0; stream < SoStreams; ++stream) {
    auto captured = expectedSoCapture(stream, first);
    check(soCaptureMatches(captured, stream, first));
    unsigned written = 0;
    for (unsigned i = 0; i < SoWords; ++i) {
      const unsigned record = i / SoStrideWords[stream];
      const unsigned component = i % SoStrideWords[stream];
      const bool output = record < 3 * (stream + 1) && component < 2;
      const uint32_t scalar = output ? (component == 0 ? first + record / (stream + 1) + 100 * stream
        : 900 + stream + record % (stream + 1)) : SoPoison;
      check(captured[i] == scalar);
      written += output;
      for (unsigned bit = 0; bit < 32; ++bit) {
        captured[i] ^= uint32_t(1) << bit;
        check(!soCaptureMatches(captured, stream, first));
        captured[i] ^= uint32_t(1) << bit;
        ++bitFlips;
      }
    }
    check(written == 6 * (stream + 1));
    check(!soCaptureMatches(captured, SoStreams, first));
    check(!soCaptureMatches(captured, stream, first + 1));
  }
  for (bool alternate : {false, true}) {
    auto captured = expectedNullSoCapture(alternate);
    check(nullSoCaptureMatches(captured, alternate));
    check(!nullSoCaptureMatches(captured, !alternate));
    for (unsigned i = 0; i < SoWords; ++i) for (unsigned bit = 0; bit < 32; ++bit) {
      captured[i] ^= uint32_t(1) << bit;
      check(!nullSoCaptureMatches(captured, alternate));
      captured[i] ^= uint32_t(1) << bit;
      ++bitFlips;
    }
  }
  std::printf("SO capture oracle verified checks=%u bit_flips=%u streams=4 raw_bits=1 callback_controls=%u\n", checks, bitFlips, callbackChecks);
}
