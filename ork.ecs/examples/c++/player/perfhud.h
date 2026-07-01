#pragma once
////////////////////////////////////////////////////////////////
// PlayerPerfHud — on-screen performance HUD for ork.ecs.player.exe.
//
// Phase 1: FPS (render thread), UPS (update thread), and render-thread frame
// work time (ms, avg + worst), plus a 2-second history graph of frame-ms.
// Later phases add the per-ECS-system and per-render-phase breakdowns (engine
// instrumentation feeding stats sinks this HUD reads).
//
// Three modes cycled by the '~' key: OFF -> TEXT -> TEXT+GRAPH. Anchored LOWER-LEFT,
// bottom-aligned by the real font line height (so the block grows upward as lines are
// added) and scaled for HIDPI/SSAA (MSAA needs no handling — the main rtg is MSAA_1X
// resolved, so sample count never changes width/height).
//
// Drawn IMMEDIATE-MODE in the player's onDraw (after controller->render), since the
// player bypasses the EzTopWidget/uicontext widget paint path. Pattern lifted from
// ork.lev2/src/aud/singularity/hud/hud.cpp + the over-scene wrapper
// ork.lev2/src/ez_secondary_win.cpp, with SSAA scaling per scenegraph_render.cpp's
// PICKHUD (ssaaScale = targetW / mainSurfaceWidth).
//
// NOTE: the profiler (ork::Profiler) is compiled out in the default build
// (-DPROFILER=OFF), so this HUD uses always-on lightweight instrumentation only.
////////////////////////////////////////////////////////////////

#include <deque>
#include <string>
#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include <ork/kernel/timer.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/fbi.h>
#include <ork/lev2/gfx/mtxi.h>
#include <ork/lev2/gfx/gbi.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/renderphasestats.h> // Phase 2: engine render-phase timings
#include <ork/ecs/system_stats.h>          // Phase 3: per-ECS-system timings
#include <array>

namespace ork::ecs::player {

using namespace ork;
using namespace ork::lev2;

struct PerfHud {
  enum Mode { OFF = 0, TEXT = 1, GRAPH = 2, NUM_MODES = 3 };

  int            _mode = OFF;
  orkezapp_ptr_t _ezapp;

  // windowed FPS / UPS (from the ezapp atomic counters)
  Timer  _rate_timer;
  int    _prev_render = 0;
  int    _prev_update = 0;
  double _fps = 0.0;
  double _ups = 0.0;

  // render-thread frame-work time
  Timer  _frame_timer;
  bool   _frame_started     = false;
  float  _frame_ms          = 0.0f;
  float  _frame_ms_max      = 0.0f; // accumulating worst in the current window
  float  _frame_ms_max_disp = 0.0f;
  std::deque<float> _hist;          // frame-ms ring (~2 s)
  static constexpr size_t kHistMax = 256;

  // lazily-created graph draw resources
  std::shared_ptr<FreestyleMaterial>                  _mtl;
  std::shared_ptr<DynamicVertexBuffer<SVtxV16T16C16>> _vbuf;

  void init(orkezapp_ptr_t ez) {
    _ezapp = ez;
    _rate_timer.Start();
    _frame_timer.Start();
    if (const char* v = getenv("ORKID_PLAYER_HUD"))
      _mode = atoi(v) % NUM_MODES;
  }
  void cycleMode() { _mode = (_mode + 1) % NUM_MODES; }

  // top of onDraw, before controller->render
  void frameBegin() {
    _frame_timer.Start();
    _frame_started = true;
    // Let the GPU-cull sites read back their result counts THIS frame only while the HUD is showing
    // text (off/graph => zero readback cost, preserving the no-readback cull path).
    CullStats::instance().setEnabled(_mode == TEXT);
  }

