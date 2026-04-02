---
name: orklev2-scenegraph
description: Answer questions about orkid's scene graph rendering system, Scene/Layer/Node hierarchy, drawables, cameras (CameraData, EzUiCam), lighting (point/directional/spot), probes, SceneGraphViewport, and Python scene composition. Use when the user asks about 3D scenes, models, cameras, lights, or rendering setup.
user-invocable: false
---

# Orkid Scene Graph Rendering Reference

When answering questions about scene graphs, 3D rendering, cameras, or lighting in orkid, consult these files. All under `ork.lev2/`.

## Key Files

| Component | Header |
|-----------|--------|
| Scene/Layer/Node | `inc/ork/lev2/gfx/scenegraph/scenegraph.h` |
| Drawable System | `inc/ork/lev2/gfx/renderer/drawable.h` |
| CameraData | `inc/ork/lev2/gfx/camera/cameradata.h` |
| EzUiCam | `inc/ork/lev2/gfx/camera/uicam.h` |
| Lighting | `inc/ork/lev2/gfx/lighting/gfx_lighting.h` |
| SceneGraphViewport | `inc/ork/lev2/ui/viewport_scenegraph.h` |
| Python Bindings | `pyext/src/pyext_gfx_scenegraph.cpp` |

**Implementations:** `src/gfx/scenegraph/`, `src/gfx/camera/`, `src/gfx/lighting/`

## Hierarchy

```
Scene
├── Layer (named, sorted by _sortkey)
│   ├── DrawableNode (holds Drawable)
│   ├── LightNode (holds Light)
│   └── ProbeNode (holds LightProbe)
└── LightManager, CompositingData, CameraDataLut
```

## Scene Creation (Python)

```python
params = core.VarMap()
params.preset = "ForwardPBR"
params.SkyboxTexPathStr = "nebula"
params.clearcolor = fvec3(0.1, 0.1, 0.5)

scene = lev2.scenegraph.Scene(params)
layer = scene.createLayer("std_forward")
```

## Drawable Types

| Type | Purpose |
|------|---------|
| `ModelDrawableData` | 3D model from file (glb/gltf) |
| `InstancedModelDrawableData` | Instanced rendering (SSBO, up to 65536) |
| `CallbackDrawable` | Custom procedural rendering |
| `StringDrawableData` | 2D text |
| `BillboardStringDrawableData` | Screen-facing text |

```python
# Model
model_data = lev2.ModelDrawableData("data://tests/testcube.glb")
drawable = model_data.createDrawable()
node = layer.createDrawableNode("cube", drawable)
node.worldTransform = matrix
node.modcolor = fvec4(1, 1, 1, 1)
node.enabled = True

# Instanced
inst_data = lev2.InstancedModelDrawableData("data://models/sphere.glb")
inst_data.resize(1000)
inst_drawable = inst_data.createDrawable()
node = layer.createDrawableNode("spheres", inst_drawable)
```

## Camera Setup

```python
# CameraData
cam = lev2.CameraData()
cam.Lookat(eye=fvec3(0,3,8), tgt=fvec3(0,0,0), up=fvec3(0,1,0))
cam.Persp(near=0.1, far=1000.0, fov_degrees=45.0)

# CameraDataLut (named camera registry)
camera_lut = lev2.CameraDataLut()
camera_lut.create("main_cam")

# EzUiCam (interactive mouse camera)
from lev2utils.cameras import setupUiCameraX
cam_data, ui_cam = setupUiCameraX(
    cameralut=camera_lut, camname="main_cam",
    eye=fvec3(0,3,8), tgt=fvec3(0,0,0), up=fvec3(0,1,0), far=1000.0)

# Camera event handler
def on_cam_event(ev):
    handled = ui_cam.uiEventHandler(ev)
    if handled:
        ui_cam.updateMatrices()
        cam_data.copyFrom(ui_cam.cameradata)
    return lev2.ui.HandlerResult()
```

## Lighting

```python
# Point Light
light_data = lev2.PointLightData()
light_data.mColor = fvec3(1, 1, 1)
light_data._intensity = 1.0
light_data.mbShadowCaster = True
light = light_data.createDrawable()
light_node = layer.createLightNode("main_light", light)
light_node.setMatrix(transform_matrix)

# Directional Light
dir_data = lev2.DirectionalLightData()
dir_data.mColor = fvec3(1, 0.9, 0.8)
dir_data._intensity = 0.5

# Spot Light
spot_data = lev2.SpotLightData()
spot_data.mFovy = 45.0  # Cone angle degrees
spot_data.mRange = 50.0
```

Light types: `ELIGHTTYPE_POINT`, `ELIGHTTYPE_DIRECTIONAL`, `ELIGHTTYPE_SPOT`, `ELIGHTTYPE_AMBIENT`

## SceneGraphViewport Widget

```python
# Create viewport in layout
item = layout_group.makeChild(
    uiclass=lev2.ui.SceneGraphViewport, args=["viewport"])
sgvp = item.widget

sgvp.scenegraph = scene
sgvp.cameraName = "main_cam"
sgvp.camera_evhandler = on_cam_event
sgvp.forkDB()  # Required for embedded viewports

# GPU update all viewports (call in render loop)
lev2.ui.SceneGraphViewport.gpuUpdateAll(context)
```

## Render Loop

```python
# In onUpdate:
scene.updateScene(camera_lut)

# In GPU render:
scene.renderOnContext(context)
```

## Node Properties

```python
node.worldTransform = fmtx4()   # World matrix
node.modcolor = fvec4(1,1,1,1)  # Color modulation
node.enabled = True              # Visibility
node.pickable = True             # Picking support
node.view_relative = False       # View-space transform
```

## Thread Safety

Scene graph uses `LockedResource<T>` for all collections:
```python
# Python access returns copies (thread-safe)
nodes = layer.drawable_nodes
layers = scene.layers
```

## How to Answer

1. For scene setup: read `scenegraph.h` and `pyext_gfx_scenegraph.cpp`
2. For drawables: read `drawable.h` for all DrawableData types
3. For cameras: read `cameradata.h` and `uicam.h` for EzUiCam
4. For lighting: read `gfx_lighting.h` for all light types
5. For examples: check `ork.lev2/pyext/tests/ui/sgui_emb_sg.py`
