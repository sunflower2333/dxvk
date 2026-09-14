# DroidVM DXVK optimization tracking

## Current focus

- command submission batching
- descriptor update pressure
- staging upload reuse
- Turnip/VIOGPU submit latency correlation

## Rules

- keep Vulkan behavior unchanged
- measure before changing hot paths
- avoid adding per-draw logging
- validate with Proton workloads
