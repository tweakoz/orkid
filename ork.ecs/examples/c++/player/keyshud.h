#pragma once
////////////////////////////////////////////////////////////////
// KeysHud — the --devkeys on-screen KEY LEGEND for ork.ecs.player.exe.
//
// Mirrors ork.hypermesh.viewer.py's always-on key HUD: an upper-left block listing
// each dev key + its CURRENT value. Present ONLY when --devkeys (flagless playback
// has no HUD and stays byte-identical to plain playback).
//
// STRICTLY DETERMINISTIC content — key names + cycle values only, NO fps / wall-clock /
// frame counters. The snapshot byte-identity gates (mode-0-vs-no-M, two-run determinism)
// depend on this: the same devkey state must always produce the exact same HUD text.
//
// Drawn IMMEDIATE-MODE in the player's onDraw, reusing perfhud.h's FontMan text path
// (the player bypasses the EzTopWidget paint path). Anchored UPPER-left so it never
// overlaps the perf HUD (lower-left, '~'-cycled).
////////////////////////////////////////////////////////////////

#include <string>
#include <algorithm>
#include <mutex>

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/fbi.h>
#include <ork/lev2/gfx/mtxi.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>

namespace ork::ecs::player {

using namespace ork;
using namespace ork::lev2;

struct KeysHud {
  bool        _enabled = false; // set true iff --devkeys
  std::mutex  _mutex;           // _text is written on the UI/update thread, read on the render thread
  std::string _text;

  // Deterministic legend rebuilt on every devkey change (called off the render thread).
  void setState(const std::string& envmap, float gamma, float exposure, float saturation, const std::string& matmode) {
    char buf[512];
    snprintf(
        buf,
        sizeof(buf),
        "[E] envmap: %s\n"
        "[G] gamma: %.2f\n"
        "[T] exposure: %.2f\n"
        "[H] sat: %.2f\n"
        "[M] mat: %s\n"
        "[R] reset",
        envmap.c_str(),
        gamma,
        exposure,
        saturation,
        matmode.c_str());
    std::lock_guard<std::mutex> lock(_mutex);
    _text = buf;
  }

  // Render-thread draw (end of onDraw). No-op unless --devkeys.
  void draw(Context* ctx) {
    if (not _enabled)
      return;
    std::string text;
    {
      std::lock_guard<std::mutex> lock(_mutex);
      text = _text;
    }
    if (text.empty())
      return;

    auto fbi = ctx->FBI();
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
    // clean RCFD so PushUIMatrix uses our viewport dims AND FontMan's endTextBlock has a
    // frame-data on the stack to read (perfhud precedent).
    ctx->pushRenderContextFrameData(std::make_shared<RenderContextFrameData>(ctx));

    auto fontman = FontMan::instance();
    fontman->setCurrentFont(uiscale >= 2.75f ? "i48" : (uiscale >= 1.5f ? "i32" : "i16"));

    float margin = 12.0f * uiscale;

    ctx->MTXI()->PushUIMatrix(int(TW), int(TH));
    ctx->PushModColor(fcolor4(1.0f, 1.0f, 0.35f, 1.0f)); // yellow — distinct from the perf HUD's green
    fontman->beginTextBlock(ctx, text.length());
    fontman->DrawText(ctx, int(margin), int(margin), "%s", text.c_str());
    fontman->endTextBlock(ctx);
    ctx->PopModColor();
    ctx->MTXI()->PopUIMatrix();

    ctx->popRenderContextFrameData();
    fbi->popScissor();
    fbi->popViewport();
  }
};

} // namespace ork::ecs::player
