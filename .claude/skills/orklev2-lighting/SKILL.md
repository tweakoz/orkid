---
name: orklev2-lighting
description: Answer questions about orkid's lighting system, light types (point/directional/spot/ambient), LightManager, shadow mapping, light cookies, light probes, forward lighting SSBO layout, EnumeratedLights, and Python lighting bindings. Use when the user asks about lights, shadows, probes, or light management.
user-invocable: false
---

# Orkid Lighting System Reference

When answering questions about lighting, shadows, or light probes in orkid, consult the files below. All under `ork.lev2/`.

## Key Files

| Component | File |
|-----------|------|
| All Light Classes | `inc/ork/lev2/gfx/lighting/gfx_lighting.h` |
| Light Implementations | `src/gfx/lighting/gfx_lighting.cpp` |
| Light Probes | `src/gfx/lighting/lightprobe.cpp` |
| AO/Lightmap Utils | `src/gfx/lighting/model_lighting_utils.cpp` |
| Scene Graph Nodes | `inc/ork/lev2/gfx/scenegraph/scenegraph.h` |
| GPU Shader Interface | `../ork.data/platform_lev2/shaders/fxv2/stdtools.i2` |
| Python Bindings | `pyext/src/pyext_gfx_lighting.cpp` |
| Type Definitions | `inc/ork/lev2/lev2_types.h` |

## Light Type Hierarchy

```
DrawableData (ork::Object)
└── LightData (abstract, gfx_lighting.h:68)
    ├── PointLightData (line 234)
    ├── DirectionalLightData (line 295)
    ├── SpotLightData (line 404)
    └── AmbientLightData (line 343)

Drawable
└── Light (abstract, gfx_lighting.h:120)
    ├── PointLight (line 261) / DynamicPointLight (line 284)
    ├── DirectionalLight (line 308) / DynamicDirectionalLight (line 333)
    ├── SpotLight (line 428) / DynamicSpotLight (line 462)
    └── AmbientLight (line 373)
```

## LightData Base (gfx_lighting.h:68–114)

| Member | Type | Default | Description |
|--------|------|---------|-------------|
| `mColor` | fvec3 | — | RGB color |
| `_intensity` | float | 1.0 | Brightness multiplier |
| `mbShadowCaster` | bool | — | Casts shadows? |
| `_shadowsamples` | int | — | PCF shadow samples |
| `mShadowBlur` | float | — | Shadow softness [0,1] |
| `mShadowBias` | float | 0.002 | Shadow depth bias |
| `_cookie` | asset_ptr_t | — | Cookie texture asset |
| `_decal` | bool | false | Render as decal |
| `_shadowMapSize` | int | 1024 | Shadow map resolution |

## Light Types

### PointLight
- `_radius` — influence radius
- `_falloff` — attenuation falloff exponent
- Sphere-based collision tests
- `DynamicPointLight` — runtime-created with inline data

### DirectionalLight
- Unbounded — always in frustum, affects everything
- Shadow support via directional shadow maps
- `DynamicDirectionalLight` — runtime-created

### SpotLight
- `mFovy` — field of view (degrees)
- `mRange` — maximum distance
- `_cookiePath` — cookie texture path
- Frustum-based collision tests
- Shadow map: lazy-created `_shadowRTG` render target
- `shadowMatrix()` = projection * view
- `shadowCamDat()` → CameraData for shadow rendering
- `DynamicSpotLight` — runtime-created

### AmbientLight
- `mfAmbientShade` — base ambient level [0,1]
- `mvHeadlightDir` — directional component (default: (0, 0.5, 1))
- Always in frustum

## Light Manager (gfx_lighting.h:595–624)

Central manager for all lights in a scene:

- `mGlobalStationaryLights` — non-moving lights (GlobalLightContainer, max 256)
- `mGlobalMovingLights` — dynamic lights (LightContainer)
- `_lightprobes` — active light probes
- `_cookies_spot_color` / `_cookies_spot_depth` — texture arrays for cookies

Key methods:
- `enumerateInPass(CPD, out)` — categorize lights for rendering pass
- `bindEnumeratedToStorageBuffer(ctx, lights, ssbo)` — pack lights into GPU SSBO
- `gpuInit(ctx)` — initialize texture arrays

## EnumeratedLights (gfx_lighting.h:576–591)

Result of `enumerateInPass()`, categorized for GPU binding:

- `_untexturedpointlights` — point lights without cookies
- `_tex2pointlightmap` — point lights grouped by cookie texture
- `_untexturedspotlights` — spot lights without cookies
- `_tex2spotlightmap` — textured spot lights by cookie
- `_tex2shadowedspotlightmap` — shadowed spot lights by cookie
- `_tex2spotdecalmap` — decal spot lights by cookie
- `_lightprobes` — active probes

## GPU SSBO Layout (max 64 lights)

Packed by `bindEnumeratedToStorageBuffer()`:

| Offset | Data | Size |
|--------|------|------|
| 0 | `vec4 _lightcolor[64]` | 1024B — RGB + intensity |
| 1024 | `vec4 _lightsizbias[64]` | 1024B — radius, bias, shadowmap size |
| 2048 | `vec4 _lightpos[64]` | 1024B — XYZ position |
| 3072 | `mat4 _shadowmatrix[64]` | 4096B — shadow projection * view |
| 7168 | `uint _lightTexSlice[64]` | 256B — cookie texture array slice |

Shader interface (stdtools.i2:124–132):
```glsl
storage_interface storage_fwd_lighting (descriptor_set 0) {
  buffer layout(std430) lights {
    vec4 _lightcolor[64];
    vec4 _lightsizbias[64];
    vec4 _lightpos[64];
    mat4 _shadowmatrix[64];
    uint _lightTexSlice[64];
  };
}
```

Light counts passed via uniform block (stdtools.i2:115–121):
```glsl
uniform_block ublk_std_lighting {
  vec3 LightMapColors[8];
  int point_light_count;
  int spot_light_count;
}
```

## Light Probes (gfx_lighting.h:185–230)

| Member | Description |
|--------|-------------|
| `_type` | `REFLECTION` or `SH_Radiance` |
| `_activationMode` | `ALWAYS` or `BAKE_ONLY` |
| `_worldMatrix` | Probe position/rotation |
| `_cubeTexture` | Final cube map texture |
| `_dim` | Cube map resolution |
| `_supersample` | Supersampling factor |
| `_temporalFrames` | TAA frame count |
| `_renderLayer` | Layer name (default: "probe") |
| `_dynamic` | If true, probe is perpetually dirty — re-rendered every frame |
| `_dirty` | One-shot dirty flag; cleared after the probe pass renders |

Features:
- Double-buffered per-face accumulation for TAA
- `exportEquirectangular(ctx, rotation, path)` — export to equirect image
- `resize(dim)` — set cube map resolution

### Static vs Dynamic Probes (PBR2 Phase 0)

`fwdnode_impl_sub.cpp` gates the cubemap render on `(probe->_dirty || probe->_dynamic)`:
- Static (default): rendered once when staged, stays clean. Use for fixed-scene captures.
- Dynamic: re-rendered every frame. Use when reflections must respond to live scene changes (sky swap, moving geometry). Authors flip via `self.probe(..., dynamic=True)` in the ECS Scene DSL.

### Probe Registry on LightManager (PBR2 Phase 0)

A name-keyed registry on `LightManager` lets non-lighting subsystems resolve probes without ECS coupling:

```cpp
lightprobe_ptr_t LightManager::findProbeByName(const std::string& name) const;
```

Linear scan over `_lightprobes` (typically O(few)). Probes register themselves via `SG::Layer::createProbeNode` — no separate registration path. Consumers (e.g. ECS `ParticlesGlobalSystem`) look up by entity name at `_onActivateComponent` time (after all probes have staged) and cache the `lightprobe_ptr_t` on the drawable. No per-frame lookups, no stage-order coupling.

## Light Cookies

- **Color cookies**: `texturearray_ptr_t`, RGBA8, 256x256 default
- **Depth cookies**: `texturearray_ptr_t`, Z32F, 256x256 default
- **Radiance cookies**: `pbr::radiancemaps_ptr_t` per light
- Each light holds `texturearraysliceref_ptr_t` references into the arrays

## Scene Graph Integration

```python
# Python: create and add lights
from orkengine import lev2

ptl_data = lev2.PointLightData()
ptl_data.color = fvec3(1, 0.8, 0.6)
ptl_data.intensity = 5.0

light_node = ptl_data.createNode("mylight", layer)

# SpotLight with cookie
spl_data = lev2.SpotLightData()
spl_data.fovy = 45.0
spl_data.range = 100.0
spl_data.cookiePath = "data://textures/cookie.png"

# Dynamic lights
dpl = lev2.DynamicPointLight()
dpl.data.color = fvec3(1, 1, 1)

# Light probes
probe = lev2.LightProbe()
probe.imageDim = 256
probe.worldMatrix = fmtx4()
probe.active = True
probe.exportEquirectangular(ctx, fquat(), "output.png")
```

## Utility Functions (Python)

- `computeAmbientOcclusion(samples, mesh, ctx)` — AO baking
- `computeLightMaps(mesh, ctx)` — lightmap generation

## How to Answer

1. For light types/params: check `gfx_lighting.h` class definitions
2. For GPU binding: check `bindEnumeratedToStorageBuffer()` in `gfx_lighting.cpp:760–852`
3. For shader interface: check `stdtools.i2:115–132`
4. For shadow maps: check SpotLight shadow methods in `gfx_lighting.h:428–460`
5. For probes: check `LightProbe` in `gfx_lighting.h:185–230` and `lightprobe.cpp`
6. For Python: check `pyext_gfx_lighting.cpp`
