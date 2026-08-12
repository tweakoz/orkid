---
name: orklev2-compositor
description: Answer questions about orkid's compositor system, CompositingData/Scene/Technique, render nodes (Forward/Unlit/Picking), post-FX nodes (ACES/HSVG/Bloom/User), output nodes (Screen/RtGroup/VR/File), presets (ForwardPBR/Unlit/Picking), RtGroup render targets, and Python compositor bindings. Use when the user asks about compositing, post-processing, render targets, or rendering presets.
user-invocable: false
---

# Orkid Compositor System Reference

When answering questions about compositing, post-processing, or render presets in orkid, consult the files below. All under `ork.lev2/`.

## Key Files

| Component | File |
|-----------|------|
| Core Compositor Structures | `inc/ork/lev2/gfx/renderer/compositor.h` |
| Node Base Classes | `inc/ork/lev2/gfx/renderer/NodeCompositor/NodeCompositor.h` |
| Compositor Material | `inc/ork/lev2/gfx/renderer/compositormaterial.h` |
| PBR Common (IBL/SSAO) | `inc/ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h` |
| Render Targets | `inc/ork/lev2/gfx/rtgroup.h` |
| Forward Node | `inc/ork/lev2/gfx/renderer/NodeCompositor/pbr_node_forward.h` |
| Unlit Node | `inc/ork/lev2/gfx/renderer/NodeCompositor/unlit_node.h` |
| Screen Output | `inc/ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h` |
| RtGroup Output | `inc/ork/lev2/gfx/renderer/NodeCompositor/OutputNodeRtGroup.h` |
| VR Output | `inc/ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorVr.h` |
| File Output | `inc/ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorFile.h` |
| Post-FX ACES | `inc/ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeACES.h` |
| Post-FX HSVG | `inc/ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeHSVG.h` |
| Post-FX Bloom | `inc/ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeDecompBlur.h` |
| Post-FX User | `inc/ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeUser.h` |
| Preset Implementations | `src/gfx/renderer/compositordata.cpp` |
| CompositingImpl Runtime | `src/gfx/renderer/compositorimpl.cpp` |
| Node Implementations | `src/gfx/renderer/NodeCompositor/` |
| Forward Node Impl | `src/gfx/renderer/NodeCompositor/forward/fwdnode_impl.h` |
| Python Bindings | `pyext/src/pyext_gfx_compositor.cpp` |

## Architecture Overview

3-stage pipeline:

```
RenderCompositingNode ──→ PostCompositingNode(s) ──→ OutputCompositingNode
  (generate frame)         (effects chain)             (output sink)
```

### Data Flow
```
CompositingData
  └─ CompositingScene
       └─ CompositingSceneItem
            └─ NodeCompositingTechnique
                 ├─ _renderNode (RenderCompositingNode)
                 ├─ _postEffectNodes (vector<PostCompositingNode>)
                 └─ _outputNode (OutputCompositingNode)
```

### Runtime
```
CompositingImpl::assemble()
  └─ NodeCompositingTechnique::assemble()
       └─ _assemblerFn():
            1. outputNode->beginAssemble()
            2. renderNode->Render()
            3. outputNode->endAssemble()
            4. for each postFxNode: Render()
            5. Store final output

CompositingImpl::composite()
  └─ outputNode->composite()
```

## Render Nodes

### RenderCompositingNode (NodeCompositor.h:57) — Abstract Base
- `_renderingmodel` — e.g., `"FORWARD_PBR"_crcu`
- `_layers` — which scene layers to render
- `DoRender(CompositorDrawData&)` — pure virtual

### ForwardNode (pbr_node_forward.h:17)
Full forward PBR rendering:
- Depth prepass → skybox → SSAO → color pass
- Environment probe updates
- Shadow map updates
- `_pbrcommon` — shared PBR resources (IBL, SSAO params)

### UnlitNode (unlit_node.h:15)
Simple unlit rendering with configurable clear color.

### PickingCompositingNode
Color-based object selection rendering.

## Post-FX Nodes

### PostCompositingNode (NodeCompositor.h:89) — Abstract Base
- `_disabled` — skip without removing from chain
- `DoRender(CompositorDrawData&)` — pure virtual
- `GetOutputGroup()` → `rtgroup_ptr_t`

