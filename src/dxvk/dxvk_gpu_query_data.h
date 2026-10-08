#pragma once
// SPDX-License-Identifier: Zlib
#include <cstdint>

namespace dxvk {

  /**
   * \brief Query status
   *
   * Reports whether a query is in
   * signaled or unsignaled state.
   */
  enum class DxvkGpuQueryStatus : uint32_t {
    Invalid   = 0,
    Pending   = 1,
    Available = 2,
    Failed    = 3,
  };


  /**
   * \brief Occlusion query data
   *
   * Stores the number of samples
   * that passes fragment tests.
   */
  struct DxvkQueryOcclusionData {
    uint64_t samplesPassed;
  };

  /**
   * \brief Timestamp data
   *
   * Stores a GPU time stamp.
   */
  struct DxvkQueryTimestampData {
    uint64_t time;
  };

  /**
   * \brief Pipeline statistics
   *
   * Stores the counters for
   * pipeline statistics queries.
   */
  struct DxvkQueryStatisticData {
    uint64_t iaVertices;
    uint64_t iaPrimitives;
    uint64_t vsInvocations;
    uint64_t gsInvocations;
    uint64_t gsPrimitives;
    uint64_t clipInvocations;
    uint64_t clipPrimitives;
    uint64_t fsInvocations;
    uint64_t tcsPatches;
    uint64_t tesInvocations;
    uint64_t csInvocations;
  };

  /**
   * \brief Transform feedback stream query
   *
   * Stores the number of primitives written to the
   * buffer, as well as the number of primitives
   * generated. The latter can be used to check for
   * overflow.
   */
  struct DxvkQueryXfbStreamData {
    uint64_t primitivesWritten;
    uint64_t primitivesNeeded;
  };

  /**
   * \brief Query data
   *
   * A union that stores query data. Select an
   * appropriate member based on the query type.
   */
  union DxvkQueryData {
    DxvkQueryOcclusionData occlusion;
    DxvkQueryTimestampData timestamp;
    DxvkQueryStatisticData statistic;
    DxvkQueryXfbStreamData xfbStream;
  };


}
