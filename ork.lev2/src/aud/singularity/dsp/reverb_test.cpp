////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/modulation.h>
#include <ork/lev2/aud/singularity/alg_pan.inl>
#include <iostream>
#include <string>
#include <iomanip> // For std::setw and std::setprecision

ImplementReflectionX(ork::audio::singularity::TestReverbData, "DspFxReverbTest");

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

void TestReverbData::describeX(class_t* clazz) {}

TestReverbData::TestReverbData(std::string name){

}
dspblk_ptr_t TestReverbData::createInstance() const {
  return std::make_shared<TestReverb>(this);
}
TestReverb::TestReverb(const TestReverbData* trd)
  : DspBlock(trd) {

}
void TestReverb::compute(DspBuffer& dspbuf) {
  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;
  auto ilbuf     = getInpBuf(dspbuf, 0) + ibase;
  auto irbuf     = getInpBuf(dspbuf, 1) + ibase;
  auto olbuf     = getOutBuf(dspbuf, 0) + ibase;
  auto orbuf     = getOutBuf(dspbuf, 1) + ibase;
  for (int i = 0; i < inumframes; i++) {
    float inl    = ilbuf[i];
    float inr    = irbuf[i];
    float mono_inp   = (inl + inr) * 0.5f;

    float hp = _hipass.compute(mono_inp+_out*0.5); // big
    //float hp = _hipass.compute(mono_inp+_out*0.15);
    float lp0 = _lopass[0].compute(hp);
    float ap0 = _apdel[0].compute(lp0);
    float ap1 = _apdel[1].compute(ap0);
    float ap2 = _apdel[2].compute(ap1);
    float ap3 = _apdel[3].compute(ap2);
    float ap4 = _apdel[4].compute(ap3);

    float lp1 = _lopass[1].compute(ap4);
    
    float ap5 = _apdel[5].compute(lp1);
    float ap6 = _apdel[6].compute(ap5);
    float ap7 = _apdel[7].compute(ap6);
    float ap8 = _apdel[8].compute(ap7);
    float ap9 = _apdel[9].compute(ap8);

    float lp2 = _lopass[2].compute(ap9);

    float ap10 = _apdel[10].compute(lp2);
    float ap11 = _apdel[11].compute(lp2);

    _out = (ap10 + ap11);
    olbuf[i]     = inl+_out*0.0725;
    orbuf[i]     = inr+_out*0.0725;
  }
}
void TestReverb::doKeyOn(const KeyOnInfo& koi) {
  float gain1 = 0.85f; 
  float gain2 = 0.85f; 

  _apdel[0].setParams(0.00624f, gain1);
  _apdel[1].setParams(0.0001f, gain1);
  _apdel[2].setParams(0.00720f, gain1);
  _apdel[3].setParams(0.0056f, gain1);
  _apdel[4].setParams(0.00660f, gain1);

  _apdel[5].setParams(0.0088f, gain1);
  _apdel[6].setParams(0.0106f, gain1);
  _apdel[7].setParams(0.0112f, gain1);
  _apdel[8].setParams(0.0128f, gain1);
  _apdel[9].setParams(0.0138f, gain1);

  _apdel[10].setParams(0.0328f, gain2);
  _apdel[11].setParams(0.0661f, gain2);

  _hipass.set(20.0f,getSampleRate());
  _lopass[0].set(262.0f);
  _lopass[1].set(19000.0f);
  _lopass[2].set(4300.0f);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity {
