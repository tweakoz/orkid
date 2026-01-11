# Multi-Window Implementation Plan

## Goal
Add support for secondary GLFW windows while maintaining full backward compatibility with:
- Existing single-window applications
- DRM fullscreen mode (Linux)
- All existing UI widgets and event handling

## Guiding Principles

1. **Additive Changes Only** - Each phase adds new code without modifying existing behavior
2. **Feature Flags** - New functionality is opt-in, not opt-out
3. **Test After Each Phase** - Run existing demos/tests to verify no regressions
4. **Rollback Points** - Each phase can be reverted independently

---

## Pre-Implementation: Baseline Testing

Before starting, establish baseline by running:
```bash
# Core rendering tests
ork.lev2/pyext/tests/renderer/lighting/probe.py

# UI tests (if any exist)
# List other key test scripts here
```

Document current behavior for comparison.

---

## Phase 1: Code Analysis & Documentation (No Code Changes)

### 1.1 Audit Existing PopupWindow Implementation
**Files to study:**
- `ork.lev2/src/glfw/ctx_glfw.cpp` lines 1077-1330 (PopupImpl)
- Pattern: Creates own CtxGLFW, own ui::Context, own GLFWwindow

### 1.2 Document Current Window Ownership
```
Current:
  OrkEzApp
    └── _mainWindow (EzMainWin)
          ├── _appwin (AppWindow - orkid Window abstraction)
          ├── _ctqt (CTXBASE* - actually CtxGLFW or CtxDRM)
          └── uses app's _uicontext

  CtxGLFW
    ├── _glfwWindow (GLFWwindow*)
    ├── _target (Context* - VkContext)
    ├── _orkwindow (Window* - back-pointer)
    └── _eventSINK (routes events)
```

### 1.3 Verify VkContext Device Sharing
Confirm in `vulkan_ctx.cpp:220-267` that second VkContext reuses VkDevice.

**Checkpoint 1:** No code changes. Understanding documented. All tests pass.

---

## Phase 2: Add EzSecondaryWin Struct (Additive Only)

### 2.1 Create New Header
**File:** `ork.lev2/inc/ork/lev2/ez_secondary_win.h`

```cpp
#pragma once
#include <ork/lev2/lev2_types.h>
#include <ork/lev2/ui/context.h>
#include <ork/lev2/ui/group.h>

namespace ork::lev2 {

struct EzSecondaryWin;
using ezsecondarywin_ptr_t = std::shared_ptr<EzSecondaryWin>;

struct EzSecondaryWinConfig {
  int _width = 640;
  int _height = 480;
  int _x = 100;
  int _y = 100;
  std::string _title = "Secondary Window";
  bool _decorated = true;
  bool _resizable = true;
};

struct EzSecondaryWin {
  // Callbacks (user-provided)
  using draw_cb_t = std::function<void(ui::drawevent_constptr_t)>;
  using resize_cb_t = std::function<void(int w, int h)>;
  using uievent_cb_t = std::function<ui::HandlerResult(ui::event_constptr_t)>;
  using gpuinit_cb_t = std::function<void(Context* ctx)>;

  draw_cb_t _onDraw;
  resize_cb_t _onResize;
  uievent_cb_t _onUiEvent;
  gpuinit_cb_t _onGpuInit;

  // State queries
  bool shouldClose() const;
  void requestClose();
  int width() const;
  int height() const;

  // Access
  ui::Context* uiContext();
  lev2::Context* gfxContext();

  // Internal (constructed by factory only)
  EzSecondaryWin(const EzSecondaryWinConfig& config);
  ~EzSecondaryWin();

  void _render();  // Called by main loop
  void _handleResize(int w, int h);

private:
  friend struct OrkEzApp;
  friend struct SecondaryWinImpl;

  svar64_t _impl;
  ui::context_ptr_t _uicontext;
  bool _shouldClose = false;
  bool _gpuInitialized = false;
};

} // namespace ork::lev2
```

### 2.2 Create Stub Implementation
**File:** `ork.lev2/src/ez_secondary_win.cpp`

