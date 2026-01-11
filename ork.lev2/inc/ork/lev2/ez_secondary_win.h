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

  //////////////////////////////////////////////
  // User-assignable callbacks
  //////////////////////////////////////////////

  draw_cb_t _onDraw;
  resize_cb_t _onResize;
  uievent_cb_t _onUiEvent;
  gpuinit_cb_t _onGpuInit;

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
  lev2::Context* gfxContext();

  //////////////////////////////////////////////
  // Construction (via factory in OrkEzApp)
  //////////////////////////////////////////////

  EzSecondaryWin(const EzSecondaryWinConfig& config);
  ~EzSecondaryWin();

  //////////////////////////////////////////////
  // Internal - called by main runloop
  //////////////////////////////////////////////

  void _render();
  void _handleResize(int w, int h);

private:
  friend struct OrkEzApp;
  friend struct SecondaryWinImpl;

  svar64_t _impl;
  ui::context_ptr_t _uicontext;
  bool _shouldClose = false;
  bool _gpuInitialized = false;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
