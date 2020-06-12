#include <ork/lev2/aud/singularity/hud.h>

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////

void DrawLfo(lev2::Context* context, const ItemDrawReq& EDR) {
  hudlines_t lines;
  const auto& R = EDR.rect;
  float X2      = R.X1 + R.W;
  float Y2      = R.Y1 + R.H;

  const auto& KFIN     = EDR.s->_curhud_kframe;
  const auto& LFOFRAME = EDR._data.Get<lfoframe>();
  const LfoData* LFOD  = LFOFRAME._data;

  if (nullptr == LFOD)
    return;

  auto ld  = EDR.ld;
  auto lyr = EDR.l;

  auto lfoname  = LFOD->_name;
  bool collsamp = true;

  auto& HUDSAMPS = EDR.s->_hudsample_map[lfoname];

  float ktime = 4.0f / LFOFRAME._currate;
  if (ktime > 4.0f)
    ktime = 4.0f;

  int maxsamps = int(ktime * 50.0f);

  while (HUDSAMPS.size() > maxsamps) {
    HUDSAMPS.erase(HUDSAMPS.begin());
  }

  if (collsamp) {
    // printf( "time<%f> ampDB<%f>\n", _lnotetime, ampDB );
    hudsample hs;
    hs._time  = EDR.s->_lnotetime;
    hs._value = LFOFRAME._value;

    HUDSAMPS.push_back(hs);
  }

  int spcperseg = 70;
  float env_bx  = R.X1 + 70.0;
  float env_by  = R.Y1 + 20;
  float fxb     = R.X1;
  float fyb     = Y2 - 1;
  float fw      = R.W;
  float fpx     = fxb;
  float fh      = 200;
  float sx0     = env_bx + spcperseg * 1;
  float sx1     = env_bx + spcperseg * 2;
  float sx2     = env_bx + spcperseg * 3;
  float by0     = env_by + 20;
  float by1     = env_by + 40;
  float by2     = env_by + 60;
  float by3     = env_by + 80;

  auto& ctrl = LFOD->_controller;

  ///////////////////////

  // drawtext(context, LFOD->_name, R.X1, env_by, fontscale, 1, 0, .5);

  // drawtext(context, "shape", R.X1 + 15, by0, fontscale, 1, 1, 0);
  // drawtext(context, "rate", R.X1 + 15, by1, fontscale, 1, 1, 0);
  // drawtext(context, "phase", R.X1 + 15, by2, fontscale, 1, 1, 0);
  // drawtext(context, "ctrl", R.X1 + 15, by3, fontscale, 1, 1, 0);
  // drawtext(context, LFOD->_shape, sx0, by0, fontscale, 1, 1, 1);
  // drawtext(context, FormatString("%0.2f", LFOFRAME._currate), sx0, by1, fontscale, 1, 1, 1);
  // drawtext(context, FormatString("%d", int(LFOD->_initialPhase * 90.0f)), sx0, by2, fontscale, 1, 1, 1);
  // drawtext(context, ctrl, sx0, by3, fontscale, 1, 1, 1);

  ///////////////////////

  R.PushOrtho(context);

  ///////////////////////
  // draw border, grid, origin
  ///////////////////////

  float fyc = fyb - (fh * 0.5f);
  float x1  = fxb;
  float x2  = x1 + fw;

  DrawBorder(context, R.X1, R.Y1, X2, Y2);

  lines.push_back(HudLine{fvec2(x1, fyc), fvec2(x2, fyc), fvec3(.5, .2, .5)});

  ///////////////////////
  // from hud samples
  ///////////////////////

  float fpy = fyc;
  if (HUDSAMPS.size()) {
    const auto& HS0 = HUDSAMPS[0];
    float ftimebase = HS0._time;
    float s0        = HS0._value;
    fpy             = fyc - s0 * 100.0f;

    for (int i = 0; i < HUDSAMPS.size(); i++) {
      const auto& hs = HUDSAMPS[i];
      if (fpx >= R.X1 and fpx <= X2) {
        auto p1 = fvec2(fpx, fpy);
        float t = hs._time - ftimebase;
        fpx     = fxb + t * (fw / ktime);

        float fval = hs._value;
        fpy        = fyc - fval * 100.0f;
        auto p2    = fvec2(fpx, fpy);

        lines.push_back(HudLine{p1, p2, fvec3(0.5, 0.7, 0.7)});
      }
    }
  }

  R.PopOrtho(context);
  // drawHudLines(context, lines);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
