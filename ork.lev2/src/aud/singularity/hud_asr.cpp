#include <ork/lev2/aud/singularity/hud.h>
#include <ork/kernel/string/string.h>

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////

void DrawAsr(const ItemDrawReq& EDR) {
  const auto& R = EDR.rect;
  float X2      = R.X1 + R.W;
  float Y2      = R.Y1 + R.H;

  const auto& KFIN     = EDR.s->_curhud_kframe;
  const auto& AFIN     = EDR.s->_curhud_aframe;
  const auto& ASRFRAME = EDR._data.Get<asrframe>();
  const AsrData* ASRD  = ASRFRAME._data;

  if (nullptr == ASRD)
    return;

  auto ld  = EDR.ld;
  auto lyr = EDR.l;

  const auto& ENVCT = ld->_envCtrlData;
  bool collsamp     = EDR.shouldCollectSample();

  std::string sampname = FormatString("asr%d", EDR.ienv + 1);

  /////////////////////////////////////////////////
  // collect hud samples
  /////////////////////////////////////////////////

  auto& HUDSAMPS = EDR.s->_hudsample_map[sampname];

  if (collsamp) {
    // printf( "time<%f> ampDB<%f>\n", _lnotetime, ampDB );
    hudsample hs;
    hs._time  = EDR.s->_lnotetime;
    hs._value = ASRFRAME._value;
    HUDSAMPS.push_back(hs);
  }
  /////////////////////////////////////////////////
  // draw envelope values
  /////////////////////////////////////////////////
  float env_by  = R.Y1 + 20;
  float env_bx  = R.X1 + 78.0;
  int spcperseg = 70;

  {
    auto& mode = ASRD->_mode;
    auto& trig = ASRD->_trigger;
    drawtext(ASRD->_name, R.X1, env_by, fontscale, 1, 0, .5);
    int icurseg   = ASRFRAME._curseg;
    float deltime = ASRD->_delay;
    float atktime = ASRD->_attack;
    float reltime = ASRD->_release;
    float sx0     = env_bx + spcperseg * 0;
    float sx1     = env_bx + spcperseg * 1;
    float sx2     = env_bx + spcperseg * 2;
    float by0     = env_by + 20;
    float by1     = env_by + 40;
    float by2     = env_by + 60;
    float by3     = env_by + 80;

    float r  = 1;
    float gD = (icurseg == 0) ? 0.5 : 1;
    float gA = (icurseg == 1) ? 0.5 : 1;
    float gR = (icurseg == 3) ? 0.5 : 1;
    drawtext("del", sx0, by0, .45, r, gD, 0);
    drawtext("atk", sx1, by0, .45, r, gA, 0);
    drawtext("rel", sx2, by0, .45, r, gR, 0);

    drawtext("time", R.X1 + 15, by1, .45, 1, 1, 0);
    drawtext("mode", R.X1 + 15, by2, .45, 1, 1, 0);
    drawtext("trig", R.X1 + 15, by3, .45, 1, 1, 0);

    drawtext(FormatString("%0.2f", deltime), sx0, by1, fontscale, r, gD, 1);
    drawtext(FormatString("%0.2f", atktime), sx1, by1, fontscale, r, gA, 1);
    drawtext(FormatString("%0.2f", reltime), sx2, by1, fontscale, r, gR, 1);

    drawtext(mode.c_str(), sx0, by2, fontscale, 1, 1, 1);
    drawtext(trig.c_str(), sx0, by3, fontscale, 1, 1, 1);
  }
  /////////////////////////////////////////////////
  // draw envelope segments
  /////////////////////////////////////////////////

  float fxb = R.X1;
  float fyb = Y2;
  float fw  = R.W;
  float fpx = fxb;
  float fpy = fyb;

  R.PushOrtho();

  ///////////////////////
  // draw border
  ///////////////////////

  DrawBorder(R.X1, R.Y1, X2, Y2);

  const float ktime = 20.0f;

  fyb -= 1;
  float fh = 400 - 1;

  ///////////////////////
  // from hud samples
  ///////////////////////

  glColor4f(1, 1, 1, 1);
  glBegin(GL_LINES);
  for (int i = 0; i < HUDSAMPS.size(); i++) {
    const auto& hs = HUDSAMPS[i];
    if (fpx >= R.X1 and fpx <= X2) {
      glVertex3f(fpx, fpy, 0);
      fpx = fxb + hs._time * (fw / ktime);

      float fval = hs._value;
      fpy        = fyb - (fh * 0.5f) * fval;

      glVertex3f(fpx, fpy, 0);
    }
  }
  glEnd();

  R.PopOrtho();

  /////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