```cpp
#include <ork/lev2/ez_secondary_win.h>

namespace ork::lev2 {

EzSecondaryWin::EzSecondaryWin(const EzSecondaryWinConfig& config) {
  _uicontext = std::make_shared<ui::Context>();
  // Impl created in Phase 3
}

EzSecondaryWin::~EzSecondaryWin() = default;

bool EzSecondaryWin::shouldClose() const { return _shouldClose; }
void EzSecondaryWin::requestClose() { _shouldClose = true; }
int EzSecondaryWin::width() const { return 0; }  // Stub
int EzSecondaryWin::height() const { return 0; } // Stub
ui::Context* EzSecondaryWin::uiContext() { return _uicontext.get(); }
lev2::Context* EzSecondaryWin::gfxContext() { return nullptr; } // Stub
void EzSecondaryWin::_render() {} // Stub
void EzSecondaryWin::_handleResize(int w, int h) {} // Stub

} // namespace ork::lev2
```

### 2.3 Add to Build
**File:** `ork.lev2/CMakeLists.txt`
- Add `src/ez_secondary_win.cpp` to sources

### 2.4 Add Forward Declaration to Types
**File:** `ork.lev2/inc/ork/lev2/lev2_types.h`
```cpp
struct EzSecondaryWin;
using ezsecondarywin_ptr_t = std::shared_ptr<EzSecondaryWin>;
```

**Checkpoint 2:** Build succeeds. New files compile. All existing tests pass. New code is unused.

---

## Phase 3: Add Factory to OrkEzApp (Additive Only)

### 3.1 Add to ezapp.h
**File:** `ork.lev2/inc/ork/lev2/ezapp.h`

Add include:
```cpp
#include <ork/lev2/ez_secondary_win.h>
```

Add to OrkEzApp struct:
```cpp
struct OrkEzApp : public OrkEzAppBase {
  // ... existing members ...

  // Secondary window support (Phase 3)
  std::vector<ezsecondarywin_ptr_t> _secondaryWindows;

  ezsecondarywin_ptr_t createSecondaryWindow(const EzSecondaryWinConfig& config);
  void closeSecondaryWindow(ezsecondarywin_ptr_t win);
  void closeAllSecondaryWindows();

  // Internal
  void _renderSecondaryWindows();
  void _cleanupClosedSecondaryWindows();
};
```

### 3.2 Implement Factory (Stub)
**File:** `ork.lev2/src/ezapp.cpp`

Add stub implementations:
```cpp
ezsecondarywin_ptr_t OrkEzApp::createSecondaryWindow(const EzSecondaryWinConfig& config) {
  // Phase 4 will implement this
  return nullptr;
}

void OrkEzApp::closeSecondaryWindow(ezsecondarywin_ptr_t win) {
  if (win) {
    win->requestClose();
  }
}

void OrkEzApp::closeAllSecondaryWindows() {
  for (auto& win : _secondaryWindows) {
    win->requestClose();
  }
}

void OrkEzApp::_renderSecondaryWindows() {
  // Phase 5 will implement
}

void OrkEzApp::_cleanupClosedSecondaryWindows() {
  std::erase_if(_secondaryWindows, [](const auto& w) {
    return w->shouldClose();
  });
}
```

**Checkpoint 3:** Build succeeds. Factory exists but returns nullptr. All existing tests pass.

---

## Phase 4: Implement SecondaryWinImpl (GLFW Integration)

### 4.1 Create Implementation Structure
**File:** `ork.lev2/src/ez_secondary_win.cpp` (expand)

