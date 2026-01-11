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

---

# UI Event Routing System

This section documents the complete event routing architecture for the Orkid UI system.

---

## Overview

The orkid UI event routing is a **four-layer system**:

1. **Routing Layer** (`routeUiEvent` + `doRouteUiEvent`) - Finds the target widget
2. **Filtering Layer** (`IWidgetEventFilter`) - Transforms/validates events
3. **Handler Layer** (`OnUiEvent` + `_evhandler`) - Dispatches to handler
4. **Implementation Layer** (`DoOnUiEvent`) - Widget-specific behavior

---

## Entry Points

### Primary Entry Point: `Event::sendToContext()`

Location: `ork.lev2/src/ui/event.cpp:7-12`

Called when an OS event needs to be routed through the UI system. Delegates to `Context::handleEvent()` via the event's `_uicontext` pointer.

### Secondary Entry Point: `EzTopWidget::DoOnUiEvent()`

Location: `ork.lev2/src/ezapp_topwidget.cpp:179-189`

The top-level widget in the EzApp framework. Wraps raw events and forwards them to application-level handlers via `_mainwin->_onUiEvent`.

---

## Complete Event Flow Diagram

```
OS Event
   ↓
Event::sendToContext(ev)
   ↓
Context::handleEvent(ev)
   ├─→ [Special event handling + state updates]
   ├─→ _top->routeUiEvent(ev)  [Widget tree routing]
   │   └─→ Widget::routeUiEvent(ev)
   │       ├─→ Check _evrouter lambda (if set)
   │       └─→ Call doRouteUiEvent(ev) (virtual method)
   │
   └─→ targetWidget->OnUiEvent(ev)  [Handler invocation]
       └─→ Widget::OnUiEvent(ev)
           ├─→ Apply event filters (_eventfilterstack)
           ├─→ Call _evhandler lambda (if set) ← RETURNS HERE, DoOnUiEvent NOT CALLED
           └─→ Call DoOnUiEvent(ev) (virtual method) ← Only if _evhandler is null
```

---

## Context-Level Routing: `Context::handleEvent()`

Location: `ork.lev2/src/ui/context.cpp:33-186`

This is the **central orchestration point** for all event routing.

### Special Event Type Handling

| Event Type | Behavior | Purpose |
|------------|----------|---------|
| `KEY_DOWN` / `KEY_UP` | Routes via `_top->routeUiEvent()` | Keyboard input |
| `PUSH` (Mouse Press) | Synthesizes DOUBLECLICK if timing qualifies | Mouse interaction |
| `DRAG` | Routes to `_evdragtarget` (widget where drag started) | Persistent drag tracking |
| `MOVE` (Mouse Motion) | Synthesizes MOUSE_LEAVE/MOUSE_ENTER on focus change | Mouse tracking |
| `RELEASE` (Mouse Release) | Routes to `_evdragtarget`; synthesizes END_DRAG | Drag completion |
| `GOT_KEYFOCUS` / `LOST_KEYFOCUS` | Sets `_hasKeyboardFocus` state | Keyboard focus |
| Default | Routes via `_top->routeUiEvent()` | Standard case |

### Drag State Management

```cpp
Widget* _evdragtarget = nullptr;  // Widget where drag started
```

- When drag begins: widget is captured for all subsequent DRAG events
- Synthetic events: `BEGIN_DRAG`, `DRAG`, `END_DRAG`
- Key behavior: Once drag starts, target is "locked in" regardless of mouse position

### Mouse Focus Tracking

```cpp
const Widget* _mousefocuswidget = nullptr;
```

- Tracks current widget under mouse
- Synthesizes `MOUSE_LEAVE` when leaving a widget
- Synthesizes `MOUSE_ENTER` when entering a widget

### Double-Click Detection

- Qualifies DOUBLECLICK if `dblclickdelta > 0.75s` AND `clickdelta < 0.5s`
- Converts raw PUSH event to DOUBLECLICK event

---

## The Four-Layer Routing System

### Layer 1: `routeUiEvent()` - Public Router

Location: `widget.cpp:75-83`

```cpp
Widget* Widget::routeUiEvent(event_constptr_t ev) {
  auto ret = _evrouter ? _evrouter(ev)        // Lambda takes preference
                       : doRouteUiEvent(ev);  // Fall back to virtual method
  return ret;
}
```

**Key Feature:** Lambda `_evrouter` allows dynamic routing override without subclassing.

### Layer 2: `doRouteUiEvent()` - Virtual Router

Location: `widget.cpp:85-92`

