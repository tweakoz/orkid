#pragma once
////////////////////////////////////////////////////////////////
// PlayerPerfHud — on-screen performance HUD for ork.ecs.player.exe.
//
// FIVE FIXED PAGES of named slots (perfhud_pages.h owns the templates + the slot
// registry; this file owns the sinks, the input plumbing and the draw):
//   1 FRAME    rates, frame time + worst, the record bracket, gpuUpdate, present-idle,
//              and the 2-second frame-time history graph (which lives on this page).
//   2 GPU      per-pass DEVICE time from timestamp queries (render passes, compute
//              phases, XR blits) under the whole-frame GPU span.
//   3 PASSES   the engine render-phase breakdown, in pipeline order.
//   4 CULL     the cull/dispatch phases + the GPU-cull result funnels.
//   5 SYSTEMS  the per-ECS-system breakdown, the VR camera rows, hypermesh-gen.
// Every slot of the active page always renders; a silent sink keeps its last value and
// shows its age in frames, so page height is constant and the cadenced rows stay
// legible (see perfhud_pages.h).
//
// PLUS any EDITOR PAGES the host registers (registerEditorPage), which extend the same
// ring behind the fixed four. An editor page is a list of live property descriptors —
// the host owns the get/set closures, the page owns the layout — so a new one costs no
// layout code. Selection is DPAD up/down (pad) or '-' / '=' (desktop); L2/R2 or
// '[' / ']' adjust the selected property, sliders repeating while held and enums
// stepping once per press (see editAdjustHold). The desktop keys deliberately avoid the
// cursor keys, which stay the scene's rotation. A COLOR row is a REUSABLE H/S/V
// sub-editor on one line: CROSS (pad) or '\' (desktop) opens it, up/down then walk the
// channels and the adjust inputs move the open one — no row ever appears or disappears.
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
// Pages cycled by the '~' key (and by pad R1/R3 forward, SHIFT-'~' / pad L1 back):
// OFF -> 1 .. N -> OFF, wrapping the same way in both directions. Anchored
// LOWER-LEFT, bottom-aligned by the real font line height off the ACTIVE PAGE's fixed
// line count, and scaled for HIDPI/SSAA (MSAA needs no handling — the main rtg is
// MSAA_1X resolved, so sample count never changes width/height).
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
#include <ork/lev2/gfx/gpupassstats.h>     // the GPU page: per-pass device timestamps + XR pacing
#include <ork/lev2/gfx/nvmlstats.h>        // the GPU page: NVIDIA driver telemetry (when present)
#include <ork/lev2/vr/vr_hud_overlay.h>    // VR: publish the panel texture for the VR output node
#include <ork/ecs/system_stats.h>          // Phase 3: per-ECS-system timings
#include "perfhud_pages.h"                 // the slot registry + fixed page templates
#include <array>

namespace ork::ecs::player {

using namespace ork;
using namespace ork::lev2;

struct PerfHud {
  enum {
    PAGE_OFF     = PerfHudPages::PAGE_OFF,
    PAGE_FRAME   = PerfHudPages::PAGE_FRAME,
    PAGE_GPU     = PerfHudPages::PAGE_GPU,
    PAGE_PASSES  = PerfHudPages::PAGE_PASSES,
    PAGE_CULL    = PerfHudPages::PAGE_CULL,
    PAGE_SYSTEMS = PerfHudPages::PAGE_SYSTEMS,
    NUM_PAGES    = PerfHudPages::NUM_PAGES,
  };

  // atomic: written by the '~' key (main thread) AND the gamepad toggles (update
  //  thread), read by the draw (render thread). Same benign cross-thread pattern the
  //  '~' toggle always had; the pad add just makes it explicit.
  std::atomic<int>  _page{PAGE_OFF};
  std::atomic<bool> _vrmode{false}; // set by the host when a VR render model is active
  orkezapp_ptr_t    _ezapp;
  PerfHudPages      _pages; // slot registry + page templates (render thread only)

  // VR: offscreen RT the HUD content renders into; its texture is published to
  //  VrHudOverlay for the VR output node to draw as a head-locked panel in both eyes.
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

  // ORKID_PLAYER_HUD_STDOUT=<secs>: periodically print EVERY page to stdout (headless/
  // DRM/scripted runs where the on-screen HUD can't be read and there is no key to cycle
  // with). Works with the on-screen page OFF — but it does arm the cull readbacks, since
  // a dump with empty cull funnels would be a dump of nothing.
  double _stdout_period = 0.0;
  Timer  _stdout_timer;

