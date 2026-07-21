---
name: orklev2-ui
description: Answer questions about orkid's UI widget system, layout, event handling, overlays, property sheets, outliners, toolbars, PrimCanvas, composite widgets, theme engine, and Python UI bindings. Use when the user asks about UI widgets, layout, events, property editors, PrimCanvas drawing, or the overlay system.
user-invocable: false
---

# Orkid Lev2 UI Widget System Reference

When answering questions about the UI system in orkid, consult the files below. All under `ork.lev2/`.

## Key Files

| Component | Header |
|-----------|--------|
| Widget (base) | `inc/ork/lev2/ui/widget.h` |
| Group | `inc/ork/lev2/ui/group.h` |
| LayoutGroup | `inc/ork/lev2/ui/layoutgroup.inl` |
| Anchor/Guide Layout | `inc/ork/lev2/ui/anchor.h` |
| Event | `inc/ork/lev2/ui/event.h` |
| Context & Overlays | `inc/ork/lev2/ui/context.h` |
| Style & Themes | `inc/ork/lev2/ui/style.h` |
| Pack Widgets | `inc/ork/lev2/ui/pack.h` |
| Outliner | `inc/ork/lev2/ui/outliner.h` |
| PropertySheet | `inc/ork/lev2/ui/property_sheet.h` |
| Toolbar | `inc/ork/lev2/ui/toolbar.h` |
| LineEdit | `inc/ork/lev2/ui/lineedit.h` |
| Checkbox | `inc/ork/lev2/ui/checkbox.h` |
| Tabs | `inc/ork/lev2/ui/tabs.h` |
| DockablePanel | `inc/ork/lev2/ui/dockable_panel.h` |
| BorderFrame | `inc/ork/lev2/ui/border_frame.h` |
| ScrollContainer | `inc/ork/lev2/ui/scroll_container.h` |
| ComboBox | `inc/ork/lev2/ui/combobox.h` |
| ChoicelistWidget | `inc/ork/lev2/ui/choicelist_widget.h` |
| DropdownMenu | `inc/ork/lev2/ui/dropdown_menu.h` |
| OverlayLineEdit | `inc/ork/lev2/ui/overlay_lineedit.h` |
| FilesystemView | `inc/ork/lev2/ui/filesystem_view.h` |
| FilesystemModel | `inc/ork/lev2/ui/filesystem_model.h` |
| PrimCanvas | `inc/ork/lev2/ui/prim_canvas.h` |
| GraphView | `inc/ork/lev2/ui/graphview.h` |

**Python Bindings:**
| File | Content |
|------|---------|
| `pyext/src/pyext_ui.cpp` | Main UI module (Widget, Group, Pack, LineEdit, etc.) |
| `pyext/src/pyext_ui_toolbar.cpp` | Toolbar and ToolbarButton |
| `pyext/src/pyext_ui_property_sheet.cpp` | PropertySheet and models |
| `pyext/src/pyext_ui_outliner.cpp` | Outliner and OutlinerModel |
| `pyext/src/pyext_ui_filesystem.cpp` | FilesystemView and model |
| `pyext/src/pyext_ui_layout.cpp` | LayoutGroup bindings |

**Examples:**
| File | Shows |
|------|-------|
| `pyext/tests/ui/property_sheet.py` | PropertySheet + VarMap + custom editors |
| `pyext/tests/ui/outliner_model.py` | OutlinerModel subclass (add/rename/delete) |
| `pyext/tests/ui/widget_pack.py` | VPack/HPack layout, LineEdit, Slider |
| `pyext/tests/ui/overlay_dropdown_test.py` | DropdownMenu overlay |

## Architecture Overview

### Widget Hierarchy
```
Widget (base: geometry, events, drawing)
+-- Group (child management)
|   +-- LayoutGroup (anchor-based constraints)
|   +-- VerticalPack / HorizontalPack (auto-stacking)
|   +-- DockablePanel / BorderFrame / ScrollContainer
|   +-- Toolbar
+-- Outliner (tree view with OutlinerModel)
+-- PropertySheet (form editor with PropertySheetModel)
+-- LineEdit, Checkbox, Button, Slider, ComboBox, etc.
```

### Event System
- `EventCode`: PUSH, RELEASE, MOVE, DRAG, KEY_DOWN, KEY_UP, MOUSEWHEEL, etc.
- Routing: `routeUiEvent()` -> `doRouteUiEvent()` (finds target) -> `OnUiEvent()` -> `DoOnUiEvent()`
- `HandlerResult` — indicates which widget handled the event

