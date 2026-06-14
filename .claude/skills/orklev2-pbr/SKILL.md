---
name: orklev2-pbr
description: Answer questions about orkid's PBR material system, GfxMaterial hierarchy, PBRMaterial texture channels (albedo/normal/metallic-roughness/emissive), FreestyleMaterial, IBL/environment maps, BRDF integration, lightmaps, XGM serialization, FxPipeline, shader technique selection, and Python material bindings. Use when the user asks about PBR, materials, IBL, environment maps, or material rendering.
user-invocable: false
---

# Orkid PBR Material System Reference

When answering questions about PBR materials, shading, or IBL in orkid, consult the files below. All under `ork.lev2/`.

## Key Files

| Component | Header/File |
|-----------|-------------|
| GfxMaterial Base | `inc/ork/lev2/gfx/gfxmaterial.h` |
| PBRMaterial | `inc/ork/lev2/gfx/material_pbr.inl` |
| FreestyleMaterial | `inc/ork/lev2/gfx/material_freestyle.h` |
| IBL/Radiance | `inc/ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h` |
| Drawable Integration | `inc/ork/lev2/gfx/renderer/drawable.h` |
| Model/XGM | `inc/ork/lev2/gfx/gfxmodel.h` |
| PBR Main Impl | `src/gfx/material/material_pbr.cpp` |
| PBR I/O (XGM) | `src/gfx/material/material_pbr_io.cpp` |
| PBR Images/Textures | `src/gfx/material/material_pbr_image.cpp` |
| PBR Lightmaps | `src/gfx/material/material_pbr_lightmaps.cpp` |
| BRDF Generation | `src/gfx/material/material_pbr_gen.cpp` |
| Pipeline/Techniques | `src/gfx/material/material_pbr_pipeline.cpp` |
| Depth Prepass | `src/gfx/material/material_pbr_depthprepass.cpp` |
| Picking | `src/gfx/material/material_pbr_pick.cpp` |
| Unlit | `src/gfx/material/material_pbr_unlit.cpp` |
| Misc Techniques | `src/gfx/material/material_pbr_misc.cpp` |
| Python Bindings | `pyext/src/pyext_gfx_material.cpp` |

## Class Hierarchy

```
ork::Object
└── GfxMaterial (abstract, gfxmaterial.h:113)
    ├── PBRMaterial (material_pbr.inl:36)
    └── FreestyleMaterial (material_freestyle.h:20)
```

### GfxMaterial Base (gfxmaterial.h)
- `mMaterialName`, `miNumPasses` — identity and pass count
- `_rasterstate` — blend, depth, cull state
- `_varmap` — dynamic parameter storage (VarMap)
- `_variant` — CRC32 variant identifier for technique override
- `_state_lambdas` — per-frame parameter binding callbacks
- `_bound_params` — fxparam → value bindings

### PBRMaterial
Full physically-based rendering material with metallic-roughness workflow.

### FreestyleMaterial
Generic shader wrapper — wraps an `FxShader*` with techniques/parameters. Used internally by PBRMaterial (`_as_freestyle` member).

## PBR Texture Channels

| Channel | Image Member | Texture Member | Description |
|---------|-------------|----------------|-------------|
| Color/Albedo | `_image_color` | `_texColor` | Base color (RGBA) |
| Normal | `_image_normal` | `_texNormal` | Tangent-space normal map |
| Metallic+Roughness | `_image_mtlruf` | `_texMtlRuf` | Packed metallic/roughness |
| Emissive | `_image_emissive` | `_texEmissive` | Emissive glow map |
| Ambient Occlusion | `_image_ambocc` | `_texAmbOcc` | AO map |
| Light Map | — | `_texLightMap` | Pre-baked lighting |

All textures are packed into `_texArrayCNMREA` (TextureArray) via `assignImages()` — images are conformed to the same dimensions and converted to RGBA8.

## PBR Scalar Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `_metallicFactor` | 0.0 | Metallic weight |
| `_roughnessFactor` | 1.0 | Roughness weight |
| `_baseColor` | (1,1,1,1) | Base color tint |
| `_alphaCutoff` | 0.0 | Alpha test threshold |
| `_alphaMode` | 0 | 0=OPAQUE, 1=MASK, 2=BLEND |
| `_doubleSided` | false | Two-sided rendering |
| `_shaderpath` | `"orkshader://pbr"` | Shader asset path |

