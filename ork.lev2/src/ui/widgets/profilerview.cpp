#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/kernel/timer.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/context.h>
#include <ork/lev2/ui/profilerview.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/math/misc_math.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
///////////////////////////////////////////////////////////
namespace ork::ui {
///////////////////////////////////////////////////////////////////////////////
using vtx_t    = lev2::SVtxV16T16C16;
using vtxbuf_t = lev2::DynamicVertexBuffer<vtx_t>;
///////////////////////////////////////////////////////////////////////////////
ProfilerView::ProfilerView()
    : PrimCanvas("ProfilerView", 0, 0, 32, 32) {
  _bg_color = fvec4(0, 0, 0, 1);
}
/////////////////////////////////////////////////////////////////////////
HandlerResult ProfilerView::DoOnUiEvent(event_constptr_t ev) {
  if (ev->_eventcode == ui::EventCode::MOVE) {
    int mx = ev->miX;
    int my = ev->miY;
    std::string hit;
    for (auto& e : _legend_entries) {
      if (mx >= e.x && mx < e.x + e.w && my >= e.y && my < e.y + e.h) {
        hit = e.series_name;
        break;
      }
    }
    if (hit != _hovered_series) {
      _hovered_series = hit;
      SetDirty();
    }
  }
  return HandlerResult();
}
///////////////////////////////////////////////////////////////////////////////
// Draw one profiler channel into a single lane. All series share the lane's
// Y-axis (shared auto-ranging). Legend on the right lists each series.
///////////////////////////////////////////////////////////////////////////////
void ProfilerView::_drawContextChannel(
    const std::string&    channel_label,
    ork::ProfilerChannel* channel,
    float                 lane_y_top,
    float                 lane_height,
    int                   max_label_width) {

  if (channel->_series_iter.empty()) return;

  // Sort series by the level recorded in their most recent sample so that
  // shallower call-stack entries appear first (higher in the legend / drawn first).
  std::vector<ork::ProfilerSeries*> sorted_series;
  for (auto* s : channel->_series_iter) {
    if (!s->_samples.empty() && s->_samples.back().level != -1)
      sorted_series.push_back(s);
  }
  std::sort(sorted_series.begin(), sorted_series.end(),
      [](const ork::ProfilerSeries* a, const ork::ProfilerSeries* b) {
        return a->_samples.back().level < b->_samples.back().level;
      });

  // Measure max level so the legend area is wide enough to accommodate indentation.
  int max_level = 0;
  for (auto* s : sorted_series)
    max_level = std::max(max_level, s->_samples.empty() ? 0 : s->_samples.back().level);

  const int   indent_px = 8;
  const int   ms_col_w  = lev2::FontMan::stringWidth(5) + 8; // fixed ms column ("XX.XX")
  const int   W         = width();
  const int   box_x1    = W - (max_label_width + ms_col_w + max_level * indent_px + 28);
  const int   box_x2    = W - 16;
  const float chart_x0  = 0.0f;
  const float chart_x1  = float(box_x1 - 8);
  const int   legend_x0 = box_x1 + 4;

  // ------------------------------------------------------------------
  // Channel label — centered above the legend area
  // ------------------------------------------------------------------
  {
    int header_w  = lev2::FontMan::stringWidth(channel_label.length());
    int legend_cx = (box_x1 + W) / 2;
    int header_x  = legend_cx - header_w / 2;
    _context->RefModColor() = fvec3(0.9f, 0.9f, 0.9f);
    lev2::FontMan::beginTextBlock(_context, 128);
    lev2::FontMan::DrawText(_context, header_x, int(lane_y_top) + 4, channel_label.c_str());
    lev2::FontMan::endTextBlock(_context);
  }

  // ------------------------------------------------------------------
  // Per-series legend entries
  // ------------------------------------------------------------------
  const float legend_row_h   = 14.0f;
  const float legend_start_y = lane_y_top + 18.0f;

  // Pass 1: assign colors and update exclusive currentValues.
  for (int i = 0; i < (int)sorted_series.size(); i++) {
    auto* series = sorted_series[i];
    auto& rstate = _render_state_map[std::string(series->_name.strval())];
    if (rstate.color_index < 0) {
      float hue = std::fmod(float(_color_index) * 0.618034f, 1.0f);
      rstate.color.setHSV(hue, 0.65f, 0.8f);
      rstate.color_index = _color_index++;
    }
    if (!series->_samples.empty())
      rstate.currentValue = float(series->_samples.back().time * 1000.0);
  }

  // Pass 2: compute inclusive display values.
  // Series at level L = its own exclusive time + exclusive time of ALL series
  // at strictly deeper levels (> L).  Siblings at the same level are excluded.
  std::vector<float> inclusive_vals(sorted_series.size());
  for (int i = 0; i < (int)sorted_series.size(); i++) {
    int   level_i = sorted_series[i]->_samples.empty() ? 0 : sorted_series[i]->_samples.back().level;
    float sum     = _render_state_map[std::string(sorted_series[i]->_name.strval())].currentValue;
    for (int j = 0; j < (int)sorted_series.size(); j++) {
      if (j == i) continue;
      int level_j = sorted_series[j]->_samples.empty() ? 0 : sorted_series[j]->_samples.back().level;
      if (level_j > level_i)
        sum += _render_state_map[std::string(sorted_series[j]->_name.strval())].currentValue;
    }
    inclusive_vals[i] = sum;
  }

  // Pass 3: draw legend entries.
  for (int i = 0; i < (int)sorted_series.size(); i++) {
    auto*       series = sorted_series[i];
    const char* sname  = series->_name.strval();
    std::string key(sname);
    auto& rstate = _render_state_map[key];

    int   level   = series->_samples.empty() ? 0 : series->_samples.back().level;
    float entry_y = legend_start_y + float(i) * legend_row_h;
    int   iy      = int(entry_y);

    int name_x  = legend_x0 + ms_col_w + level * indent_px;
    int entry_w = W - name_x;
    _legend_entries.push_back({name_x, iy, entry_w, int(legend_row_h), key});

    bool  hovered    = (!_hovered_series.empty() && key == _hovered_series);
    fvec3 draw_color = hovered ? rstate.color * 1.8f : rstate.color;
    _context->RefModColor() = draw_color;

    // Inclusive ms value — own time + all deeper levels
    auto valstr = FormatString("%0.2f", inclusive_vals[i]);
    lev2::FontMan::beginTextBlock(_context, 128);
    lev2::FontMan::DrawText(_context, legend_x0, iy, valstr.c_str());
    lev2::FontMan::endTextBlock(_context);

    // Series name — indented by call-stack level
    lev2::FontMan::beginTextBlock(_context, 128);
    lev2::FontMan::DrawText(_context, name_x, iy, sname);
    lev2::FontMan::endTextBlock(_context);
  }

  // ------------------------------------------------------------------
  // Cumulative Y auto-range for stacked representation.
  // The max value is the largest per-sample sum across all series so the
  // Y axis covers the full stacked height.
  // ------------------------------------------------------------------
  size_t n_max_samples = 0;
  for (auto* series : sorted_series)
    n_max_samples = std::max(n_max_samples, series->_samples.size());

  float cum_max_value = 0.0f;
  for (size_t si = 0; si < n_max_samples; si++) {
    float cum_at_i = 0.0f;
    for (auto* series : sorted_series) {
      if (si < series->_samples.size())
        cum_at_i += float(series->_samples[si].time * 1000.0);
    }
    cum_max_value = std::max(cum_max_value, cum_at_i);
  }
  if (cum_max_value < 0.1f) cum_max_value = 0.1f;

  auto& crange      = _channel_range_map[channel_label];
  float b           = crange.blend_rate;
  crange.min_value  = 0.0f;
  crange.max_value  = crange.max_value * (1.0f - b) + cum_max_value * b;
  crange.blend_rate = std::max(0.01f, crange.blend_rate - 0.001f);

  float series_max    = std::max(crange.max_value, 0.1f);
  float y_bottom      = lane_y_top + lane_height - 2.0f;
  float y_top_line    = lane_y_top + 2.0f;
  float y_pixel_range = y_bottom - y_top_line;
  float y_scale       = (y_pixel_range > 0.0f) ? y_pixel_range / series_max : 1.0f;

  // ------------------------------------------------------------------
  // Horizontal separator at top of lane (skip first lane)
  // ------------------------------------------------------------------
  if (lane_y_top > 0.5f) {
    lev2::VtxWriter<vtx_t> sv;
    sv.Lock(_context, _vbuf.get(), 2);
    sv.AddVertex(vtx_t(fvec3(0,       lane_y_top, 0), fvec4(), fvec3(0.3f, 0.3f, 0.3f)));
    sv.AddVertex(vtx_t(fvec3(chart_x1,lane_y_top, 0), fvec4(), fvec3(0.3f, 0.3f, 0.3f)));
    sv.UnLock(_context);
    _mtl->begin(_tek, _RCFD);
    _mtl->bindParamMatrix(_par_mvp, _mtxi->RefMVPMatrix());
    _mtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
    _gbi->DrawPrimitiveEML(sv, lev2::PrimitiveType::LINES);
    _mtl->end(_RCFD);
  }

  // ------------------------------------------------------------------
  // Stacked line plots — series drawn at cumulative Y positions.
  // Each series is plotted at (running_baseline[i] + own_value[i]),
  // measured upward from y_bottom.  After drawing, the series' values
  // are added to baseline so the next (deeper) series stacks on top.
  // ------------------------------------------------------------------
  auto rs      = _mtl->_rasterstate;
  auto omacro  = rs->_blendingMacro;
  int prev_pri = rs->_priority;
  rs->setBlendingMacro(lev2::BlendingMacro::ADDITIVE);
  rs->setDepthTest(lev2::EDepthTest::OFF);
  rs->setCullTest(lev2::ECullTest::OFF);
  rs->setWriteMaskZ(false);
  rs->_priority = 1 << 16;
  _fxi->pushRasterState(rs);

  // Precompute per-series inclusive sample values.
  // inclusive_samples[si][idx] = this series' own value + values of all series
  // at strictly deeper levels (level > level_si).  Siblings are excluded.
  std::vector<std::vector<float>> inclusive_samples(sorted_series.size());
  for (int si = 0; si < (int)sorted_series.size(); si++) {
    int level_si = sorted_series[si]->_samples.empty() ? 0 : sorted_series[si]->_samples.back().level;
    inclusive_samples[si].assign(n_max_samples, 0.0f);
    // Own values
    for (size_t idx = 0; idx < sorted_series[si]->_samples.size() && idx < n_max_samples; idx++)
      inclusive_samples[si][idx] = float(sorted_series[si]->_samples[idx].time * 1000.0);
    // All strictly deeper levels
    for (int sj = 0; sj < (int)sorted_series.size(); sj++) {
      if (sj == si) continue;
      int level_sj = sorted_series[sj]->_samples.empty() ? 0 : sorted_series[sj]->_samples.back().level;
      if (level_sj > level_si) {
        for (size_t idx = 0; idx < sorted_series[sj]->_samples.size() && idx < n_max_samples; idx++)
          inclusive_samples[si][idx] += float(sorted_series[sj]->_samples[idx].time * 1000.0);
      }
    }
  }

  for (int si = 0; si < (int)sorted_series.size(); si++) {
    auto*       series = sorted_series[si];
    size_t      n      = series->_samples.size();
    std::string skey(series->_name.strval());
    auto& rstate = _render_state_map[skey];

    bool any_hover  = !_hovered_series.empty();
    bool is_hovered = (skey == _hovered_series);
    fvec3 line_color = rstate.color;
    if (any_hover)
      line_color = is_hovered ? rstate.color * 1.8f : rstate.color * 0.2f;

    if (n >= 2) {
      float x_step = (chart_x1 - chart_x0) / float(n - 1);
      lev2::VtxWriter<vtx_t> vw;
      vw.Lock(_context, _vbuf.get(), n * 2);
      for (size_t i = 1; i < n; i++) {
        float incl_prev = inclusive_samples[si][i-1];
        float incl_curr = inclusive_samples[si][i];
        float sx_prev   = chart_x0 + float(i-1) * x_step;
        float sx_curr   = chart_x0 + float(i)   * x_step;
        float sy_prev   = y_bottom - incl_prev * y_scale;
        float sy_curr   = y_bottom - incl_curr * y_scale;
        vw.AddVertex(vtx_t(fvec3(sx_prev, sy_prev, 0), fvec4(), line_color));
        vw.AddVertex(vtx_t(fvec3(sx_curr, sy_curr, 0), fvec4(), line_color));
      }
      vw.UnLock(_context);
      _mtl->begin(_tek, _RCFD);
      _mtl->bindParamMatrix(_par_mvp, _mtxi->RefMVPMatrix());
      _gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::LINES);
      _mtl->end(_RCFD);
    }
  }

