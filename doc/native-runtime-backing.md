# Native runtime allocation and submission backing

The native CreateDevice entry now copies its original runtime callbacks before
creating the embedded DXVK backend. It supplies the matching Mesa private v1
callback owner in VkDeviceCreateInfo. Turnip's first internal buffer allocation,
all later backend BO allocation/map operations and queue RenderCb submissions
therefore use the same runtime device and native context. An unsupported ICD
is rejected before device creation; there is no direct-KMT fallback in this path.
The explicit development factory without native callbacks retains its prior
direct-KMT mode.

RuntimeGpu owns callbacks and allocation metadata outside runtime private
storage. The VkDevice owner retains it through final worker drain and Vulkan
destruction. The DDI releases the backend before closing callbacks. A callback
can synchronously retire the runtime: the provider then stops using all opaque
runtime handles. Final kernel teardown owns residual backing in that case.
This does not establish Microsoft's backing teardown serialization guarantee.

The native context query validates the exact LUID, generation, VA arena,
context and queue identity. Allocations use runtime AllocateCb with actual
native IOVA, generation and ContextId. Tokens do not expose KMT handles and
are never reused within the owner. Retain/import aliases share allocation
identity; nested maps balance locks and propagate renamed handles. A submitted
reference must belong to this owner, have valid ranges/access and non-overlapping
patches. Referenced records remain reserved across runtime callback reentry.
Replacement RenderCb command/allocation/patch buffers are consumed on both
success and failure. Completion uses the existing 56-byte KMD escape.

The private protocol header is byte-identical to Mesa dfbe4f3. This checkpoint
needs that compatible Turnip ICD for each process architecture; it changes no
Mesa/KMD wire ABI or external-memory advertisement. Standalone DLLs are unsigned
development artifacts until the parent assembles/signs the full driver package.

Regression links the actual native DDI and runtime allocator sources. Its
controlled backend allocates/maps/submits/releases a BO during CreateDevice,
after overwriting the caller's callback table. It checks failed backend creation,
map renaming, alias/stale-token ownership, reset, malformed callback success,
replaced submit buffers, invalid owners/ranges, failed release and recursive
retirement at every kernel callback. WARP is used only for fixture device
lifetime; these tests do not prove Turnip hardware or ordinary system-runtime
activation.

Native resources now obtain their backend memory from the runtime allocator.
This slice does not yet associate a distinct hRTResource with a dedicated
import token: DXVK may suballocate BO memory. The prior synchronous Present
publication path remains separate. Explicit native resource import, opened
sharing, primary/flip ownership, complete feature-level DDIs and registration
remain open, and supported production versions remain zero.
