---
name: orklev2-renderer
description: Answer questions about orkid's renderer/drawable system, Drawable hierarchy (Model/Instanced/Callback/String/Billboard), DrawQueue triple-buffering, IRenderer sort/draw, RenderQueue radix sorting, renderables (ModelRenderable/CallbackRenderable), SSBO instancing, skinned mesh, and Python drawable bindings. Use when the user asks about drawables, render queues, instanced rendering, or the draw pipeline.
user-invocable: false
---

# Orkid Renderer & Drawable System Reference

When answering questions about drawables, render queues, or the draw pipeline in orkid, consult the files below. All under `ork.lev2/`.

## Key Files

| Component | File |
|-----------|------|
| Drawable / DrawableData / DrawQueue | `inc/ork/lev2/gfx/renderer/drawable.h` |
| IRenderer | `inc/ork/lev2/gfx/renderer/renderer.h` |
| RenderQueue | `inc/ork/lev2/gfx/renderer/renderqueue.h` |
| IRenderable / ModelRenderable / CallbackRenderable | `inc/ork/lev2/gfx/renderer/renderable.h` |
| Render Context (RCID/RCFD) | `inc/ork/lev2/gfx/renderer/rendercontext.h` |
| Drawable Impl (base) | `src/gfx/renderer/drawable/drawable.cpp` |
| Model Drawable | `src/gfx/renderer/drawable/drawable_model.cpp` |
| Instanced Drawable | `src/gfx/renderer/drawable/drawable_instanced.cpp` |
| Instanced Model | `src/gfx/renderer/drawable/drawable_instanced_model.cpp` |
| Callback Drawable | `src/gfx/renderer/drawable/drawable_callback.cpp` |
| Billboard String | `src/gfx/renderer/drawable/drawable_billboard_string.cpp` |
| Draw Queue | `src/gfx/renderer/drawable/draw_queue.cpp` |
| IRenderer Sort/Draw | `src/gfx/renderer/irenderer.cpp` |
| Python Drawable Bindings | `pyext/src/pyext_gfx_drawable.cpp` |
| Python DrawableData Bindings | `pyext/src/pyext_gfx_drawable_data.cpp` |

## Architecture Overview

```
Update Thread                              Render Thread
─────────────                              ─────────────
acquireForWriteLocked()                    acquireForReadLocked()
  └─ DrawQueue                               └─ DrawQueue
       └─ DrawQueueLayer                          └─ enqueueLayerToRenderQueue()
            └─ enqueueDrawable()                       └─ Drawable::enqueueToRenderQueue()
                 └─ DrawQueueItem                           └─ IRenderer::enqueue{Model,Callback}()
releaseFromWriteLocked()                                         └─ drawEnqueuedRenderables()
                                                                      └─ radix sort → Render()
                                           releaseFromReadLocked()

Triple-buffered via concurrent_triple_buffer<DrawQueue>
```

## Drawable Hierarchy

```
Drawable (drawable.h:308)
├── ModelDrawable (line 430) — 3D model from file
├── InstancedDrawable (line 478) — base for instanced rendering
│   ├── InstancedModelDrawable (line 514) — instanced 3D models
│   └── InstancedBillboardStringDrawable — instanced text
├── CallbackDrawable (line 713) — custom render lambda
├── StringDrawable (line 552) — 2D screen text
└── BillboardStringDrawable (line 620) — 3D billboard text

DrawableData : ork::Object (drawable.h:376) — factory pattern
├── ModelDrawableData (line 414) — asset path based
├── InstancedModelDrawableData — instanced model factory
├── CallbackDrawableData — callback factory
├── StringDrawableData — 2D text factory
└── BillboardStringDrawableData — billboard text factory
```

## Drawable Base (drawable.h:308–364)

| Member | Type | Description |
|--------|------|-------------|
| `_pickID` | pickvariant_t | Object picking ID |
| `_modcolor` | fvec4 | Tint color |
| `_sortkey` | int | Render sort order |
| `_drawable_type` | uint64_t | Type ID (e.g., `"model"_crcu`) |
| `_tag` | uint64_t | User filtering tag |
| `_envmapOverride` | radiancemaps_ptr_t | Per-drawable IBL override |
| `_properties` | varmap_ptr_t | User properties |
| `_rendercb` / `_rendercb_user` | on_render_rcid_t | Render callbacks |
| `_sg` / `_sgnode` | scene/node ptr | Scene graph association |

Key methods:
- `enqueueOnLayer(xfdata, layer)` — add to draw queue layer
- `enqueueToRenderQueue(item, renderer)` — submit renderables to renderer

## DrawQueue & Triple Buffering

### DrawQueueContext (drawable.h:269–290)
- `acquireForWriteLocked()` / `releaseFromWriteLocked()` — update thread
- `acquireForReadLocked()` / `releaseFromReadLocked()` — render thread
- Uses `concurrent_triple_buffer<DrawQueue>` — lock-free rotation

