---
name: orklev2-gfxcontext
description: Answer questions about orkid's graphics Context class, sub-interface accessors (GBI/TXI/FXI/DWI/CI/FBI), frame lifecycle, context creation, RenderingConventions, GfxEnv singleton, display modes, mod color stack, ContextDummy, VulkanContext, and Python context bindings. Use when the user asks about the graphics context, rendering backend, or frame management.
user-invocable: false
---

# Orkid Graphics Context Reference

When answering questions about the graphics Context or its sub-interfaces in orkid, consult the files below. All under `ork.lev2/`.

## Key Files

| Component | File |
|-----------|------|
| Context Class | `inc/ork/lev2/gfx/gfxenv.h` (lines 201–509) |
| GfxEnv Singleton | `inc/ork/lev2/gfx/gfxenv.h` (lines 722–839) |
| Enums (formats, types) | `inc/ork/lev2/gfx/gfxenv_enum.h` |
| ContextDummy | `inc/ork/lev2/gfx/gfxctxdummy.h` |
| VulkanContext | `src/gfx/vulkan/headers/vulkan_ctx.h` (lines 664–903) |
| Type Definitions | `inc/ork/lev2/lev2_types.h` |
| Python Bindings | `pyext/src/pyext_gfx.cpp` |

## Architecture Overview

`Context` is the central abstract rendering device. All graphics operations flow through it and its six domain-specific sub-interfaces:

```
Context (gfxenv.h:201)
├── GBI()  → GeometryBufferInterface*   — vertex/index buffers, draw calls
├── TXI()  → TextureInterface*          — texture creation, loading, sampling
├── FXI()  → FxInterface*               — shaders, params, UBOs, SSBOs
├── DWI()  → DrawingInterface*          — 2D/3D quad/line drawing
├── CI()   → ComputeInterface*          — compute dispatch, barriers
├── FBI()  → FrameBufferInterface*      — render targets, viewport, capture
├── MTXI() → MatrixStackInterface*      — M/V/P matrix stacks
├── IMI()  → ImmInterface*              — immediate-mode vertex buffers
└── PRI()  → PrimitivesInterface*       — high-level primitive rendering
```

## Context Class (gfxenv.h:201–509)

### Frame Lifecycle
- `beginFrame(bool visual=true)` / `endFrame()` — frame boundaries
- `scheduleOnBeginFrame(cb)` / `scheduleOnEndFrame(cb)` — callbacks
- `scheduleBeforeDoEndFrameOneShot(cb)` — one-shot pre-endframe

### Command Buffers
- `beginRecordCommandBuffer()` / `endRecordCommandBuffer()` — secondary CB recording
- `enqueueSecondaryCommandBuffer()` — submit recorded CB

### Surface Management
- `mainSurfaceWidth()` / `mainSurfaceHeight()` / `mainSurfaceAspectRatio()`
- `resizeMainSurface(w, h)`

### Mod Color Stack (8 levels)
- `PushModColor(fvec4)` / `PopModColor()` / `RefModColor()`

### Render Context Data
- `topRenderContextFrameData()` / `pushRenderContextFrameData()` / `popRenderContextFrameData()`
- `GetRenderContextInstData()` / `SetRenderContextInstData()`

### Debug
- `debugPushGroup(name)` / `debugPopGroup()` / `debugMarker(name)`

### Context Creation
- `initializeWindowContext(Window*, CTXBASE*)` — windowed
- `initializeOffscreenContext(DisplayBuffer*)` — headless
- `initializeLoaderContext()` — asset loading
- `makeCurrentContext()` / `TakeThreadOwnership()`

### Key Members
- `meTargetType` — WINDOW, OFFSCREEN, LOADING, NONE
- `miW, miH` — surface dimensions
- `_defaultRTG` — default render target group
- `miTargetFrame` — current frame number
- `maModColorStack[8]` — mod color stack

## RenderingConventions (gfxenv.h:166–182)

```cpp
struct RenderingConventions {
  bool _isRightHanded = true;       // RH (Vulkan/GL)
  bool _isLogicalYUp = true;        // Y-up logical coords
  bool _isNativeYUp = true;         // Y-up native
  bool _ndcZRange01 = false;        // false=[-1,1] (GL), true=[0,1] (Vulkan)
  bool _defaultWindingCCW = true;   // CCW front faces
};
```

## EBufferFormat (gfxenv_enum.h:121–161)

| Category | Formats |
|----------|---------|
| Uncompressed | R8, RGB8, RGBA8, BGR8, BGRA8, SRGB_BGRA8, RGB10A2 |
| Float | RG16F, RG32F, RGB16, RGBA16F, RGB32F, RGBA32F, R32F |
| Integer | R16UI, R32UI, RGBA16UI, RGBA32UI |
| Depth | Z16F, Z24S8, Z32F, Z32FS8 |
| Compressed | S3TC_DXT1/3/5, RGBA_BPTC_UNORM, RGBA_ASTC_4X4 |
| Video | NV12, YUV420P |

## MsaaSamples

`MSAA_1X`, `MSAA_4X`, `MSAA_8X`, `MSAA_9X`, `MSAA_16X`, `MSAA_25X`, `MSAA_36X`

## GfxEnv Singleton (gfxenv.h:722–839)

- `GetRef()` — static accessor
- `setContextClass()` / `contextClass()` — context factory
- `GetMainWindow()` / `SetMainWindow()`
- `GetSharedDynamicVB()` — shared vertex buffers
- `supportsBC7()` — feature detection

## VulkanContext (vulkan_ctx.h:664–903)

Primary backend implementation:
- `VkDevice`, `VkPhysicalDevice`, `VkQueue`, `VkCommandPool`
- `VkDescriptorPool` — descriptor management
- `MAX_FRAMES_IN_FLIGHT = 2` — triple-buffering
- `DEPTH_FORMAT = Z24S8`
- Output types: `VkSwapChain`, `VkOffscreen`, `VkSwapChainDRM`

## ContextDummy (gfxctxdummy.h)

No-op implementation for headless testing. Provides dummy versions of all sub-interfaces.

## Python API

```python
from orkengine import lev2

ctx = ezapp.context  # From EzApp

# Properties
w = ctx.mainSurfaceWidth()
h = ctx.mainSurfaceHeight()
rtg = ctx.defaultRTG()

# Sub-interfaces
gbi = ctx.GBI
txi = ctx.TXI
fxi = ctx.FXI
dwi = ctx.DWI
ci = ctx.CI
fbi = ctx.FBI

# Frame control
ctx.beginFrame()
ctx.endFrame()
ctx.makeCurrent()

# Debug
ctx.debugPushGroup("pass_name")
ctx.debugPopGroup()

# Callbacks
ctx.scheduleBeforeDoEndFrameOneShot(lambda: ...)
```

## How to Answer

1. For Context class: check `gfxenv.h:201–509`
2. For sub-interface headers: check `gbi.h`, `txi.h`, `fxi.h`, `dwi.h`, `ci.h`, `fbi.h`
3. For enums/formats: check `gfxenv_enum.h`
4. For Vulkan details: check `vulkan_ctx.h`
5. For Python: check `pyext_gfx.cpp`
6. For specific sub-interfaces: consult the dedicated skills (orklev2-gbi, orklev2-txi, etc.)