## IBL / Environment Maps

### RadianceMaps (pbr_common.h:51–64)
```cpp
struct RadianceMaps {
  texturearray_ptr_t _filtenvSpecularMapArray;  // Roughness-stratified specular
  texture_ptr_t _filtenvDiffuseMap;             // Diffuse irradiance
  texture_ptr_t _brdfIntegrationMapGGX;         // GGX BRDF LUT
  texture_ptr_t _brdfIntegrationMapVelvet;      // Velvet BRDF LUT
  texture_ptr_t _brdfIntegrationMapGGXRIM;      // GGX+rim BRDF LUT
  texture_ptr_t _brdfIntegrationMapBlinn;       // Blinn BRDF LUT
  texture_ptr_t _brdfIntegrationMapPhong;       // Phong BRDF LUT
  std::vector<float> _specularRoughnessValues;
  int _numRoughnessLevels;
};
```

### CommonStuff (pbr_common.h:68–159)
Per-scene IBL state including:
- `_radiance_maps`, `_environmentIntensity` (default 1.0)
- `_environmentMipBias`, `_environmentMipScale`
- `_diffuseLevel`, `_specularLevel`, `_ambientLevel`, `_skyboxLevel`
- `_roughnessPower`
- SSAO parameters: `_ssaoRadius`, `_ssaoBias`, `_ssaoWeight`, `_ssaoPower`, `_ssaoNumSamples`, `_ssaoNumSteps`

### BRDF Integration Maps
- Generated via compute shader in `material_pbr_gen.cpp`
- Supports GGX, Blinn, Phong, Velvet, GGX-Rim models
- Cached in DataBlockCache with CRC hash
- Dimensions: 512x512

### Per-Drawable Overrides (Two Distinct IBL Channels — PBR2 Phase 0)

`Drawable` (and the renderable / RCID it propagates to) carries two independent IBL override fields:

```cpp
// drawable.h
pbr::radiancemaps_ptr_t _envmapOverride;  // baked equirect — swaps MapSpecularEnv per draw
lightprobe_ptr_t        _probeOverride;   // live cube — swaps reflectionPROBE per draw
bool                    _excludeFromProbe = false;
```

Both ride the propagation chain `Drawable → IRenderable → RCID`. `fwdnode_pipeline.cpp` picks the per-draw probe binding in this order:

1. `RCID._probeOverride` if set (per-drawable live cube)
2. `enumlights->_lightprobes[0]` if `should_bind_probes` (scene-global probe)
3. (otherwise) per-drawable `_envmapOverride` falls back to the equirect path

The shader's `has_reflection_probe` flag is set per-draw based on whether a probe was bound. When the cube channel is active it **wins** over the equirect — the per-drawable equirect override does not reach the shader if any probe is bound globally for the draw.

`_excludeFromProbe` is read by `DrawQueue::enqueueLayerToRenderQueue`: if the active `RCFD` has user-property `"renderingPROBE"_crcu == true`, drawables flagged true are skipped during a probe cubemap capture. Particle drawables default to true (would feedback-loop / add noise to their own reflection).

## Lightmap Support

- Up to `kMaxLightmaps = 8` lightmaps per material
- Stored as `_texLightMapArray` (TextureArray), conformed to same size, RGB16 format
- `setActiveLightMap(name, color)` — activate and tint a named lightmap
- `conformLightmaps()` → `assignLightmaps(ctx)` pipeline

## Shader Technique Naming Convention

Techniques encode their rendering configuration:
```
{Model}_{Content}_{Skeleton}_{Instancing}_{Stereo}
```

| Segment | Values |
|---------|--------|
| Model | `FWD` (forward), `GBU` (deferred/GBuffer), `PIK` (picking), `DPP` (depth prepass) |
| Content | `UNLIT`, `SKYBOX`, `CM` (ModColor), `CT` (TexColor), `CV` (VertexColor), `CF` (CharFont), `DB` (DebugViz), `EMI` (Emissive) |
| Normal | `NM` (normal map), `NV` (vertex normal) |
| Skeleton | `RI` (rigid), `SK` (skinned) |
| Instancing | `IN` (instanced), `NI` (non-instanced) |
| Stereo | `MO` (mono), `ST` (stereo) |

