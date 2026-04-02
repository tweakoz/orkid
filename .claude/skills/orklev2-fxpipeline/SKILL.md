---
name: orklev2-fxpipeline
description: Answer questions about orkid's FxPipeline system, FxPipelinePermutation, FxPipelineCache, shader technique selection, parameter binding, state lambdas, named parameter providers, RenderContextInstData/FrameData, and Python pipeline bindings. Use when the user asks about render pipelines, technique selection, shader parameter binding, or render context.
user-invocable: false
---

# Orkid FxPipeline System Reference

When answering questions about FxPipeline, render context, or shader parameter binding in orkid, consult the files below. All under `ork.lev2/`.

## Key Files

| Component | File |
|-----------|------|
| FxPipeline / Cache / Permutation | `inc/ork/lev2/gfx/fx_pipeline.h` |
| FxPipeline Implementation | `src/gfx/fx_pipeline.cpp` |
| Shader System | `inc/ork/lev2/gfx/shadman.h` |
| Render Context (RCID/RCFD) | `inc/ork/lev2/gfx/renderer/rendercontext.h` |
| GfxMaterial Base | `inc/ork/lev2/gfx/gfxmaterial.h` |
| PBR Pipeline Creation | `src/gfx/material/material_pbr_pipeline.cpp` |
| Python Material Bindings | `pyext/src/pyext_gfx_material.cpp` |
| Python Shader Bindings | `pyext/src/pyext_gfx_shader.cpp` |
| Type Definitions | `inc/ork/lev2/lev2_types.h` |

## Architecture Overview

```
GfxMaterial
  └─ pipelineCache() → FxPipelineCache
       └─ findPipeline(permutation) → FxPipeline
            ├─ _technique → FxShaderTechnique
            ├─ _params → { FxShaderParam* : varval_t }
            ├─ _uniformbuffers → { FxUniformBlock* : varval_t }
            ├─ _storages → { FxShaderStorageBlock* : varval_t }
            └─ _statelambdas → [ (RCID) → void ]
```

## Core Data Structures

### FxPipelinePermutation (fx_pipeline.h:37–52)

Encodes rendering configuration as a 64-bit hash:

| Flag | Bit | Description |
|------|-----|-------------|
| `_stereo` | 1 | Single-pass stereo |
| `_instanced` | 2 | Instance rendering |
| `_skinned` | 3 | Skeletal animation |
| `_is_picking` | 4 | Picking/selection mode |
| `_has_vtxcolors` | 5 | Per-vertex colors |
| `_rendering_model` | 16+ | CRC hash (e.g., `"FORWARD_PBR"_crcu`) |

- `_forced_technique` — override technique selection
- `_vr_mono` — VR mono mode
- `genIndex()` — generates 64-bit hash combining all flags

### FxPipeline (fx_pipeline.h:84–127)

Active rendering state for a specific permutation:

- `_technique` — active shader technique
- `_params` — `{ fxparam_constptr_t : varval_t }` parameter bindings
- `_uniformbuffers` — `{ fxuniformblock_constptr_t : varval_t }` UBO bindings
- `_storages` — `{ fxparamstorageblock_constptr_t : varval_t }` SSBO bindings
- `_statelambdas` — callbacks executed during `beginBlock()`
- `_rasterstate` — blend, depth, cull state
- `_material_ptr` — parent material
- `_vars` — VarMap for user data

Key methods:
- `beginBlock(RCID)` — activate pipeline, bind params, execute state lambdas, return pass count
- `endBlock(RCID)` — deactivate
- `wrappedDrawCall(RCID, drawcall)` — begin → draw → end
- `bindParam(param, value)` — bind shader parameter
- `bindUniformBuffer(block, value)` — bind UBO
- `bindStorage(block, value)` — bind SSBO
- `addStateLambda(fn)` — register per-frame binding callback

### FxPipelineCache (fx_pipeline.h:129–137)

Caches pipelines by permutation hash:

- `findPipeline(RCID)` — derives permutation from render context, looks up or creates
- `findPipeline(permutation)` — direct lookup by permutation hash
- `_on_miss` — factory callback for cache misses
- `_lut` — `{ uint64_t : fxpipeline_ptr_t }` hash table

### FxPipelineCacheImpl<MtlClass> (fx_pipeline.h:139–166)

Template for per-material-class caches. Sets `_on_miss` to call `MtlClass::_createFxPipeline(permu, material)`.

## Parameter Binding Types

`varval_t` (= `varmap::VarMap::value_type`) supports:

| Type | Examples |
|------|---------|
| Scalar | `float`, `int`, `bool`, `uint32_t` |
| Vector | `fvec2`, `fvec3`, `fvec4` |
| Matrix | `fmtx3`, `fmtx4` |
| Texture | `texture_ptr_t`, `TextureArray*` |
| Buffer | `FxShaderStorageBuffer*`, `storagebufferptr_t` |
| CrcString | Named parameter provider lookup |
| Generator | `varval_generator_t` — deferred evaluation lambda |

## Named Parameter Providers (fx_pipeline.cpp:190–400)

**Key design pattern:** When a `CrcString` is bound as a parameter value (from C++ or Python), the runtime resolves it at render time via a singleton map of name → C++ provider function. This is a **declarative binding** — the wiring is set up once, then the C++ runtime evaluates the provider every frame with zero scripting overhead. Engine-internal data (camera matrices, PBR environment maps, time, etc.) flows to shader parameters without the caller needing to know how that data is computed. This pattern is used throughout the engine in both C++ material setup (e.g., PBRMaterial state lambdas) and Python scene composition.

Registered providers:

| Provider Name | Returns |
|---------------|---------|
| `"RCID_PickID"` | Pick variant encoding |
| `"RCFD_M"` | Model matrix |
| `"RCFD_Camera_MVP_Mono"` | Model-View-Projection |
| `"RCFD_Camera_MV_Mono"` | Model-View |
| `"RCFD_Camera_V_Mono"` | View matrix |
| `"RCFD_Camera_P_Mono"` | Projection matrix |
| `"RCFD_TIME"` | Current time |
| `"CPD_Rtg_Dim"` | Render target dimensions |
| `"RCFD_MODCOLOR"` | Modifier color |
| `"RCFD_MONOCAM_NEAR_FAR"` | Near/far planes |
| `"RCFD_EYE_POSITION"` | Camera eye position |
| `"RCFD_PBR_BRDF_INTEGRATION_GGX"` | BRDF LUT texture |
| `"RCFD_PBR_DIFFUSE_ENV"` | Diffuse environment map |
| `"RCFD_PBR_SPECULAR_ENV"` | Specular environment array |

Stereo variants append `"_Left"` / `"_Right"` suffixes.

## Render Context Data Structures

### RenderContextInstData (RCID) — per-drawable (rendercontext.h:34–85)

- `_isSkinned`, `_isInstanced` — permutation flags
- `_forced_technique` — technique override
- `_pipeline_cache` — material's pipeline cache
- `_pickID` — per-drawable pick ID
- `_envmapOverride` — per-drawable environment map
- `rcfd()` — access frame data
- `context()` — graphics context

### RenderContextFrameData (RCFD) — per-frame (rendercontext.h:92–164)

- `_target` — graphics context
- `_renderingmodel` — e.g., `"FORWARD_PBR"_crcu`
- `_pbrcommon` — PBR environment data
- `_passID`, `_subpassID` — current pass identifiers
- `topCPD()` — current compositing pass data
- `isStereo()` — single-pass stereo check
- `GetDB()` — drawable queue

## Pipeline Execution Flow

### beginBlock() (fx_pipeline.cpp:89–156)
1. Get FxInterface from context
2. `FXI->BeginBlock(technique, RCID)` → pass count
3. Execute all state lambdas with RCID
4. Bind all parameters via type dispatch
5. Bind all storage buffers
6. Apply rasterstate (via priority resolution — see below)
7. Return pass count

### Cache Lookup (fx_pipeline.cpp:703–740)
1. Extract permutation flags from RCID/RCFD
2. `genIndex()` → 64-bit hash
3. Check `_lut[hash]`
4. On miss: call `_on_miss(permutation)` → create pipeline
5. Cache and return

### Raster State Priority Override

Raster state is resolved via a `priority_stack<rasterstate_ptr_t>` (`ork.core/inc/ork/kernel/priority_stack.inl`). When a rasterstate is pushed onto FXI, it competes with the shader's stateblock rasterstate based on the `_priority` field — **higher priority wins**.

**Resolution logic** (vulkan_fxi_pipelines.cpp:71–81):
```cpp
rasterstate_ptr_t effective = _rasterstate_stack.resolve();  // highest-priority pushed state
int iraspri = effective ? effective->_priority : 0;
if (_currentVKPASS && _currentVKPASS->_stateblock_rasterstate) {
  auto try_rs = _currentVKPASS->_stateblock_rasterstate;
  if (try_rs->_priority >= iraspri) {
    effective = _currentVKPASS->_stateblock_rasterstate;  // shader stateblock wins
  }
}
```

**Usage pattern** — push a higher-priority rasterstate to override the shader's stateblock:
```cpp
// Override shader's blend/depth/cull for this draw call
auto RSTATE = std::make_shared<RasterState>();
RSTATE->setDepthTest(EDepthTest::OFF);
RSTATE->setCullTest(ECullTest::OFF);
RSTATE->setBlendingMacro(BlendingMacro::ALPHA);
RSTATE->_priority = 1 << 10;  // higher than shader stateblock (default 0)

context->FXI()->pushRasterState(RSTATE);
// ... draw calls use this rasterstate instead of shader's ...
context->FXI()->popRasterState();
```

**Examples in codebase:**
- `fontman_render.cpp:111` — font rendering overrides depth/cull for 2D text
- `gfxmodel_render_skeleton.cpp:396` — skeleton debug vis with `_priority = 1<<10`
- `sgnode_billboard.cpp:224` — billboard rendering with custom blend
- `sgnode_manipgizmo.cpp:239+` — two-pass gizmo rendering (backface then frontface)
- `sgnode_curvepath.cpp:149` — curve path line rendering
- UI widgets (outliner, toolbar, textbox, etc.) — override for 2D UI rendering

## Python API

```python
from orkengine import lev2

# Permutation
permu = lev2.FxPipelinePermutation(rendermodel="FORWARD_PBR")
permu.stereo = False
permu.instanced = True
permu.skinned = False
permu.is_picking = False

# Cache lookup
cache = material.fxcache
pipeline = cache.findPipeline(permu)

# Pipeline usage
pipeline.technique = tek
pipeline.bindParam(param, 0.5)          # float — stored, applied in C++ at render time
pipeline.bindParam(param, fvec4(...))   # vector — same, no Python on render path
pipeline.bindParam(param, texture)      # texture — same
pipeline.bindParam(param, crcstring)    # named provider — C++ resolves engine data each frame
pipeline.bindParam(param, lambda: 0.5)  # Python lambda — only case where Python runs per-frame
pipeline.bindStorage(storage, ssbo)     # SSBO
pipeline.wrappedDrawCall(rcid, lambda: draw())

# NOTE: All bindings except Python lambdas execute entirely in C++ at render time.
# Python sets up the wiring once; the C++ runtime evaluates it every frame.
# This means Python scene composition has zero per-frame scripting overhead
# for parameter binding — only Python lambdas invoke the interpreter.
```

## How to Answer

1. For permutation flags: check `fx_pipeline.h:37–52` and `genIndex()` in `fx_pipeline.cpp:20–32`
2. For parameter binding: check `_set_typed_param()` in `fx_pipeline.cpp`
3. For named providers: check the singleton map in `fx_pipeline.cpp:190–400`
4. For PBR-specific pipeline creation: check `material_pbr_pipeline.cpp`
5. For render context: check `rendercontext.h`
6. For Python: check `pyext_gfx_material.cpp:233–410`
