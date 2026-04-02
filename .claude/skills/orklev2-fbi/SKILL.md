---
name: orklev2-fbi
description: Answer questions about orkid's FrameBufferInterface (FBI), render target management (RtGroup push/pop), viewport/scissor stacks, framebuffer clear, MSAA resolve, pixel capture/readback, pick buffers, and Python FBI bindings. Use when the user asks about render targets, viewports, framebuffer operations, or pixel readback.
user-invocable: false
---

# Orkid FrameBufferInterface (FBI) Reference

When answering questions about render targets, viewports, or framebuffer operations in orkid, consult the files below. All under `ork.lev2/`.

## Key Files

| Component | File |
|-----------|------|
| FBI Interface | `inc/ork/lev2/gfx/fbi.h` (lines 18–154) |
| RtGroup / RtBuffer | `inc/ork/lev2/gfx/rtgroup.h` |
| Capture Types | `inc/ork/lev2/gfx/targetinterfaces.h` |
| Pick Buffer | `inc/ork/lev2/gfx/pickbuffer.h` |
| Vulkan FBI | `src/gfx/vulkan/headers/vulkan_ctx.h` (lines 351–411) |
| Python Bindings | `pyext/src/pyext_gfx.cpp` |

## FBI Interface (fbi.h:18–154)

### Clear & Display
- `SetClearColor(fcolor4)` / `GetClearColor()`
- `SetAutoClear(bool)` / `GetAutoClear()`
- `SetVSyncEnable(bool)`
- `GetBufferTexture()` / `SetBufferTexture(tex)`

### Render Target Group (RtGroup) Management
- `PushRtGroup(RtGroup*)` — push RTG onto stack, begin rendering to it
- `PopRtGroup()` — pop and restore previous RTG
- `bindRtGroup(RtGroup*)` — bind without stack management
- `rtGroupClear(RtGroup*)` — clear all MRT buffers
- `rtGroupMipGen(RtGroup*)` — generate mips for RTG textures
- `validateRtGroup(RtGroup*)` — ensure GPU resources allocated
- `rtGroupTransitionToTexture(RtGroup*)` — transition to `SHADER_READ_ONLY` layout

### Viewport Stack (max 16 levels)
- `pushViewport(x, y, w, h)` / `pushViewport(ViewportRect)`
- `setViewport(x, y, w, h)` / `setViewport(ViewportRect)`
- `popViewport()`
- `viewport()` → `const ViewportRect&`
- `GetVPX()`, `GetVPY()`, `GetVPW()`, `GetVPH()`
- `currentAspectRatio()` → float

### Scissor Stack (max 16 levels)
- `pushScissor(x, y, w, h)` / `pushScissor(ViewportRect)`
- `pushScissorIntersected(x, y, w, h)` — intersect with current
- `setScissor(x, y, w, h)` / `setScissor(ViewportRect)`
- `popScissor()`
- `scissor()` → `const ViewportRect&`

### Framebuffer Blit/Resolve
- `msaaBlit(src, dst)` — MSAA resolve
- `blit(src, dst)` — copy between RTGs
- `cloneDepthBuffer(src, dst)` — copy depth
- `downsample2x2(src, dst)` — 2x downsample

### Pixel Capture/Readback (Async)
- `capture(rtbuf, path, on_complete)` — capture to file
- `captureAsFormat(rtbuf, buffer, fmt, on_complete)` — capture with format conversion
- `captureToTexture(rtbuf, tex, on_complete)` — GPU→GPU copy
- `capturePixelAsync(pfc, x, y, on_complete)` — single pixel readback

### Pixel Fetch (Sync)
- `GetPixel(pos, PixelFetchContext)` — synchronous pixel read

### Pick State
- `EnterPickState(PickBuffer*)` / `LeavePickState()`
- `isPickState()` → bool
- `currentPickBuffer()` → `PickBuffer*`

### Frame Lifecycle
- `BeginFrame()` / `EndFrame()`

### Key Members
- `_active_rtgroup` — top of RtGroup stack (null before first push)
- `_main_rtg` — lazily created swapchain or offscreen RTG
- `_clearColor` — current clear color
- `maViewportStack[16]` / `maScissorStack[16]` — viewport/scissor stacks

## RtGroup (rtgroup.h:60–110) — Multiple Render Targets

| Member | Description |
|--------|-------------|
| `mMrt[8]` | Up to 8 render target buffers |
| `_depthBuffer` | Depth buffer |
| `miW, miH` | Dimensions |
| `_msaa_samples` | MSAA sample count |
| `_autoclear` | Auto-clear on bind |
| `_cubeMap` | Cube map rendering mode |

Key methods:
- `createRenderTarget(format, usage, with_texture)` → `rtbuffer_ptr_t`
- `createDepthBuffer(format, with_texture)` → `rtbuffer_ptr_t`
- `buffer(idx)` → `rtbuffer_ptr_t`
- `texture(idx)` → `texture_ptr_t`
- `depthTexture()` → `texture_ptr_t`
- `Resize(w, h)`

## RtBuffer (rtgroup.h:27–58) — Single Render Target

- `_texture` — GPU texture (if `with_texture`)
- `mFormat` — `EBufferFormat`
- `_clearColor` — per-buffer clear color
- `_clearDepth` — depth clear value (default 1.0)
- `_mipgen` — `NONE`, `AUTOCOMPUTE`, or `USER`

## RtgSet (rtgroup.h:111+) — Indexed Collection

- `fetch(key)` → get or create RtGroup by CRC hash
- `addBuffer(name, format)` — define buffer template
- Used by render nodes for different output configurations

## Typical Usage

```cpp
auto* FBI = ctx->FBI();

// Push render target
FBI->PushRtGroup(myRTG);
FBI->pushViewport(0, 0, width, height);
FBI->pushScissor(0, 0, width, height);

// Render into RTG...

FBI->popScissor();
FBI->popViewport();
FBI->PopRtGroup();

// Use RTG texture as input
auto* tex = myRTG->texture(0);
FXI->bindParamTexture(param, tex);

// Async capture
FBI->capture(myRTG->buffer(0), "screenshot.png", []{
    printf("Capture complete!\n");
});
```

## Python API

```python
fbi = ctx.FBI

# Render targets
fbi.pushRtGroup(rtg)
fbi.popRtGroup()

# Viewport
fbi.pushViewport(x, y, w, h)
fbi.popViewport()

# Scissor
fbi.pushScissor(x, y, w, h)
fbi.popScissor()

# Clear
fbi.setClearColor(fvec4(0, 0, 0, 1))

# Capture
fbi.capture(rtbuf, "output.png")

# MSAA resolve
fbi.msaaBlit(src_rtg, dst_rtg)

# Access
aspect = fbi.aspectRatio
```

## How to Answer

1. For FBI methods: check `fbi.h:18–154`
2. For RtGroup/RtBuffer: check `rtgroup.h`
3. For capture types: check `targetinterfaces.h` (CaptureBuffer, CaptureAsync)
4. For pick buffers: check `pickbuffer.h`
5. For Vulkan details: check `vulkan_ctx.h:351–411`
6. For Python: check `pyext_gfx.cpp`