Example: `_tek_FWD_CT_NM_RI_IN_MO` = Forward, TexColor, NormalMapped, Rigid, Instanced, Mono

## Shader Parameter Bindings

### Matrices
`_paramM`, `_paramV`, `_paramP`, `_paramVP`, `_paramMVP`, `_paramIP`, `_paramIV`, `_paramIVP`
Stereo: `_paramVL/VR`, `_paramVPL/VPR`, `_paramMVPL/MVPR`

### Material
`_parMetallicFactor`, `_parRoughnessFactor`, `_parRoughnessPower`, `_parAlphaCutoff`, `_parModColor`, `_parPickID`

### Environment
`_parMapSpecularEnv` (TextureArray), `_parMapDiffuseEnv`, `_parMapBrdfIntegration`
`_parEnvironmentMipBias`, `_parEnvironmentMipScale`, `_parSpecularMipBias`

### Lighting
`_paramAmbientLevel`, `_paramDiffuseLevel`, `_paramSpecularLevel`, `_paramSkyboxLevel`
`_parForwardLightBlock` (SSBO, 32KB), `_parInstanceBlock` (SSBO for instancing)

### SSAO
`_paramSSAOTexture`, `_paramSSAOWeight`, `_paramSSAOPower`, `_paramSSAOBias`, `_paramSSAORadius`, `_paramSSAONumSteps`, `_paramSSAONumSamples`

## XGM Serialization

PBR materials are read/written via XGM model files:

```cpp
// Registered in Describe():
c->annotate("xgm.reader", reader);  // material_ptr_t _xgmReader(XgmMaterialReaderContext&)
c->annotate("xgm.writer", writer);  // void _xgmWriter(XgmMaterialWriterContext&)
```

**Reader** (`material_pbr_io.cpp:9`): parses texture channel names, creates Image objects from embedded data, reads scalar params.
**Writer** (`material_pbr_io.cpp:114`): serializes texture names/paths, scalar params, lightmap metadata.

## Material Pipeline Flow

```
PBRMaterial._varmap
    → bindParam(fxparam, value)
    → _bound_params[fxparam] = value
    → _state_lambdas execute at render time
    → Context::FXI()->bindParam*() to GPU
```

Key methods:
- `gpuInit(Context*)` — load shader, find techniques/params
- `addBasicStateLambda(pipe)` — raster state setup
- `addLightingLambda(pipe)` — lighting parameter binding
- `_doFxPipelineCache(perms)` — technique selection based on permutations

## Python API

```python
from orkengine import lev2

# FreestyleMaterial (generic shader wrapper)
mtl = lev2.FreestyleMaterial(ctx, "orkshader://pbr")
tek = mtl.technique("tek_FWD_CT_NM_RI_NI_MO")
par = mtl.param("MetallicFactor")
mtl.bindParamFloat(par, 0.5)
mtl.begin(tek, rcfd)
# ... draw ...
mtl.end(rcfd)

# FxPipeline
pipeline = lev2.FxPipeline(fxcache)
pipeline.technique = tek
pipeline.bindParam(par, 0.5)
pipeline.wrappedDrawCall(rcid, lambda: draw())

# FxPipelinePermutation
permu = lev2.FxPipelinePermutation(
    rendermodel="forward",
    stereo=False,
    instanced=True,
    skinned=False
)

# Material access via model
model_drawable = lev2.ModelDrawableData("data://model.glb")
# Materials live on XgmSubMesh._material
```

## How to Answer

1. For texture channels/params: check `material_pbr.inl` member declarations
2. For IBL/environment: check `pbr_common.h` for RadianceMaps and CommonStuff
3. For technique selection: check `material_pbr_pipeline.cpp` and the naming convention
4. For serialization: check `material_pbr_io.cpp` for XGM reader/writer
5. For lightmaps: check `material_pbr_lightmaps.cpp`
6. For Python: check `pyext_gfx_material.cpp`
7. For shader params: check the param declarations in `material_pbr.inl` lines 124–202
