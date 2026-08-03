#pragma once
////////////////////////////////////////////////////////////////
// PlayerPerfHud — on-screen performance HUD for ork.ecs.player.exe.
//
// Phase 1: FPS (render thread), UPS (update thread), frame time (ms, avg + worst)
// plus a 2-second history graph of it, and the render-callback record bracket.
// Later phases add the per-ECS-system and per-render-phase breakdowns (engine
// instrumentation feeding stats sinks this HUD reads).
//
// TWO DISTINCT TIME ROWS, never interchangeable:
//   "frame"      — OrkEzAppBase::_frame_period_ms, the wall-clock period between
//                  displayed frames (whole cycle incl. present wait + swap + pump).
//                  The only number that may be read as frame time / 1000-over-it FPS.
//   "cpu-record" — the bracket around this player's onDraw callback (scene command
//                  record + HUD draw). A SUBSET of the frame, typically a small
//                  fraction of it; reporting it as "frame" hid 11-49ms of present
//                  wait, which is why it carries its own name now.
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
#include <cctype>
#include <atomic>

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
#include <ork/lev2/vr/vr_hud_overlay.h>    // VR: publish the panel texture for the DMVR node
#include <ork/ecs/system_stats.h>          // Phase 3: per-ECS-system timings
#include <array>

namespace ork::ecs::player {

using namespace ork;
using namespace ork::lev2;

struct PerfHud {
  enum Mode { OFF = 0, TEXT = 1, GRAPH = 2, NUM_MODES = 3 };

  // atomic: written by the '~' key (main thread) AND the gamepad L2 toggle (update
  //  thread), read by the draw (render thread). Same benign cross-thread pattern the
  //  '~' toggle already had; the L2 add just makes it explicit.
  std::atomic<int>  _mode{OFF};
  int               _on_mode = GRAPH; // mode the pad toggle restores to (last shown)
  std::atomic<bool> _vrmode{false};   // set by the host when DualMonoVr is active
  orkezapp_ptr_t    _ezapp;

  // VR: offscreen RT the HUD content renders into; its texture is published to
  //  VrHudOverlay for the DMVR node to draw as a head-locked panel in both eyes.
  rtgroup_ptr_t _hudRTG;

  // windowed FPS / UPS (from the ezapp atomic counters)
  Timer  _rate_timer;
  int    _prev_render = 0;
  int    _prev_update = 0;
  double _fps = 0.0;
  double _ups = 0.0;

  // TRUE frame time — sampled by the engine at the displayed-frame seam, read here.
  float  _frame_ms          = 0.0f;
  float  _frame_ms_max      = 0.0f; // accumulating worst in the current window
  float  _frame_ms_max_disp = 0.0f;
  std::deque<float> _hist;          // frame-ms ring (~2 s)
  static constexpr size_t kHistMax = 256;

  // render-callback record bracket (onDraw only — NOT frame time, see header)
  Timer  _cpu_record_timer;
  bool   _cpu_record_started = false;
  float  _cpu_record_ms      = 0.0f;

  // ORKID_PLAYER_HUD_STDOUT=<secs>: periodically print the stats text to stdout
  // (headless/DRM/scripted runs where the on-screen HUD can't be read). Works with
  // the on-screen mode OFF, so the cull readbacks stay disabled and don't perturb
  // perf measurements.
  double _stdout_period = 0.0;
  Timer  _stdout_timer;

  // lazily-created graph draw resources
  std::shared_ptr<FreestyleMaterial>                  _mtl;
  std::shared_ptr<DynamicVertexBuffer<SVtxV16T16C16>> _vbuf;

  static int _parseMode(const char* v) {
    if (not v)
      return -1;
    std::string s(v);
    for (auto& c : s)
      c = char(std::tolower((unsigned char)c));
    if (s == "off" or s == "0")
      return OFF;
    if (s == "text" or s == "1")
      return TEXT;
    if (s == "graph" or s == "2")
      return GRAPH;
    int n = atoi(v);
    return ((n % NUM_MODES) + NUM_MODES) % NUM_MODES;
  }