  // end of onDraw (after controller->render + movie pump); collects + draws
  void frameEndAndDraw(Context* ctx) {
    if (_frame_started) {
      _frame_ms      = float(_frame_timer.SecsSinceStart() * 1000.0);
      _frame_started = false;
      _frame_ms_max  = std::max(_frame_ms_max, _frame_ms);
      _hist.push_back(_frame_ms);
      while (_hist.size() > kHistMax)
        _hist.pop_front();
    }
    double el = _rate_timer.SecsSinceStart();
    if (el >= 0.25) {
      int rc = _ezapp->_render_count.load();
      int uc = _ezapp->_update_count.load();
      _fps   = double(rc - _prev_render) / el;
      _ups   = double(uc - _prev_update) / el;
      _prev_render       = rc;
      _prev_update       = uc;
      _frame_ms_max_disp = _frame_ms_max;
      _frame_ms_max      = 0.0f;
      _rate_timer.Start();
    }
    if (_mode == OFF)
      return;
    _draw(ctx);
  }

  ///////////////////////////////////////////////////////////////

  std::string _statsText() {
    char hdr[96];
    snprintf(hdr, sizeof(hdr), "FPS %6.1f\nUPS %6.1f", _fps, _ups);
    std::string out(hdr);

    // Phase 3 — per-ECS-system breakdown (u=update thread, g=gpuUpdate, r=render; EMA ms).
    // Sits right after UPS (it's the update-thread context).
    auto sysmap = ork::ecs::SystemStats::instance().snapshot();
    if (not sysmap.empty()) {
      std::map<std::string, std::array<double, 3>> bysys; // SystemType -> {u,g,r}
      for (auto& kv : sysmap) {
        if (kv.first.size() < 3)
          continue;
        char        p    = kv.first[0];
        std::string name = kv.first.substr(2);
        const std::string suf = "System"; // strip the common suffix for width
        if (name.size() > suf.size() and name.compare(name.size() - suf.size(), suf.size(), suf) == 0)
          name.resize(name.size() - suf.size());
        bysys[name][(p == 'u') ? 0 : (p == 'g') ? 1 : 2] = kv.second;
      }
      bool ecshdr = false;
      for (auto& kv : bysys) {
        auto& a = kv.second;
        if (a[0] + a[1] + a[2] < 0.0005) // skip only truly-idle systems (sub-microsecond)
          continue;
        if (not ecshdr) {
          char hb[64]; // same field widths as the rows so columns line up; u@~480/s so it's small
          snprintf(hb, sizeof(hb), "\n%-12.12s %5s %5s %5s (ms)", "ECS", "u", "g", "r");
          out += hb;
          ecshdr = true;
        }
        char b[128];
        // %-12.12s = left-justified, TRUNCATED to exactly 12 (so long names like
        // CharacterController don't push the value columns out of alignment).
        snprintf(b, sizeof(b), "\n%-12.12s %5.3f %5.3f %5.3f", kv.first.c_str(), a[0], a[1], a[2]);
        out += b;
      }
    }

    // render-thread frame work time
    char fb[96];
    snprintf(fb, sizeof(fb), "\nframe %5.2f ms (max %5.2f)", _frame_ms, _frame_ms_max_disp);
    out += fb;

    // Phase 2 — engine render-phase breakdown (only rows that actually ran this frame).
    auto snap = RenderPhaseStats::instance().snapshot();
    if (not snap.empty()) {
      static const char* kOrder[] = {"gpuUpdate",    "preRender", "assemble",     "composite",   "hypermesh-gen",
                                      "hm-cull",      "terrain-cull", "compute-cull", "present-idle"};
      auto row = [&](const std::string& nm, double ms) {
        char b[96];
        snprintf(b, sizeof(b), "\n%-13s %5.2f", nm.c_str(), ms);
        out += b;
      };
      for (const char* nm : kOrder) {
        auto it = snap.find(nm);
        if (it != snap.end()) {
          row(nm, it->second.ms);
          snap.erase(it);
        }
      }
      for (auto& kv : snap) // anything not in the preferred order
        row(kv.first, kv.second.ms);
    }

    // GPU-cull result funnels (terrain + instanced hypermesh). Only present while a cull ran this
    // frame; readback is enabled above (frameBegin) only in TEXT mode. total | frustum pass/fail |
    // of the frustum-passers, occlusion pass(drawn)/fail(hidden).
    auto cull = CullStats::instance().snapshot();
    if (cull.terrain_valid) {
      char b[176];
      snprintf(b, sizeof(b),
               "\nTERR: total<%u> | frustum pass<%u> fail<%u> | +occlusion pass<%u> fail<%u>",
               cull.t_total, cull.t_frustum, cull.t_total - cull.t_frustum,
               cull.t_visible, cull.t_frustum - cull.t_visible);
      out += b;
    }
    if (cull.hyper_valid) {
      char b[208];
      snprintf(b, sizeof(b),
               "\nHYPM: variants<%d> total<%llu> | frustum pass<%llu> fail<%llu> | +occlusion pass<%llu> fail<%llu>",
               cull.h_variants, (unsigned long long)cull.h_total,
               (unsigned long long)cull.h_frustum, (unsigned long long)(cull.h_total - cull.h_frustum),
               (unsigned long long)cull.h_visible, (unsigned long long)cull.h_occluded);
      out += b;
    }
    return out;
  }

