////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <ork/orktypes.h>
#include <ork/math/audiomath.h>
#include <ork/file/file.h>
#include <ork/kernel/string/string.h>
#include <stdint.h>
#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/lev2/aud/singularity/cz1.h>
#include <ork/lev2/aud/singularity/fmosc.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/kernel/string/string.h>
#include <ork/util/logger.h>

using namespace ork;

namespace ork::audio::singularity {

static logchannel_ptr_t logchan_czx = logger()->createChannel("singul.czx", fvec3(1, 0.6, .8), false);

inline double sinc(double i) { // ph --1 .. +1
  if (i == 0.0)
    return 1.0;
  return sinf(i) / i;
}

///////////////////////////////////////////////////////////////////////////////

CZX::CZX(const DspBlockData* dbd)
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
  float SR          = getSampleRate();
  float ISR         = getInverseSampleRate();
  float* outsamples  = dspbuf.channel(_dspchannel[0]) + _layer->_dspwritebase;

  ////////////////////////////////////////////////
  // test tone ?
  ////////////////////////////////////////////////
  if (0) {
    int inumframes = _layer->_dspwritecount;
    for (int i = 0; i < inumframes; i++) {
      double phase  = 60.0 * PI2XISR * double(_phase);
      float samp    = sinf(phase) * .6;
      outsamples[i] = samp;
      _phase++;
    }
    return;
  }
  //////////////////////////////////////////
  constexpr double kscale    = double(1 << 24);
  constexpr double kinvscale = 1.0 / kscale;
  //////////////////////////////////////////
  /////////////////////////
  // read modulation data
  // TODO - krate computation
  /////////////////////////
  float pitch_cents  = _param[0].eval();
  float modindex = std::clamp(_param[1].eval(), 0.0f, 1.0f);
  modindex       = powf(modindex, 0.5);
  _fval[0]       = pitch_cents;
  double note     = (pitch_cents) * 0.01;
  double frq     = midi_note_to_frequency(note) * _oscdata->_octaveScale;
  double per     = SR / frq;

  if(_updatecount==0)
    logchan_czx->log("osc<%p> pitch_cents<%g> note<%g> frq<%g>", this, pitch_cents, note, frq);