Base implementation for leaf widgets:
```cpp
Widget* Widget::doRouteUiEvent(event_constptr_t ev) {
  if(_ignoreEvents) return nullptr;
  bool inside = IsEventInside(ev);
  return inside ? this : nullptr;
}
```

Container widgets override this to route to children.

### Layer 3: `OnUiEvent()` - Public Handler

Location: `widget.cpp:98-117`

```cpp
HandlerResult Widget::OnUiEvent(event_constptr_t ev) {
  // 1. Reset filtered event
  ev->mFilteredEvent.Reset();

  // 2. Apply event filters
  if (_eventfilterstack.size()) {
    auto top = _eventfilterstack.top();
    top->Filter(ev);
    if (ev->mFilteredEvent._eventcode == EventCode::UNKNOWN)
      return HandlerResult();  // Filter consumed event
  }

  // 3. CRITICAL: Handler precedence
  if (_evhandler) {
    return _evhandler(ev);  // Lambda takes precedence - RETURNS HERE!
  }
  return DoOnUiEvent(ev);  // Only called if _evhandler is null
}
```

**IMPORTANT:** When `_evhandler` is set, `DoOnUiEvent()` is **never called**.

### Layer 4: `DoOnUiEvent()` - Virtual Handler

Location: `widget.cpp:272-274`

Base implementation returns empty result:
```cpp
HandlerResult Widget::DoOnUiEvent(event_constptr_t Ev) {
  return HandlerResult();
}
```

Subclasses override for widget-specific behavior (Button, Slider, etc.).

---

## Lambda vs Virtual Method Interaction

### Type Definitions

From `ork.lev2/inc/ork/lev2/ui/ui.h:89-90`:

```cpp
using evrouter_t  = std::function<Widget*(event_constptr_t ev)>;
using evhandler_t = std::function<HandlerResult(event_constptr_t ev)>;
```

### Member Variables

```cpp
struct Widget {
  evrouter_t _evrouter   = nullptr;   // Replaces doRouteUiEvent() if set
  evhandler_t _evhandler = nullptr;   // Replaces DoOnUiEvent() if set
};
```

### Precedence Rules

**For Routing:**
1. If `_evrouter` is set → use it
2. Otherwise → call `doRouteUiEvent()` (virtual)

**For Handling:**
1. Apply event filters first
2. If `_evhandler` is set → use it and **return** (DoOnUiEvent NOT called)
3. Otherwise → call `DoOnUiEvent()` (virtual)

---

## Container Widget Routing

### Group/LayoutGroup

Location: `group.cpp:134-159` (Group), `group.cpp:874-925` (LayoutGroup)

Group iterates children in forward order (first child = bottom layer):
```cpp
Widget* Group::doRouteUiEvent(event_constptr_t ev) {
  for (auto& child : _children) {
    if (child->IsEventInside(ev)) {
      auto child_target = child->routeUiEvent(ev);
      if (child_target && !child_target->_ignoreEvents) {
        return child_target;
      }
    }
  }
  return IsEventInside(ev) ? this : nullptr;
}
```

LayoutGroup iterates in **reverse order** (last added = top layer, highest priority):
```cpp
for (size_t i = num_children-1; i >= 0; i--) {
  // ... check children in reverse
}
```

### Panel - Single Child Container

Location: `panel.cpp:152-162`

Routes to child only if `mPanelUiState == 0`:
```cpp
Widget* Panel::doRouteUiEvent(event_constptr_t ev) {
  if (_child && _child->IsEventInside(ev) && mPanelUiState == 0) {
    return _child->routeUiEvent(ev);
  }
  return IsEventInside(ev) ? this : nullptr;
}
```

### Splitters

Location: `split.cpp:73-95`

Routes based on position relative to split ratio:
```cpp
Widget* HorizontalSplit::doRouteUiEvent(event_constptr_t ev) {
  int split_x = int(_geometry._w * _split_ratio);
  if (localX < split_x && _children.size() > 0) {
    return _children[0]->doRouteUiEvent(ev);  // Left child
  }
  if (localX >= split_x && _children.size() > 1) {
    return _children[1]->doRouteUiEvent(ev);  // Right child
  }
  return nullptr;
}
```

**Note:** Calls `doRouteUiEvent()` directly, bypassing `_evrouter` lambda.

### TabWidget

Location: `tabs.cpp:161-189`

- Tab bar area routes to self (for tab switching)
- Content area routes to active tab only

---

## Event Filtering System

Location: `widget.cpp:44-75`

### Filter Stack

```cpp
std::stack<eventfilter_ptr_t> _eventfilterstack;

template <typename T> std::shared_ptr<T> pushEventFilter();
void popEventFilter();
```

