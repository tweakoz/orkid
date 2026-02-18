////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <algorithm>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/aud/singularity/alg_pan.inl>
#include <ork/lev2/aud/singularity/modulation.h>
#include <ork/math/audiomath.h>

ImplementReflectionX(ork::audio::singularity::PANNER_DATA, "DspAmpPanner");
ImplementReflectionX(ork::audio::singularity::PANNER2D_DATA, "DspAmpPanner2D");
ImplementReflectionX(ork::audio::singularity::PANNER2DU_DATA, "DspAmpPanner2DU");

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

void PANNER_DATA::describeX(class_t* clazz) {
}

PANNER_DATA::PANNER_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "PANNER";
  addParam("POS")->useDefaultEvaluator(); // position: eval: "POS"
}
dspblk_ptr_t PANNER_DATA::createInstance() const {
  return std::make_shared<PANNER>(this);
}

PANNER::PANNER(const DspBlockData* dbd)
    : DspBlock(dbd) {
}
void PANNER::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  float* bufL    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  float* bufR    = getOutBuf(dspbuf, 1) + _layer->_dspwritebase;

  float pos = _param[0].eval();
  pos       = 0.5f + pos * 0.5f;

  // TODO: constant power
  float lmix = cosf(pos * PI * 0.5f);
  float rmix = sinf(pos * PI * 0.5f);

  _fval[0] = pos;

  // printf( "pan<%f> lmix<%f> rmix<%f>\n", pan, lmix, rmix );
  if (1)
    for (int i = 0; i < inumframes; i++) {
      float input = bufL[i] * _dbd->_inputPad;
      _plmix      = _plmix * 0.995f + lmix * 0.005f;
      _prmix      = _prmix * 0.995f + rmix * 0.005f;

      bufL[i] = input * _plmix;
      bufR[i] = input * _prmix;
    }
}
void PANNER::doKeyOn(const KeyOnInfo& koi) // final
{
  _plmix = 0.0f;
  _prmix = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

void PANNER2D_DATA::describeX(class_t* clazz) {
}

PANNER2D_DATA::PANNER2D_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "PANNER2D";
  auto P     = addParam("ANGLE");
  P->useDefaultEvaluator(); // angle: 0..2pi
  P->_units    = "radians";
  P->_keyTrack = 0;
  P->_velTrack = 0;
  P->_coarse   = 0;
  P->_fine     = 0;
  // 0 radians == directly forward

  P = addParam("DISTANCE");
  P->useDefaultEvaluator(); // angle: 0..2pi
  P->_units    = "meters";
  P->_keyTrack = 0;
  P->_velTrack = 0;
  P->_coarse   = 1;
  P->_fine     = 0;
}
dspblk_ptr_t PANNER2D_DATA::createInstance() const {
  return std::make_shared<PANNER2D>(this);
}

PANNER2D::PANNER2D(const DspBlockData* dbd)
    : DspBlock(dbd) {
  _mixL   = 0.5;
  _mixR   = 0.5;
  float Q = 0.0f;
  _filter1L.Clear();
  _filter1R.Clear();
  _fbLP.Clear();
  _dcBLOCK.Clear();
  _filter1L.SetWithQ(EM_LPF, 10000.0f, Q);
  _filter1R.SetWithQ(EM_LPF, 10000.0f, Q);
  _dcBLOCK.SetWithQ(EM_HPF, 100, Q);
  _fbLP.SetWithQ(EM_LPF, 19000.0f, Q);

  auto syni = synth::instance();
  _delayL   = syni->allocDelayLine();
  _delayR   = syni->allocDelayLine();

  _delayL->setNextDelayTime(0.001);
  _delayR->setNextDelayTime(0.001);
  _allpassA.Clear();
  _allpassB.Clear();
  _allpassC.Clear();
  _allpassA.set(1000.0f);
  _allpassB.set(1000.0f);
  _allpassC.set(1000.0f);
}