### PostFxNodeACES — ACES Tonemapping
- `_exposure` (float, default 1.0)
- Uses "framefx_aces" technique

### PostFxNodeHSVG — Color Grading
- `_hue` (0.0), `_saturation` (1.0), `_value` (1.0), `_gamma` (1.0)

### PostFxNodeDecompBlur — Bloom
- `_threshold` (0.5) — brightness cutoff
- `_amount` (0.5) — bloom intensity
- `_blurfactor` (1/32) — blur kernel scale
- `_blurwidth` (16) — blur kernel width

### PostFxNodeUser — Custom Shader
- `_shader_path` — path to shader file
- `_technique_name` — technique to use
- `_bindings` — parameter map
- `_double_buffer` — ping-pong texture
- `_flip_vertical` — vertical flip

### Op2CompositingNode — Binary Operations
- Two inputs (A, B) with scale/bias
- Modes: A*B, A/B, B/(A+C), A+B+C, lerp(A,B,C), solo

## Output Nodes

### OutputCompositingNode (NodeCompositor.h:27) — Abstract Base
- `_flipY` — flip Y axis (default true)
- `beginAssemble()` / `endAssemble()` / `composite()`
- Python callbacks: `_onBeginAssemble`, `_onEndAssemble`, `_onCameraChange`

### ScreenOutputCompositingNode (NodeCompositorScreen.h:19)
- Direct screen/window output
- `_supersample` — supersampling factor
- `_format` — buffer format (RGBA8, RGBA32F, etc.)

### RtGroupOutputCompositingNode (OutputNodeRtGroup.h:15)
- Render-to-texture output
- `resize(w, h)` — resize output
- `_temporalFrames` — temporal accumulation

### VrOutputNode (NodeCompositorVr.h:29)
- Stereo VR output
- `_distortion_lambda` — barrel distortion callback
- Mono viewer mode option

### SinglePassStereoVrOutputNode (NodeCompositorVr.h:53)
- THE VR output node: both eyes in ONE layered (2-view multiview) scene pass
- Preset strings `FWDPBRSPVR` and `FWDPBRVRDM` BOTH route here (compositordata.cpp
  `presetForwardPBRSPVR`); a device without multiview THROWS in the scenegraph
  preset resolver — there is no degraded VR path
- `downsampledEyeRtGroup(left)` — the per-eye final downsampled RtGroup (the buffer
  the XR runtime and the desktop mirror consume); pybound, headless-capturable

### FileOutputCompositingNode (NodeCompositorFile.h:50)
- Image sequence output
- Frame rates: 15/24/30/48/60/72/96/120/240 fps
- Resolutions: 640x480 through 1920x1080, with multipliers (Quarter–Quadruple)

## Render Targets (rtgroup.h)

### RtGroup (line 60) — Multiple Render Targets
- Max 8 MRT buffers (`kmaxmrts`)
- `createRenderTarget(format, usage, with_texture)` → `rtbuffer_ptr_t`
- `createDepthBuffer(format, with_texture)` → `rtbuffer_ptr_t`
- MSAA support, auto-clear, cube map mode

### RtBuffer (line 27) — Single Buffer
- Format, dimensions, texture access
- Mip generation: NONE, AUTOCOMPUTE, USER
- Clear color/depth configuration

### RtgSet (line 111) — Indexed Collection
- `fetch(key)` → get or create RtGroup by CRC key
- Used by render nodes for different output configurations

## Presets (compositordata.cpp)

| Preset | Method | Description |
|--------|--------|-------------|
| ForwardPBR | `presetForwardPBR()` | Forward PBR with optional skybox |
| Unlit | `presetUnlit()` | Simple unlit rendering |
| Picking | `presetPicking()` | Object selection pass |
| PickingDebug | `presetPickingDebug()` | Picking with screen output |
| ForwardPBRVR | `presetForwardPBRVR()` | Forward PBR for VR |
| ForwardPBRSPVR | `presetForwardPBRSPVR()` | Single-pass stereo VR (preset strings FWDPBRSPVR + FWDPBRVRDM) |

Note: DeferredPBR is currently disabled (`OrkAssert(false)`).

Each preset returns `RenderPresetContext`:
- `_nodetek` — technique
- `_outputnode` — output node
- `_rendernode` — render node