  void _draw(Context* ctx) {
    auto fbi = ctx->FBI();
    // Draw into the ACTIVE resolved target. Using its real width/height makes the
    // lower-left anchor land on the visible image regardless of window size, MSAA (the
    // main rtg is MSAA_1X-resolved — sample count never changes width/height) or SSAA
    // (if the active target is supersampled, ssaa>1 scales the whole HUD up so it reads
    // the same on-screen size after the downsample, per the PICKHUD precedent).
    RtGroup* rtg = fbi->_active_rtgroup ? fbi->_active_rtgroup : fbi->_main_rtg.get();
    if (not rtg)
      return;
    float TW      = float(rtg->width());
    float TH      = float(rtg->height());
    float logW    = float(std::max(1, ctx->mainSurfaceWidth()));
    float ssaa    = TW / logW; // 1.0 unless the active target is supersampled
    float uiscale = (ork::lev2::_HIDPI() ? 2.0f : 1.0f) * ssaa;

    ViewportRect rect(0, 0, int(TW), int(TH));
    fbi->pushViewport(rect);
    fbi->pushScissor(rect);
    // clean RCFD (no compositor) so PushUIMatrix uses our viewport dims AND FontMan's
    // endTextBlock has a frame-data on the stack to read (else it derefs empty).
    ctx->pushRenderContextFrameData(std::make_shared<RenderContextFrameData>(ctx));

    // pick a font for the effective scale; line height from its real metrics drives the
    // bottom-anchor, so the block stays aligned however many lines it grows to.
    auto fontman = FontMan::instance();
    fontman->setCurrentFont(uiscale >= 2.75f ? "i48" : (uiscale >= 1.5f ? "i32" : "i16"));
    float lineH = float(FontMan::currentFont()->description().miCharHeight);

    std::string text     = _statsText();
    int         numLines = 1 + int(std::count(text.begin(), text.end(), '\n'));

    float margin  = 12.0f * uiscale;
    float textTop = TH - margin - float(numLines) * lineH; // bottom-anchored

    // graph sits just ABOVE the text block (also lower-left)
    if (_mode == GRAPH) {
      float gw   = 240.0f * uiscale;
      float gh   = 78.0f * uiscale;
      float gbot = textTop - 8.0f * uiscale;
      _drawGraph(ctx, TW, TH, margin, gbot - gh, gw, gh);
    }
    _drawText(ctx, TW, TH, text, margin, textTop);

    ctx->popRenderContextFrameData();
    fbi->popScissor();
    fbi->popViewport();
  }

  void _drawText(Context* ctx, float TW, float TH, const std::string& s, float x, float y) {
    ctx->MTXI()->PushUIMatrix(int(TW), int(TH));
    ctx->PushModColor(fcolor4(0.35f, 1.0f, 0.45f, 1.0f));
    auto fontman = FontMan::instance(); // font already set in _draw
    fontman->beginTextBlock(ctx, s.length());
    fontman->DrawText(ctx, int(x), int(y), "%s", s.c_str());
    fontman->endTextBlock(ctx);
    ctx->PopModColor();
    ctx->MTXI()->PopUIMatrix();
  }

