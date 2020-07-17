#pragma once

#include <ork/pch.h>
#include <ork/lev2/ui/anchor.h>
#include <ork/lev2/ui/box.h>
#include <ork/lev2/ui/split_panel.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/ui/context.h>
#include <ork/lev2/ui/label.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <queue>
#include "harness.h"
#include <stdint.h>

using namespace std::string_literals;
using namespace ork;
using namespace ork::hdl;
using namespace ork::hdl::vcd;
using namespace ork::lev2;
using namespace ork::ui;

///////////////////////////////////////////////////////////////////////////////
struct ViewParams;
struct Overlay;
struct SignalTrackWidget;
///////////////////////////////////////////////////////////////////////////////
using viewparams_ptr_t = std::shared_ptr<ViewParams>;
using overlay_ptr_t    = std::shared_ptr<Overlay>;
using sigtrack_ptr_t   = std::shared_ptr<SignalTrackWidget>;
///////////////////////////////////////////////////////////////////////////////
struct ViewParams {
  static viewparams_ptr_t instance();
  uint64_t _min_timestamp      = 0;
  uint64_t _max_timestamp      = 0;
  uint64_t _cursor_actual      = 0;
  uint64_t _cursor_nearest     = 0;
  SignalTrackWidget* _curtrack = nullptr;
  std::vector<sigtrack_ptr_t> _sigtracks;
};
///////////////////////////////////////////////////////////////////////////////
struct SignalTrack {
  std::string _name;
  signal_ptr_t _signal;
};
///////////////////////////////////////////////////////////////////////////////
struct ScopeTrack {
  std::string _name;
  scope_ptr_t _scope;
  std::vector<SignalTrack> _sigtracks;
};
///////////////////////////////////////////////////////////////////////////////
struct SignalTrackWidget final : public Widget {

  using vtx_t = SVtxV12C4T16;

  SignalTrackWidget(
      signal_ptr_t sig, //
      fvec4 color);

  HandlerResult DoOnUiEvent(event_constptr_t evptr) override;
  void DoDraw(ui::drawevent_constptr_t drwev) override;
  void setTimeStamp(uint64_t ts);

  fvec4 _color;
  fvec4 _textcolor;
  signal_ptr_t _signal;
  std::string _label;
  std::string _hdrlabel;
  size_t _numsamples;
  std::string _font          = "i14";
  bool _vbdirty              = true;
  uint64_t _nearest_timestep = 0xffffffffffffffff;
  DynamicVertexBuffer<vtx_t> _vtxbuf;
  int _numvertices = 0;
  std::string _curlabel;
  int _stringwidth = 0;
  int _labelY      = 0;
};
///////////////////////////////////////////////////////////////////////////////
struct Overlay final : public Widget {

  using vtx_t = SVtxV12C4T16;

  static overlay_ptr_t instance();

  Overlay(
      const std::string& name, //
      fvec4 color);
  fvec4 _color;
  fvec4 _textcolor;
  std::string _font = "i14";
  DynamicVertexBuffer<vtx_t> _vtxbuf;
  int _numvertices = 0;
  int _vtxbase     = 0;
  bool _vbdirty    = true;

  void DoDraw(ui::drawevent_constptr_t drwev) override;
};
