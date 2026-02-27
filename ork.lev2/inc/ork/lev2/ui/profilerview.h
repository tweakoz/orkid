////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/widget.h>
#include <map>
#include <string>
#include <vector>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////
struct SeriesRenderState {
  fvec3 color;
  float currentValue = 0.0f;
  int   color_index  = -1;
};

struct ChannelRange {
  float min_value  = 0.0f;
  float max_value  = 1.0f;
  float blend_rate = 0.5f;
};

struct LegendEntry {
  int         x, y, w, h;
  std::string series_name;
};

///////////////////////////////////////////////////////////////////////////////
// ProfilerView: plots ork::ProfilerChannel data as overlaid line plots.
// One lane per channel (MainThread, GPU), all series share the lane's Y-axis.
// Call setContext() after GPU init.
///////////////////////////////////////////////////////////////////////////////
struct ProfilerView : public ui::Widget {
  ProfilerView();
  void DoDraw(ui::drawevent_constptr_t drwev) override;
  ui::HandlerResult DoOnUiEvent(ui::event_constptr_t EV) override;

  void setContext(lev2::Context* ctx) { _context = ctx; }

  // Context providing profiler channel data
  lev2::Context* _context = nullptr;

  // Per-series and per-channel render state
  std::map<std::string, SeriesRenderState> _render_state_map;
  std::map<std::string, ChannelRange>      _channel_range_map;
  int _color_index = 0;

  // Background
  fvec4 _bg_color = fvec4(0, 0, 0, 1);

  // Hover state — series name under the mouse cursor (empty if none)
  std::string              _hovered_series;
  std::vector<LegendEntry> _legend_entries;

private:
  void _drawContextChannel(
      const std::string&              channel_label,
      ork::ProfilerChannel*           channel,
      float                           lane_y_top,
      float                           lane_height,
      lev2::rcfd_ptr_t                RCFD);

  // Draw state — initialized once
  lev2::freestyle_mtl_ptr_t                     _mtl;
  const lev2::FxShaderTechnique*                _tek     = nullptr;
  const lev2::FxShaderParam*                    _par_mvp = nullptr;
  std::shared_ptr<lev2::VertexBufferBase>       _vbuf;
};
using profilerview_ptr_t = std::shared_ptr<ProfilerView>;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
