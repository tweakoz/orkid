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
    : Widget("ProfilerView", 0, 0, 32, 32) {
}
/////////////////////////////////////////////////////////////////////////
HandlerResult ProfilerView::DoOnUiEvent(event_constptr_t ev) {
  if (ev->_eventcode == ui::EventCode::MOVE) {
    int lx, ly;
    RootToLocal(ev->miX, ev->miY, lx, ly);
    std::string hit;
    for (auto& e : _legend_entries) {
      if (lx >= e.x && lx < e.x + e.w && ly >= e.y && ly < e.y + e.h) {
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
    lev2::rcfd_ptr_t      RCFD) {

  if (channel->_series_iter.empty()) return;

  const int max_label_width = lev2::FontMan::stringWidth(64);

  auto  _gbi  = _context->GBI();
  auto  _mtxi = _context->MTXI();
  auto  _fxi  = _context->FXI();

  auto& sorted_series = channel->_series_iter;

  // Three equally-spaced columns: total | isolated | name
  // col_w driven by the widest header label ("isolated" = 8 chars)
  const int   col_w    = lev2::FontMan::stringWidth(8) + 8;
  const int   W        = width();
  const int   box_x1   = W - int(0.6f * float(max_label_width + 2 * col_w + 20));
  const float chart_x0     = 0.0f;
  const float chart_x1     = float(box_x1 - 8);
  const int   legend_x0    = box_x1 + 4;

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
  const float legend_start_y = lane_y_top + 32.0f;

  // Column headers
  {
    int header_y = int(lane_y_top) + 18;
    _context->RefModColor() = fvec3(0.6f, 0.6f, 0.6f);
    lev2::FontMan::beginTextBlock(_context, 128);
    lev2::FontMan::DrawText(_context, legend_x0,             header_y, "total");
    lev2::FontMan::DrawText(_context, legend_x0 + col_w,     header_y, "isolated");
    lev2::FontMan::DrawText(_context, legend_x0 + 2 * col_w, header_y, "name");
    lev2::FontMan::endTextBlock(_context);
  }

  // Pass 1: assign colors and update exclusive currentValues.
  for (int i = 0; i < (int)sorted_series.size(); i++) {
    auto* series = sorted_series[i];
    auto& rstate = _render_state_map[series->_name];
    if (rstate.color_index < 0) {
      float hue = std::fmod(float(_color_index) * 0.618034f, 1.0f);
      rstate.color.setHSV(hue, 0.65f, 0.8f);
      rstate.color_index = _color_index++;
    }
  }

  // Pass 2: draw legend entries.
  for (int i = 0; i < (int)sorted_series.size(); i++) {
    auto*       series = sorted_series[i];
    const std::string& key = series->_name;
    const char*        sname = key.c_str();
    auto& rstate = _render_state_map[key];

    float entry_y = legend_start_y + float(i) * legend_row_h;
    int   iy      = int(entry_y);

    int name_x  = legend_x0 + 2 * col_w;
    int entry_w = W - name_x;
    _legend_entries.push_back({name_x, iy, entry_w, int(legend_row_h), key});

    bool  hovered    = (!_hovered_series.empty() && key == _hovered_series);
    fvec3 draw_color = hovered ? rstate.color * 1.8f : rstate.color;
    _context->RefModColor() = draw_color;

    float total_ms    = series->_samples.empty() ? 0.0f : float(series->_samples.back().total_time    * 1000.0);
    float isolated_ms = series->_samples.empty() ? 0.0f : float(series->_samples.back().isolated_time * 1000.0);

    // total_time ms
    lev2::FontMan::beginTextBlock(_context, 128);
    lev2::FontMan::DrawText(_context, legend_x0, iy,
        FormatString("%0.2f", total_ms).c_str());
    lev2::FontMan::endTextBlock(_context);

    // isolated_time ms
    lev2::FontMan::beginTextBlock(_context, 128);
    lev2::FontMan::DrawText(_context, legend_x0 + col_w, iy,
        FormatString("%0.2f", isolated_ms).c_str());
    lev2::FontMan::endTextBlock(_context);

    // Series name
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
  for (auto* series : sorted_series) {
    for (auto& sample : series->_samples)
      cum_max_value = std::max(cum_max_value, float(sample.total_time * 1000.0));
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
    _mtl->begin(_tek, RCFD);
    _mtl->bindParamMatrix(_par_mvp, _mtxi->RefMVPMatrix());
    _mtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
    _gbi->DrawPrimitiveEML(sv, lev2::PrimitiveType::LINES);
    _mtl->end(RCFD);
  }

  // ------------------------------------------------------------------
  // Band plots — filled region from (total - isolated) up to total_time,
  // with a bright top edge line.
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

  for (int si = 0; si < (int)sorted_series.size(); si++) {
    auto*       series = sorted_series[si];
    size_t      n      = series->_samples.size();
    const std::string& skey = series->_name;
    auto& rstate = _render_state_map[skey];

    bool any_hover  = !_hovered_series.empty();
    bool is_hovered = (skey == _hovered_series);
    fvec3 line_color = rstate.color;
    if (any_hover)
      line_color = is_hovered ? rstate.color * 1.8f : rstate.color * 0.2f;
    fvec3 fill_color = line_color * 0.25f;

    if (n >= 2) {
      float x_step = (chart_x1 - chart_x0) / float(n - 1);

      // Pass 1: filled band (2 triangles per segment)
      {
        lev2::VtxWriter<vtx_t> vw;
        vw.Lock(_context, _vbuf.get(), (n - 1) * 6);
        for (size_t i = 1; i < n; i++) {
          float tot_prev  = float(series->_samples[i-1].total_time    * 1000.0);
          float tot_curr  = float(series->_samples[i].total_time      * 1000.0);
          float isol_prev = float(series->_samples[i-1].isolated_time * 1000.0);
          float isol_curr = float(series->_samples[i].isolated_time   * 1000.0);
          float sx_prev   = chart_x0 + float(i-1) * x_step;
          float sx_curr   = chart_x0 + float(i)   * x_step;
          float top_prev  = y_bottom - tot_prev  * y_scale;
          float top_curr  = y_bottom - tot_curr  * y_scale;
          float bot_prev  = y_bottom - (tot_prev  - isol_prev)  * y_scale;
          float bot_curr  = y_bottom - (tot_curr  - isol_curr)  * y_scale;
          // Triangle 1
          vw.AddVertex(vtx_t(fvec3(sx_prev, top_prev, 0), fvec4(), fill_color));
          vw.AddVertex(vtx_t(fvec3(sx_curr, top_curr, 0), fvec4(), fill_color));
          vw.AddVertex(vtx_t(fvec3(sx_prev, bot_prev, 0), fvec4(), fill_color));
          // Triangle 2
          vw.AddVertex(vtx_t(fvec3(sx_prev, bot_prev, 0), fvec4(), fill_color));
          vw.AddVertex(vtx_t(fvec3(sx_curr, top_curr, 0), fvec4(), fill_color));
          vw.AddVertex(vtx_t(fvec3(sx_curr, bot_curr, 0), fvec4(), fill_color));
        }
        vw.UnLock(_context);
        _mtl->begin(_tek, RCFD);
        _mtl->bindParamMatrix(_par_mvp, _mtxi->RefMVPMatrix());
        _gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::TRIANGLES);
        _mtl->end(RCFD);
      }

      // Pass 2: top edge line at total_time
      {
        lev2::VtxWriter<vtx_t> vw;
        vw.Lock(_context, _vbuf.get(), (n - 1) * 2);
        for (size_t i = 1; i < n; i++) {
          float tot_prev = float(series->_samples[i-1].total_time * 1000.0);
          float tot_curr = float(series->_samples[i].total_time   * 1000.0);
          float sx_prev  = chart_x0 + float(i-1) * x_step;
          float sx_curr  = chart_x0 + float(i)   * x_step;
          float sy_prev  = y_bottom - tot_prev * y_scale;
          float sy_curr  = y_bottom - tot_curr * y_scale;
          vw.AddVertex(vtx_t(fvec3(sx_prev, sy_prev, 0), fvec4(), line_color));
          vw.AddVertex(vtx_t(fvec3(sx_curr, sy_curr, 0), fvec4(), line_color));
        }
        vw.UnLock(_context);
        _mtl->begin(_tek, RCFD);
        _mtl->bindParamMatrix(_par_mvp, _mtxi->RefMVPMatrix());
        _gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::LINES);
        _mtl->end(RCFD);
      }
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

  auto _mtxi = tgt->MTXI();
  auto RCFD  = std::make_shared<lev2::RenderContextFrameData>(tgt);

  _drawColoredBox(drwev, _bg_color, lev2::BlendingMacro::ALPHA);

  // Collect channels
  struct CtxEntry { std::string label; ork::ProfilerChannel* channel; };
  std::vector<CtxEntry> ctx_channels;
  ctx_channels.push_back({"RenderContext", Profiler::acquireChannel<CpuProfilerChannel>("render_context", "render_context"_crcu)});
  ctx_channels.push_back({"GPU", _context->_gpu_channel.get()});
  // ctx_channels.push_back({"ez_app", Profiler::acquireChannel<CpuProfilerChannel>("ez_app", "ez_app"_crcu)});

  bool any_series = false;
  for (auto& e : ctx_channels)
    if (!e.channel->_series_iter.empty()) { any_series = true; break; }
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
        RCFD);
  }
  _mtxi->PopUIMatrix();

  ork::lev2::FontMan::PopFont();
  fbi->popScissor();
  fbi->popViewport();
}
/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