### Filter Application

```cpp
if (_eventfilterstack.size()) {
  auto top = _eventfilterstack.top();
  top->Filter(ev);
  if (ev->mFilteredEvent._eventcode == EventCode::UNKNOWN)
    return HandlerResult();  // Filter consumed/rejected event
}
```

### Filter Types

1. **NopEventFilter** - Pass-through filter
2. **Apple3ButtonMouseEmulationFilter** - Converts keyboard (z,x,c) to mouse buttons

---

## EzApp Integration

### EzTopWidget

Location: `ork.lev2/src/ezapp_topwidget.cpp:18-27`

```cpp
EzTopWidget::EzTopWidget(EzMainWin* mainwin)
    : ui::Group("ezviewport", 1, 1, 1, 1)
    , _mainwin(mainwin) {
}

ui::HandlerResult EzTopWidget::DoOnUiEvent(ui::event_constptr_t ev) {
  if (_mainwin->_onUiEvent) {
    return _mainwin->_onUiEvent(ev);
  }
  return ui::HandlerResult();
}
```

### Entry Point Chain

```
OS Window → EzMainWin → Event::sendToContext()
  → Context::handleEvent()
  → _top->routeUiEvent() (top = EzTopWidget)
  → EzTopWidget::doRouteUiEvent() [Group's implementation]
    → Routes to child widgets
  → Target widget's OnUiEvent()
```

---

## SceneGraphViewport Camera Event Flow

### The Standard Pattern

From `obt.project/scripts/ork/app/std_scenegraph.py`:

```python
# In _onGpuLink():
SGVPW.evhandler = lambda x: self._onCameraUiEvent(x)

# Handler method:
def _onCameraUiEvent(self, uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
        self.uicam.updateMatrices()
        self.camera.copyFrom(self.uicam.cameradata)
    return lev2.ui.HandlerResult()
```

### The Problem for UI Surface Routing

When `_evhandler` is set (for camera controls):
1. Events reach `SceneGraphViewport::OnUiEvent()`
2. `_evhandler` is set → camera lambda is called
3. `DoOnUiEvent()` is **never reached**
4. Any UI surface routing in `DoOnUiEvent()` is bypassed

### Solution Options

1. **Python-level interception**: Modify `_onCameraUiEvent` to first check UI surfaces
2. **C++ interceptor**: Override `evhandler` property to wrap with interceptor
3. **Widget base class change**: Modify `OnUiEvent` to call `DoOnUiEvent` first

---

## Coordinate Systems

### Transformation Methods

```cpp
void LocalToRoot(int lx, int ly, int& rx, int& ry) const;
void RootToLocal(int rx, int ry, int& lx, int& ly) const;
```

### Hit Testing

```cpp
bool Widget::IsEventInside(event_constptr_t Ev) const {
  if (not _clipEvents) return true;  // Unclamped mode

  int ix, iy;
  RootToLocal(Ev->miX, Ev->miY, ix, iy);

  auto ngeo = _geometry;
  ngeo._x = 0; ngeo._y = 0;
  return ngeo.isPointInside(ix, iy);
}
```

---

## Virtual Methods Summary

| Method | Signature | Purpose | When Called |
|--------|-----------|---------|-------------|
| `doRouteUiEvent()` | `Widget* (event_constptr_t)` | Find target widget | Routing phase |
| `DoOnUiEvent()` | `HandlerResult (event_constptr_t)` | Handle event | Handler phase (if no `_evhandler`) |
| `DoFilter()` | `void (event_constptr_t)` | Transform event | Before handler |

---

## Handler Result

```cpp
struct HandlerResult {
  Widget* mHandler = nullptr;  // Widget that handled the event

  bool wasHandled() const { return mHandler != nullptr; }
  void setHandled(Widget* w) { mHandler = w; }
};
```

Events are considered "consumed" when `mHandler != nullptr`.

---

## Related Files

- `ork.lev2/src/ui/context.cpp` - Central event orchestration
- `ork.lev2/src/ui/event.cpp` - Event class, sendToContext
- `ork.lev2/src/ui/widgets/widget.cpp` - Base widget routing/handling
- `ork.lev2/src/ui/widgets/group.cpp` - Container widget routing
- `ork.lev2/src/ui/widgets/viewport_scenegraph.cpp` - SceneGraphViewport
- `ork.lev2/src/ezapp_topwidget.cpp` - EzApp top-level widget
- `ork.lev2/inc/ork/lev2/ui/widget.h` - Widget class definition
- `ork.lev2/inc/ork/lev2/ui/ui.h` - Type definitions (evrouter_t, evhandler_t)