### DrawQueue (drawable.h:154–262)
- Max 8 layers (`kmaxlayers`)
- `MergeLayer(name)` — get or create layer
- `enqueueLayerToRenderQueue(name, renderer)` — submit layer to renderer
- `setPreRenderCallback(key, cb)` — per-frame callbacks
- `copyCameras(cameralut)` — copy camera data for render thread

### DrawQueueItem (drawable.h:92–112)
- `_drawable` — pointer to source drawable
- `_dqxferdata` — transform + modcolor (DrawQueueTransferData)
- `_sortkey` — sort order
- `_usermap` — per-item user data

### DrawQueueTransferData (drawable.h:83–88)
- `_worldTransform` — decomposed transform (pos, rot, scale)
- `_modcolor` — tint color
- `_use_modcolor` — whether to apply modcolor

## IRenderer (renderer.h:32–87)

Sort and draw renderables:

- `enqueueModel()` → `ModelRenderable&`
- `enqueueCallback()` → `CallbackRenderable&`
- `drawEnqueuedRenderables(reset_after)` — radix sort by sortkey, then render in order
- Max 65536 model renderables, 8192 callback renderables
- Uses 32-bit radix sort (O(n)) via `RadixSort`

Sort key constants:
- `kFirstRenderableSortKey = 0` (back-most)
- `kLastRenderableSortKey = 0x7ffffffe`
- `kManipRenderableSortKey = 0x7fffffff` (top-most)

## Renderable Types

### IRenderable (renderable.h:35–70)
Base with: `_worldMatrix`, `_modColor`, `_pickID`, `_sortkey`, `_instanced`, `_drawable`

### ModelRenderable (renderable.h:74–93)
- `_submeshinst` — submesh to render
- `_modelinst` — model instance (skeleton, etc.)
- `_cluster` — mesh cluster
- `_scale`, `_offset`, `_orientation` — transform adjustments
- `_envmapOverride` — per-drawable IBL

### CallbackRenderable (renderable.h:110–123)
- `cbtype_t = std::function<void(RenderContextInstData& RCID)>`
- User lambda has full control over rendering

## Instanced Rendering (SSBO-based)

### InstancedDrawable (drawable.h:478–510)
- Max 65536 instances (`k_max_instances`)
- ~5.5MB SSBO total:

| Offset | Data | Size per 65536 |
|--------|------|-----------------|
| 0 | `mat4 matrices[]` | 4MB |
| 4194304 | `vec4 colors[]` | 1MB |
| 5242880 | `uint64 pickids[]` | 512KB |

- Triple-buffered via `concurrent_triple_buffer<InstancedDrawableInstanceData>`
- Single draw call renders all instances

### InstancedDrawableInstanceData (drawable.h:538–541)
- `_worldmatrices` — per-instance 4x4 matrices
- `_modcolors` — per-instance RGBA colors
- `_pickids` — per-instance picking IDs
- `allocInstance()` / `freeInstance(idx)` — pool management

## Model Drawable Flow

`ModelDrawable::enqueueToRenderQueue()` (drawable_model.cpp:172):
1. Frustum-cull bounding sphere
2. For each visible submesh cluster:
   - Create `ModelRenderable`
   - Set submesh, matrix, colors, sort key
   - Enqueue to IRenderer
3. Skinned: bind `XgmWorldPose` bone matrices

## Callback Drawable

```cpp
auto cbdrw = std::make_shared<CallbackDrawable>(nullptr);
cbdrw->setRenderLambda([](RenderContextInstData& RCID) {
    auto ctx = RCID.context();
    // Full rendering control: bind pipeline, set params, draw
});
```

## Python API

```python
from orkengine import lev2

# Model drawable
mddata = lev2.ModelDrawableData("data://models/mesh.glb")
drawable = mddata.createDrawable()
sg_drawable = mddata.createSGDrawable(scenegraph)

# Instanced model
imdata = lev2.InstancedModelDrawableData("data://models/tree.glb")
imdata.resize(1000)
imdraw = imdata.createDrawable()

# Access instance data
idata = imdraw.instance_data
matrices = idata.matrices   # numpy-compatible buffer (count, 4, 4)
colors = idata.colors        # numpy-compatible buffer (count, 4)

# Billboard string
bddata = lev2.BillboardStringDrawableData()
bddata.initialString = "Hello"
bddata.offset = fvec3(0, 1, 0)
bddata.scale = 0.5
bddraw = bddata.createDrawable()

# DrawableData properties
ddata.modcolor = fvec4(1, 1, 1, 1)
ddata.environmentMapPath = "<assetcache>/env/skybox.dds"
```

## How to Answer

1. For drawable types: check class definitions in `drawable.h`
2. For draw queue threading: check `DrawQueueContext` and `concurrent_triple_buffer`
3. For sort/draw: check `irenderer.cpp:56–150` (radix sort pipeline)
4. For instanced rendering: check `drawable_instanced_model.cpp` (SSBO upload)
5. For model rendering: check `drawable_model.cpp:172+` (frustum cull, submesh loop)
6. For Python: check `pyext_gfx_drawable.cpp` and `pyext_gfx_drawable_data.cpp`
