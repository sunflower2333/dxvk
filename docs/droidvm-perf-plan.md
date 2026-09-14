# DroidVM DXVK optimization scope

## Goals

Optimize the DXVK path for DroidVM VIOGPU without changing Vulkan ABI.

## Phase 1

- Add lightweight submit/batch observation hooks.
- Measure command list lifetime and queue submission behavior.
- Avoid per-draw logging.

## Phase 2

- Reduce descriptor heap churn.
- Improve staging upload reuse.
- Validate pipeline cache hit behavior.

## Validation

All changes must be checked against existing Vulkan correctness tests before workload tuning.