  /////////////////////////
  // printf("centoff<%g>\n", centoff);
  /////////////////////////
  int inumframes = _layer->_dspwritecount;
  for (int i = 0; i < inumframes; i++) {
    //////////////////////////////////////////////
    // interpolate modindex
    //////////////////////////////////////////////
    _modIndex             = _modIndex * 0.9993f + modindex * .0007f;
    double invmodindex    = 1.0 - _modIndex;
    double sawmodindex    = 0.5 - _modIndex * 0.5;
    double invsawmodindex = 1.0 - sawmodindex;
    double sinpulindex    = _modIndex * 0.75;
    ///////////////////////////////////////////////////////////////////////////////
    // The Casio CZ noise waveform is a standard PD Osc Pitch modulated by a pseudo-random signal.
    // The modulator alternates between only two values at random every 8 samples (@ 44.1k), so that's a signal that flips
    // randomly between two states at a rate of about 5500 Hz. [White Noise]->[Sample&Hold]->[Comparator]-->[PD Osc] The
    // comparator should output either 0.0 or 2.2 Volts. The Sample and hold should be clocked at about 5.5 KHz
    ///////////////////////////////////////////////////////////////////////////////
    float moddedfrq = frq;
    if (_noisemod) {
      _noisemodcounter -= (1 << 24);
      if (_noisemodcounter <= 0) {
        _noisemodcounter += int64_t(kscale * SR / 5500.0);
        _noisevalue = (rand() & 0xffff);
      }
      double dnoise = double(_noisevalue) / 65536.0;
      moddedfrq     = frq * (8.0 + (dnoise - 0.5) * 8.0);
      // printf("moddedfrq<%g> dnoise<%g>\n", moddedfrq, dnoise);
    }
    //////////////////////////////////////////////
    // main phase accumulator
    //////////////////////////////////////////////
    int64_t pos        = _phase & 0xffffff;
    bool waveswitch    = (_phase >> 24) & 1;
    bool pasthalf      = (_phase >> 23) & 1;
    double linphase    = double(pos) * kinvscale;
    double neglinphase = 1.0 - linphase;
    ////////////////////////////////////////////
    // double speed phase accumulator
    ////////////////////////////////////////////
    constexpr double kdoublescale    = double(1 << 23);
    constexpr double kinvdoublescale = 1.0 / kdoublescale;
    int64_t dpos                     = (_phase & 0x7fffff);
    double double_linphase           = double(dpos) * kinvdoublescale;
    double double_neglinphase        = 1.0 - double_linphase;
    ////////////////////////////////////////////
    int64_t phaseinc  = int64_t(kscale * moddedfrq / SR);
    int64_t nextphase = _phase + phaseinc;
    ////////////////////////////////////////////
    // output nullwave
    ////////////////////////////////////////////
    _waveoutputs[3] = 0.0f;
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
    // saw (wave==0)
    ////////////////////////////////////////////
    {
      int64_t saw_x1  = int64_t(sawmodindex * 0xffffff);
      double m1       = .5 / sawmodindex;
      double m2       = .5 / (1.0 - sawmodindex);
      double b2       = 1.0 - m2;
      double sawphase = (pos < saw_x1)        //
                            ? (m1 * linphase) //
                            : (m2 * linphase + b2);
      double saw      = cosf(sawphase * PI2);
      _waveoutputs[0] = saw;
    }
    ////////////////////////////////////////////
    // square
    ////////////////////////////////////////////
    {
      double yy          = step(linphase, 2) * 0.5;
      double yya         = 1.0 + _modIndex * 64.0;
      double yyb         = std::clamp(linphase * yya, 0.0, 0.5);
      double yyc         = std::clamp((linphase - 0.5) * yya, 0.0, 0.5);
      double squarephase = yyb + yyc;
      double square      = cosf(squarephase * PI2); // + std::clamp(linphase * 4.0, 0.0, 0.5);
      _waveoutputs[1]    = square;
    }
    ////////////////////////////////////////////
    // pulse, pulse2 (doublepulse), sine-pulse
    ////////////////////////////////////////////
    {
      double xx = 1.0 - std::clamp(_modIndex * 0.95, 0.0, 0.95);
      double m3 = 1.0 / xx;
      ////////////////////////////////////////////
      // pulse
      ////////////////////////////////////////////
      double warped2  = std::clamp(linphase * m3, -1.0, 1.0);
      double cospulse = cosf(warped2 * PI2);
      _waveoutputs[2] = cospulse; // pulse
      ////////////////////////////////////////////
      // pulse2
      ////////////////////////////////////////////
      double double_warped   = std::clamp(double_linphase * m3, -1.0, 1.0);
      double double_cospulse = cosf(double_warped * PI2);
      _waveoutputs[7]        = double_cospulse;
      ////////////////////////////////////////////
      // sine-pulse
      ////////////////////////////////////////////
      _waveoutputs[4] = cosf(linphase * PI2) * lerp(1.0, cospulse * 0.5, _modIndex);
    }
    ////////////////////////////////////////////
    // sawpulse
    ////////////////////////////////////////////
    {
      double mmi      = (sawmodindex)*0.5;
      double immi     = 0.5 - mmi;
      double sawpos   = std::clamp((linphase - 0.5) / (1.0f - 0.5), 0.0, 1.0);
      double sawidx   = 0.5 + sawmodindex;
      double m1       = .5 / sawidx;
      double m2       = .5 / (1.0 - sawidx);
      double b2       = 1.0 - m2;
      double sawphase = (sawpos < sawidx)   //
                            ? (m1 * sawpos) //
                            : (m2 * sawpos + b2);
      double sawpulsephase = pow(linphase, 1.0 + _modIndex * 7.0);
      double sawpulse      = cosf(sawpulsephase * PI2);
      _waveoutputs[5]      = sawpulse;
    }
    ////////////////////////////////////////////
    // resowaves (window excluded)
    ////////////////////////////////////////////
    {
      if (hsync_trigger){
        _aafade = 0;
        _resophaseB = 0;
      } 

      double frqscale  = (1.0f + _modIndex * 15.0f);
      double resoinc   = kscale * frq * frqscale * ISR;
      int64_t posA      = (_resophaseA)&0xffffff;
      double _linphaseA = double(posA) * kinvscale;
      double resowaveA  = cosf(_linphaseA * PI2);
      int64_t posB      = (_resophaseB)&0xffffff;
      double _linphaseB = double(posB) * kinvscale;
      double resowaveB  = cosf(_linphaseB * PI2);


      if( _aafade < _gaafademax){
        double lerpval = double(_aafade) / float(_gaafademax);
        lerpval = powf(lerpval, 2);
        double resowave = lerp(resowaveA, resowaveB, lerpval);
        _waveoutputs[6] = resowave;
        _aafade++;
        if(_aafade==_gaafademax)
          _resophaseA = _resophaseB;
      }
      else{
        _waveoutputs[6] = resowaveA;
      }
      _resophaseA += int64_t(resoinc);
      _resophaseB += int64_t(resoinc);

    }
    ////////////////////////////////////////////
    // windowing
    ////////////////////////////////////////////
    float window = 1.0;
    if(1)switch (_oscdata->_dcoWindow) {
      case 0: // none
        break;
      case 1: // saw
        window = neglinphase;
        break;
      case 2:                      // triangle
        window = (linphase <= 0.5) //
                     ? linphase * 2.0
                     : neglinphase * 2.0;
        break;
      case 3:                      // trapezoid
        window = (linphase <= 0.5) //
                     ? 1.0
                     : neglinphase * 2.0;
        break;
      case 4:                      // pulse
        window = (linphase <= 0.5) //
                     ? 1.0 - double_linphase
                     : 0.0;
        break;
      case 5: // doublesaw
      case 6: // doublesaw
      case 7: // doublesaw
        window = double_linphase;
        break;
    }
    ////////////////////////////////////////////
    _phase = nextphase;
    ////////////////////////////////////////////
    float waveraw     = _waveoutputs[waveswitch ? _waveIDB : _waveIDA];
    float waveclamped = std::clamp(waveraw, -8.0f, 8.0f);
    float waveout     = (1.0f - waveclamped) * window;
    outsamples[i]     = (1.0f - waveout);
    //outsamples[i] = _waveoutputs[6];
    // outsamples[i] = linphase;
    // printf("i<%d> v<%g>\n", i, linphase);
    // outsamples[i] = double_linphase;
  }
  ////////////////////////////////////////////////
  _updatecount++;
} // namespace ork::audio::singularity

