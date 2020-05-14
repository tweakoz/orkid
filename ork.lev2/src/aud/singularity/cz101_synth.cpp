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
float step(float u, float steps) {
  return floor(u * steps) / (steps - 1.0);
}

///////////////////////////////////////////////////////////////////////////////

void CZX::compute(DspBuffer& dspbuf) // final
{
  float centoff  = _param[0].eval();
  _fval[0]       = centoff;
  int inumframes = _numFrames;
  float* U       = dspbuf.channel(0);
  //_layer->_curPitchOffsetInCents = centoff;
  // todo: dco(pitch) env mod
  // todo: mi from dcw env
  static double _ph  = 0.0;
  double modindex    = 0.5f - cosf(_ph * 3) * 0.5f;
  double invmodindex = 1.0 - modindex;
  //////////////////////////////////////////
  double sawmodindex = 0.5 - modindex * 0.5;
  double sinpulindex = modindex * 0.75;
  //////////////////////////////////////////
  double lyrcents = _layer->_layerBasePitch;
  double cin      = (lyrcents + centoff) * 0.01;
  double frq      = midi_note_to_frequency(cin);
  // printf("note<%g> frq<%g>\n", cin, frq);
  //////////////////////////////////////////
  constexpr double kscale    = double(1 << 24);
  constexpr double kinvscale = 1.0 / kscale;
  //////////////////////////////////////////
  double saw, dblsine, square, sawpulse, tozpulse, reso1, reso2, reso3, coswave;
  double sawphase, squarephase, sawpulsephase, tozpulsephase;

  for (int i = 0; i < inumframes; i++) {
    int64_t pos        = _phase & 0xffffff;
    int64_t dpos       = (_phase << 1) & 0xffffff;
    bool waveswitch    = (_phase >> 24) & 1;
    bool pasthalf      = (_phase >> 23) & 1;
    double linphase    = double(pos) * kinvscale;
    double invlinphase = 1.0 - linphase;
    double linphasex2  = double(dpos) * kinvscale;
    int64_t phaseinc   = int64_t(kscale * frq / 48000.0);
    int64_t nextphase  = _phase + phaseinc;
    ////////////////////////////////////////////
    // hard sync track
    ////////////////////////////////////////////
    bool a                    = (_phase >> 24) & 1;
    bool b                    = (nextphase >> 24) & 1;
    bool hsync_trigger        = (a != b);
    _hsynctrack->_triggers[i] = hsync_trigger;
    ////////////////////////////////////////////
    // oscope track
    ////////////////////////////////////////////
    _scopetrack->_triggers[i] = (hsync_trigger and not waveswitch);
    ////////////////////////////////////////////
    // cos
    ////////////////////////////////////////////
    coswave = cosf(linphase * PI2);
    ////////////////////////////////////////////
    // tri
    ////////////////////////////////////////////
    double uni_htri = std::min(invlinphase, linphase); // 0 .. 0.5
    double uni_tri  = uni_htri * 2.0;                  // 0 .. 1
    double uni_itri = uni_htri * 2.0;                  // 1 .. 0
    double tri      = (uni_htri - 0.25) * 4.0;         // -1 .. 1
    ////////////////////////////////////////////
    // saw
    ////////////////////////////////////////////
    {
      int64_t saw_x1  = int64_t(sawmodindex * 0xffffff);
      double m1       = .5 / sawmodindex;
      double m2       = .5 / (1.0 - sawmodindex);
      double b2       = 1.0 - m2;
      double sawphase = (pos < saw_x1)        //
                            ? (m1 * linphase) //
                            : (m2 * linphase + b2);
      saw     = cosf(sawphase * PI2);
      dblsine = sinf(sawphase * PI2 * 2.0);
    }
    ////////////////////////////////////////////
    // square
    ////////////////////////////////////////////
    {
      double yy   = step(linphase, 2) * 0.5;
      double yya  = 1.0 + modindex * 64.0;
      double yyb  = std::clamp(linphase * yya, 0.0, 0.5);
      double yyc  = std::clamp((linphase - 0.5) * yya, 0.0, 0.5);
      squarephase = yyb + yyc;
      square      = cosf(squarephase * PI2); // + std::clamp(linphase * 4.0, 0.0, 0.5);
    }
    ////////////////////////////////////////////
    // sawpulse
    ////////////////////////////////////////////
    {

      double mmi  = (invmodindex)*0.5; // std::clamp(modindex, 0.0, 0.5);
      double immi = 0.5 - mmi;         // std::clamp(modindex, 0.0, 0.5);

      double sawpos   = std::clamp((linphase - 0.5) / (1.0f - 0.5), 0.0, 1.0);
      double sawidx   = 0.5 + sawmodindex;
      double m1       = .5 / sawidx;
      double m2       = .5 / (1.0 - sawidx);
      double b2       = 1.0 - m2;
      double sawphase = (sawpos < sawidx)   //
                            ? (m1 * sawpos) //
                            : (m2 * sawpos + b2);

      sawpulsephase = (linphase < immi) //
                          ? 0           //
                          : sawphase;

      sawpulsephase = lerp(linphase, sawpulsephase, invmodindex);

      sawpulse = cosf(sawpulsephase * PI2);
    }
    ////////////////////////////////////////////
    // tozpulse, yo
    ////////////////////////////////////////////
    tozpulsephase = pasthalf ? squarephase : std::clamp(sawphase, 0.0, 0.5);
    tozpulse      = cosf(tozpulsephase * PI2);
    ////////////////////////////////////////////
    double xx        = 1.0 - modindex;
    double m3        = 1.0 / xx;
    double warped2   = std::clamp(linphase * m3, -1.0, 1.0);
    double sinpulse  = sinf(warped2 * PI2);
    double cospulse  = cosf(warped2 * PI2);
    double dcospulse = cosf(warped2 * PI2 * 4.0);
    ////////////////////////////////////////////
    {
      double reso3index = std::clamp(invlinphase * 2, 0.0, 1.0);
      float res1mask    = lerp(1.0, invlinphase, modindex);
      float res2mask    = lerp(1.0, uni_itri, modindex);
      float res3mask    = lerp(1.0, reso3index, modindex);

      int64_t pos         = (_resophase)&0xffffff;
      double _linphase    = double(pos) * kinvscale;
      double _invlinphase = 1.0 - _linphase;
      double reso_bip     = cosf(_linphase * PI2);
      double reso_uni     = 0.5 + reso_bip * 0.5;
      reso1               = 1.0 - ((1.0 - reso_uni) * res1mask * 2.0);
      reso2               = 1.0 - (reso_uni * res2mask * 2.0);
      reso3               = 1.0 - ((1.0 - reso_uni) * res3mask * 2.0);
      /////////////////
      // free running resoosc (but hard synced to base osc)
      /////////////////
      double frqscale = (1.0f + modindex * 15.0f);
      double resoinc  = kscale * frq * frqscale / 48000.0;
      _resophase += int64_t(resoinc);
      if (hsync_trigger) // hardsync it
        _resophase = 0;
    }
    ////////////////////////////////////////////
    _phase = nextphase;
    ////////////////////////////////////////////
    // U[i] = waveswitch ? sawpulse : saw;
    U[i] = waveswitch ? sawpulse : saw;
    U[i] = sawpulse;
    ;
  }
  _ph += 0.003f;
} // namespace ork::audio::singularity

///////////////////////////////////////////////////////////////////////////////

void CZX::doKeyOn(const DspKeyOnInfo& koi) // final
{
  auto dspb         = koi._prv;
  auto dbd          = dspb->_dbd;
  auto oscdata      = dbd->getExtData("CZX").Get<czxdata_constptr_t>();
  auto l            = koi._layer;
  l->_HKF._miscText = FormatString("CZ\n");
  l->_HKF._useFm4   = false;

  _hsynctrack = l->_oschsynctracks[_verticalIndex];
  _scopetrack = l->_scopesynctracks[_verticalIndex];

  _phase = 0;
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