  void init(orkezapp_ptr_t ez) {
    _ezapp = ez;
    _rate_timer.Start();
    _cpu_record_timer.Start();
    // ORKID_PERFHUD forces the startup mode (off|text|graph or 0|1|2). Lets a
    //  keyboardless VR/headless rig get the [perfhud] cull-counter stdout (TEXT mode)
    //  with no input device; the L2 pad toggle then flips it live in the headset.
    if (const char* v = getenv("ORKID_PERFHUD")) {
      int m = _parseMode(v);
      if (m >= 0) {
        _mode = m;
        if (m != OFF)
          _on_mode = m;
      }
    }
    // ORKID_VR_HUD_DIST: head-relative distance (meters) of the in-headset HUD panel.
    if (const char* v = getenv("ORKID_VR_HUD_DIST")) {
      float d = float(atof(v));
      if (d > 0.05f)
        VrHudOverlay::instance()._distance_m = d;
    }
    // ORKID_VR_HUD_YOFF: fraction of the frame height the panel sits BELOW center
    //  (default 0.15 — owner call; 0 = centered, negative = raise).
    if (const char* v = getenv("ORKID_VR_HUD_YOFF")) {
      VrHudOverlay::instance()._yoffset_frac = float(atof(v));
    }
    if (const char* v = getenv("ORKID_PLAYER_HUD_STDOUT")) {
      _stdout_period = atof(v);
      if (_stdout_period <= 0.0)
        _stdout_period = 2.0;
      _stdout_timer.Start();
    }
  }
  // '~' key (desktop): cycle OFF -> TEXT -> TEXT+GRAPH.
  void cycleMode() {
    int m = (_mode.load() + 1) % NUM_MODES;
    _mode = m;
    if (m != OFF)
      _on_mode = m;
  }
  // gamepad L2 (VR): on/off toggle — restores the last shown mode, else hides.
  void toggleShown() {
    int m = _mode.load();
    if (m == OFF)
      _mode = _on_mode;
    else {
      _on_mode = m;
      _mode    = OFF;
    }
  }

  // top of onDraw, before controller->render
  void frameBegin() {
    _cpu_record_timer.Start();
    _cpu_record_started = true;
    // Let the GPU-cull sites read back their result counts THIS frame only while the HUD is showing
    // text (off/graph => zero readback cost, preserving the no-readback cull path).
    CullStats::instance().setEnabled(_mode == TEXT);
  }

