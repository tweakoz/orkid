#include <ork/lev2/aud/singularity/hud.h>

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////

void DrawFun(lev2::Context* context, const ItemDrawReq& EDR) {
  hudlines_t lines;
  const auto& R = EDR.rect;
  float X2      = R.X1 + R.W;
  float Y2      = R.Y1 + R.H;

  const auto& KFIN     = EDR.s->_curhud_kframe;
  const auto& FUNFRAME = EDR._data.get<funframe>();
  const FunData* FUND  = FUNFRAME._data;

  if (FUND == nullptr)
    return;

  auto ld  = EDR.ld;
  auto lyr = EDR.l;

  int spcperseg = 70;
  float env_bx  = R.X1 + 70.0;
  float env_by  = R.Y1 + 20;
  float fxb     = R.X1;
  float fyb     = Y2 - 1;
  float fw      = R.W;
  float fpx     = fxb;
  float fh      = R.H;
  float fgcy    = R.Y1 + (R.H * 0.5f) + 20;
  float sx0     = env_bx + spcperseg * 0;
  float sx1     = env_bx + spcperseg * 1;
  float sx2     = env_bx + spcperseg * 2;
  float by0     = env_by + 20;
  float by1     = env_by + 40;
  float by2     = env_by + 60;

  // printf( "fun<%p>\n", fun );

  auto funname = FUND->_name;

  float ktime  = 3.0f;
  int maxsamps = int(ktime * 35.0f);

  auto& HUDSAMPS = EDR.s->_hudsample_map[funname];

  while (HUDSAMPS.size() > maxsamps) {
    HUDSAMPS.erase(HUDSAMPS.begin());
  }

  hudsample hs;
  hs._time  = EDR.s->_lnotetime;
  hs._value = FUNFRAME._value;
  HUDSAMPS.push_back(hs);

  // drawtext(context, FUND->_name, R.X1, env_by, fontscale, 1, 0, .5);

  // drawtext(context, "a", R.X1 + 15, by0, fontscale, 1, 1, 0);
  // drawtext(context, "b", R.X1 + 15, by1, fontscale, 1, 1, 0);
  // drawtext(context, "op", R.X1 + 15, by2, fontscale, 1, 1, 0);
  // drawtext(context, FUND->_a, sx0, by0, fontscale, 1, 1, 1);
  // drawtext(context, FUND->_b, sx0, by1, fontscale, 1, 1, 1);
  // drawtext(context, FUND->_op, sx0, by2, fontscale, 1, 1, 1);

  R.PushOrtho(context);
  DrawBorder(context, R.X1, R.Y1, X2, Y2);

  ///////////////////////
  // draw border, grid, origin
  ///////////////////////

  float fyc = fgcy; // fyb-(fh*0.5f);
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
        lines.push_back(HudLine{p1, p2, fvec3(0.7, 0.5, 0.7)});
      }
    }
  }

  R.PopOrtho(context);
  // drawHudLines(context, lines);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
