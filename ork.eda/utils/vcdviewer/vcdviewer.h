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

struct ViewParams {
  int _min_timestamp = 0;
  int _max_timestamp = 0;
};

///////////////////////////////////////////////////////////////////////////////

using viewparams_ptr_t = std::shared_ptr<ViewParams>;

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

  fvec4 _color;
  fvec4 _textcolor;
  signal_ptr_t _signal;
  viewparams_ptr_t _viewparams;
  std::string _label;
  size_t _numsamples;
  std::string _font = "i14";
  bool _vbdirty     = true;
  DynamicVertexBuffer<vtx_t> _vtxbuf;
  int _numvertices = 0;
  HandlerResult DoOnUiEvent(event_constptr_t evptr) override;
  void DoDraw(ui::drawevent_constptr_t drwev) override;
};