```cpp
#include <ork/lev2/ez_secondary_win.h>
#include <ork/lev2/glfw/ctx_glfw.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/util/logger.h>

namespace ork::lev2 {

static logchannel_ptr_t logchan_secwin = logger()->createChannel("SECWIN", fvec3(0.4, 0.8, 0.4));

struct SecondaryWinImpl {
  SecondaryWinImpl(EzSecondaryWin* owner, const EzSecondaryWinConfig& config);
  ~SecondaryWinImpl();

  void _setupEventHandlers();
  void _render();
  void _onResize(int w, int h);

  EzSecondaryWin* _owner = nullptr;
  EzSecondaryWinConfig _config;

  // GLFW resources
  GLFWwindow* _glfwWindow = nullptr;
  CtxGLFW* _ctxglfw = nullptr;
  Window* _orkWindow = nullptr;  // Orkid window wrapper

  // Graphics
  lev2::Context* _gfxContext = nullptr;

  // State
  int _width = 0;
  int _height = 0;
  int _buttonState = 0;
  int _mouseX = 0;
  int _mouseY = 0;
};

SecondaryWinImpl::SecondaryWinImpl(EzSecondaryWin* owner, const EzSecondaryWinConfig& config)
    : _owner(owner)
    , _config(config)
    , _width(config._width)
    , _height(config._height) {

  logchan_secwin->log("Creating secondary window: %s (%dx%d)",
                       config._title.c_str(), config._width, config._height);

  // Get global context for sharing
  auto global = CtxGLFW::globalOffscreenContext();
  OrkAssert(global != nullptr);

  // Configure window hints
  glfwWindowHint(GLFW_DECORATED, config._decorated ? GLFW_TRUE : GLFW_FALSE);
  glfwWindowHint(GLFW_RESIZABLE, config._resizable ? GLFW_TRUE : GLFW_FALSE);

  // Vulkan: no client API
  extern uint64_t GRAPHICS_API;
  if (GRAPHICS_API == "VULKAN"_crcu) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  }

  // Create window (shares context with global)
  _glfwWindow = glfwCreateWindow(
    config._width,
    config._height,
    config._title.c_str(),
    nullptr,                    // No monitor (windowed)
    global->_glfwWindow         // Share with global
  );
  OrkAssert(_glfwWindow != nullptr);

  glfwSetWindowPos(_glfwWindow, config._x, config._y);

  // Create orkid Window wrapper
  _orkWindow = new Window(config._x, config._y, config._width, config._height,
                          config._title.c_str());

  // Create CtxGLFW for this window
  _ctxglfw = new CtxGLFW(_orkWindow);
  _ctxglfw->_glfwWindow = _glfwWindow;
  _orkWindow->mpCTXBASE = _ctxglfw;

  // Set user pointer for event routing
  glfwSetWindowUserPointer(_glfwWindow, _ctxglfw);

  // Initialize graphics context (VkContext will share device)
  _orkWindow->initContext();
  _gfxContext = _orkWindow->context();

  _setupEventHandlers();

  glfwShowWindow(_glfwWindow);

  logchan_secwin->log("Secondary window created successfully");
}

SecondaryWinImpl::~SecondaryWinImpl() {
  logchan_secwin->log("Destroying secondary window");

  if (_glfwWindow) {
    glfwDestroyWindow(_glfwWindow);
    _glfwWindow = nullptr;
  }

  delete _orkWindow;
  _orkWindow = nullptr;

  // Note: _ctxglfw is owned by _orkWindow
}

void SecondaryWinImpl::_setupEventHandlers() {
  auto sink = _ctxglfw->_eventSINK;

  // Resize callback
  sink->_on_callback_fbresized = [this](int w, int h) {
    _width = w;
    _height = h;
    _onResize(w, h);
  };

  // Close callback via GLFW
  glfwSetWindowCloseCallback(_glfwWindow, [](GLFWwindow* win) {
    auto ctx = static_cast<CtxGLFW*>(glfwGetWindowUserPointer(win));
    // Signal close - will be picked up by owner
  });

  // Mouse button callback
  sink->_on_callback_mousebuttons = [this](int button, int action, int mods) {
    auto uiev = std::make_shared<ui::Event>();
    bool DOWN = (action == GLFW_PRESS);

    switch (button) {
      case GLFW_MOUSE_BUTTON_LEFT:
        uiev->mbLeftButton = DOWN;
        _buttonState = (_buttonState & 6) | int(DOWN);
        break;
      case GLFW_MOUSE_BUTTON_MIDDLE:
        uiev->mbMiddleButton = DOWN;
        _buttonState = (_buttonState & 5) | (int(DOWN) << 1);
        break;
      case GLFW_MOUSE_BUTTON_RIGHT:
        uiev->mbRightButton = DOWN;
        _buttonState = (_buttonState & 3) | (int(DOWN) << 2);
        break;
    }

    uiev->mbALT = (mods & GLFW_MOD_ALT);
    uiev->mbCTRL = (mods & GLFW_MOD_CONTROL);
    uiev->mbSHIFT = (mods & GLFW_MOD_SHIFT);
    uiev->mbSUPER = (mods & GLFW_MOD_SUPER);
    uiev->_eventcode = DOWN ? ui::EventCode::PUSH : ui::EventCode::RELEASE;
    uiev->miX = _mouseX;
    uiev->miY = _mouseY;
    uiev->_uicontext = _owner->_uicontext.get();

    if (_owner->_onUiEvent) {
      _owner->_onUiEvent(uiev);
    } else if (_owner->_uicontext) {
      _owner->_uicontext->handleEvent(uiev);
    }
  };

  // Cursor movement callback
  sink->_on_callback_cursor = [this](double x, double y) {
    _mouseX = int(x);
    _mouseY = int(y);

    auto uiev = std::make_shared<ui::Event>();
    uiev->miX = _mouseX;
    uiev->miY = _mouseY;
    uiev->_eventcode = (_buttonState == 0) ? ui::EventCode::MOVE : ui::EventCode::DRAG;
    uiev->_uicontext = _owner->_uicontext.get();

    if (_owner->_onUiEvent) {
      _owner->_onUiEvent(uiev);
    } else if (_owner->_uicontext) {
      _owner->_uicontext->handleEvent(uiev);
    }
  };

  // Keyboard callback
  sink->_on_callback_keyboard = [this](int key, int scancode, int action, int mods) {
    auto uiev = std::make_shared<ui::Event>();
    uiev->miKeyCode = key;
    uiev->mbALT = (mods & GLFW_MOD_ALT);
    uiev->mbCTRL = (mods & GLFW_MOD_CONTROL);
    uiev->mbSHIFT = (mods & GLFW_MOD_SHIFT);
    uiev->mbSUPER = (mods & GLFW_MOD_SUPER);

    switch (action) {
      case GLFW_PRESS:   uiev->_eventcode = ui::EventCode::KEY_DOWN; break;
      case GLFW_RELEASE: uiev->_eventcode = ui::EventCode::KEY_UP; break;
      case GLFW_REPEAT:  uiev->_eventcode = ui::EventCode::KEY_REPEAT; break;
    }
    uiev->_uicontext = _owner->_uicontext.get();

    if (_owner->_onUiEvent) {
      _owner->_onUiEvent(uiev);
    } else if (_owner->_uicontext) {
      _owner->_uicontext->handleEvent(uiev);
    }
  };

  // Scroll callback
  sink->_on_callback_scroll = [this](double xoff, double yoff) {
    auto uiev = std::make_shared<ui::Event>();
    uiev->_eventcode = ui::EventCode::MOUSEWHEEL;
    uiev->miMWX = int(xoff);
    uiev->miMWY = int(yoff);
    uiev->_uicontext = _owner->_uicontext.get();

    if (_owner->_onUiEvent) {
      _owner->_onUiEvent(uiev);
    } else if (_owner->_uicontext) {
      _owner->_uicontext->handleEvent(uiev);
    }
  };
}

void SecondaryWinImpl::_render() {
  if (!_gfxContext || !_glfwWindow) return;

  // Check for window close
  if (glfwWindowShouldClose(_glfwWindow)) {
    _owner->_shouldClose = true;
    return;
  }

  _gfxContext->makeCurrentContext();

  // GPU init on first render
  if (!_owner->_gpuInitialized) {
    if (_owner->_onGpuInit) {
      _owner->_onGpuInit(_gfxContext);
    }
    _owner->_gpuInitialized = true;
  }

  // Render
  if (_owner->_onDraw) {
    auto drwev = std::make_shared<ui::DrawEvent>(_gfxContext);
    _owner->_onDraw(drwev);
  } else {
    // Default: clear to dark gray
    _gfxContext->beginFrame();
    // Could draw ui::Context here if _top is set
    _gfxContext->endFrame();
  }
}

void SecondaryWinImpl::_onResize(int w, int h) {
  _width = w;
  _height = h;

  if (_gfxContext) {
    _gfxContext->resizeMainSurface(w, h);
  }

  if (_owner->_onResize) {
    _owner->_onResize(w, h);
  }
}

// EzSecondaryWin implementation using SecondaryWinImpl
EzSecondaryWin::EzSecondaryWin(const EzSecondaryWinConfig& config) {
  _uicontext = std::make_shared<ui::Context>();
  _impl.makeShared<SecondaryWinImpl>(this, config);
}

EzSecondaryWin::~EzSecondaryWin() {
  _impl.clear();
}

bool EzSecondaryWin::shouldClose() const { return _shouldClose; }
void EzSecondaryWin::requestClose() { _shouldClose = true; }

int EzSecondaryWin::width() const {
  if (auto impl = _impl.tryAsShared<SecondaryWinImpl>()) {
    return impl.value()->_width;
  }
  return 0;
}

int EzSecondaryWin::height() const {
  if (auto impl = _impl.tryAsShared<SecondaryWinImpl>()) {
    return impl.value()->_height;
  }
  return 0;
}

ui::Context* EzSecondaryWin::uiContext() {
  return _uicontext.get();
}

lev2::Context* EzSecondaryWin::gfxContext() {
  if (auto impl = _impl.tryAsShared<SecondaryWinImpl>()) {
    return impl.value()->_gfxContext;
  }
  return nullptr;
}

void EzSecondaryWin::_render() {
  if (auto impl = _impl.tryAsShared<SecondaryWinImpl>()) {
    impl.value()->_render();
  }
}

void EzSecondaryWin::_handleResize(int w, int h) {
  if (auto impl = _impl.tryAsShared<SecondaryWinImpl>()) {
    impl.value()->_onResize(w, h);
  }
}

} // namespace ork::lev2
```

