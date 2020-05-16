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

inline double sinc(double i) { // ph --1 .. +1
  if (i == 0.0)
    return 1.0;
  return sinf(i) / i;
}

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
  int inumframes = _numFrames;
  float* U       = dspbuf.channel(0);
  //////////////////////////////////////////
  constexpr double kscale    = double(1 << 24);
  constexpr double kinvscale = 1.0 / kscale;
  //////////////////////////////////////////
  double saw, dblsine, square, sawpulse, tozpulse, reso1, reso2, reso3, coswave;
  double sawphase, squarephase, sawpulsephase, tozpulsephase, test;

  double lyrcents = _layer->_layerBasePitch;

  /////////////////////////
  // read modulation data
  // TODO - krate computation
  /////////////////////////
  float centoff  = _param[0].eval();
  float modindex = std::clamp(_param[1].eval(), 0.0f, 1.0f);
  _fval[0]       = centoff;
  double cin     = (lyrcents + centoff) * 0.01;
  double frq     = midi_note_to_frequency(cin);
  double per     = 48000.0 / frq;

  /////////////////////////
  // printf("modindex<%g>\n", modindex);
  /////////////////////////

  for (int i = 0; i < inumframes; i++) {

    //////////////////////////////////////////////
    // interpolate modindex
    //////////////////////////////////////////////

    _modIndex             = _modIndex * 0.9993f + modindex * .0007f;
    double invmodindex    = 1.0 - _modIndex;
    double sawmodindex    = 0.5 - _modIndex * 0.5;
    double invsawmodindex = 1.0 - sawmodindex;
    double sinpulindex    = _modIndex * 0.75;

    //////////////////////////////////////////////
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
      double yya  = 1.0 + _modIndex * 64.0;
      double yyb  = std::clamp(linphase * yya, 0.0, 0.5);
      double yyc  = std::clamp((linphase - 0.5) * yya, 0.0, 0.5);
      squarephase = yyb + yyc;
      square      = cosf(squarephase * PI2); // + std::clamp(linphase * 4.0, 0.0, 0.5);
    }
    ////////////////////////////////////////////
    // sawpulse
    ////////////////////////////////////////////
    {

      double mmi  = (sawmodindex)*0.5; // std::clamp(modindex, 0.0, 0.5);
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

      sawpulsephase = lerp(linphase, sawpulsephase, _modIndex);

      sawpulse = cosf(sawpulsephase * PI2);
    }
    ////////////////////////////////////////////
    // tozpulse, yo
    ////////////////////////////////////////////
    tozpulsephase = pasthalf ? squarephase : std::clamp(sawphase, 0.0, 0.5);
    tozpulse      = cosf(tozpulsephase * PI2);
    ////////////////////////////////////////////
    double xx        = 1.0 - _modIndex;
    double m3        = 1.0 / xx;
    double warped2   = std::clamp(linphase * m3, -1.0, 1.0);
    double sinpulse  = sinf(warped2 * PI2);
    double cospulse  = cosf(warped2 * PI2);
    double dcospulse = cosf(warped2 * PI2 * 4.0);
    ////////////////////////////////////////////
    {
      /////////////////
      float window = smoothstep(0.9, 1.0, linphase);
      /////////////////
      double res3mask = std::clamp(invlinphase * 2, 0.0, 1.0);
      float res1mask  = lerp(1.0, 0.0, linphase);
      float res2mask  = uni_itri;
      // float res3mask    = lerp(1.0, reso3index, _modIndex);
      /////////////////
      // free running resoosc (but hard synced to base osc)
      /////////////////
      double frqscale  = (1.0f + _modIndex * 15.0f);
      double resoinc   = kscale * frq * frqscale / 48000.0;
      int64_t pos      = (_resophase)&0xffffff;
      double _linphase = double(pos) * kinvscale;
      double resowave  = cosf(_linphase * PI2);
      _resophase += int64_t(resoinc);
      /////////////////
      double reso_uni = 0.5 + resowave * 0.5;
      reso1           = 1.0 - ((1.0 - reso_uni) * res1mask * 2.0);
      reso2           = 1.0 - (reso_uni * res2mask * 2.0);
      reso3           = 1.0 - ((1.0 - reso_uni) * res3mask * 2.0);
      /////////////////
      if (hsync_trigger)
        _resophase = 0;
    }
    ////////////////////////////////////////////
    _phase = nextphase;
    ////////////////////////////////////////////
    // U[i] = waveswitch ? sawpulse : saw;
    U[i] = waveswitch ? saw : reso1;
    U[i] = reso3;
    ;
  }
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
  blockdata->addParam().usePitchEvaluator();
  blockdata->addParam().useDefaultEvaluator();
  blockdata->_extdata["CZX"].Set<czxdata_constptr_t>(czdata);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
