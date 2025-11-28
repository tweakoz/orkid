# Orkid UI Transform Flow - Technical Reference

This document details the complete coordinate transform and matrix setup flow for the Orkid UI system across all rendering scenarios.

---

## Overview

The UI rendering system uses an orthographic projection matrix (`PushUIMatrix`) to transform pixel coordinates to normalized device coordinates (NDC). The critical challenge is ensuring the ortho matrix dimensions match the actual render target dimensions across different rendering contexts.

---

## Coordinate System Basics

### Widget Local Coordinates

Each widget has local geometry relative to its parent:
- `_geometry._x`, `_geometry._y`: Position within parent
- `_geometry._w`, `_geometry._h`: Widget dimensions

### LocalToRoot Transform

Widgets convert local coordinates to root coordinates by accumulating offsets up the parent chain:

```cpp
// ork.lev2/src/ui/widgets/widget.cpp:292
void Widget::LocalToRoot(int lx, int ly, int& rx, int& ry) const {
  rx = lx;
  ry = ly;
  const Widget* w = this;
  while (w) {
    rx += w->x();
    ry += w->y();
    w = w->parent();
  }
}
```

**Key Point**: `LocalToRoot` produces coordinates in the **root widget's coordinate space**, which must match the ortho projection matrix dimensions.

---

## The UI Matrix: PushUIMatrix()

The orthographic projection matrix is the key to correct UI rendering. It maps pixel coordinates to NDC [-1, 1].

### Implementation (ork.lev2/src/gfx/context/mtxi.cpp:34-53)

```cpp
void MatrixStackInterface::PushUIMatrix() {
  // Step 1: Get viewport dimensions from FBI (FrameBuffer Interface)
  float fw = _target.FBI()->GetVPW();
  float fh = _target.FBI()->GetVPH();

  // Step 2: COMPOSITOR OVERRIDE - Critical behavior!
  auto pfdata = _target.topRenderContextFrameData();
  if (pfdata and pfdata->topCompositor()) {
    const auto& CPD = pfdata->topCPD();
    fw = float(CPD.GetDstRect()._w);  // Uses compositor's destination rect
    fh = float(CPD.GetDstRect()._h);  // NOT the viewport!
  }

  // Step 3: Create orthographic matrix and push M/V/P matrices
  fmtx4 mtxP = _target.MTXI()->Ortho(0.0f, fw, 0.0f, fh, 0.0f, 1.0f);
  PushPMatrix(mtxP);
  PushVMatrix(fmtx4::Identity());
  PushMMatrix(fmtx4::Identity());
}
```

### The Explicit Dimensions Overload

```cpp
void MatrixStackInterface::PushUIMatrix(int iw, int ih) {
  fmtx4 mtxMVP = uiMatrix(iw, ih);  // Uses provided dimensions directly
  PushPMatrix(mtxMVP);
  PushVMatrix(fmtx4::Identity());
  PushMMatrix(fmtx4::Identity());
}
```

**Important**: The parameterized version bypasses the compositor override entirely.

---

## Rendering Scenarios

### Scenario 1: Direct Window Rendering (No Compositor)

**Context**: Simple applications rendering UI directly to the window.

**Flow**:
1. `PushUIMatrix()` called (no args)
2. `topCompositor()` returns `nullptr`
3. Dimensions come from viewport: `FBI()->GetVPW()`, `FBI()->GetVPH()`
4. Ortho matrix matches viewport → **Correct rendering**

**Example**:
```
Window: 1024x768
Viewport: 1024x768 (same as window)
Ortho matrix: 1024x768
Widget at LocalToRoot(100, 100) → Renders at pixel (100, 100) ✓
```

### Scenario 2: Compositor Rendering (Scene Graph)

**Context**: Rendering through a compositor (e.g., `pbr_compositor`, `deferred_compositor`).

**Flow**:
1. Scene graph pushes `RenderContextFrameData` with compositor
2. Compositor sets `CPD.GetDstRect()` to window dimensions
3. UI widgets call `PushUIMatrix()` (no args)
4. `topCompositor()` returns non-null
5. Dimensions come from `CPD.GetDstRect()` (window size)
6. Ortho matrix matches window → **Correct for full-window UI**