  // ORKID_PERFHUD verbatim — re-resolved after each editor-page registration.
  std::string _startup_page;

  // editor held-adjust auto-repeat (update thread only)
  int    _adj_dir  = 0;
  double _adj_held = 0.0;
  static constexpr double kAdjDelay  = 0.30; // seconds held before a slider repeats
  static constexpr double kAdjPeriod = 0.04; // seconds between repeats

  // lazily-created graph draw resources
  std::shared_ptr<FreestyleMaterial>                  _mtl;
  std::shared_ptr<DynamicVertexBuffer<SVtxV16T16C16>> _vbuf;

  int _parsePage(const std::string& v) const {
    if (v.empty())
      return -1;
    std::string s(v);
    for (auto& c : s)
      c = char(std::tolower((unsigned char)c));
    if (s == "off")
      return PAGE_OFF;
    if (s == "frame")
      return PAGE_FRAME;
    if (s == "gpu")
      return PAGE_GPU;
    if (s == "passes")
      return PAGE_PASSES;
    if (s == "cull")
      return PAGE_CULL;
    if (s == "systems")
      return PAGE_SYSTEMS;
    // registered editor pages answer to their registered name, uppercased ("sky", "post")
    for (int p = PerfHudPages::NUM_PAGES; p < _pages.numPages(); p++) {
      std::string n = _pages.editorPage(p)->_name;
      for (auto& c : n)
        c = char(std::tolower((unsigned char)c));
      if (n == s)
        return p;
    }
    if (s.find_first_not_of("+-0123456789") != std::string::npos)
      return -1; // a name we do not know yet — it may register later
    int np = _pages.numPages();
    int n  = atoi(v.c_str());
    return ((n % np) + np) % np;
  }

  // (re)apply ORKID_PERFHUD. Called at init AND after every editor-page registration,
  //  since a page selected BY NAME cannot resolve before that page exists.
  void _applyStartupPage() {
    if (_startup_page.empty())
      return;
    int p = _parsePage(_startup_page);
    if (p < 0)
      return;
    _page = p;
  }

