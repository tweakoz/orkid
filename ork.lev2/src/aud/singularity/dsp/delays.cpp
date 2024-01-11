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

ImplementReflectionX(ork::audio::singularity::StereoDynamicEchoData, "DspFxDelayStereoDynamicEcho");

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

void StereoDynamicEchoData::describeX(class_t* clazz) {}

StereoDynamicEchoData::StereoDynamicEchoData(std::string name)
    : DspBlockData(name) {
  _blocktype            = "StereoDynamicEcho";
  auto delaytime_paramL = addParam();
  auto delaytime_paramR = addParam();
  auto feedback_param   = addParam();
  auto mix_param        = addParam();
  delaytime_paramL->useDefaultEvaluator();
  delaytime_paramR->useDefaultEvaluator();
  feedback_param->useDefaultEvaluator();
  mix_param->useDefaultEvaluator();
}

///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t StereoDynamicEchoData::createInstance() const { // override
  return std::make_shared<StereoDynamicEcho>(this);
}

///////////////////////////////////////////////////////////////////////////////

StereoDynamicEcho::StereoDynamicEcho(const StereoDynamicEchoData* dbd)
    : DspBlock(dbd) {
}

///////////////////////////////////////////////////////////////////////////////

void StereoDynamicEcho::compute(DspBuffer& dspbuf) // final
{
  float delaytimeL = _param[0].eval();
  float delaytimeR = _param[1].eval();
  float feedback   = _param[2].eval();
  float mix        = _param[3].eval();

  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;

  auto ilbuf = getInpBuf(dspbuf, 0) + ibase;
  auto irbuf = getInpBuf(dspbuf, 1) + ibase;
  auto olbuf = getOutBuf(dspbuf, 0) + ibase;
  auto orbuf = getOutBuf(dspbuf, 1) + ibase;

  float invfr = 1.0f / inumframes;

  delaytimeL = std::clamp(delaytimeL, 0.001f, 10.0f);
  delaytimeR = std::clamp(delaytimeR, 0.001f, 10.0f);


  _delayL.setNextDelayTime(delaytimeL);
  _delayR.setNextDelayTime(delaytimeR);
  for (int i = 0; i < inumframes; i++) {
    float fi = float(i) * invfr;

    float inl = ilbuf[i];
    float inr = irbuf[i];

    /////////////////////////////////////
    // read delayed signal
    /////////////////////////////////////

    float delayoutL = _delayL.out(fi);
    float delayoutR = _delayR.out(fi);

    /////////////////////////////////////
    // input to delayline
    /////////////////////////////////////

    _delayL.inp(inl + delayoutL * feedback);
    _delayR.inp(inr + delayoutR * feedback);

    /////////////////////////////////////
    // output to dsp channels
    /////////////////////////////////////

    olbuf[i] = lerp(inl, delayoutL, mix);
    orbuf[i] = lerp(inr, delayoutR, mix);
  }
}

///////////////////////////////////////////////////////////////////////////////

void StereoDynamicEcho::doKeyOn(const KeyOnInfo& koi) // final
{
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
