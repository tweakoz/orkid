#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/aud/singularity/envelope.h>
#include <ork/lev2/aud/singularity/sampler.h>
namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

void DrawEnv(lev2::Context* context, const ItemDrawReq& EDR) {
  const auto& R = EDR.rect;
  hudlines_t lines;

  float X2 = R.X1 + R.W;
  float Y2 = R.Y1 + R.H;

  const auto& KFIN             = EDR.s->_curhud_kframe;
  const auto& AFIN             = EDR.s->_curhud_aframe;
  const auto& ENVFRAME         = EDR._data.Get<envframe>();
  const RateLevelEnvData* ENVD = ENVFRAME._data;

  // auto ENVCT    = EDR.ld->_envCtrlData;
  bool collsamp = EDR.shouldCollectSample();

  bool useNENV = false; // ENVCT->_useNatEnv && (EDR.ienv == 0);

  std::string sampname = "???";

  if (useNENV)
    sampname = "natural";
  else
    sampname = FormatString("env%d", EDR.ienv + 1);

  assert(EDR.ienv < kmaxenvperlayer);

  auto lyr = EDR.l;

  /////////////////////////////////////////////////
  // collect hud samples
  /////////////////////////////////////////////////

  auto& HUDSAMPS = EDR.s->_hudsample_map[sampname];

  if (collsamp) {
    hudsample hs;
    hs._time  = EDR.s->_lnotetime;
    hs._value = ENVFRAME._value;
    HUDSAMPS.push_back(hs);
  }

  /////////////////////////////////////////////////
  // draw envelope values
  /////////////////////////////////////////////////

  float env_by  = R.Y1 + 20;
  float env_bx  = R.X1 + 70.0;
  int spcperseg = 90;
  float minval  = 0.0f;
  float maxval  = 1.00f;
  float range   = 1.0f;
  if (useNENV) {
    auto sample     = KFIN._kmregion->_sample;
    const auto& NES = sample->_natenv;
    int nseg        = NES.size();
    int icurseg     = ENVFRAME._curseg;

    drawtext(context, "NATENV", env_bx, env_by, fontscale, 1, 0, 0);

    for (int i = 0; i < nseg; i++) {
      bool iscurseg = (i == icurseg);
      float r       = 1;
      float g       = iscurseg ? 0.5 : 1;

      auto hudstr = FormatString("seg%d", i);
      drawtext(context, hudstr, env_bx + spcperseg * i, env_by + 20, fontscale, r, g, 0);
    }
    drawtext(context, "dB/s\ntim", R.X1 + 15, env_by + 40, .45, 1, 1, 0);
    for (int i = 0; i < nseg; i++) {
      const auto& seg = NES[i];

      auto hudstr   = FormatString("%0.1f\n%d", seg._slope, int(seg._time));
      bool iscurseg = (i == icurseg);
      float r       = 1;
      float g       = iscurseg ? 0 : 1;
      float b       = iscurseg ? 0 : 1;
      drawtext(context, hudstr, env_bx + spcperseg * i, env_by + 40, fontscale, r, g, 1);
    }
  } else if (ENVD) {

    minval = 0.0f;
    maxval = 0.00f;

    auto& AE = ENVD->_segments;
    for (int i = 0; i < 7; i++) {
      auto seg = AE[i];
      auto lev = seg._level;
      if (lev > maxval)
        maxval = lev;
      if (lev < minval)
        minval = lev;
    }
    range = maxval - minval;

    drawtext(context, ENVD->_name, R.X1, env_by, fontscale, 1, 0, .5);
    int icurseg = ENVFRAME._curseg;
    for (int i = 0; i < 7; i++) {
      std::string segname;
      if (i < 3)
        segname = FormatString("atk%d", i + 1);
      else if (i == 3)
        segname = "dec";
      else
        segname = FormatString("rel%d", i - 3);
      auto hudstr = FormatString("%s", segname.c_str());

      bool iscurseg = (i == icurseg);
      float r       = 1;
      float g       = iscurseg ? 0.5 : 1;

      drawtext(context, hudstr, env_bx + spcperseg * i, env_by + 20, fontscale, r, g, 0);
    }
    drawtext(context, "lev\ntim", R.X1 + 15, env_by + 40, .45, 1, 1, 0);
    for (int i = 0; i < 7; i++) {
      auto hudstr   = FormatString("%0.2f\n%0.3f", AE[i]._level, AE[i]._time);
      bool iscurseg = (i == icurseg);
      float r       = 1;
      float g       = iscurseg ? 0 : 1;
      float b       = iscurseg ? 0 : 1;
      drawtext(context, hudstr, env_bx + spcperseg * i, env_by + 40, fontscale, r, g, 1);
    }
  }

  if (false == (EDR.ienv == 0 or ENVD))
    return;

  /////////////////////////////////////////////////
  // draw envelope segments
  /////////////////////////////////////////////////

  float fxb = R.X1;
  float fyb = Y2;
  float fw  = R.W;
  float fpx = fxb;
  float fpy = fyb;

  R.PushOrtho(context);

  ///////////////////////
  // draw border
  ///////////////////////

  DrawBorder(context, R.X1, R.Y1, X2, Y2);

  const float ktime = 20.0f;

  float fh = useNENV ? 200 : 400;

  bool bipolar = ENVD ? ENVD->_bipolar : false;

  ///////////////////////
  // draw grid, origin
  ///////////////////////

  if (bipolar) {
    float fy = fyb - (fh * 0.5f) * 0.5f;
    float x1 = fxb;
    float x2 = x1 + fw;
    lines.push_back(HudLine{fvec2(x1, fy), fvec2(x2, fy), fvec3(.5, .2, .5)});
  }

  ///////////////////////
  // from hud samples
  ///////////////////////

  for (int i = 0; i < HUDSAMPS.size(); i++) {
    const auto& hs = HUDSAMPS[i];
    if (fpx >= R.X1 and fpx <= X2) {
      auto p1    = fvec2(fpx, fpy);
      fpx        = fxb + hs._time * (fw / ktime);
      float fval = (hs._value - minval) / range;
      fpy        = fyb - fval * fh * 0.5;
      auto p2    = fvec2(fpx, fpy);
      lines.push_back(HudLine{p1, p2, fvec3(1, 1, 1)});
    }
  }

  ///////////////////////
  // natural env dB slopehull
  ///////////////////////

  if (useNENV) {
    fpx = fxb;
    fpy = fyb;
    for (int i = 0; i < HUDSAMPS.size(); i++) {
      const auto& hs = HUDSAMPS[i];
      auto p1        = fvec2(fpx, fpy);

      float val = hs._value;
      val       = 96.0 + linear_amp_ratio_to_decibel(val);
      if (val < 0.0f)
        val = 0.0f;
      val = val / 96.0f;

      fpx     = fxb + hs._time * (fw / ktime);
      fpy     = fyb - fh * val;
      auto p2 = fvec2(fpx, fpy);
      lines.push_back(HudLine{p1, p2, fvec3(1, 0, 0)});
    }
  }
  R.PopOrtho(context);
  /////////////////////////////////////////////////
  drawHudLines(context, lines);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