### Overlay System
```python
uicontext.pushOverlay(widget, x, y, w, h, dismiss_on_click_outside=True)
uicontext.popOverlay()
uicontext.dismissAllOverlays()
uicontext.createOverlayWidget(uiclass, args)  # Sets _uicontext for proper event routing
```
- Overlays receive events before the widget tree
- Support stacking (overlay on overlay)
- `createOverlayWidget` ensures `_uicontext` propagation to children

### Python Widget Creation Pattern
```python
from orkengine import lev2

# Via layout group (preferred)
item = layout_group.makeChild(uiclass=lev2.ui.VerticalPack, args=["name"])
widget = item.widget

# Via wfactory (standalone, for overlays)
vpack = lev2.ui.VerticalPack.wfactory(["name"])

# Children
child = vpack.makeChild(uiclass=lev2.ui.Toolbar, args=["toolbar"])
vpack.fill_widget = child  # Fill remaining space
```

### PropertySheet + VarMap
```python
# Set data
vm = core.VarMap()
vm.name = "hello"
vm.value = 42.0
propsheet.data = vm

# Annotations for custom editors
model = propsheet.model
annot = core.VarMap()
annot.type = tokens.Color
model.setAnnotations("diffuse", annot)

# Change callback
propsheet.onPropertyChanged(lambda key, value: ...)

# Custom inline editor factory
propsheet.registerEditorFactory(tokens.MyType, my_factory_fn)
```

### Dropdown / Choice Widgets

Three dropdown/selection widgets, each suited for different contexts:

**ComboBox** — cyclic selector with +/- buttons, for fixed choices in layout packs:
```python
combo = parent.makeChild(uiclass=lev2.ui.ComboBox, args=["name", vec3(0.15, 0.15, 0.2)])
combo.setItems(["option1", "option2", "option3"])
combo.onSelectionChanged = lambda w: print(w.selectedItem())
combo.selected_index = 0
```

**ChoicelistWidget** — shows current value, opens dropdown on click. Best for PropertySheet custom editors:
```python
widget = lev2.ui.ChoicelistWidget("name", current_value)
widget.setChoices(["${VAR_A}", "${VAR_B}", "${VAR_C}"])
widget.onChoiceSelected = lambda selected: print(selected)
```

**DropdownMenu** — hierarchical overlay menu with slash-delimited paths. Best for dynamic popups:
```python
# Flat list
paths = ["/Option A", "/Option B", "/Option C"]
# Hierarchical
paths = ["/Group1/Sub A", "/Group1/Sub B", "/Group2/Sub C"]

rx, ry = widget.localToRoot(0, widget.height)
lev2.ui.DropdownMenu.show(
    context=uicontext,
    paths=paths,
    x=rx, y=ry,
    on_selected=lambda val: print(val.lstrip("/")),
    sort_alphabetically=True)
```

| Widget | Header | Python Class | Best For |
|--------|--------|-------------|----------|
| ComboBox | `combobox.h` | `lev2.ui.ComboBox` | Fixed choices in layouts |
| ChoicelistWidget | `choicelist_widget.h` | `lev2.ui.ChoicelistWidget` | PropertySheet custom editors |
| DropdownMenu | `dropdown_menu.h` | `lev2.ui.DropdownMenu` | Overlay popup menus |

### OutlinerModel (subclass in Python)
```python
class MyModel(lev2.ui.OutlinerModel):
    def __init__(self):
        super().__init__()
        self.allow_rename = True
        self.allow_delete = True
        self.allow_add = True
    def getChildren(self, parent_key): ...
    def getDisplayName(self, key): ...
    def hasChildren(self, key): ...
    def getFactories(self, parent_key): ...
    def createItem(self, parent_key, name, factory_id): ...
    def renameItem(self, old_key, new_name): ...
    def removeItem(self, key): ...
```

### Theme Engine
- `StyleDatabase` with parent/child inheritance
- `Style` struct: colors, border, corner radius, font, icons
- `ThemeEngine` — SDF-based rendering of boxes, tabs, circles, text
- Factory: `createDefaultStyleDatabase()`

### PrimCanvas (GPU-Rendered Drawing Surface)
Header: `inc/ork/lev2/ui/prim_canvas.h`, Bindings: `pyext/src/pyext_ui.cpp` (lines 2834-3170)

Extends `Surface`. Provides layer-based GPU drawing with SSBO-backed primitives.

