#include <ork/lev2/aud/singularity/hud.h>

using namespace ork;
using namespace ork::lev2;

namespace ork::lev2 {
extern ork::audio::singularity::synth* the_synth;
}

///////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////

void DrawOscope(
    lev2::Context* context, //
    const hudaframe& HAF,
    const float* samples,
    fvec2 xy,
    fvec2 wh) { //
  hudlines_t lines;

  int inumframes = the_synth->_oswidth;

  const float OSC_X1 = xy.x;
  const float OSC_Y1 = xy.y;
  const float OSC_W  = wh.x;
  const float OSC_H  = wh.y;
  const float OSC_X2 = (xy + wh).x;
  const float OSC_Y2 = (xy + wh).y;
  const float OSC_HH = OSC_H * 0.5;
  const float OSC_CY = OSC_Y1 + OSC_HH;

  DrawBorder(context, OSC_X1, OSC_Y1, OSC_X2, OSC_Y2);

  int ycursor = OSC_Y1;

  double width = double(the_synth->_oswidth) / double(48.0);

  drawtext(
      context, //
      FormatString("width: %g msec", width),
      OSC_X1,
      ycursor,
      fontscale,
      1,
      1,
      0);

  ycursor += hud_lineheight();

  double track = double(the_synth->_ostrack) / double(65536.0);
  drawtext(
      context, //
      FormatString("track: %0.4f", track),
      OSC_X1,
      ycursor,
      fontscale,
      1,
      1,
      0);

  /////////////////////////////////////////////
  // oscilloscope centerline
  /////////////////////////////////////////////

  lines.push_back(HudLine{
      fvec2(OSC_X1, OSC_CY), //
      fvec2(OSC_X2, OSC_CY),
      fvec3(1, 1, 1)});

  /////////////////////////////////////////////
  // oscilloscope trace
  /////////////////////////////////////////////

  int trakoff = HAF._trackoffset >> 16;
  auto mapI   = [&](int i) -> int {
    int j = (i + trakoff) % inumframes;

    assert(j >= 0);
    assert(j < inumframes);
    return j;
  };

  //

  float x1 = OSC_X1;
  float y1 = OSC_CY + samples[mapI(0)] * -OSC_HH;
  float x2, y2;

  const int koscfr = inumframes / 4;

  for (int i = 0; i < inumframes; i++) {
    int j   = mapI(i);
    float x = OSC_W * float(i) / float(koscfr);
    float y = samples[j] * -OSC_HH;
    x2      = OSC_X1 + x;
    y2      = OSC_CY + y;
    if (i < koscfr)
      lines.push_back(HudLine{
          fvec2(x1, y1), //
          fvec2(x2, y2),
          fvec3(.3, 1, .3)});
    x1 = x2;
    y1 = y2;
  }

  drawHudLines(context, lines);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////