### 4.2 Update Factory Implementation
**File:** `ork.lev2/src/ezapp.cpp`

Replace stub:
```cpp
ezsecondarywin_ptr_t OrkEzApp::createSecondaryWindow(const EzSecondaryWinConfig& config) {
  auto win = std::make_shared<EzSecondaryWin>(config);
  _secondaryWindows.push_back(win);
  return win;
}
```

**Checkpoint 4:** Build succeeds. Secondary window can be created. Main window still works. All existing tests pass.

---

## Phase 5: Integrate with Main Runloop

### 5.1 Modify _mainThreadLoopIter
**File:** `ork.lev2/src/ezapp.cpp`

Find existing `_mainThreadLoopIter` or equivalent and add:
```cpp
void OrkEzApp::_mainThreadLoopIter() {
  // ... existing code for main window ...

  // Render secondary windows
  _renderSecondaryWindows();

  // Cleanup closed windows
  _cleanupClosedSecondaryWindows();
}

void OrkEzApp::_renderSecondaryWindows() {
  for (auto& win : _secondaryWindows) {
    if (!win->shouldClose()) {
      win->_render();
    }
  }
}
```

### 5.2 Ensure glfwPollEvents Covers All Windows
GLFW's `glfwPollEvents()` already processes events for ALL windows. Verify this is called once per frame in the main loop (it is, in `CtxGLFW::_runloopIter`).

