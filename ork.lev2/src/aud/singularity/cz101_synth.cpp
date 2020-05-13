#include <stdio.h>
#include <ork/orktypes.h>
#include <ork/math/audiomath.h>
#include <ork/file/file.h>
#include <ork/kernel/string/string.h>
#include <stdint.h>
#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/lev2/aud/singularity/cz101.h>
#include <ork/lev2/aud/singularity/fmosc.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/kernel/string/string.h>

using namespace ork;

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

CZX::CZX(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

///////////////////////////////////////////////////////////////////////////////

float staircase(float x) {
  return x - sin(x);
}
float step(float u) {
  return 0.5 - 0.5 * cosf(PI * u);
}
float stair(float x, float b, float c) {
  const float width = b + c;
  const float base  = floor(x / width); // base of this step
  const float o     = fmod(x, width);   // offset, between 0 and width

  return base + (o < b ? 0 : step((o - b) / c));
}

///////////////////////////////////////////////////////////////////////////////

void CZX::compute(DspBuffer& dspbuf) // final
{
  float centoff  = _param[0].eval();
  _fval[0]       = centoff;
  int inumframes = dspbuf._numframes;
  float* U       = dspbuf.channel(0);
  //_layer->_curPitchOffsetInCents = centoff;
  // todo: dco(pitch) env mod
  // todo: mi from dcw env
  static float _ph = 0.0;
  float modindex   = 0.45f + sinf(_ph) * 0.45f;
  _ph += 0.003f;
  //////////////////////////////////////////
  float lyrcents = _layer->_layerBasePitch;
  float cin      = (lyrcents + centoff) * 0.01;
  float frq      = midi_note_to_frequency(cin);
  // printf("note<%g> frq<%g>\n", cin, frq);
  //////////////////////////////////////////
  constexpr double kscale    = double(1 << 24);
  constexpr double kinvscale = 1.0 / kscale;
  //////////////////////////////////////////

  for (int i = 0; i < inumframes; i++) {
    int64_t pos = _phase & 0xffffff;
    double dpos = double(pos) * kinvscale;
    int64_t x1  = int64_t(modindex * 0xffffff);
    ////////////////////////////////////////////
    float m1      = .5 / modindex;
    float m2      = .5 / (1.0 - modindex);
    float b2      = 1.0 - m2;
    double warped = (pos < x1) //
                        ? (m1 * dpos)
                        : (m2 * dpos + b2);
    float saw      = cosf(warped * PI2);
    float sawpulse = sinf(warped * PI2);
    float dblsine  = sinf(warped * PI2 * 2.0);
    ////////////////////////////////////////////
    float xx        = 1.0 - modindex;
    float m3        = 1.0 / xx;
    double warped2  = std::clamp(dpos * m3, -1.0, 1.0);
    float sinpulse  = sinf(warped2 * PI2);
    float cospulse  = cosf(warped2 * PI2);
    float dcospulse = cosf(warped2 * PI2 * 4.0);
    ////////////////////////////////////////////
    float yy     = floor(dpos * 2.0) * 0.50;
    float yya    = yy + std::clamp((dpos - yy) * 4, 0.0, 0.5);
    float yyb    = lerp(dpos, yya, modindex);
    float square = cosf(yyb * PI2);
    ////////////////////////////////////////////
    float reso1 = sinf(dpos * PI2 * (1.0 + 5.0 * modindex)) * (1.0 - dpos);
    float reso2 = sinf(dpos * PI2 * (1.0 + 3.0 * modindex)) * (1.0 - cosf(dpos * PI2)) * 0.5;
    float reso3 = sinf(dpos * PI2 * (1.0 + 4.0 * modindex)) * std::clamp(2.0 - 2.0 * dpos, 0.0, 1.0);
    ////////////////////////////////////////////
    double phaseinc = kscale * frq / 48000.0f;
    _phase += int64_t(phaseinc);
    ////////////////////////////////////////////
    U[i] = reso3;
  }
}

///////////////////////////////////////////////////////////////////////////////

void CZX::doKeyOn(const DspKeyOnInfo& koi) // final
{
  auto dspb         = koi._prv;
  auto dbd          = dspb->_dbd;
  auto oscdata      = dbd->getExtData("CZX").Get<czxdata_constptr_t>();
  auto l            = koi._layer;
  l->_HKF._miscText = FormatString("CZ\n");
  l->_HKF._useFm4   = false;
  _phase            = 0;
}
///////////////////////////////////////////////////////////////////////////////

void CZX::doKeyOff() // final
{
}

///////////////////////////////////////////////////////////////////////////////

void CZX::initBlock(dspblkdata_ptr_t blockdata, czxdata_constptr_t czdata) {
  blockdata->_dspBlock = "CZX";
  blockdata->_paramd[0].usePitchEvaluator();
  blockdata->_extdata["CZX"].Set<czxdata_constptr_t>(czdata);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
