#include <ork/lev2/aud/singularity/hud.h>

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////

void DrawOp4(lev2::Context* context, const Op4DrawReq& OPR) {
  const auto& R = OPR.rect;
  float X2      = R.X1 + R.W;
  float Y2      = R.Y1 + R.H;

  const auto& KFIN = OPR.s->_curhud_kframe;
  const auto& AFIN = OPR.s->_curhud_aframe;

  // auto ld = OPR.ld;

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

  auto op4frame = AFIN._op4frame[OPR.iop];

  std::string op4n = FormatString("OP4.%d", OPR.iop);
  float ktime      = 3.0f;
  int maxsamps     = int(ktime * 35.0f);

  auto& HUDSAMPS = OPR.s->_hudsample_map[op4n];

  while (HUDSAMPS.size() > maxsamps) {
    HUDSAMPS.erase(HUDSAMPS.begin());
  }

  const auto& op4f = AFIN._op4frame[OPR.iop];
  hudsample hs;
  hs._time  = OPR.s->_lnotetime;
  hs._value = op4f._envout;
  HUDSAMPS.push_back(hs);

  drawtext(FormatString("Op%d - olev<%d> wave<%d>", OPR.iop, op4f._olev, op4f._wav), R.X1, env_by, fontscale, 1, 0, .5);

  drawtext(FormatString("mi<%0.2f> r<%0.2f> f<%0.2f>", op4f._mi, op4f._r, op4f._f), R.X1 + 15, by0, fontscale, 1, 1, 0);
  drawtext(
      FormatString(
          "AR<%d> D1R<%d> D1L<%d> D2R<%d> RR<%d> EGS<%d> EV<%d:%0.2f>",
          op4f._ar,
          op4f._d1r,
          op4f._d1l,
          op4f._d2r,
          op4f._rr,
          op4f._egshift,
          op4f._envph,
          op4f._envout),
      R.X1 + 15,
      by1,
      fontscale,
      1,
      1,
      0);

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

  // glColor4f(0.5, 0.0, 0.5, 1);
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
  // glEnd();

  R.PopOrtho(context);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