  // end of onDraw (after controller->render + movie pump); collects + draws
  void frameEndAndDraw(Context* ctx) {
    if (_cpu_record_started) {
      _cpu_record_ms      = float(_cpu_record_timer.SecsSinceStart() * 1000.0);
      _cpu_record_started = false;
    }
    // Frame time is the engine's wall-clock period, published one frame in arrears —
    //  THIS frame's period can only close after endFrame/present, downstream of here.
    _frame_ms     = _ezapp->_frame_period_ms.load();
    _frame_ms_max = std::max(_frame_ms_max, _frame_ms);
    _hist.push_back(_frame_ms);
    while (_hist.size() > kHistMax)
      _hist.pop_front();
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
    if (_stdout_period > 0.0 and _stdout_timer.SecsSinceStart() >= _stdout_period) {
      _stdout_timer.Start();
      printf("[perfhud]\n%s\n", _statsText().c_str());
      fflush(stdout);
    }
    // VR: don't flat-draw onto the mirror surface. Render the content to an offscreen
    //  RT and publish it; the DualMonoVr node draws it as a head-locked 3m panel into
    //  EACH eye's buffer (which the headset AND the desktop mirror then inherit).
    if (_vrmode.load()) {
      _renderPanelRT(ctx);
      return;
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

    // VR camera diagnostics (terrain-invisibility hunt): the world-root translation and the
    //  composed center-eye world position the VR node used this frame. root=(0,0,0) is the
    //  vrroot-lookup-miss smoking gun. Non-VR/before-any-VR-frame these read zeros — labeled
    //  honestly; the VR node is the only writer.
    {
      const auto& ov = ork::lev2::VrHudOverlay::instance();
      char        cb[128];
      snprintf(cb, sizeof(cb),
               "\nroot %8.1f %8.1f %8.1f\neye  %8.1f %8.1f %8.1f",
               ov._cam_root.x, ov._cam_root.y, ov._cam_root.z,
               ov._cam_eye.x, ov._cam_eye.y, ov._cam_eye.z);
      out += cb;
    }

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

    // wall-clock frame time, then the onDraw record bracket it must never be confused with
    char fb[128];
    snprintf(fb, sizeof(fb),
             "\nframe %5.2f ms (max %5.2f)"
             "\ncpu-record %5.2f ms",
             _frame_ms, _frame_ms_max_disp, _cpu_record_ms);
    out += fb;

    // Phase 2 — engine render-phase breakdown (only rows that actually ran this frame).
    auto snap = RenderPhaseStats::instance().snapshot();
    if (not snap.empty()) {
      // execution order: the shadow-maps..env-probes block is the forward node's frame
      // prologue, which runs INSIDE assemble (its rows are a breakdown of that row).
      static const char* kOrder[] = {"gpuUpdate",    "preRender",    "assemble",     "shadow-maps", "sun-cascades",
                                      "sky-lut",      "sky-ibl",      "env-probes",   "composite",   "hypermesh-gen",
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
    // Split TERR/HYPM into two SHORT lines each — the lens blurs long lines at the panel
    //  edges. total | frustum pass/fail, then occlusion fail(hidden)/pass(drawn).
    auto cull = CullStats::instance().snapshot();
    if (cull.terrain_valid) {
      char b[192];
      snprintf(b, sizeof(b),
               "\nTERR n<%u> frus p<%u> f<%u>"
               "\nTERR occl f<%u> vis<%u>",
               cull.t_total, cull.t_frustum, cull.t_total - cull.t_frustum,
               cull.t_frustum - cull.t_visible, cull.t_visible);
      out += b;
    }
    if (cull.hyper_valid) {
      char b[224];
      snprintf(b, sizeof(b),
               "\nHYPM v<%d> n<%llu> frus p<%llu> f<%llu>"
               "\nHYPM occl f<%llu> vis<%llu>",
               cull.h_variants, (unsigned long long)cull.h_total,
               (unsigned long long)cull.h_frustum, (unsigned long long)(cull.h_total - cull.h_frustum),
               (unsigned long long)cull.h_occluded, (unsigned long long)cull.h_visible);
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

  ///////////////////////////////////////////////////////////////

  void _ensureHudRTG(Context* ctx, int w, int h) {
    if (not _hudRTG) {
      _hudRTG          = std::make_shared<RtGroup>(ctx, w, h, MsaaSamples::MSAA_1X);
      _hudRTG->_name   = "perfhud.panel";
      auto buf         = _hudRTG->createRenderTarget(EBufferFormat::RGBA8);
      buf->_debugName  = "PerfHudPanel";
      buf->_clearColor = fvec4(0, 0, 0, 0); // transparent — only content is visible
    } else if (_hudRTG->width() != w or _hudRTG->height() != h) {
      _hudRTG->Resize(w, h);
    }
  }

  // VR path: render the HUD content into an offscreen RT sized to the content and
  //  publish its texture (+ aspect) to VrHudOverlay. The DMVR output node reads it
  //  next frame and draws it as a head-locked panel in both eyes. Uses a FIXED scale
  //  (not the window SSAA scale) since the panel is magnified in-headset, not viewed
  //  1:1 on-screen.
  void _renderPanelRT(Context* ctx) {
    auto& overlay = VrHudOverlay::instance();
    int   mode    = _mode.load();
    if (mode == OFF) {
      overlay._enabled.store(false);
      return;
    }
    std::string text     = _statsText();
    int         numLines = 1 + int(std::count(text.begin(), text.end(), '\n'));
    // longest line in chars (the debug fonts are fixed-width, so stringWidth is exact)
    int maxchars = 0, cur = 0;
    for (char ch : text) {
      if (ch == '\n') {
        maxchars = std::max(maxchars, cur);
        cur      = 0;
      } else
        cur++;
    }
    maxchars = std::max(maxchars, cur);

    const float uiscale = 2.0f; // fixed crisp panel scale
    auto        fontman = FontMan::instance();
    fontman->setCurrentFont(uiscale >= 2.75f ? "i48" : (uiscale >= 1.5f ? "i32" : "i16"));
    float lineH  = float(FontMan::currentFont()->description().miCharHeight);
    float margin = 12.0f * uiscale;
    float textW  = float(FontMan::stringWidth(maxchars));
    float graphH = (mode == GRAPH) ? (78.0f * uiscale + 8.0f * uiscale) : 0.0f;
    float graphW = (mode == GRAPH) ? 240.0f * uiscale : 0.0f;
    int   TW     = int(std::max(textW, graphW) + 2.0f * margin);
    int   TH     = int(float(numLines) * lineH + graphH + 2.0f * margin);
    TW           = std::max(TW, 16);
    TH           = std::max(TH, 16);

    _ensureHudRTG(ctx, TW, TH);
    auto fbi = ctx->FBI();
    fbi->PushRtGroup(_hudRTG.get());
    ViewportRect rect(0, 0, TW, TH);
    fbi->pushViewport(rect);
    fbi->pushScissor(rect);
    ctx->pushRenderContextFrameData(std::make_shared<RenderContextFrameData>(ctx));

    // The RT holds only the PREMULTIPLIED FOREGROUND (text + graph) over the transparent
    //  clear — NO dark slate here. FontMan's glyph blend writes alpha = coverage (VK ALPHA
    //  macro = alpha factors (ONE,ZERO)), so a slate drawn under it would be PUNCHED to
    //  alpha-0 holes at every glyph quad — the "black box" defect. Instead the slate is a
    //  separate ALPHA quad in the DM eye-pass, and this premultiplied RT composites over it
    //  with PREMA (fringeless, no double-darken). Text over the transparent clear is already
    //  premultiplied (rgb = color*coverage, a = coverage).
    float textTop = margin + graphH; // graph sits above the text block
    if (mode == GRAPH)
      _drawGraph(ctx, float(TW), float(TH), margin, margin, graphW, 78.0f * uiscale);
    _drawText(ctx, float(TW), float(TH), text, margin, textTop);

    ctx->popRenderContextFrameData();
    fbi->popScissor();
    fbi->popViewport();
    fbi->PopRtGroup();

    overlay._texture = _hudRTG->texture(0);
    overlay._aspect  = float(TW) / float(TH);
    overlay._enabled.store(true);
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
