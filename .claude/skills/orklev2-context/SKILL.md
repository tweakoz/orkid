---
name: orklev2-context
description: Answer questions about orkid's abstract graphics API, rendering context, shader/material/pipeline system, render targets, textures, buffers, scene graph, compositor, and drawing. Use when the user asks about rendering, shaders, materials, framebuffers, textures, or the graphics abstraction layer.
user-invocable: false
---

# Orkid Lev2 Graphics Context & Rendering Reference

When answering questions about rendering, graphics API, or the material system in orkid, consult the files below. All under `ork.lev2/`.

## Key Files

| Component | Header |
|-----------|--------|
| Context (abstract) | `inc/ork/lev2/gfx/gfxenv.h` |
| Context Base | `inc/ork/lev2/gfx/ctxbase.h` |
| Shader/FX System | `inc/ork/lev2/gfx/shadman.h` |
| FX Pipeline | `inc/ork/lev2/gfx/fx_pipeline.h` |
| Material Base | `inc/ork/lev2/gfx/gfxmaterial.h` |
| Raster State | `inc/ork/lev2/gfx/rasterstate.h` |
| Render Targets | `inc/ork/lev2/gfx/rtgroup.h` |
| Framebuffer Interface | `inc/ork/lev2/gfx/fbi.h` |
| Texture | `inc/ork/lev2/gfx/texman.h` |
| Texture Interface | `inc/ork/lev2/gfx/txi.h` |
| Vertex/Index Buffers | `inc/ork/lev2/gfx/gfxvtxbuf.h` |
| Geometry Interface | `inc/ork/lev2/gfx/gbi.h` |
| Matrix Interface | `inc/ork/lev2/gfx/mtxi.h` |
| Compute Interface | `inc/ork/lev2/gfx/ci.h` |
| FX Interface | `inc/ork/lev2/gfx/fxi.h` |
| Renderer | `inc/ork/lev2/gfx/renderer/renderer.h` |
| Render Context | `inc/ork/lev2/gfx/renderer/rendercontext.h` |
| Drawable | `inc/ork/lev2/gfx/renderer/drawable.h` |
| Compositor | `inc/ork/lev2/gfx/renderer/compositor.h` |
| Scene Graph | `inc/ork/lev2/gfx/scenegraph/scenegraph.h` |
| Enums | `inc/ork/lev2/gfx/gfxenv_enum.h` |
| Type Aliases | `inc/ork/lev2/lev2_types.h` |

## Architecture Overview

### Context — Composite Interface Pattern
`Context` exposes domain-specific sub-interfaces:
- `FXI()` — shader/effect management (FxInterface)
- `MTXI()` — matrix stack (MatrixStackInterface)
- `GBI()` — vertex/index buffers (GeometryBufferInterface)
- `FBI()` — framebuffer/render targets (FrameBufferInterface)
- `TXI()` — textures (TextureInterface)
- `CI()` — compute dispatch (ComputeInterface)
- `PRI()` — primitives helper (PrimitivesInterface)

### Shader/Pipeline System
```
FxShader (program)
+-- FxShaderTechnique (variant)
|   +-- FxShaderPass (pass)
+-- FxShaderParam (parameter)
+-- FxUniformBlock (UBO)
+-- FxComputeShader
```

- `FxPipeline` — caches technique selection per permutation
- `FxPipelinePermutation` — rendering_model, stereo, instanced, skinned, picking flags

### Material System
- `GfxMaterial` (abstract) — `Update()`, `gpuInit()`, `BeginBlock()`/`EndBlock()`
- Texture binding: `SetTexture(ETextureDest, TextureContext)`
- Parameter binding through `FxPipeline`
- `RasterState` — depth test, cull, blending, write masks, polygon mode

### Render Targets
- `RtGroup` — container for multiple color buffers + depth (up to 8 MRT)
- `RtBuffer` — individual render target slot
- FBI stack: `PushRtGroup()`/`PopRtGroup()`, viewport/scissor management
- MSAA: 1X, 4X, 8X, 9X, 16X, 25X, 36X

### Textures
- Types: 1D, 2D, 3D, CUBE, arrays
- Formats: RGBA8, RGBA16F, RGBA32F, Z16F, Z32F, NV12, etc.
- Sources: FROM_DATA, FROM_RTG, FROM_IMAGE, FROM_MIPCHAIN, FROM_DDS

### Scene Graph
- `Scene` — multi-layer container
- `Layer` — collection of drawable/light/probe nodes
- Node types: `DrawableNode`, `CameraNode`, `LightNode`, `ProbeNode`

### Compositor
- `CompositingTechnique` — abstract rendering technique
- `CompositingPassData` — camera, layer, viewport per pass
- Multi-pass rendering with post-processing

### Backends
- Vulkan: `src/gfx/vulkan/`
- Software: `src/gfx/swrast/`

## How to Answer

1. For API questions: read the abstract interface headers (fbi.h, gbi.h, txi.h, fxi.h)
2. For material/shader: read `shadman.h` and `fx_pipeline.h`
3. For render target setup: read `rtgroup.h` and `fbi.h`
4. Check `gfxenv_enum.h` for all enumeration types
