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
#include <ork/lev2/aud/singularity/fft.h>

ImplementReflectionX(ork::audio::singularity::TimeDomainConvolveData, "DspFxTimeDomainConvolve");

namespace ork::audio::singularity {

void TimeDomainConvolveData::describeX(class_t* clazz) {}

///////////////////////////////////////////////////////////////////////////////

struct TDCONVOLVE_IMPL{
  TDCONVOLVE_IMPL(){
  }
  ~TDCONVOLVE_IMPL(){
  }
};

///////////////////////////////////////////////////////////////////////////////

TimeDomainConvolveData::TimeDomainConvolveData(std::string name)
    : DspBlockData(name) {
  _blocktype       = "TimeDomainConvolve";
  auto mix_param   = addParam();
  auto pitch_param = addParam();

  mix_param->useDefaultEvaluator();
  pitch_param->useDefaultEvaluator();
}
///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t TimeDomainConvolveData::createInstance() const { // override
  return std::make_shared<TimeDomainConvolve>(this);
}

///////////////////////////////////////////////////////////////////////////////

TimeDomainConvolve::TimeDomainConvolve(const TimeDomainConvolveData* dbd)
    : DspBlock(dbd) {
  _mydata = dbd;

  auto syni = synth::instance();
  auto impl = _impl[0].makeShared<TDCONVOLVE_IMPL>();

}
TimeDomainConvolve::~TimeDomainConvolve(){

}

///////////////////////////////////////////////////////////////////////////////

void TimeDomainConvolve::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;
  auto impl = _impl[0].getShared<TDCONVOLVE_IMPL>();
  //impl->compute1(this, dspbuf, ibase, inumframes);

}

///////////////////////////////////////////////////////////////////////////////

void TimeDomainConvolve::doKeyOn(const KeyOnInfo& koi) // final
{
  auto impl = _impl[0].getShared<TDCONVOLVE_IMPL>();
  //impl->clear();
}
} // namespace ork::audio::singularity