  _fxi->popRasterState();
  rs->_priority      = prev_pri;
  rs->_blendingMacro = omacro;
}
/////////////////////////////////////////////////////////////////////////
void ProfilerView::DoDraw(drawevent_constptr_t drwev) {
  if (!_context) return;

  auto tgt = drwev->GetTarget();

  gpuInit(tgt);

  // Initialize GPU resources once
  if (!_mtl) {
    auto vb = std::make_shared<vtxbuf_t>(16 << 20, 0);
    vb->SetRingLock(true);
    _vbuf    = vb;
    _mtl     = std::make_shared<lev2::FreestyleMaterial>();
    _mtl->gpuInit(tgt, "orkshader://solid");
    _tek     = _mtl->technique("vtxcolor");
    _par_mvp = _mtl->param("MatMVP");
  }

  // Per-frame interface pointers and frame data
  _gbi  = tgt->GBI();
  _mtxi = tgt->MTXI();
  _fxi  = tgt->FXI();
  _RCFD = std::make_shared<lev2::RenderContextFrameData>(tgt);

  if (_draw_background) {
    _drawColoredBox(drwev, _bg_color, lev2::BlendingMacro::ALPHA);
  }

  // Collect channels
  struct CtxEntry { std::string label; ork::ProfilerChannel* channel; };
  std::vector<CtxEntry> ctx_channels;
  ctx_channels.push_back({"MainThread", _context->_main_thread_channel.get()});
  if (_context->_gpu_channel)
    ctx_channels.push_back({"GPU", _context->_gpu_channel.get()});

  // Max label width across all series
  int max_label_width = 0;
  bool any_series = false;
  for (auto& e : ctx_channels) {
    for (auto* series : e.channel->_series_iter) {
      int sw = lev2::FontMan::stringWidth(strlen(series->_name.strval()));
      max_label_width = std::max(max_label_width, sw);
      any_series = true;
    }
  }
  if (!any_series) return;

  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);
  lev2::ViewportRect vprect(ix1, iy1, width(), height());
  auto fbi = tgt->FBI();
  fbi->pushViewport(vprect);
  fbi->pushScissor(vprect);

  ork::lev2::FontMan::PushFont("i14");

  size_t num_channels = ctx_channels.size();
  float  lane_height  = float(height()) / float(num_channels);

  _legend_entries.clear();

  _mtxi->PushUIMatrix(width(), height());
  for (size_t ci = 0; ci < num_channels; ci++) {
    _drawContextChannel(
        ctx_channels[ci].label,
        ctx_channels[ci].channel,
        float(ci) * lane_height,
        lane_height,
        max_label_width);
  }
  _mtxi->PopUIMatrix();

  ork::lev2::FontMan::PopFont();
  fbi->popScissor();
  fbi->popViewport();
}
/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
