////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/lev2_types.h>
#include <ork/lev2/ui/context.h>
#include <ork/lev2/ui/group.h>
#include <ork/kernel/svariant.h>
#include <ork/kernel/timer.h>
#include <functional>
#include <string>
#include <memory>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
// Forward declarations
///////////////////////////////////////////////////////////////////////////////

struct EzSecondaryWin;
using ezsecondarywin_ptr_t = std::shared_ptr<EzSecondaryWin>;

///////////////////////////////////////////////////////////////////////////////
// Configuration for creating a secondary window
///////////////////////////////////////////////////////////////////////////////

struct EzSecondaryWinConfig {
  int _width = 640;
  int _height = 480;
  int _x = 100;
  int _y = 100;
  std::string _title = "Secondary Window";
  bool _decorated = true;
  bool _resizable = true;

  // Popup-specific options
  bool _floating = false;       // Always on top (for popups)
  bool _transparent = false;    // Transparent framebuffer (for styled popups)
  bool _focusOnShow = false;    // Auto-focus when shown

  // Focus behavior
  bool _focusFollowsMouse = false; // Focus window when mouse enters
  bool _focusToFront = false;      // Raise window to front when focused

  // Fullscreen on a named monitor (empty = windowed)
  std::string _fullscreenMonitor;

  // Convenience factory for popup-style windows
  static EzSecondaryWinConfig popup(int x, int y, int w, int h, bool transparent = false) {
    EzSecondaryWinConfig cfg;
    cfg._x = x;
    cfg._y = y;
    cfg._width = w;
    cfg._height = h;
    cfg._decorated = false;
    cfg._resizable = false;
    cfg._floating = true;
    cfg._transparent = transparent;
    cfg._focusOnShow = true;
    return cfg;
  }
};

///////////////////////////////////////////////////////////////////////////////
// Secondary window - can be created from OrkEzApp
// Each secondary window has its own:
//   - GLFW window
//   - VkContext (shares VkDevice with main window)
//   - ui::Context (independent event/focus handling)
//   - Widget tree
///////////////////////////////////////////////////////////////////////////////

struct EzSecondaryWin {

  //////////////////////////////////////////////
  // Callback types
  //////////////////////////////////////////////

  using draw_cb_t = std::function<void(ui::drawevent_constptr_t)>;
  using resize_cb_t = std::function<void(int w, int h)>;
  using uievent_cb_t = std::function<ui::HandlerResult(ui::event_constptr_t)>;
  using gpuinit_cb_t = std::function<void(Context* ctx)>;
  using closed_cb_t = void_lambda_t;

  //////////////////////////////////////////////
  // User-assignable callbacks
  //////////////////////////////////////////////

  draw_cb_t _onDraw;
  resize_cb_t _onResize;
  uievent_cb_t _onUiEvent;
  gpuinit_cb_t _onGpuInit;
  closed_cb_t _onClosed;  // Called when window is closed

  //////////////////////////////////////////////
  // State queries
  //////////////////////////////////////////////

  bool shouldClose() const;
  void requestClose();
  int width() const;
  int height() const;

  //////////////////////////////////////////////
  // Access to contexts
  //////////////////////////////////////////////

  ui::Context* uiContext();
  ui::context_ptr_t uiContextPtr();
  lev2::Context* gfxContext();

  //////////////////////////////////////////////
  // Construction (via factory in OrkEzApp)
  //////////////////////////////////////////////

  EzSecondaryWin(const EzSecondaryWinConfig& config);
  ~EzSecondaryWin();

  //////////////////////////////////////////////
  // Dirty-flag rendering control
  //  Secondary windows only re-render when their
  //  content has changed (UI event, resize, etc.)
  //  to avoid blocking the main loop with vsync waits.
  //  A maximum staleness interval provides a safety net.
  //////////////////////////////////////////////

  void markDirty();
  bool needsRender() const;

  //////////////////////////////////////////////
  // Internal - called by main runloop
  //////////////////////////////////////////////

  void _render();
  void _handleResize(int w, int h);
  void _forceClose();  // Explicitly destroy GLFW window (for cleanup when Python holds refs)

private:
  friend struct OrkEzApp;
  friend struct SecondaryWinImpl;

  svar64_t _impl;
  ui::context_ptr_t _uicontext;
  bool _shouldClose = false;
  bool _gpuInitialized = false;
  bool _dirty = true;                  // starts dirty (needs initial render)
  ork::Timer _lastRenderTimer;         // time since last render
};

///////////////////////////////////////////////////////////////////////////////
// GLFW Monitor enumeration (works on macOS, Linux/X11, Linux/Wayland)
///////////////////////////////////////////////////////////////////////////////

struct GlfwMonitorInfo {
  std::string _name;          // Monitor name (use with fullscreen_monitor)
  int _x = 0;                 // Position x
  int _y = 0;                 // Position y
  int _width = 0;             // Current video mode width
  int _height = 0;            // Current video mode height
  int _refreshRate = 0;       // Current video mode refresh rate (Hz)
  int _physicalWidthMM = 0;   // Physical width in millimeters
  int _physicalHeightMM = 0;  // Physical height in millimeters
  float _contentScaleX = 1.0f; // Content scale X (>1 = HiDPI)
  float _contentScaleY = 1.0f; // Content scale Y (>1 = HiDPI)
  bool _primary = false;      // Is primary monitor
};
using glfwmonitorinfo_ptr_t = std::shared_ptr<GlfwMonitorInfo>;

std::vector<glfwmonitorinfo_ptr_t> enumerateGlfwMonitors();

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
