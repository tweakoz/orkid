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

ImplementReflectionX(ork::audio::singularity::ToFrequencyDomainData, "DspFxToFrequencyDomain");

namespace ork::audio::singularity {

void ToFrequencyDomainData::describeX(class_t* clazz) {}

struct TO_FD_IMPL{
  TO_FD_IMPL(){
    constexpr size_t kSPECTRALSIZE = ToFrequencyDomainData::kSPECTRALSIZE;
    _input.resize(kSPECTRALSIZE);
    size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);
    _real.resize(complex_size);
    _imag.resize(complex_size);
  }
  ~TO_FD_IMPL(){
  }
  void compute(){
    _fft.init(ToFrequencyDomainData::kSPECTRALSIZE);
    _fft.fft(_input.data(), _real.data(), _imag.data());
  }
  audiofft::AudioFFT _fft;
  std::vector<float> _input;
  std::vector<float> _real;
  std::vector<float> _imag;
};

///////////////////////////////////////////////////////////////////////////////

ToFrequencyDomainData::ToFrequencyDomainData(std::string name, float fb)
    : DspBlockData(name) {
  _blocktype       = "ToFrequencyDomain";
  auto mix_param   = addParam();
  auto pitch_param = addParam();

  mix_param->useDefaultEvaluator();
  pitch_param->useDefaultEvaluator();
}
///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t ToFrequencyDomainData::createInstance() const { // override
  return std::make_shared<ToFrequencyDomain>(this);
}

///////////////////////////////////////////////////////////////////////////////

ToFrequencyDomain::ToFrequencyDomain(const ToFrequencyDomainData* dbd)
    : DspBlock(dbd) {
  _mydata = dbd;

  auto syni = synth::instance();
  auto impl = _impl[0].makeShared<TO_FD_IMPL>();
}
ToFrequencyDomain::~ToFrequencyDomain(){

}

///////////////////////////////////////////////////////////////////////////////

void ToFrequencyDomain::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;
  auto impl = _impl[0].getShared<TO_FD_IMPL>();
  //impl->compute1(this, dspbuf, ibase, inumframes);

}

///////////////////////////////////////////////////////////////////////////////

void ToFrequencyDomain::doKeyOn(const KeyOnInfo& koi) // final
{
  auto impl = _impl[0].getShared<TO_FD_IMPL>();
  //impl->clear();
}
} // namespace ork::audio::singularity