**Example**:
```
Window: 1024x768
Compositor CPD DstRect: 1024x768
Ortho matrix: 1024x768
Widget at LocalToRoot(100, 100) → Renders at pixel (100, 100) ✓
```

### Scenario 3: Render-to-Texture (Surface/LayoutSurface)

**Context**: Rendering UI content to an off-screen texture (RtGroup).

**Expected Flow**:
1. `PushRtGroup(rtgroup)` sets viewport to rtgroup dimensions (e.g., 256x256)
2. Widget calls `PushUIMatrix()` (no args)
3. Should use viewport dimensions (256x256)
4. Ortho matrix should be 256x256

**The Problem** (Pre-Fix):

When rendering occurs **during compositor rendering** (e.g., UISurfacePrimitiveData in scene graph):

1. Scene graph already pushed RCFD with compositor
2. `PushRtGroup(256x256)` sets viewport correctly
3. Widget calls `PushUIMatrix()` (no args)
4. `topCompositor()` returns **non-null** (from scene graph's RCFD)
5. Uses `CPD.GetDstRect()` → **Window dimensions (1024x768)**
6. Ortho matrix is 1024x768, viewport is 256x256 → **MISMATCH**

**Result**: Content intended for 256x256 gets coordinates scaled as if for 1024x768, appearing cramped in a corner.

**Visual**:
```
Expected:                    Actual (Bug):
+------------------+         +------------------+
|  [Grid fills    |         |[tiny]            |
|   entire        |         |                  |
|   texture]      |         |                  |
|                 |         |                  |
+------------------+         +------------------+
256x256 texture              256x256 texture
```

### Scenario 4: Render-to-Texture with Isolated RCFD (The Fix)

**Context**: Same as Scenario 3, but with proper isolation.

**Solution** (ork.lev2/src/ui/widgets/layoutsurface.cpp):

```cpp
void LayoutSurface::updateTextureIfNeeded(lev2::Context* ctx) {
  // ... setup code ...

  auto fbi = ctx->FBI();
  _rtgroup->_autoclear = true;
  _rtgroup->buffer(0)->_clearColor = _clearColor;
  _rtgroup->buffer(0)->_clearDepth = mfClearDepth;

  // KEY: Push isolated RCFD WITHOUT compositor
  // This makes topCompositor() return nullptr during UI rendering
  auto rcfd = std::make_shared<lev2::RenderContextFrameData>(ctx);
  ctx->pushRenderContextFrameData(rcfd);  // No compositor attached!

  fbi->PushRtGroup(_rtgroup.get());  // Viewport now 256x256
  {
    auto drwev = std::make_shared<DrawEvent>(ctx);
    DoRePaintSurface(drwev);  // Widgets render here
  }
  fbi->PopRtGroup();

  ctx->popRenderContextFrameData();  // Restore previous RCFD
}
```

**Flow with Fix**:
1. Scene graph has RCFD with compositor (window dimensions)
2. `updateTextureIfNeeded()` pushes **new RCFD without compositor**
3. `PushRtGroup(256x256)` sets viewport
4. Widgets call `PushUIMatrix()` (no args)
5. `topCompositor()` returns **nullptr** (isolated RCFD has no compositor)
6. Uses viewport dimensions → 256x256
7. Ortho matrix matches viewport → **Correct rendering**

---

## RenderContextFrameData (RCFD) Stack

The context maintains a stack of `RenderContextFrameData` objects:

```cpp
void Context::pushRenderContextFrameData(rcfd_ptr_t rcfd);
void Context::popRenderContextFrameData();
rcfd_ptr_t Context::topRenderContextFrameData();
```

Each RCFD can have:
- `Compositor*` via `topCompositor()`
- `CompositorPassData` via `topCPD()`
- Camera matrices
- Other per-frame rendering state

**Isolation Principle**: Push a bare RCFD (no compositor) to isolate render-to-texture operations from the parent rendering context.

---

## Viewport and Scissor Stacks

`FrameBufferInterface` maintains separate stacks for viewport and scissor:

```cpp
// ork.lev2/src/gfx/context/fbi.cpp

void FrameBufferInterface::PushRtGroup(RtGroup* rtg) {
  _pushRtGroup(rtg);

  int iw = rtg ? rtg->width() : _target.mainSurfaceWidth();
  int ih = rtg ? rtg->height() : _target.mainSurfaceHeight();

  pushScissor(0, 0, iw, ih);
  pushViewport(0, 0, iw, ih);
}

void FrameBufferInterface::PopRtGroup() {
  _popRtGroup();
  popViewport();
  popScissor();
}
```

**Note**: `PushRtGroup` correctly sets viewport/scissor to rtgroup dimensions. The issue is that `PushUIMatrix()` may ignore these and use compositor CPD instead.

---

## Summary: When to Use Each Pattern

| Scenario | RCFD State | PushUIMatrix Result | Correct? |
|----------|-----------|---------------------|----------|
| Direct window rendering | No compositor | Viewport dimensions | ✓ |
| Compositor (full window UI) | Has compositor | CPD dst rect (window) | ✓ |
| RTT during compositor (broken) | Has compositor | CPD dst rect (window) | ✗ |
| RTT with isolated RCFD (fixed) | Isolated (no compositor) | Viewport dimensions | ✓ |
| Explicit dimensions | Any | Provided dimensions | ✓ |

---

## Best Practices

### For Render-to-Texture UI

**Always isolate the RCFD when rendering UI to a texture during compositor rendering**:

```cpp
// Push isolated RCFD
auto rcfd = std::make_shared<lev2::RenderContextFrameData>(ctx);
ctx->pushRenderContextFrameData(rcfd);

// Render to texture
fbi->PushRtGroup(myRtGroup.get());
{
  // UI rendering here - PushUIMatrix() will use rtgroup dimensions
  renderUI(drwev);
}
fbi->PopRtGroup();

ctx->popRenderContextFrameData();
```

### For Widgets Using PushUIMatrix

If you know the exact dimensions needed, use the explicit overload:

```cpp
// Explicit dimensions - ignores compositor override
mtxi->PushUIMatrix(virtualWidth, virtualHeight);
```

### For Container Groups (LayoutGroup::DoDraw)

LayoutGroup already uses explicit dimensions:

```cpp
// ork.lev2/src/ui/widgets/group.cpp
void LayoutGroup::DoDraw(ui::drawevent_constptr_t drwev) {
  // ...
  mtxi->PushUIMatrix(_geometry._w, _geometry._h);  // Explicit!
  // Children render here
  mtxi->PopUIMatrix();
}
```

However, child widgets may call `PushUIMatrix()` without args, which can cause issues in nested scenarios.

---

## Related Files

- `ork.lev2/src/gfx/context/mtxi.cpp` - Matrix stack implementation, PushUIMatrix
- `ork.lev2/src/gfx/context/fbi.cpp` - FrameBuffer interface, viewport/scissor stacks
- `ork.lev2/src/ui/widgets/widget.cpp` - LocalToRoot, coordinate transforms
- `ork.lev2/src/ui/widgets/layoutsurface.cpp` - LayoutSurface with isolated RCFD fix
- `ork.lev2/src/ui/widgets/group.cpp` - LayoutGroup rendering with explicit dimensions
- `ork.lev2/src/gfx/scenegraph/sgnode_uisurface.cpp` - 3D UI surface embedding

---

## Historical Context

This document was created after debugging an issue where UI content rendered to a LayoutSurface (for 3D billboard embedding) appeared cramped in a corner. The root cause was the compositor override in `PushUIMatrix()` using window dimensions instead of rtgroup dimensions during scene graph rendering.

The fix (pushing an isolated RCFD without compositor) ensures `PushUIMatrix()` uses the correct viewport dimensions when rendering UI to off-screen textures.