**Checkpoint 5:** Secondary windows render. Events route correctly. Main window unaffected. All existing tests pass.

---

## Phase 6: Add Python Bindings (Optional)

### 6.1 Extend pyext_ui.cpp or Create New File
**File:** `ork.lev2/pyext/src/pyext_secondary_win.cpp`

```cpp
void pyinit_secondary_win(py::module& module_lev2) {
  auto secwin_type = py::class_<EzSecondaryWin, ezsecondarywin_ptr_t>(
      module_lev2, "EzSecondaryWin")
    .def_property_readonly("width", &EzSecondaryWin::width)
    .def_property_readonly("height", &EzSecondaryWin::height)
    .def_property_readonly("should_close", &EzSecondaryWin::shouldClose)
    .def("request_close", &EzSecondaryWin::requestClose)
    .def_property_readonly("ui_context", &EzSecondaryWin::uiContext)
    .def_property_readonly("gfx_context", &EzSecondaryWin::gfxContext);

  auto config_type = py::class_<EzSecondaryWinConfig>(
      module_lev2, "EzSecondaryWinConfig")
    .def(py::init<>())
    .def_readwrite("width", &EzSecondaryWinConfig::_width)
    .def_readwrite("height", &EzSecondaryWinConfig::_height)
    .def_readwrite("x", &EzSecondaryWinConfig::_x)
    .def_readwrite("y", &EzSecondaryWinConfig::_y)
    .def_readwrite("title", &EzSecondaryWinConfig::_title)
    .def_readwrite("decorated", &EzSecondaryWinConfig::_decorated)
    .def_readwrite("resizable", &EzSecondaryWinConfig::_resizable);
}
```