///////////////////////////////////////////////////////////////////////////////

void CZX::doKeyOn(const KeyOnInfo& koi) // final
{
  _oscdata       = _dbd->_vars.typedValueForKey<czxdata_constptr_t>("CZX").value();
  int dcochannel = _dbd->_vars.typedValueForKey<int>("dcochannel").value();

  // printf("CZX<%p> dcochannel<%d> keyon\n", this, dcochannel);

  auto l            = koi._layer;
  l->_HKF._miscText = FormatString("CZ\n");
  l->_HKF._useFm4   = false;
  _waveIDA          = _oscdata->_dcoBaseWaveA;
  _waveIDB          = _oscdata->_dcoBaseWaveB;
  _hsynctrack       = l->_oschsynctracks[_verticalIndex];
  _scopetrack       = l->_scopesynctracks[_verticalIndex];
  _noisemod         = _oscdata->_noisemod;
  _modIndex         = 0.0f;
  _noisemodcounter  = 0;
  _phase            = 0;
  _resophaseA       = 0;
  _resophaseB       = 0;
  _noisevalue       = 0;
  _aafade = 0;

  for (int i = 0; i < 8; i++)
    _waveoutputs[i] = 0.0f;

  _updatecount = 0;

  auto czdata = (const CZXDATA*) _dbd;
  //czdata->_cxzdata->dump();
}
///////////////////////////////////////////////////////////////////////////////

void CZX::doKeyOff() // final
{
  int dcochannel = _dbd->_vars.typedValueForKey<int>("dcochannel").value();
  // printf("CZX<%p> dcochannel<%d> keyoff\n", this, dcochannel);
}
///////////////////////////////////////////////////////////////////////////////
CZXDATA::CZXDATA(std::string name, czxdata_constptr_t czdata, int dcochannel)
    : DspBlockData(name)
    , _cxzdata(czdata)
    , _dcochannel(dcochannel) {
  _blocktype = "CZX";
  auto P = addParam("pitch","cents"); 
  P->_keyTrack = 100.0;
  P->_coarse = 60;
  P->usePitchEvaluator();
  addParam("index", "x")->useDefaultEvaluator();
  _vars.makeValueForKey<czxdata_constptr_t>("CZX") = _cxzdata;
  _vars.makeValueForKey<int>("dcochannel")         = dcochannel;
}
///////////////////////////////////////////////////////////////////////////////
dspblk_ptr_t CZXDATA::createInstance() const { // override
  auto instance = std::make_shared<CZX>(this);
  return instance;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