**Layers and Primitives:**
```python
canvas.gpuInit(ctx)                          # Must call once
layer = canvas.createLayer("main")

# QuadPrimitive — batched rectangles
qp = lev2.ui.QuadPrimitive(pipeline=canvas.pipelineSolid)
qd = lev2.ui.QuadData()
qd.setPosition(x, y_from_top)               # Note: y is from TOP of canvas
qd.setSize(w, h)
qd.setColor(vec4(r, g, b, a))
qp.addQuad(qd)
layer.addPrimitive(qp)

# TextPrimitive — batched text items
tp = lev2.ui.TextPrimitive(font=my_font, color=vec4(1,1,1,1))
tp.addItem("Hello", vec2(x, y))
tp.clearItems()                              # Clear for redraw
layer.addPrimitive(tp)

# SpritePrimitive + SpriteInstance — instanced sprites
sprite = lev2.ui.SpritePrimitive(canvas.pipelineSpriteSolid, texture)
inst = lev2.ui.SpriteInstance(sprite)
inst.setPosition(x, y)
inst.setRotation(radians)
inst.tint = vec4(1, 0.5, 0.5, 1)
layer.addPrimitive(inst)

# TriListPrimitive / TriStripPrimitive — custom geometry
tri = lev2.ui.TriListPrimitive(canvas.pipelineVtxSolid)
vd = lev2.ui.VertexData(); vd.setPosition(x,y); vd.setColor(c)
tri.addVertex(vd)
```

**Pipelines:** `pipelineSolid`, `pipelineTextured`, `pipelineVtxSolid`, `pipelineVtxTextured`, `pipelineSpriteSolid`, `pipelineSpriteTextured`

**Properties:** `bg_color`, `draw_background`, `supersample` (0-6), `desiredWidth/Height`

**Callbacks:** `canvas.onUiEvent = fn`, `canvas.onPreRender = fn`

**Coordinate note:** the whole canvas is top-left-origin / Y-down since `9359c9e20` (2026-04-16, y-flip removal). `QuadData.setPosition(x, y)` places the quad's TOP-LEFT at canvas pixel (x, y) directly — no flip, no `canvas.height - y` compensation. TriList/TriStrip `VertexData`, `TextPrimitive` item positions, per-layer `_transform` translations, and mouse coords from `rootToLocal` all share this same Y-down pixel space. Any `h - y` flip in canvas code is a pre-2026-04-16 leftover and is a bug (migration precedent: `catalog_tool_ui.py` in `1ec7a6f4d`).

**Event handling with button regions (ork.catalog.tool.py pattern):**
```python
self.buttons = {}  # name -> (x, y, w, h)
# In redraw: register clickable regions
self.buttons["FETCH"] = (bx, by, bw, bh)
# In event handler: hit-test
mx, my = canvas.rootToLocal(ev.x, ev.y)
for name, (bx, by, bw, bh) in self.buttons.items():
    if bx <= mx < bx+bw and by <= my < by+bh:
        handle_button(name)
```

### Composite Python Widgets
Located in `obt.project/scripts/ork/ui/`. All have `uifactory` and `wfactory` patterns.

| Widget | File | Purpose |
|--------|------|---------|
| FilesystemBrowser | `obt.project/scripts/ork/ui/filesystem_browser.py` | Full file browser: toolbar, filter, favorites, list/icon view |
| ColorPicker | `obt.project/scripts/ork/ui/color_picker.py` | RGB sliders + ColorEdit + commit/cancel |
| TransformEdit | `obt.project/scripts/ork/ui/transform_edit.py` | Position/rotation/scale editor with F32Edit widgets |
| AnalogClock | `obt.project/scripts/ork/ui/analog_clock.py` | PrimCanvas-based analog clock |
| icon_library | `obt.project/scripts/ork/ui/icon_library.py` | SVG-to-image factory with caching |
| standard_icons | `obt.project/scripts/ork/ui/standard_icons.py` | SVG icon definitions |

**FilesystemBrowser usage:**
```python
from ork.ui.filesystem_browser import FilesystemBrowser
browser_item = root.makeChild(
  uiclass=FilesystemBrowser,
  args=["browser", initial_path, ".json", vec3(0.1,0.1,0.1), "load"], fill=True)
browser = browser_item.widget.uservars.filesystem_browser
browser.model.directories_only = True  # Folders only mode
browser.onActivate = lambda path: handle(path)
browser.onCancel = lambda: close()
```

**ColorPicker usage (as PropertySheet detail editor):**
```python
from ork.ui.color_picker import ColorPicker
picker_widget = ColorPicker.wfactory(["picker", bg_color, initial_color])
picker = picker_widget.uservars.color_picker
picker.onCommit = lambda color: commit(color)
picker.onCancel = lambda: cancel()
```

## Example Code Index

### `ork.lev2/pyext/tests/ui/` — Working Examples by Category