PANNER2D::~PANNER2D() {
  auto syni = synth::instance();
  syni->freeDelayLine(_delayL);
  syni->freeDelayLine(_delayR);
}
void PANNER2D::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  float* bufL    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  float* bufR    = getOutBuf(dspbuf, 1) + _layer->_dspwritebase;

  // compute delay for L,R
  // assume point source at 1m
  constexpr float SOS = 343.0; // m/s
  constexpr float E2E = .152;  // ear to ear distance in m
  float new_angle     = _param[0].eval();
  float new_distance  = _param[1].eval();

  _fval[0] = new_angle;
  fvec3 snd_pos2d(0, 0, 1);
  snd_pos2d.rotateOnY(new_angle);
  fvec3 earposL(-E2E * 0.5, 0, 0);
  fvec3 earposR(E2E * 0.5, 0, 0);

  float dL = (snd_pos2d - earposL).magnitude(); // distance to L ear in meters
  float dR = (snd_pos2d - earposR).magnitude(); // distance to R ear in meters
  float tL = dL / SOS;                          // time (seconds) for sound to travel from source to ear
  float tR = dR / SOS;                          // time (seconds) for sound to travel from source to ear
  // NOTE: delay times and filter coefficients are computed per-sample
  // inside the active branch below — no block-level state changes here
  if (0)
    printf(
        "angle<%g> snd_pos2d<%g %g %g> dL<%g> dR<%g> tL<%g> tR<%g>\n", //
        new_angle,                                                     //
        snd_pos2d.x,                                                   //
        snd_pos2d.y,                                                   //
        snd_pos2d.z,                                                   //
        dL,
        dR, //
        tL,
        tR);

  float pos = snd_pos2d.x;
  pos       = 0.5f + pos * 0.5f;

  // TODO: constant power

  float lmix = cosf(pos * PI * 0.5f);
  float rmix = sinf(pos * PI * 0.5f);

  _feedback = 0.9999;
  _a0       = 1.0f;
  _a1       = 1.0f;
  _a2       = 1.0f;

  // printf( "pan<%f> lmix<%f> rmix<%f>\n", pan, lmix, rmix );

  if (0) { // ITD cues only test
    float distanceSquared = new_distance * new_distance;
    if (distanceSquared < 1.0f)
      distanceSquared = 1.0f;
    float oneOverDistanceSquared = 1.0f / distanceSquared;
    for (int i = 0; i < inumframes; i++) {
      float fi    = float(i) / float(inumframes);
      float fb    = _fbLP.Tick(_ap2) * _feedback;
      float input = bufL[i] * _dbd->_inputPad;
      _delayL->inp(input);
      _delayR->inp(input);
      float delayedL = _delayL->out(fi);
      float delayedR = _delayR->out(fi);
      bufL[i]        = delayedL * oneOverDistanceSquared;
      bufR[i]        = delayedR * oneOverDistanceSquared;
    }
  } else if (0) { // IID cues only test
    float distanceSquared = new_distance * new_distance;
    if (distanceSquared < 1.0f)
      distanceSquared = 1.0f;
    float oneOverDistanceSquared = 1.0f / distanceSquared;
    for (int i = 0; i < inumframes; i++) {
      float fi    = float(i) / float(inumframes);
      float fb    = _fbLP.Tick(_ap2) * _feedback;
      float input = bufL[i] * _dbd->_inputPad;
      bufL[i]     = input * lmix * oneOverDistanceSquared;
      bufR[i]     = input * rmix * oneOverDistanceSquared;
    }
  } else { // ITD+IID+allpasses
    // Read configurable spatializer params from PANNER2D_DATA
    auto pd2d = static_cast<const PANNER2D_DATA*>(_dbd);
    const float refDist       = pd2d->_refDistance;
    const float maxDist       = pd2d->_maxDistance;
    const float rolloff       = pd2d->_rolloff;
    const float minGain       = ork::audiomath::decibel_to_linear_amp_ratio(pd2d->_minGainDB);
    const float headShadowMix = pd2d->_headShadowMix;
    const float iidBase       = pd2d->_iidBaseFreq;
    const float iidRange      = pd2d->_iidMaxFreq - pd2d->_iidBaseFreq;

    // One-pole smoothing: ~2ms time constant at 48kHz gives C-inf continuity
    constexpr float kSmoothAlpha = 0.01f; // 1 - exp(-1/(0.002*48000))
    for (int i = 0; i < inumframes; i++) {

      _prevAngle    += (new_angle - _prevAngle) * kSmoothAlpha;
      _prevDistance  += (new_distance - _prevDistance) * kSmoothAlpha;
      float angle    = _prevAngle;
      float distance = _prevDistance;

      // Per-sample panning coefficients from interpolated angle
      fvec3 snd_interp(0, 0, 1);
      snd_interp.rotateOnY(angle);
      float p    = 0.5f + snd_interp.x * 0.5f;
      float lm   = cosf(p * PI * 0.5f);
      float rm   = sinf(p * PI * 0.5f);

      // Per-sample ITD delay times
      fvec3 earL(-E2E * 0.5, 0, 0);
      fvec3 earR(E2E * 0.5, 0, 0);
      float distL = (snd_interp - earL).magnitude();
      float distR = (snd_interp - earR).magnitude();
      _delayL->setNextDelayTime(distL / SOS);
      _delayR->setNextDelayTime(distR / SOS);

      // Per-sample IID filter coefficients (with configurable head shadow)
      float fpFront    = 0.5f + cosf(angle) * 0.5;
      float fpLeft     = 0.5f + cosf(angle + pi * 0.5) * 0.5;
      float fpRight    = 0.5f + cosf(angle + pi * 1.5) * 0.5;
      float frqL_full  = iidBase + fpFront * iidRange * 0.6f + fpLeft * iidRange * 0.4f;
      float frqR_full  = iidBase + fpFront * iidRange * 0.6f + fpRight * iidRange * 0.4f;
      // Blend between full-bandwidth (no shadow) and filtered (full shadow)
      constexpr float kBypassFreq = 20000.0f;
      float frqL_s = frqL_full + (kBypassFreq - frqL_full) * (1.0f - headShadowMix);
      float frqR_s = frqR_full + (kBypassFreq - frqR_full) * (1.0f - headShadowMix);
      _filter1L.SetWithQ(EM_LPF, frqL_s, 0.0f);
      _filter1R.SetWithQ(EM_LPF, frqR_s, 0.0f);

      // Distance attenuation: OpenAL inverse-distance-clamped model
      float clampedDist = std::clamp(distance, refDist, maxDist);
      float distGain    = refDist / (refDist + rolloff * (clampedDist - refDist));
      distGain          = std::max(distGain, minGain);

      float fb    = _fbLP.Tick(_ap2) * _feedback;
      float input = _dcBLOCK.Tick(fb) // DC blocking for feedback loop
                    + (bufL[i] * _dbd->_inputPad);

      //////////////////
      // allpass
      //////////////////

      _ap2A._feed = 0.5f;
      _ap2B._feed = 0.5f;
      _ap2C._feed = 0.5f;
      float ap0   = _ap2A.compute(input);
      float ap1   = _ap2B.compute(ap0);
      _ap2        = _ap2C.compute(ap1);

      float ap_output = ap0 * _a0 + ap1 * _a1 + _ap2 * _a2;

      //////////////////
      // select between allpass and dry
      //  based on normalized distance
      //////////////////

      float distNorm    = std::clamp((distance - refDist) / (maxDist - refDist), 0.0f, 1.0f);
      float delay_input = distNorm * ap_output //
                          + (1.0f - distNorm) * input;

      //////////////////
      // distance attenuation
      //////////////////

      delay_input *= distGain;

      //////////////////
      // ITD delay
      //////////////////

      _delayL->inp(delay_input);
      _delayR->inp(delay_input);
      // 1.0f: per-sample setNextDelayTime means target is already exact
      float delayedL = _filter1L.Tick(_delayL->out(1.0f));
      float delayedR = _filter1R.Tick(_delayR->out(1.0f));

      //////////////////
      // final panning
      //////////////////

      bufL[i] = delayedL * lm;
      bufR[i] = delayedR * rm;
    }
  }
  // _prevAngle/_prevDistance updated continuously per-sample above
}
void PANNER2D::doKeyOn(const KeyOnInfo& koi) // final
{
  _mixL = 0.0f;
  _mixR = 0.0f;
  // Snap prev values to current so first block doesn't interpolate from 0
  _prevAngle    = _param[0].eval();
  _prevDistance  = _param[1].eval();
}

