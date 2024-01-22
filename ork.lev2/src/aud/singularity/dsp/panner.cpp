////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/aud/singularity/alg_pan.inl>
#include <ork/lev2/aud/singularity/modulation.h>

ImplementReflectionX(ork::audio::singularity::PANNER_DATA, "DspAmpPanner");
ImplementReflectionX(ork::audio::singularity::PANNER2D_DATA, "DspAmpPanner2D");

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

void PANNER_DATA::describeX(class_t* clazz){

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

  float pos      = _param[0].eval();
  pos = 0.5f+pos*0.5f;

  // TODO: constant power
  float lmix = cosf(pos*PI*0.5f);
  float rmix = sinf(pos*PI*0.5f);

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

void PANNER2D_DATA::describeX(class_t* clazz){
}

PANNER2D_DATA::PANNER2D_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "PANNER2D";
  auto P = addParam("ANGLE");
  P->useDefaultEvaluator(); // angle: 0..2pi
  P->_units = "radians";
  P->_keyTrack = 0;
  P->_velTrack = 0;
  P->_coarse = 0;
  P->_fine = 0;
  // 0 radians == directly forward

  P = addParam("DISTANCE");
  P->useDefaultEvaluator(); // angle: 0..2pi
  P->_units = "meters";
  P->_keyTrack = 0;
  P->_velTrack = 0;
  P->_coarse = 1;
  P->_fine = 0;

}
dspblk_ptr_t PANNER2D_DATA::createInstance() const {
  return std::make_shared<PANNER2D>(this);
}

PANNER2D::PANNER2D(const DspBlockData* dbd)
    : DspBlock(dbd) {
    _mixL = 0.5;
    _mixR = 0.5;
    float Q = 0.0f;
    _filter1L.Clear();
    _filter1R.Clear();
    _fbLP.Clear();
    _dcBLOCK.Clear();
    _filter1L.SetWithQ(EM_LPF,10000.0f,Q);
    _filter1R.SetWithQ(EM_LPF,10000.0f,Q);
    _dcBLOCK.SetWithQ(EM_HPF,100,Q);
    _fbLP.SetWithQ(EM_LPF,19000.0f,Q);

    auto syni = synth::instance();
    _delayL = syni->allocDelayLine();
    _delayR = syni->allocDelayLine();

    _delayL->setNextDelayTime(0.001);
    _delayR->setNextDelayTime(0.001);
    _allpassA.Clear();
    _allpassB.Clear();
    _allpassC.Clear();
    _allpassA.set(1000.0f);
    _allpassB.set(1000.0f);
    _allpassC.set(1000.0f);
}
void PANNER2D::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  float* bufL    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  float* bufR    = getOutBuf(dspbuf, 1) + _layer->_dspwritebase;

  // compute delay for L,R
  // assume point source at 1m
  constexpr float SOS = 343.0; // m/s
  constexpr float E2E = .152; // ear to ear distance in m
  float angle = _param[0].eval();
  float distance = _param[1].eval();

  _fval[0] = angle;
  fvec3 snd_pos2d(0,0,1);
  snd_pos2d.rotateOnY(angle);
  fvec3 earposL(-E2E*0.5,0,0);
  fvec3 earposR(E2E*0.5,0,0);

  float dL = (snd_pos2d-earposL).magnitude(); // distance to L ear in meters
  float dR = (snd_pos2d-earposR).magnitude(); // distance to R ear in meters
  float tL = dL/SOS; // time (seconds) for sound to travel from source to ear
  float tR = dR/SOS; // time (seconds) for sound to travel from source to ear
  _delayL->setNextDelayTime(tL);
  _delayR->setNextDelayTime(tR);

  float filtposFront  = 0.5f+cosf(angle)*0.5;
  float filtposLeft  = 0.5f+cosf(angle+pi*0.5)*0.5;
  float filtposRight = 0.5f+cosf(angle+pi*1.5)*0.5;
  constexpr float termBASE = 3000.0f;
  constexpr float termX = 2000.0f;
  constexpr float termZ = 3000.0f;
  float frqL = termBASE+filtposFront*termZ+filtposLeft*termX;
  float frqR = termBASE+filtposFront*termZ+filtposRight*termX;
  float Q = 0.0f;
  _filter1L.SetWithQ(EM_LPF,frqL,Q);
  _filter1R.SetWithQ(EM_LPF,frqR,Q);
  if(0) printf("angle<%g> snd_pos2d<%g %g %g> dL<%g> dR<%g> tL<%g> tR<%g>\n", //
          angle, //
          snd_pos2d.x, //
          snd_pos2d.y, //
          snd_pos2d.z, //
          dL, dR, //
          tL, tR );

  float pos = snd_pos2d.x;
  pos = 0.5f+pos*0.5f;

  // TODO: constant power

  float lmix = cosf(pos*PI*0.5f);
  float rmix = sinf(pos*PI*0.5f);

  _feedback = 0.9999;
  _a0 = 1.0f;
  _a1 = 1.0f;
  _a2 = 1.0f;

  float distanceSquared = distance*distance;
  if(distanceSquared<1.0f) 
    distanceSquared = 1.0f;
  float oneOverDistanceSquared = 1.0f/distanceSquared;

  // printf( "pan<%f> lmix<%f> rmix<%f>\n", pan, lmix, rmix );

  if (0){ // ITD cues only test
    for (int i = 0; i < inumframes; i++) {
     float fi = float(i)/float(inumframes);
      float fb = _fbLP.Tick(_ap2)*_feedback;
      float input = bufL[i] * _dbd->_inputPad;
      _delayL->inp(input);
      _delayR->inp(input);
      float delayedL = _delayL->out(fi);
      float delayedR = _delayR->out(fi);
      bufL[i] = delayedL*oneOverDistanceSquared;
      bufR[i] = delayedR*oneOverDistanceSquared;
    }
  }
  else if (0){ // IID cues only test
    for (int i = 0; i < inumframes; i++) {
     float fi = float(i)/float(inumframes);
      float fb = _fbLP.Tick(_ap2)*_feedback;
      float input = bufL[i] * _dbd->_inputPad;
      bufL[i] = input*lmix*oneOverDistanceSquared;
      bufR[i] = input*rmix*oneOverDistanceSquared;
    }
  }
  else{ // ITD+IID+allpasses
    for (int i = 0; i < inumframes; i++) {
      float fi = float(i)/float(inumframes);
      
      float fb = _fbLP.Tick(_ap2)*_feedback;
      float input = _dcBLOCK.Tick(fb) // DC blocking for feedback loop
                  + (bufL[i] * _dbd->_inputPad);


      //////////////////
      // allpass 
      //////////////////

      _ap2A._feed = 0.5f;
      _ap2B._feed = 0.5f;
      _ap2C._feed = 0.5f;
      float ap0 = _ap2A.compute(input);
      float ap1 = _ap2B.compute(ap0);
      _ap2 = _ap2C.compute(ap1);

      float ap_output = ap0*_a0 + ap1*_a1 + _ap2*_a2;

      //////////////////
      // select between allpass and dry
      //. based on distance
      //////////////////

      float delay_input = distance*ap_output //
                        +(1.0f-distance)*input;

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

      bufL[i] = delayedL*lmix;
      bufR[i] = delayedR*rmix;
    }
  }
}
void PANNER2D::doKeyOn(const KeyOnInfo& koi) // final
{
  _mixL = 0.0f;
  _mixR = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

} //namespace ork::audio::singularity {
