#include <ork/lev2/aud/singularity/hud.h>

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////

void DrawFun(lev2::Context* context, const ItemDrawReq& EDR) {
  const auto& R = EDR.rect;
  float X2      = R.X1 + R.W;
  float Y2      = R.Y1 + R.H;

  const auto& KFIN     = EDR.s->_curhud_kframe;
  const auto& AFIN     = EDR.s->_curhud_aframe;
  const auto& FUNFRAME = EDR._data.Get<funframe>();
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

  drawtext(FUND->_name, R.X1, env_by, fontscale, 1, 0, .5);

  drawtext("a", R.X1 + 15, by0, fontscale, 1, 1, 0);
  drawtext("b", R.X1 + 15, by1, fontscale, 1, 1, 0);
  drawtext("op", R.X1 + 15, by2, fontscale, 1, 1, 0);
  drawtext(FUND->_a, sx0, by0, fontscale, 1, 1, 1);
  drawtext(FUND->_b, sx0, by1, fontscale, 1, 1, 1);
  drawtext(FUND->_op, sx0, by2, fontscale, 1, 1, 1);

  R.PushOrtho(context);
  DrawBorder(context, R.X1, R.Y1, X2, Y2);

  ///////////////////////
  // draw border, grid, origin
  ///////////////////////

  float fyc = fgcy; // fyb-(fh*0.5f);
  float x1  = fxb;
  float x2  = x1 + fw;

  DrawBorder(context, R.X1, R.Y1, X2, Y2);

  // glColor4f(.5, .2, .5, 1);
  // glBegin(GL_LINES);
  // glVertex3f(x1, fyc, 0);
  // glVertex3f(x2, fyc, 0);
  // glEnd();

  ///////////////////////
  // from hud samples
  ///////////////////////

  // glColor4f(0.7, 0.5, 0.7, 1);
  // glBegin(GL_LINES);
  float fpy = fyc;
  if (HUDSAMPS.size()) {
    const auto& HS0 = HUDSAMPS[0];
    float ftimebase = HS0._time;
    float s0        = HS0._value;
    fpy             = fyc - s0 * 100.0f;

    for (int i = 0; i < HUDSAMPS.size(); i++) {
      const auto& hs = HUDSAMPS[i];
      if (fpx >= R.X1 and fpx <= X2) {
        // glVertex3f(fpx, fpy, 0);
        float t = hs._time - ftimebase;
        fpx     = fxb + t * (fw / ktime);

        float fval = hs._value;
        fpy        = fyc - fval * 100.0f;

        // glVertex3f(fpx, fpy, 0);
      }
    }
  }
  //  glEnd();

  R.PopOrtho(context);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