  void _drawGraph(Context* ctx, float TW, float TH, float px, float py, float pw, float ph) {
    if (_hist.size() < 2)
      return;
    if (not _mtl) {
      _mtl = std::make_shared<FreestyleMaterial>();
      _mtl->gpuInit(ctx, "orkshader://solid");
    }
    if (not _vbuf) {
      _vbuf = std::make_shared<DynamicVertexBuffer<SVtxV16T16C16>>(64 << 10, 0);
      _vbuf->SetRingLock(true);
    }
    auto tek     = _mtl->technique("vtxcolor");
    auto par_mvp = _mtl->param("MatMVP");
    auto RCFD    = std::make_shared<RenderContextFrameData>(ctx);
    auto mtxi    = ctx->MTXI();
    auto gbi     = ctx->GBI();

    float maxms = 33.4f; // full-scale = 2 frames @60fps

    mtxi->PushUIMatrix(int(TW), int(TH));

    // (0,0) = top-left, y down. higher ms draws higher on the panel (smaller y).
    // newest sample anchored at the right edge; older scroll left.
    size_t n = _hist.size();
    auto sampleXY = [&](size_t i) -> fvec3 {
      float frac = float(kHistMax - n + i) / float(kHistMax - 1);
      float x    = px + pw * frac;
      float ms   = _hist[i];
      float y    = (py + ph) - ph * std::min(ms / maxms, 1.0f);
      return fvec3(x, y, 0.0f);
    };

    // 1) translucent background panel (ALPHA)
    {
      VtxWriter<SVtxV16T16C16> vw;
      vw.Lock(ctx, _vbuf.get(), 6);
      fvec4 bg(0.0f, 0.0f, 0.0f, 0.45f);
      auto  V = [&](float x, float y) { vw.AddVertex(SVtxV16T16C16(fvec3(x, y, 0), fvec4(), bg)); };
      V(px, py);  V(px + pw, py);       V(px + pw, py + ph);
      V(px, py);  V(px + pw, py + ph);  V(px, py + ph);
      vw.UnLock(ctx);
      _mtl->begin(tek, RCFD);
      _mtl->bindParamMatrix(par_mvp, mtxi->RefMVPMatrix());
      _mtl->_rasterstate->setBlendingMacro(BlendingMacro::ALPHA);
      gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
      _mtl->end(RCFD);
    }

    // 2) 60fps reference line + frame-ms history (LINES)
    {
      float y60 = (py + ph) - ph * std::min(16.67f / maxms, 1.0f);
      VtxWriter<SVtxV16T16C16> vw;
      vw.Lock(ctx, _vbuf.get(), (n - 1) * 2 + 2);
      fvec4 refc(0.5f, 0.5f, 0.5f, 0.6f);
      vw.AddVertex(SVtxV16T16C16(fvec3(px, y60, 0), fvec4(), refc));
      vw.AddVertex(SVtxV16T16C16(fvec3(px + pw, y60, 0), fvec4(), refc));
      fvec4 col(0.35f, 1.0f, 0.45f, 1.0f);
      for (size_t i = 0; i + 1 < n; i++) {
        vw.AddVertex(SVtxV16T16C16(sampleXY(i), fvec4(), col));
        vw.AddVertex(SVtxV16T16C16(sampleXY(i + 1), fvec4(), col));
      }
      vw.UnLock(ctx);
      _mtl->begin(tek, RCFD);
      _mtl->bindParamMatrix(par_mvp, mtxi->RefMVPMatrix());
      _mtl->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
      gbi->DrawPrimitiveEML(vw, PrimitiveType::LINES);
      _mtl->end(RCFD);
    }

    mtxi->PopUIMatrix();
  }
};

} // namespace ork::ecs::player