### 6.2 Add to OrkEzApp Bindings
```cpp
// In existing ezapp bindings
.def("create_secondary_window", &OrkEzApp::createSecondaryWindow)
.def("close_secondary_window", &OrkEzApp::closeSecondaryWindow)
.def("close_all_secondary_windows", &OrkEzApp::closeAllSecondaryWindows)
```

**Checkpoint 6:** Python can create secondary windows. All existing Python scripts work. All tests pass.

---

## Phase 7: Integration Testing

### 7.1 Create Test Script
**File:** `ork.lev2/pyext/tests/multiwindow/basic_multiwin.py`

```python
#!/usr/bin/env python3
import ork.lev2 as lev2

# Test: Create app with secondary window
def test_multiwindow():
    app = lev2.EzApp.create()

    # Create secondary window
    config = lev2.EzSecondaryWinConfig()
    config.width = 400
    config.height = 300
    config.title = "Secondary Window Test"

    secondary = app.create_secondary_window(config)
    assert secondary is not None
    assert secondary.width == 400
    assert secondary.height == 300

    # Close it
    secondary.request_close()

    print("Multi-window test passed!")

if __name__ == "__main__":
    test_multiwindow()
```

### 7.2 Run All Existing Tests
```bash
# Run baseline tests established in Phase 0
# Verify no regressions
```

**Checkpoint 7:** All tests pass. Multi-window functionality verified.

---

## Phase 8: Documentation & Examples

### 8.1 Update Documentation
- Add multi-window section to relevant docs
- Document EzSecondaryWin API

### 8.2 Create Example
**File:** `ork.lev2/examples/python/multiwindow_demo.py`

Demonstrates:
- Creating multiple secondary windows
- Independent drawing per window
- Event handling per window
- Closing windows

**Checkpoint 8:** Documentation complete. Examples work.

---

## Rollback Procedure

If any phase fails:

1. **Phase 2-3:** Delete new files, remove from CMakeLists
2. **Phase 4:** Revert ez_secondary_win.cpp to stubs
3. **Phase 5:** Revert ezapp.cpp changes
4. **Phase 6:** Remove Python bindings

Each phase is independently revertible via git.

---

## Testing Checklist Per Phase

After each phase, verify:

- [ ] `ork.build.py` succeeds
- [ ] Existing single-window apps still work
- [ ] DRM mode still works (if on Linux)
- [ ] No new compiler warnings
- [ ] Existing Python scripts run without error
- [ ] Memory: no leaks introduced (optional: run with sanitizer)

---

## Files Modified/Created Summary

| Phase | File | Action |
|-------|------|--------|
| 2 | `ork.lev2/inc/ork/lev2/ez_secondary_win.h` | Create |
| 2 | `ork.lev2/src/ez_secondary_win.cpp` | Create |
| 2 | `ork.lev2/inc/ork/lev2/lev2_types.h` | Modify (add typedef) |
| 2 | `ork.lev2/CMakeLists.txt` | Modify (add source) |
| 3 | `ork.lev2/inc/ork/lev2/ezapp.h` | Modify (add members) |
| 3 | `ork.lev2/src/ezapp.cpp` | Modify (add stubs) |
| 4 | `ork.lev2/src/ez_secondary_win.cpp` | Modify (full impl) |
| 4 | `ork.lev2/src/ezapp.cpp` | Modify (factory impl) |
| 5 | `ork.lev2/src/ezapp.cpp` | Modify (runloop integration) |
| 6 | `ork.lev2/pyext/src/pyext_*.cpp` | Modify (bindings) |
| 7 | `ork.lev2/pyext/tests/multiwindow/` | Create (tests) |

---

## Success Criteria

1. Can create N secondary windows from main app
2. Each window has independent:
   - Widget tree
   - Event handling
   - Drawing
3. Main window behavior unchanged
4. DRM mode unchanged
5. All existing tests pass
6. No memory leaks
7. Clean shutdown (all windows close properly)