///////////////////////////////////////////////////////////////////////////////

void PANNER2DU_DATA::describeX(class_t* clazz) {
}

PANNER2DU_DATA::PANNER2DU_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "PANNER2DU";
  auto P     = addParam("ANGLE");
  P->useDefaultEvaluator(); // angle: 0..2pi
  P->_units    = "radians";
  P->_keyTrack = 0;
  P->_velTrack = 0;
  P->_coarse   = 0;
  P->_fine     = 0;
  // 0 radians == directly forward

  P = addParam("DISTANCE");
  P->useDefaultEvaluator(); // angle: 0..2pi
  P->_units    = "meters";
  P->_keyTrack = 0;
  P->_velTrack = 0;
  P->_coarse   = 1;
  P->_fine     = 0;
}
dspblk_ptr_t PANNER2DU_DATA::createInstance() const {
  return std::make_shared<PANNER2DU>(this);
}

PANNER2DU::PANNER2DU(const DspBlockData* dbd)
    : DspBlock(dbd) {
  _mixL   = 0.5;
  _mixR   = 0.5;
  float Q = 0.0f;
  _filter1L.Clear();
  _filter1R.Clear();
  _fbLP.Clear();
  _dcBLOCK.Clear();
  _filter1L.SetWithQ(EM_LPF, 10000.0f, Q);
  _filter1R.SetWithQ(EM_LPF, 10000.0f, Q);
  _dcBLOCK.SetWithQ(EM_HPF, 100, Q);
  _fbLP.SetWithQ(EM_LPF, 19000.0f, Q);

  auto syni = synth::instance();
  _delayL   = syni->allocDelayLine();
  _delayR   = syni->allocDelayLine();

  _delayL->setNextDelayTime(0.001);
  _delayR->setNextDelayTime(0.001);
  _allpassA.Clear();
  _allpassB.Clear();
  _allpassC.Clear();
  _allpassA.set(1000.0f);
  _allpassB.set(1000.0f);
  _allpassC.set(1000.0f);
}