## Forward Pass Roles + HUD Overlay (PBR2 Phase 0)

`fwdnode_impl_top.cpp:k_roles[]` enumerates the layer roles known to the forward pass CPD — `topCPD.HasLayer(name)` returns true only for roles present in this list:

```cpp
static const char* k_roles[] = {
  "depth_prepass", "std_forward", "std_editor",
  "probe", "depth_probe",
  "hud_overlay",   // PBR2 Phase 0 — overlay layer rendered AFTER scene
};
```

### Probe-Cubemap CPD: Hard Layer Reset, not Additive

The probe cubemap pass uses `CompositingPassData::assignLayers(probe->renderLayer())` to **clear** the layer set inherited from `CPD.clone()` (which would otherwise carry `hud_overlay`, `std_editor`, etc. into the cubemap render). `assignLayers` clears both `_layernames` and `_layernameset` before assigning — both containers must be reset, the original implementation only cleared one.

### HUD/Editor Overlay Post-Pass

`_render_colorpass` in `fwdnode_impl_sub.cpp` ends with a separate enqueue for overlay layers, gated on `!_renderingPROBE`:

```cpp
if (not fpass->_renderingPROBE) {
  if (_currentDrawQueue->_enableEditorLayers)
    _currentDrawQueue->enqueueLayerToRenderQueue("std_editor", _currentIRenderer);
  _currentDrawQueue->enqueueLayerToRenderQueue("hud_overlay", _currentIRenderer);
  _currentIRenderer->drawEnqueuedRenderables(true);
}
```

Probe captures skip the overlay post-pass entirely. The main viewport renders the scene, then the overlay layers as a second `drawEnqueuedRenderables`.

## CompositorDrawData Properties

Key CRC-hashed properties passed through the pipeline:

| Key | Type | Description |
|-----|------|-------------|
| `"render_out"` | RtBuffer* | Render node output buffer |
| `"render_outgroup"` | rtgroup_ptr_t | Render node output group |
| `"postfx_in"` | rtgroup_ptr_t | Input to current post-FX |
| `"final_out"` | RtBuffer* | Final output buffer |
| `"final_outgroup"` | rtgroup_ptr_t | Final output group |
| `"DB"` | DrawQueue* | Drawable buffer |

## Python API

```python
from orkengine import lev2

# Build compositor manually
comp_tek = lev2.NodeCompositingTechnique()
render_node = comp_tek.createRenderNode(lev2.ForwardNode, pbr_common)
output_node = comp_tek.createOutputNode(lev2.ScreenOutputCompositingNode)

# Add post-FX
aces = lev2.PostFxNodeACES()
aces.exposure = 1.5
comp_tek.postEffectNodes.append(aces)

bloom = lev2.PostFxNodeDecompBlur()
bloom.threshold = 0.8
bloom.amount = 0.3

hsvg = lev2.PostFxNodeHSVG()
hsvg.gamma = 2.2

# Scene setup
comp_data = lev2.CompositingData()
comp_scene = comp_data.createScene("scene1")
comp_item = comp_scene.createSceneItem("item1")
comp_item.technique = comp_tek

# Or use presets (via Scene constructor)
params = core.VarMap()
params.preset = "ForwardPBR"
params.SkyboxTexPathStr = "nebula"
scene = lev2.scenegraph.Scene(params)

# Output node callbacks
output_node.onBeginAssemble(lambda drawdata: ...)
output_node.onEndAssemble(lambda drawdata: ...)

# View data access
viewdata = drawdata.viewData
print(viewdata.camposmono)  # Camera position
print(viewdata.near, viewdata.far)

# Render node properties
render_node.layers = "std_forward"
render_node.debugRenderingModel = "FORWARD_PBR"
```

## How to Answer

1. For compositor structure: check `compositor.h` data classes
2. For node types: check individual headers in `NodeCompositor/`
3. For presets: check `compositordata.cpp` (preset methods)
4. For forward rendering pipeline: check `fwdnode_impl.h` and `fwdnode_impl_top.cpp`
5. For post-FX: check individual `PostFxNode*.h` and `.cpp` files
6. For render targets: check `rtgroup.h` (RtGroup, RtBuffer, RtgSet)
7. For Python: check `pyext_gfx_compositor.cpp`
8. For PBR environment/SSAO: check `pbr_common.h`
