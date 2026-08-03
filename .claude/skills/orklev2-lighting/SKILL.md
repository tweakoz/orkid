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
| Sun Cascade / Cookie Driver | `src/gfx/renderer/NodeCompositor/forward/fwdnode_impl.h` + `fwdnode_impl_sub.cpp` |
| GPU Shader Interface | `../ork.data/platform_lev2/shaders/fxv2/stdtools.i2` |
| Python Bindings | `pyext/src/pyext_gfx_lighting.cpp` |
| Type Definitions | `inc/ork/lev2/lev2_types.h` |

## Light Type Hierarchy

```
DrawableData (ork::Object)
└── LightData (abstract, gfx_lighting.h:70)
    ├── PointLightData (line 286)
    ├── DirectionalLightData (line 347)
    ├── SpotLightData (line 575)
    └── AmbientLightData (line 514)

Drawable
└── Light (abstract, gfx_lighting.h:131)
    ├── PointLight (line 313) / DynamicPointLight (line 336)
    ├── DirectionalLight (line 469) / DynamicDirectionalLight (line 504)
    ├── SpotLight (line 599) / DynamicSpotLight (line 693)
    └── AmbientLight (line 544)
```

## LightData Base (gfx_lighting.h:70–125)

| Member | Type | Default | Description |
|--------|------|---------|-------------|
| `mColor` | fvec3 | — | RGB color |
| `_intensity` | float | 1.0 | Brightness multiplier |
| `_priority` | float | 0.0 | Selection rank among lights of the same kind; `enumerateInPass` sorts the directional list DESCENDING by it, so index 0 is the authoritative sun/cascade pick |
| `_skyBody` | int | 0 | Which celestial body this light is for the procedural sky's discs: 0 none, 1 sun, 2 moon (declared, never inferred) |
| `mbShadowCaster` | bool | false | Casts shadows? |
| `_shadowsamples` | int | 1 | PCF shadow samples |
| `mShadowBlur` | float | 0.0 | Shadow softness [0,1] |
| `mShadowBias` | float | 0.002 | Shadow depth bias |
| `_cookie` | asset_ptr_t | — | Cookie texture asset |
| `_decal` | bool | false | Render as decal |
| `_shadowMapSize` | int | 1024 | Shadow map resolution (DirectionalLightData ctor overrides to 2048) |

The `Light` drawable exposes null-tolerant live reads `priority()` and `skyBody()` (Dynamic*Light constructs its base with a null LightData before installing inline data), plus `_castsShadows`, and a non-owning `_sgnode` back-ref to the owning scenegraph LightNode so `enabled()` can consult the node's `_enabled` flag without scenegraph headers.

## Light Types

### PointLight
- `_radius` — influence radius
- `_falloff` — attenuation falloff exponent
- Sphere-based collision tests
- `DynamicPointLight` — runtime-created with inline data

### DirectionalLight
- Unbounded — always in frustum, affects everything
- Sun cascade shadows + sun cookie (cloud shadows) — see the SKYLIGHT section below
- `lookAt(eye, tgt, up)` — points the light along (tgt-eye); installs the generator lambda ONCE and reads `_explicit_world` thereafter (per-frame std::function replacement races render-thread `worldMatrix()` calls)
- `DynamicDirectionalLight` — runtime-created

### SpotLight
- `mFovy` — field of view (degrees, default 10)
- `mRange` — maximum distance (default 1)
- `_cookiePath` — cookie texture path
- Frustum-based collision tests
- Shadow map: lazy-created `_shadowRTG` render target
- `shadowMatrix()` = projection * view
- `shadowCamDat()` → CameraData for shadow rendering
- `setViewProj` / `setOrthoViewProj` / `setPerspectiveViewProj` — explicit-matrix override (marks `_matrices_explicit` so xformgenerator lambdas / lookAt skip the recalc)
- `DynamicSpotLight` — runtime-created

### AmbientLight
- `mfAmbientShade` — base ambient level [0,1]
- `mvHeadlightDir` — directional component (default: (0, 0.5, 1))
- Always in frustum

## Light Manager (gfx_lighting.h:829–920)

Central manager for all lights in a scene:

- `mGlobalStationaryLights` — non-moving lights (GlobalLightContainer, max 256)
- `mGlobalMovingLights` — dynamic lights (LightContainer)
- `_lightprobes` — active light probes
- `_cookies_spot_color` / `_cookies_spot_depth` — texture arrays for cookies
- `_sun_shadow_cascades` / `_sun_cookie` (+ matrix/strength/lod/extinction dials) — sun shadow surface, see below

Key methods:
- `enumerateInPass(CPD, out)` — categorize lights for rendering pass
- `bindEnumeratedToStorageBuffer(ctx, lights, ssbo)` — pack lights into GPU SSBO
- `gpuInit(ctx)` — initialize texture arrays
- `ensureSunCascades(ctx, dim, sets)` / `ensureSunCookie(ctx, dim)` — sun shadow allocations
- `findProbeByName(name)` — probe registry lookup (see Probe Registry below)

## EnumeratedLights (gfx_lighting.h:807–825)

Result of `enumerateInPass()`, categorized for GPU binding:

- `_directionallights` — directional bucket; the forward node's sun-cascade pass picks THE sun (first active shadow-caster) from this list, sorted descending by `LightData::_priority`
- `_untexturedpointlights` — point lights without cookies
- `_tex2pointlightmap` — point lights grouped by cookie texture
- `_untexturedspotlights` — spot lights without cookies
- `_tex2spotlightmap` — textured spot lights by cookie
- `_tex2shadowedspotlightmap` — shadowed spot lights by cookie
- `_tex2spotdecalmap` — decal spot lights by cookie
- `_lightprobes` — active probes
- `_num_active_untextured_pointlights` / `_num_active_texspotlights` — active counts

## GPU SSBO Layout (max 64 lights)

Packed by `bindEnumeratedToStorageBuffer()` (gfx_lighting.cpp:1089–1192):

| Offset | Data | Size |
|--------|------|------|
| 0 | `vec4 _lightcolor[64]` | 1024B — RGB + intensity |
| 1024 | `vec4 _lightsizbias[64]` | 1024B — radius, bias, shadowmap size |
| 2048 | `vec4 _lightpos[64]` | 1024B — XYZ position |
| 3072 | `mat4 _shadowmatrix[64]` | 4096B — shadow projection * view |
| 7168 | `uint _lightTexSlice[64]` | 256B — cookie texture array slice |

Shader interface (`storage_fwd_lighting`, stdtools.i2:375–383):
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

Light counts passed via uniform block (`ublk_std_lighting`, stdtools.i2:292–299):
```glsl
uniform_block ublk_std_lighting (descriptor_set 0) {
  vec3 LightMapColors[8];
  int point_light_count;
  int spot_light_count;
  int has_reflection_probe;
}
```

## Sun Cascade Shadows + Cloud Shadows (SKYLIGHT)

Directional-light (sun/moon) shadows are a dedicated cascade subsystem on `DirectionalLightData` / `LightManager`, driven per frame by the forward prologue (`ForwardPbrNodeImpl::_update_sun_cookie` then `_update_sun_cascades`, declared in `fwdnode_impl.h`, implemented in `forward/fwdnode_impl_sub.cpp`).

### Cascade tunables (all reflected, on DirectionalLightData)

- `_shadowCascadeCount` (default 4, range 2..`kSunCascadeStorage`) — runtime cascade count
- `_shadowMaxDistance` (250) — CASTER CEILING in meters ABOVE a band, not the coverage radius; spent as ceiling/sin(elevation) of toward-light extrusion
- `_shadowBandRadius` (10) / `_shadowBandRatio` (4) — WORLD-ANCHORED BANDS: nested world-space spheres centered on the viewer position, radius[i] = base * ratio^i (10/40/160/640m by default); fitting is view-independent by construction so a pure rotation can never invalidate a fit
- `_pcfDither` (1.0) — PCF kernel radius in shadow texels (also dither magnitude)
- `_shadowBandResRatio` (1.0 = OFF) — per-band resolution: outer bands render into a viewport sub-rect of their slice, dim[i] = mapdim / ratio^i floored at 256
- `_shadowJitterTexels` (0) — per-SNAPSHOT sub-texel jitter of the snapped ortho origin (temporal supersampling); forced to 0 unless a crossfade is armed
- `_shadowSnapshotInterval` (0 = every frame) — seconds between cascade snapshots; fit+cull+depth passes are held between ticks; only a STRUCTURAL change (caster flip sun→moon, rig edit) refits early
- `_shadowSnapshotBandsPerFrame` (0 = all) — amortize one snapshot's depth passes across frames (double-buffers the cascade array)
- `_shadowCrossfadeFrames` (0 = hard swap) / `_shadowCrossfadeSecs` (0.2) — blend the OLD snapshot's shadow FACTOR into the new on a flip; the secs value makes the window a wall-clock duration rather than a frame count
- `_shadowMapSize` defaults to 2048 for directional lights (ctor override)

### Cloud-shadow (sun cookie) tunables

- `_cloudShadowStrength` (0 = the whole path DISARMED — no fill pass, no cookie, byte-identical frames)
- `_cloudShadowExtent` (4000) — ortho half-extent about the viewer (meters)
- `_cloudShadowSoftness` (2.0) — cookie mip-LOD bias for the GROUND penumbra
- `_cloudShadowDepth` (30000) — toward-light extrusion (must clear the deck altitude)
- `_cloudShadowMapSize` (512) — cookie resolution (mipped)
- `_cloudExtinction` (7.5) — Beer-Lambert optical depth of the direct beam: transmittance = exp(-tau * occlusion); shared by BOTH consumers (ground direct term and sun/moon disc)
- `_cloudDiscSoftness` (0.5) — the DISC's own (sharper) sample LOD

### LightManager sun surface

- `ensureSunCascades(ctx, dim, sets = 1)` — dedicated sun-cascade depth array `_sun_shadow_cascades` (Z32F), NOT the 256x256 spot cookie array; `kSunCascadeStorage = 4` slices reserved per set; sets=2 double-buffers as a SLICE RANGE of the one array (crossfade / amortized snapshots); default `_sun_shadow_cascades_default` is 8x8 so the sampler descriptor stays valid in sunless scenes
- `ensureSunCookie(ctx, dim)` — one small MIPPED RGBA8 RTG the prologue draws the cloud-deck layer into from a sun-aligned ortho camera; `_sun_cookie_default` is a 1x1 WHITE texel (transmittance 1)
- Published dials: `_sun_cookie_matrix` (world → cookie uv), `_sun_cookie_strength` (0 disarms), `_sun_cookie_lod`, `_sun_cookie_extinction`, `_sun_cookie_disc_lod`, `_sun_cookie_dir` (the direction the cookie's light travels)
- Bookkeeping: `_sun_cascade_slices` / `_sun_cascade_rtgs` (indexed set*kSunCascadeStorage + band), `_sun_cascade_dim`, `_sun_cascade_sets`

### Shader interface

- `ublk_sun` (stdtools.i2:308–364) — per-frame sun UBO: `sun_shadow_matrix[4]`, `sun_split_distances`, `sun_dir` (w = has_sun, gates ALL sun work), `sun_color`, `sun_shadow_params`, `sun_cascade_basis` (band anchor), `sun_cascade_texel`, `sun_cookie_matrix`/`sun_cookie_params`, crossfade prev-snapshot state (`sun_shadow_fade`, `sun_shadow_matrix_prev[4]` — the mat4 array LAST, member order is load-bearing), `sky_ambient` (measured background luminance)
- `sset_sun` (stdtools.i2:366–373) — `sun_shadow_map` (Z32F cascade depth array) + `sun_cookie` (alpha = cloud occlusion)
- Material side: `PBRMaterial::sunDataBuffer()` (single shared UBO, `kSunDataBufferBytes = 1024`), `_parSunBlock` / `_parSunShadowMap` / `_parSunCookie`

Snapshot/crossfade job state (frozen premise set, per-band deferred rendering, publish-at-completion) lives in `ForwardPbrNodeImpl::SunCascadeJob` (fwdnode_impl.h).

## Light Probes (gfx_lighting.h:216–282)

| Member | Description |
|--------|-------------|
| `_type` | `REFLECTION` or `SH_Radiance` |
| `_activationMode` | `ALWAYS` or `BAKE_ONLY` |
| `_name` | Registry key (entity-name-derived) |
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
- Dim/supersample/temporalFrames/renderLayer read through live component-data pointers (`_pDim` etc.) when bound, else the defaults

### SH_Radiance probes (SKYLIGHT lane C)

`_shSlot` (index into the projector's SH SSBO, claimed at RTG-allocation time), `_shProjector` (shared, node-owned ProbeSHProjector — the only handle by which a captured probe's coefficients can be read back), and `_shPendingProject` (carries a finished capture to the NEXT frame — projecting the cube in the frame that rendered it samples an image whose contents have not happened yet). All three stay inert for REFLECTION probes.

### Static vs Dynamic Probes (PBR2 Phase 0)

`forward/fwdnode_impl_sub.cpp` gates the cubemap render on `(probe->_dirty || probe->_dynamic)`:
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
- **Radiance cookies**: `pbr::radiancemaps_ptr_t` per light (`Light::_RadianceCookie`)
- **Sun cookie**: single plain 2D texture (see SKYLIGHT section) — one sun, covers everything the camera can reach rather than a per-light cone
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

# Directional light with cascades + cloud shadows
dir_data = lev2.DirectionalLightData()
dir_data.shadowCascadeCount = 4
dir_data.shadowMaxDistance = 250.0
dir_data.cloudShadowStrength = 0.5

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

1. For light types/params: check `gfx_lighting.h` class definitions (symbols above)
2. For GPU binding: check `LightManager::bindEnumeratedToStorageBuffer()` in `gfx_lighting.cpp`
3. For shader interface: grep `ublk_std_lighting` / `storage_fwd_lighting` / `ublk_sun` / `sset_sun` in `stdtools.i2`
4. For spot shadow maps: check the `SpotLight` shadow methods (`shadowMatrix`, `shadowCamDat`, `rendertarget`)
5. For sun cascades/cloud shadows: check `DirectionalLightData` tunables, `LightManager::ensureSunCascades`/`ensureSunCookie`, and `ForwardPbrNodeImpl::_update_sun_cascades`/`_update_sun_cookie`
6. For probes: check `LightProbe` in `gfx_lighting.h` and `lightprobe.cpp`
7. For Python: check `pyext_gfx_lighting.cpp`