PANNER2DU::~PANNER2DU() {
  auto syni = synth::instance();
  syni->freeDelayLine(_delayL);
  syni->freeDelayLine(_delayR);
}
void PANNER2DU::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  float* bufL    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  float* bufR    = getOutBuf(dspbuf, 1) + _layer->_dspwritebase;

  // compute delay for L,R
  // assume point source at 1m
  constexpr float SOS = 343.0; // m/s
  constexpr float E2E = .152;  // ear to ear distance in m

  _feedback = 0.9999;
  _a0       = 1.0f;
  _a1       = 1.0f;
  _a2       = 1.0f;

  float new_angle    = _param[0].eval();
  float new_distance = _param[1].eval();

  // printf( "pan<%f> lmix<%f> rmix<%f>\n", pan, lmix, rmix );
  { // ITD+IID+allpasses
    for (int i = 0; i < inumframes; i++) {
      float fi = float(i) / float(inumframes);

      float distance = fi * new_distance + (1.0f - fi) * _prevDistance; // lerp distance
      float angle    = fi * new_angle + (1.0f - fi) * _prevAngle;       // lerp angle

      _fval[0] = angle;
      fvec3 snd_pos2d(0, 0, 1);
      snd_pos2d.rotateOnY(angle);
      fvec3 earposL(-E2E * 0.5, 0, 0);
      fvec3 earposR(E2E * 0.5, 0, 0);

      float dL = (snd_pos2d - earposL).magnitude(); // distance to L ear in meters
      float dR = (snd_pos2d - earposR).magnitude(); // distance to R ear in meters
      float tL = dL / SOS;                          // time (seconds) for sound to travel from source to ear
      float tR = dR / SOS;                          // time (seconds) for sound to travel from source to ear
      _delayL->setNextDelayTime(tL);
      _delayR->setNextDelayTime(tR);

      float filtposFront       = 0.5f + cosf(angle) * 0.5;
      float filtposLeft        = 0.5f + cosf(angle + pi * 0.5) * 0.5;
      float filtposRight       = 0.5f + cosf(angle + pi * 1.5) * 0.5;
      constexpr float termBASE = 3000.0f;
      constexpr float termX    = 2000.0f;
      constexpr float termZ    = 3000.0f;
      float frqL               = termBASE + filtposFront * termZ + filtposLeft * termX;
      float frqR               = termBASE + filtposFront * termZ + filtposRight * termX;
      float Q                  = 0.0f;
      _filter1L.SetWithQ(EM_LPF, frqL, Q);
      _filter1R.SetWithQ(EM_LPF, frqR, Q);

      float pos = snd_pos2d.x;
      pos       = 0.5f + pos * 0.5f;

      // TODO: constant power

      float lmix = cosf(pos * PI * 0.5f);
      float rmix = sinf(pos * PI * 0.5f);

      float distanceSquared = distance * distance;
      if (distanceSquared < 1.0f)
        distanceSquared = 1.0f;
      float oneOverDistanceSquared = 1.0f / distanceSquared;

      float fb    = _fbLP.Tick(_ap2) * _feedback;
      float input = _dcBLOCK.Tick(fb) // DC blocking for feedback loop
                    + (bufL[i] * _dbd->_inputPad);

      //////////////////
      // allpass
      //////////////////

      _ap2A._feed = 0.5f;
      _ap2B._feed = 0.5f;
      _ap2C._feed = 0.5f;
      float ap0   = _ap2A.compute(input);
      float ap1   = _ap2B.compute(ap0);
      _ap2        = _ap2C.compute(ap1);

      float ap_output = ap0 * _a0 + ap1 * _a1 + _ap2 * _a2;

      //////////////////
      // select between allpass and dry
      //. based on distance
      //////////////////

      float delay_input = distance * ap_output //
                          + (1.0f - distance) * input;

      //////////////////
      // distance attenuation
      //////////////////

      delay_input *= oneOverDistanceSquared;

      //////////////////
      // ITD delay
      //////////////////

      _delayL->inp(delay_input);
      _delayR->inp(delay_input);
      float delayedL = _filter1L.Tick(_delayL->out(fi));
      float delayedR = _filter1R.Tick(_delayR->out(fi));

      //////////////////
      // final panning
      //////////////////

      bufL[i] = delayedL * lmix;
      bufR[i] = delayedR * rmix;
    }
  }
  _prevDistance = new_distance; // save for next frame
  _prevAngle    = new_angle;    // save for next frame
}
void PANNER2DU::doKeyOn(const KeyOnInfo& koi) // final
{
  _mixL = 0.0f;
  _mixR = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity
