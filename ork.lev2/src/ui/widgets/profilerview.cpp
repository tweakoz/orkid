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

// Header bar layout — computed from widget width so draw and hit-test agree
struct HdrLayout {
  static constexpr int BTN_W = 48, BTN_H = 18, BTN_Y = 4;
  static constexpr int SMP_BTN_W = 24; // width of [-] and [+]

  int btn_x;        // play/pause button
  int rec_x;        // "Record:" label
  int smp_label_x;  // "Samples:" label
  int smp_minus_x;  // [-] button
  int smp_num_x;    // sample count text
  int smp_plus_x;   // [+] button

  explicit HdrLayout(int widget_w) {
    btn_x      = (widget_w - BTN_W) / 2;
    rec_x      = btn_x - 80;          // "Record: " ~9 chars × ~8px + gap
    smp_label_x = btn_x + BTN_W + 10;
    smp_minus_x = smp_label_x + 80;   // "Samples: " ~9 chars × ~8px + gap
    smp_num_x   = smp_minus_x + SMP_BTN_W + 4;
    smp_plus_x  = smp_num_x  + 36 + 4; // room for "1024"
  }

  bool inBtn     (int x, int y) const { return x>=btn_x       && x<btn_x+BTN_W       && y>=BTN_Y && y<BTN_Y+BTN_H; }
  bool inSmpMinus(int x, int y) const { return x>=smp_minus_x && x<smp_minus_x+SMP_BTN_W && y>=BTN_Y && y<BTN_Y+BTN_H; }
  bool inSmpPlus (int x, int y) const { return x>=smp_plus_x  && x<smp_plus_x +SMP_BTN_W && y>=BTN_Y && y<BTN_Y+BTN_H; }
};
///////////////////////////////////////////////////////////////////////////////
ProfilerView::ProfilerView()
    : Widget("ProfilerView", 0, 0, 32, 32) {
}
/////////////////////////////////////////////////////////////////////////
HandlerResult ProfilerView::DoOnUiEvent(event_constptr_t ev) {
  HdrLayout lo(width());

  if (ev->_eventcode == ui::EventCode::PUSH) {
    int lx, ly;
    RootToLocal(ev->miX, ev->miY, lx, ly);

    if (lo.inBtn(lx, ly)) {
      Profiler::_enabled.store(!Profiler::_enabled.load());
      SetDirty();
      return HandlerResult();
    }
    if (lo.inSmpMinus(lx, ly)) {
      _held_smp_delta  = -8;
      _held_smp_frames = 0;
      Profiler::maxSamples(u16(std::max(16, int(Profiler::maxSamples()) - 8)));
      SetDirty();
      HandlerResult rval; rval.setHandled(this); return rval;
    }
    if (lo.inSmpPlus(lx, ly)) {
      _held_smp_delta  = +8;
      _held_smp_frames = 0;
      Profiler::maxSamples(u16(std::min(1024, int(Profiler::maxSamples()) + 8)));
      SetDirty();
      HandlerResult rval; rval.setHandled(this); return rval;
    }
    // Click in graph area — pause and begin scrub
    constexpr float GRAPHS_TOP = 28.0f;
    if (float(ly) >= GRAPHS_TOP) {
      Profiler::_enabled.store(false);
      _scrub_x      = float(lx);
      _is_scrubbing = true;
      SetDirty();
      HandlerResult rval;
      rval.setHandled(this);
      return rval;
    }
  }

  if (ev->_eventcode == ui::EventCode::DRAG) {
    if (_is_scrubbing) {
      int lx, ly;
      RootToLocal(ev->miX, ev->miY, lx, ly);
      _scrub_x = float(lx);
      SetDirty();
      HandlerResult rval;
      rval.setHandled(this);
      return rval;
    }
  }

  if (ev->_eventcode == ui::EventCode::RELEASE) {
    _is_scrubbing    = false;
    _held_smp_delta  = 0;
    _held_smp_frames = 0;
  }

  if (ev->_eventcode == ui::EventCode::MOUSEWHEEL && ev->mbSHIFT) {
    int delta     = (ev->miMWY > 0) ? 8 : -8;
    int new_val   = std::clamp(int(Profiler::maxSamples()) + delta, 16, 1024);
    Profiler::maxSamples(u16(new_val));
    SetDirty();
    return HandlerResult();
  }

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
    lev2::Context*        ctx,
    const std::string&    channel_label,
    ork::ProfilerChannel* channel,
    float                 lane_y_top,
    float                 lane_height,
    lev2::rcfd_ptr_t      RCFD) {

  if (channel->_series_iter.empty()) return;

  const int max_label_width = lev2::FontMan::stringWidth(64);

  auto  _gbi  = ctx->GBI();
  auto  _mtxi = ctx->MTXI();
  auto  _fxi  = ctx->FXI();

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
    ctx->RefModColor() = fvec3(0.9f, 0.9f, 0.9f);
    lev2::FontMan::beginTextBlock(ctx, 128);
    lev2::FontMan::DrawText(ctx, header_x, int(lane_y_top) + 4, channel_label.c_str());
    lev2::FontMan::endTextBlock(ctx);
  }

  // ------------------------------------------------------------------
  // Per-series legend entries
  // ------------------------------------------------------------------
  const float legend_row_h   = 14.0f;
  const float legend_start_y = lane_y_top + 32.0f;

  // Column headers
  {
    int header_y = int(lane_y_top) + 18;
    ctx->RefModColor() = fvec3(0.6f, 0.6f, 0.6f);
    lev2::FontMan::beginTextBlock(ctx, 128);
    lev2::FontMan::DrawText(ctx, legend_x0,             header_y, "total");
    lev2::FontMan::DrawText(ctx, legend_x0 + col_w,     header_y, "isolated");
    lev2::FontMan::DrawText(ctx, legend_x0 + 2 * col_w, header_y, "name");
    lev2::FontMan::endTextBlock(ctx);
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
    ctx->RefModColor() = draw_color;

    float total_ms    = 0.0f;
    float isolated_ms = 0.0f;
    if (!series->_samples.empty()) {
      size_t n   = series->_samples.size();
      int    idx = int(n) - 1; // default: last sample
      if (_scrub_x >= chart_x0 && _scrub_x <= chart_x1) {
        float norm = (_scrub_x - chart_x0) / (chart_x1 - chart_x0);
        idx = std::clamp(int(norm * float(n - 1) + 0.5f), 0, int(n) - 1);
      }
      total_ms    = float(series->_samples[idx].total_time    * 1000.0);
      isolated_ms = float(series->_samples[idx].isolated_time * 1000.0);
    }

    // total_time ms
    lev2::FontMan::beginTextBlock(ctx, 128);
    lev2::FontMan::DrawText(ctx, legend_x0, iy,
        FormatString("%0.2f", total_ms).c_str());
    lev2::FontMan::endTextBlock(ctx);

    // isolated_time ms
    lev2::FontMan::beginTextBlock(ctx, 128);
    lev2::FontMan::DrawText(ctx, legend_x0 + col_w, iy,
        FormatString("%0.2f", isolated_ms).c_str());
    lev2::FontMan::endTextBlock(ctx);

    // Series name
    lev2::FontMan::beginTextBlock(ctx, 128);
    lev2::FontMan::DrawText(ctx, name_x, iy, sname);
    lev2::FontMan::endTextBlock(ctx);
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
    sv.Lock(ctx, _vbuf.get(), 2);
    sv.AddVertex(vtx_t(fvec3(0,       lane_y_top, 0), fvec4(), fvec3(0.3f, 0.3f, 0.3f)));
    sv.AddVertex(vtx_t(fvec3(chart_x1,lane_y_top, 0), fvec4(), fvec3(0.3f, 0.3f, 0.3f)));
    sv.UnLock(ctx);
    _mtl->begin(_tek, RCFD);
    _mtl->bindParamMatrix(_par_mvp, _mtxi->RefMVPMatrix());
    _mtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
    _gbi->DrawPrimitiveEML(sv, lev2::PrimitiveType::LINES);
    _mtl->end(RCFD);
  }

  // ------------------------------------------------------------------
  // Horizontal max-value line at y_top_line, labeled with series_max
  // ------------------------------------------------------------------
  {
    lev2::VtxWriter<vtx_t> sv;
    sv.Lock(ctx, _vbuf.get(), 2);
    sv.AddVertex(vtx_t(fvec3(0,       y_top_line, 0), fvec4(), fvec3(0.5f, 0.5f, 0.5f)));
    sv.AddVertex(vtx_t(fvec3(chart_x1,y_top_line, 0), fvec4(), fvec3(0.5f, 0.5f, 0.5f)));
    sv.UnLock(ctx);
    _mtl->begin(_tek, RCFD);
    _mtl->bindParamMatrix(_par_mvp, _mtxi->RefMVPMatrix());
    _mtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
    _gbi->DrawPrimitiveEML(sv, lev2::PrimitiveType::LINES);
    _mtl->end(RCFD);

    ctx->RefModColor() = fvec3(0.7f, 0.7f, 0.7f);
    lev2::FontMan::beginTextBlock(ctx, 32);
    lev2::FontMan::DrawText(ctx, 2, int(y_top_line) + 2, FormatString("%.2fms", series_max).c_str());
    lev2::FontMan::endTextBlock(ctx);
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
        vw.Lock(ctx, _vbuf.get(), (n - 1) * 6);
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
        vw.UnLock(ctx);
        _mtl->begin(_tek, RCFD);
        _mtl->bindParamMatrix(_par_mvp, _mtxi->RefMVPMatrix());
        _gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::TRIANGLES);
        _mtl->end(RCFD);
      }

      // Pass 2: top edge line at total_time
      {
        lev2::VtxWriter<vtx_t> vw;
        vw.Lock(ctx, _vbuf.get(), (n - 1) * 2);
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
        vw.UnLock(ctx);
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

  // ------------------------------------------------------------------
  // Scrub line — vertical marker at _scrub_x
  // ------------------------------------------------------------------
  float scrub_cx = std::clamp(_scrub_x, chart_x0, chart_x1);
  if (_scrub_x >= chart_x0 && _scrub_x <= chart_x1) {
    lev2::VtxWriter<vtx_t> sv;
    sv.Lock(ctx, _vbuf.get(), 2);
    sv.AddVertex(vtx_t(fvec3(scrub_cx, y_top_line, 0), fvec4(), fvec3(1.0f, 1.0f, 0.4f)));
    sv.AddVertex(vtx_t(fvec3(scrub_cx, y_bottom,   0), fvec4(), fvec3(1.0f, 1.0f, 0.4f)));
    sv.UnLock(ctx);
    _mtl->begin(_tek, RCFD);
    _mtl->bindParamMatrix(_par_mvp, _mtxi->RefMVPMatrix());
    _mtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
    _gbi->DrawPrimitiveEML(sv, lev2::PrimitiveType::LINES);
    _mtl->end(RCFD);
  }
}
/////////////////////////////////////////////////////////////////////////
void ProfilerView::DoDraw(drawevent_constptr_t drwev) {
  if (_channel_names.empty()) return;

  // Auto-repeat for held [-]/[+] buttons: 20-frame initial delay, then every 4 frames
  if (_held_smp_delta != 0) {
    _held_smp_frames++;
    constexpr int INITIAL_DELAY = 20;
    constexpr int REPEAT_EVERY  = 4;
    if (_held_smp_frames > INITIAL_DELAY && (_held_smp_frames - INITIAL_DELAY) % REPEAT_EVERY == 0) {
      int nv = std::clamp(int(Profiler::maxSamples()) + _held_smp_delta, 16, 1024);
      Profiler::maxSamples(u16(nv));
    }
  }

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

  // Default scrub line to the center of the chart on first draw
  if (_scrub_x < 0.0f)
    _scrub_x = float(width()) / 2.0f;

  // Collect channels by name (skip any not yet registered)
  struct CtxEntry { std::string label; ork::ProfilerChannel* channel; };
  std::vector<CtxEntry> ctx_channels;
  for (auto& name : _channel_names) {
    auto crc = CrcString(name.c_str()).hashed();
    auto it  = Profiler::_channels.find(crc);
    if (it != Profiler::_channels.end())
      ctx_channels.push_back({name, it->second.get()});
  }

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

  size_t      num_channels  = ctx_channels.size();
  const float gap           = 10.0f;
  const float graphs_top    = 28.0f; // leave room for play/pause button
  float       lane_height   = (float(height()) - graphs_top - gap * float(num_channels - 1)) / float(num_channels);

  _legend_entries.clear();

  _mtxi->PushUIMatrix(width(), height());

  // ------------------------------------------------------------------
  // Header bar: [Record:] [||] ... [Samples:] [-] [128] [+]
  // ------------------------------------------------------------------
  {
    HdrLayout lo(width());
    auto      gbi     = tgt->GBI();
    bool      running = Profiler::_enabled.load();
    int       smp     = int(Profiler::maxSamples());

    // helper: draw a small filled button rect
    auto drawSmallBtn = [&](int x, fvec3 col) {
      lev2::VtxWriter<vtx_t> vw;
      vw.Lock(tgt, _vbuf.get(), 6);
      vw.AddVertex(vtx_t(fvec3(x,                       HdrLayout::BTN_Y,                       0), fvec4(), col));
      vw.AddVertex(vtx_t(fvec3(x + HdrLayout::SMP_BTN_W, HdrLayout::BTN_Y,                       0), fvec4(), col));
      vw.AddVertex(vtx_t(fvec3(x,                       HdrLayout::BTN_Y + HdrLayout::BTN_H,    0), fvec4(), col));
      vw.AddVertex(vtx_t(fvec3(x,                       HdrLayout::BTN_Y + HdrLayout::BTN_H,    0), fvec4(), col));
      vw.AddVertex(vtx_t(fvec3(x + HdrLayout::SMP_BTN_W, HdrLayout::BTN_Y,                       0), fvec4(), col));
      vw.AddVertex(vtx_t(fvec3(x + HdrLayout::SMP_BTN_W, HdrLayout::BTN_Y + HdrLayout::BTN_H,    0), fvec4(), col));
      vw.UnLock(tgt);
      _mtl->begin(_tek, RCFD);
      _mtl->bindParamMatrix(_par_mvp, _mtxi->RefMVPMatrix());
      _mtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
      gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::TRIANGLES);
      _mtl->end(RCFD);
    };

    // Play/pause button
    fvec3 play_col = running ? fvec3(0.1f, 0.45f, 0.1f) : fvec3(0.45f, 0.1f, 0.1f);
    {
      lev2::VtxWriter<vtx_t> vw;
      vw.Lock(tgt, _vbuf.get(), 6);
      vw.AddVertex(vtx_t(fvec3(lo.btn_x,              HdrLayout::BTN_Y,                    0), fvec4(), play_col));
      vw.AddVertex(vtx_t(fvec3(lo.btn_x + HdrLayout::BTN_W, HdrLayout::BTN_Y,              0), fvec4(), play_col));
      vw.AddVertex(vtx_t(fvec3(lo.btn_x,              HdrLayout::BTN_Y + HdrLayout::BTN_H, 0), fvec4(), play_col));
      vw.AddVertex(vtx_t(fvec3(lo.btn_x,              HdrLayout::BTN_Y + HdrLayout::BTN_H, 0), fvec4(), play_col));
      vw.AddVertex(vtx_t(fvec3(lo.btn_x + HdrLayout::BTN_W, HdrLayout::BTN_Y,              0), fvec4(), play_col));
      vw.AddVertex(vtx_t(fvec3(lo.btn_x + HdrLayout::BTN_W, HdrLayout::BTN_Y + HdrLayout::BTN_H, 0), fvec4(), play_col));
      vw.UnLock(tgt);
      _mtl->begin(_tek, RCFD);
      _mtl->bindParamMatrix(_par_mvp, _mtxi->RefMVPMatrix());
      _mtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
      gbi->DrawPrimitiveEML(vw, lev2::PrimitiveType::TRIANGLES);
      _mtl->end(RCFD);
    }

    // [-] and [+] buttons
    fvec3 smp_btn_col(0.25f, 0.25f, 0.35f);
    drawSmallBtn(lo.smp_minus_x, smp_btn_col);
    drawSmallBtn(lo.smp_plus_x,  smp_btn_col);

    // Text labels
    tgt->RefModColor() = fvec3(0.7f, 0.7f, 0.7f);
    lev2::FontMan::beginTextBlock(tgt, 64);
    lev2::FontMan::DrawText(tgt, lo.rec_x,       HdrLayout::BTN_Y + 2, "Record:");
    lev2::FontMan::DrawText(tgt, lo.smp_label_x, HdrLayout::BTN_Y + 2, "Samples:");
    lev2::FontMan::DrawText(tgt, lo.smp_num_x,   HdrLayout::BTN_Y + 2, FormatString("%d", smp).c_str());
    lev2::FontMan::endTextBlock(tgt);

    // Play/pause icon and [-]/[+] labels in white
    tgt->RefModColor() = fvec3(1.0f, 1.0f, 1.0f);
    lev2::FontMan::beginTextBlock(tgt, 16);
    lev2::FontMan::DrawText(tgt, lo.btn_x + 6,        HdrLayout::BTN_Y + 2, running ? "||" : " >");
    lev2::FontMan::DrawText(tgt, lo.smp_minus_x + 7,  HdrLayout::BTN_Y + 2, "-");
    lev2::FontMan::DrawText(tgt, lo.smp_plus_x  + 7,  HdrLayout::BTN_Y + 2, "+");
    lev2::FontMan::endTextBlock(tgt);
  }

  for (size_t ci = 0; ci < num_channels; ci++) {
    _drawContextChannel(
        tgt,
        ctx_channels[ci].label,
        ctx_channels[ci].channel,
        graphs_top + float(ci) * (lane_height + gap),
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