  void init(orkezapp_ptr_t ez) {
    _ezapp = ez;
    _rate_timer.Start();
    _cpu_record_timer.Start();
    // ORKID_PERFHUD forces the startup page (off|frame|gpu|passes|cull|systems|<editor page
    //  name> or an index). Lets a keyboardless VR/headless rig raise the HUD with no
    //  input device; the pad bumpers then page it live in the headset.
    if (const char* v = getenv("ORKID_PERFHUD")) {
      _startup_page = v;
      _applyStartupPage();
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
  // '~' / pad R1 / pad R3 forward, SHIFT-'~' / pad L1 back: step the page ring
  // OFF -> 1 .. N -> OFF (N includes the registered editor pages, which sit behind the
  // fixed ones), wrapping through OFF whichever way it is walked.
  void cyclePage(int dir = +1) {
    _page = _pages.stepPage(_page.load(), dir);
  }

  ///////////////////////////////////////////////////////////////
  // EDITOR PAGES — registration + the input surface. See perfhud_pages.h for the
  // property descriptors; the host owns the get/set closures and nothing else.
  ///////////////////////////////////////////////////////////////

  int registerEditorPage(const std::string& name, std::vector<HudEditProp> props) {
    int page = _pages.registerEditorPage(name, std::move(props));
    _applyStartupPage(); // ORKID_PERFHUD=<name> can only resolve now
    return page;
  }
  bool editorActive() const {
    return _pages.isEditorPage(_page.load());
  }
  // DPAD up/down (pad) and cursor up/down (desktop) — once per press. With a color row
  //  open these walk its H/S/V channels instead (see PerfHudPages::editSelect).
  void editSelect(int delta) {
    _pages.editSelect(_page.load(), delta);
  }
  // CROSS / '\' — the ACTIVATE gesture: opens or closes a COLOR row's H/S/V sub-editor,
  // or FIRES an ACTION row (see perfhud_pages.h). Consumed by the host only when
  // editSelectedIsActivatable(), so off such a row it still reaches the scene.
  void editActivate() {
    _pages.editActivate(_page.load());
  }
  bool editSelectedIsActivatable() const {
    return _pages.editSelectedIsActivatable(_page.load());
  }
  void editToggleColor() {
    _pages.editToggleColor(_page.load());
  }
  bool editSelectedIsColor() const {
    return _pages.editSelectedIsColor(_page.load());
  }
  // L2/R2 (pad) and cursor left/right (desktop). HELD-state driven, ticked from the
  //  update thread: a slider repeats after kAdjDelay at kAdjPeriod (so a held trigger
  //  sweeps its range), an enum steps exactly once per press. dir = -1 / 0 / +1.
  void editAdjustHold(int dir, double dt) {
    int page = _page.load();
    if (not _pages.isEditorPage(page)) {
      _adj_dir = 0;
      return;
    }
    if (dir != _adj_dir) { // press / release / reversal edge
      _adj_dir  = dir;
      _adj_held = 0.0;
      if (dir)
        _pages.editAdjust(page, dir);
      return;
    }
    if (dir == 0 or not _pages.editSelectedIsSlider(page))
      return;
    double prev = _adj_held;
    _adj_held += dt;
    if (_adj_held < kAdjDelay)
      return;
    int fired = int((_adj_held - kAdjDelay) / kAdjPeriod) - int(std::max(0.0, prev - kAdjDelay) / kAdjPeriod);
    for (int i = 0; i < fired; i++)
      _pages.editAdjust(page, dir);
  }
  // top of onDraw, before controller->render
  void frameBegin() {
    _cpu_record_timer.Start();
    _cpu_record_started = true;
    // Let the GPU-cull sites read back their result counts THIS frame only while the HUD
    // is up (or feeding the stdout dump) — hidden => zero readback cost, preserving the
    // no-readback cull path. Any page, not just the cull page: the funnels are cheap to
    // keep current, and a slot that only refreshed while ITS page was showing would carry
    // a misleading age the moment the reader arrived.
    CullStats::instance().setEnabled(_page.load() != PAGE_OFF or _stdout_period > 0.0);
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
    // Refresh the slots ONCE per frame, and only when someone is reading them (hidden =>
    // no sink snapshots, and no age accrues while the HUD is down — see perfhud_pages.h).
    const bool want_stdout = _stdout_period > 0.0 and _stdout_timer.SecsSinceStart() >= _stdout_period;
    if (_page.load() != PAGE_OFF or want_stdout)
      _pages.publish(_gatherInputs());
    if (want_stdout) {
      _stdout_timer.Start();
      // headless/DRM/scripted runs have no key to cycle with, so dump EVERY page.
      printf("[perfhud]\n");
      for (int p = PAGE_FRAME; p < _pages.numPages(); p++)
        printf("%s\n", _pages.pageText(p).c_str());
      fflush(stdout);
    }
    // VR: don't flat-draw onto the mirror surface. Render the content to an offscreen
    //  RT and publish it; the VR output node draws it as a head-locked 3m panel into
    //  EACH eye's buffer (which the headset AND the desktop mirror then inherit).
    if (_vrmode.load()) {
      _renderPanelRT(ctx);
      return;
    }
    if (_page.load() == PAGE_OFF)
      return;
    _draw(ctx);
  }

  ///////////////////////////////////////////////////////////////

  // Snapshot every sink ONCE per frame. Nothing is formatted here — the pages own the
  // layout (perfhud_pages.h), which is what makes them testable off-device.
  HudInputs _gatherInputs() {
    HudInputs in;
    in._fps          = _fps;
    in._ups          = _ups;
    in._frame_ms     = _frame_ms;
    in._frame_ms_max = _frame_ms_max_disp;
    in._cpu_record_ms = _cpu_record_ms;

    // engine render-phase breakdown — only the phases that actually ran this frame; the
    // rest keep their slot and age (a cadenced phase reports its cadence that way).
    for (const auto& kv : RenderPhaseStats::instance().snapshot())
      in._phases[kv.first] = kv.second.ms;

    in._cull = CullStats::instance().snapshot();

    // per-pass DEVICE time, as of the timestamp readback's lag-2 frame (the sink carries
    // the lag; see gpupassstats.h).
    in._gpu = GpuPassStats::instance().snapshot();

    // XR frame pacing (published only by a live XR frame path) and NVIDIA driver
    // telemetry (polled at 1Hz off a background thread, started by this first read).
    // Both answer "unavailable" off their platform, which is what makes their HUD rows
    // absent rather than empty.
    in._vr   = ork::lev2::VrPacingStats::instance().snapshot();
    in._nvml = ork::lev2::NvmlStats::instance().snapshot();

    // per-ECS-system breakdown (u=update thread, g=gpuUpdate, r=render; EMA ms), keyed
    // "<phase>:<SystemType>" by the sink.
    for (const auto& kv : ork::ecs::SystemStats::instance().snapshot()) {
      if (kv.first.size() < 3)
        continue;
      char        p    = kv.first[0];
      std::string name = kv.first.substr(2);
      in._systems[name][(p == 'u') ? 0 : (p == 'g') ? 1 : 2] = kv.second;
    }

    // VR camera diagnostics (terrain-invisibility hunt): the world-root translation and
    //  the composed center-eye world position the VR node used this frame. root=(0,0,0)
    //  is the vrroot-lookup-miss smoking gun. Non-VR/before-any-VR-frame these read zeros
    //  — labeled honestly; the VR node is the only writer.
    const auto& ov = ork::lev2::VrHudOverlay::instance();
    in._root[0] = ov._cam_root.x; in._root[1] = ov._cam_root.y; in._root[2] = ov._cam_root.z;
    in._eye[0]  = ov._cam_eye.x;  in._eye[1]  = ov._cam_eye.y;  in._eye[2]  = ov._cam_eye.z;
    return in;
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
    // bottom-anchor off the ACTIVE PAGE's fixed line count, so the block's baseline never
    // moves while a page is up.
    auto fontman = FontMan::instance();
    fontman->setCurrentFont(uiscale >= 2.75f ? "i48" : (uiscale >= 1.5f ? "i32" : "i16"));
    float lineH = float(FontMan::currentFont()->description().miCharHeight);

    int         page  = _page.load();
    auto        lines = _pages.pageLines(page);
    std::string text;
    // WIDEST RENDERED LINE, not the page's nominal width. The nominal is a per-page
    // constant used for CENTERING the VR panel inside its fixed 52-column allocation, and
    // on the pages whose rows carry variable-width payloads (the cull page's in<>/out<>
    // counts) the real line runs past it — which is exactly where a nominal-sized backdrop
    // stopped short of the text. VR never showed it because the VR panel is sized to the
    // fixed column allocation, wider than any page.
    int cols = _pages.pageNominalCols(page);
    for (const auto& l : lines) {
      cols = std::max(cols, int(l.size()));
      if (not text.empty())
        text += "\n";
      text += l;
    }
    int numLines = _pages.pageLineCount(page);

    float margin  = 12.0f * uiscale;
    float textTop = TH - margin - float(numLines) * lineH; // bottom-anchored

    // THE PANEL SLATE, desktop edition — the same rgba the VR node draws behind the
    // head-locked panel (VrHudOverlay::_slate_rgba, one source for both), mono and sized
    // to THIS page's fixed text block rather than the frame: it has to read as the same
    // panel, and a full-screen dim is a different thing. The block's extents come from
    // the same nominal-cols x line-count metrics the layout is anchored on, so the
    // backdrop cannot disagree with the text it sits behind.
    if (page != PAGE_OFF) {
      float padx = 6.0f * uiscale;
      float pady = 4.0f * uiscale;
      float bw   = float(FontMan::stringWidth(cols)) + 2.0f * padx;
      float bh   = float(numLines) * lineH + 2.0f * pady;
      _drawSlate(ctx, TW, TH, margin - padx, textTop - pady, bw, bh);
    }
    // the frame-time history graph belongs to page 1; it sits just ABOVE its text block
    if (page == PAGE_FRAME) {
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

  // VR path: render the HUD content into an offscreen RT and publish its texture (+
  //  aspect) to VrHudOverlay. The VR output node reads it next frame and draws it as a
  //  head-locked panel in both eyes. Uses a FIXED scale (not the window SSAA scale) since
  //  the panel is magnified in-headset, not viewed 1:1 on-screen.
  //
  // The RT is sized to the TALLEST page and a fixed column count, and the graph's strip
  //  is reserved on every page: cycling pages must never resize or re-aspect a panel the
  //  wearer is reading.
  void _renderPanelRT(Context* ctx) {
    auto& overlay = VrHudOverlay::instance();
    int   page    = _page.load();
    if (page == PAGE_OFF) {
      overlay._enabled.store(false);
      return;
    }
    std::string text     = _pages.pageText(page);
    int         numLines = _pages.maxLineCount();

    const float uiscale = 2.0f; // fixed crisp panel scale
    auto        fontman = FontMan::instance();
    fontman->setCurrentFont(uiscale >= 2.75f ? "i48" : (uiscale >= 1.5f ? "i32" : "i16"));
    float lineH  = float(FontMan::currentFont()->description().miCharHeight);
    float margin = 12.0f * uiscale;
    float textW  = float(FontMan::stringWidth(PerfHudPages::maxLineCols()));
    float graphH = 78.0f * uiscale + 8.0f * uiscale;
    float graphW = 240.0f * uiscale;
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
    // CENTER each page's block in the fixed-size quad, per page. The RT never resizes
    //  (TH is sized for the tallest page + the graph strip), so a short page's block
    //  would otherwise hug the top with dead quad below it. Content height uses the
    //  PAGE'S OWN fixed line count (frozen at scene bind) and counts the graph strip
    //  only on the page that draws one — so the offset is a per-page constant and the
    //  text NEVER moves while a page is up; switching pages recenters for that page.
    //  Horizontal: the text column block and the graph each center on their own width.
    float pageGraphH = (page == PAGE_FRAME) ? graphH : 0.0f;
    float contentH   = float(_pages.pageLineCount(page)) * lineH + pageGraphH;
    float yTop       = std::max(margin, (float(TH) - contentH) * 0.5f);
    // Horizontal: center on the page's NOMINAL content width (a per-page constant),
    //  not on the panel-wide column allocation — (TW - textW)/2 degenerates to the
    //  margin because TW is DERIVED from textW. Nominal widths cannot move with
    //  value-digit changes, so the block stays put within a page.
    float nomW  = float(FontMan::stringWidth(_pages.pageNominalCols(page)));
    float textX = std::max(margin, (float(TW) - nomW) * 0.5f);
    if (page == PAGE_FRAME)
      _drawGraph(ctx, float(TW), float(TH), (float(TW) - graphW) * 0.5f, yTop, graphW, 78.0f * uiscale);
    _drawText(ctx, float(TW), float(TH), text, textX, yTop + pageGraphH);

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

  // ONE straight-ALPHA quad in the desktop HUD's own UI space, tinted from the shared
  // VrHudOverlay slate. Drawn BEFORE the text (the desktop path draws glyphs straight to
  // the frame, so there is no premultiplied-RT hole problem to work around here — that
  // split exists only because the VR panel round-trips through an RT).
  void _drawSlate(Context* ctx, float TW, float TH, float px, float py, float pw, float ph) {
    if (not _mtl) {
      _mtl = std::make_shared<FreestyleMaterial>();
      _mtl->gpuInit(ctx, "orkshader://solid");
    }
    if (not _vbuf) {
      _vbuf = std::make_shared<DynamicVertexBuffer<SVtxV16T16C16>>(64 << 10, 0);
      _vbuf->SetRingLock(true);
    }
    // the ALPHA-BAKED technique: a runtime setBlendingMacro never reaches a compiled
    // pipeline (the pass's state_block does), which is why an opaque-state quad drawn
    // with a 0.5 alpha vertex color shows nothing at all.
    auto tek     = _mtl->technique("vtxcolor_alpha");
    auto par_mvp = _mtl->param("MatMVP");
    auto RCFD    = std::make_shared<RenderContextFrameData>(ctx);
    auto mtxi    = ctx->MTXI();
    auto gbi     = ctx->GBI();
    const auto& s = VrHudOverlay::instance()._slate_rgba;
    mtxi->PushUIMatrix(int(TW), int(TH));
    {
      VtxWriter<SVtxV16T16C16> vw;
      vw.Lock(ctx, _vbuf.get(), 6);
      fvec4 bg(s[0], s[1], s[2], s[3]);
      auto  V = [&](float x, float y) { vw.AddVertex(SVtxV16T16C16(fvec3(x, y, 0), fvec4(), bg)); };
      V(px, py);  V(px + pw, py);       V(px + pw, py + ph);
      V(px, py);  V(px + pw, py + ph);  V(px, py + ph);
      vw.UnLock(ctx);
      _mtl->begin(tek, RCFD);
      _mtl->bindParamMatrix(par_mvp, mtxi->RefMVPMatrix());
      gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
      _mtl->end(RCFD);
    }
    mtxi->PopUIMatrix();
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
      _mtl->begin(_mtl->technique("vtxcolor_alpha"), RCFD);
      _mtl->bindParamMatrix(par_mvp, mtxi->RefMVPMatrix());
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
