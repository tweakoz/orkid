#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/aud/singularity/dspblocks.h>

using namespace ork;
using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////

void DrawOscope(
    lev2::Context* context, //
    const hudaframe& HAF,
    const float* samples,
    fvec2 xy,
    fvec2 wh) { //
  auto syn = synth::instance();

  auto hudl   = syn->_hudLayer;
  double time = syn->_timeaccum;

  if (false == (hudl && hudl->_LayerData))
    return;

  ///////////////////////////////////////////////

  hudlines_t lines;

  int inumframes = syn->_oswidth;

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

  int window_width = syn->_oswidth;

  double width = double(window_width) / double(48000.0);
  double frq   = 1.0 / width;

  drawtext(
      context, //
      FormatString("width: %g msec", width * 1000.0),
      OSC_X1,
      ycursor,
      fontscale,
      1,
      1,
      0);

  ycursor += hud_lineheight();

  drawtext(
      context, //
      FormatString("width-frq: %g hz", frq),
      OSC_X1,
      ycursor,
      fontscale,
      1,
      1,
      0);

  ycursor += hud_lineheight();

  float triggerlevel = syn->_ostriglev;
  drawtext(
      context, //
      FormatString("triglev: %0.4f", triggerlevel),
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

  float x1 = OSC_X1;
  float y1 = OSC_CY + samples[0] * -OSC_HH;
  float x2, y2;

  const int koscfr = window_width;

  //////////////////////////////////////////////
  // find zero crossing
  //////////////////////////////////////////////

  int64_t zero_crossing = 0;
  float ly              = samples[0];
  for (int i = 0; i < window_width; i++) {
    float y = samples[i];
    if (ly < triggerlevel and y >= triggerlevel) {
      zero_crossing = i;
      // printf("zero_crossing<%ld>\n", zero_crossing);
      break;
    }
    ly = y;
  }

  //////////////////////////////////////////////

  for (int i = 0; i < window_width; i++) {
    int j   = (i + zero_crossing) & koscopelengthmask;
    float x = OSC_W * float(i) / float(window_width);
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