**Layout & Packing:**
- `widget_pack.py` — comprehensive widget showcase (VPack/HPack, LineEdit, Slider, Checkbox)
- `fixed_pack.py` — fixed_width/fixed_height sizing constraints
- `layouttest_grid.py` — grid-based layout with rows/columns
- `layouttest_split.py` — draggable split panes
- `scrollcontainer_vertical.py` — vertical scroll with clipped content
- `scrollcontainer_split.py` — split layout with Y, X, and XY scroll modes

**Tree & Property Editing:**
- `outliner.py` — tree view with VarMap data
- `outliner_model.py` — custom Python OutlinerModel (add/delete/rename/factories)
- `property_sheet.py` — PropertySheet with VarMap, ColorSwatch inline editor, annotations

**Toolbar & Controls:**
- `toolbar.py` — image buttons with icons and separators

**Theming & SDF:**
- `themes.py` — full theming system with SDF-rendered components
- `sdf_prims.py` — all 7 SDF primitives (box, tab, circle, triangle, ring, pause, per-corner)

**PrimCanvas (GPU Drawing):**
- `prim_canvas_quads.py` — SSBO quad rendering basics
- `prim_canvas_text.py` — animated text with color variations
- `prim_canvas_trilist.py` — flow field with triangle primitives
- `prim_canvas_tristrip.py` — waveform/oscilloscope via triangle strips
- `prim_canvas_sprites.py` — sprite stress test (transforms, tints, animation)
- `prim_canvas_layers.py` — parallax scrolling with multi-layer transforms
- `prim_canvas_clock.py` — analog clock widget
- `prim_canvas_invaders.py` — Space Invaders game (keyboard input, pixel-art sprites)
- `prim_canvas_pacman.py` — Pac-Man (grid movement, maze collision)

**Overlays & Dropdowns:**
- `overlay_dropdown_test.py` — hierarchical dropdown menus via overlay system

**Secondary Windows:**
- `secondary_window.py` — multi-window app with UI on both windows

**SceneGraph + UI Integration:**
- `sgui_single.py` — single SceneGraphViewport in layout
- `sgui_quad_multi.py` — multiple scenegraph views in split layout
- `sgui_emb_sg.py` — SceneGraphViewport embedded in UI surface
- `sgui_secondary_window.py` — multi-window with themed viewport + PropertySheet
- `sgui_emb_mplib.py` — matplotlib embedded as billboard in 3D scene

**File System:**
- `filesystem.py` — FilesystemBrowser composite widget
- `filesystem_model.py` — virtual filesystem model for custom data sources
- `folder_dialog.py` / `open_dialog.py` / `save_dialog.py` — native file dialogs

**Other:**
- `graphview.py` — ring-buffer multi-series graph with real-time data
- `dynagrid.py` — smart dynamic grid (1-12 items, aspect constraints)
- `page_widget_example.py` — TabWidget in page mode (hidden tabs)
- `ged_minimal.py` — minimal generic object editor via reflection
- `manip.py` — 3D transform gizmos (translate/rotate/scale)

### `obt.project/scripts/ork/editor/` — Production Editor Code

**ecsedit.py** — Full ECS scene editor. Key patterns to reference:
- Outliner + PropertySheet wiring with `OutlinerModel` subclass (`ecs_outliner_model.py`)
- PropertySheet with custom widget factories and `onCreateWidgetEditor`
- Overlay system for curve editors (`pushOverlay`/`popOverlay`)
- Secondary windows for file browser (`createSecondaryWindow` + `FilesystemBrowser`)
- Toolbar with text/image buttons, keyboard shortcuts
- SceneGraphViewport embedding with EzUiCam
- GPU init / update / render lifecycle
- Extension hooks via `_extensions` list

**Supporting editor files:**
- `ecs_outliner_model.py` — complex OutlinerModel: Archetypes/Spawners/Systems categories, factories, rename/delete
- `scene_editor_base.py` — base class for editor modes
- `light_editor.py` — light property editing
- `ptc_factories.py` — PropertySheet custom editor factories
- `scene_io.py` — scene load/save via FilesystemBrowser

## How to Answer

1. For widget API: read the header + check Python bindings in `pyext_ui*.cpp`
2. For layout: read `anchor.h` for guide/constraint system, `pack.h` for auto-stacking
3. For PropertySheet: read `property_sheet.h` and `pyext_ui_property_sheet.cpp`
4. **For working examples: check `pyext/tests/ui/` by category above**
5. **For production patterns: read `obt.project/scripts/ork/editor/ecsedit.py`**
6. For GIL safety in callbacks: use `python::gil_safe_pyobj` pattern
