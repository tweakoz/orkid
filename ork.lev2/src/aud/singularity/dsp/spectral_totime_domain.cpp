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

ImplementReflectionX(ork::audio::singularity::ToTimeDomainData, "DspFxToTimeDomain");

namespace ork::audio::singularity {

void ToTimeDomainData::describeX(class_t* clazz) {}

struct TO_TD_IMPL{
  TO_TD_IMPL(){
    constexpr size_t kSPECTRALSIZE = ToTimeDomainData::kSPECTRALSIZE;
    _fft.init(kSPECTRALSIZE);
    _output.resize(kSPECTRALSIZE);
    size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);
    _real.resize(complex_size);
    _imag.resize(complex_size);
  }
  ~TO_TD_IMPL(){
  }
  void compute(){
    _fft.init(ToFrequencyDomainData::kSPECTRALSIZE);
    _fft.ifft(_output.data(), _real.data(), _imag.data());
  }
  audiofft::AudioFFT _fft;
  std::vector<float> _real;
  std::vector<float> _imag;
  std::vector<float> _output;
};

///////////////////////////////////////////////////////////////////////////////

ToTimeDomainData::ToTimeDomainData(std::string name, float fb)
    : DspBlockData(name) {
  _blocktype       = "ToTimeDomain";
  auto mix_param   = addParam();
  auto pitch_param = addParam();

  mix_param->useDefaultEvaluator();
  pitch_param->useDefaultEvaluator();
}
///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t ToTimeDomainData::createInstance() const { // override
  return std::make_shared<ToTimeDomain>(this);
}

///////////////////////////////////////////////////////////////////////////////

ToTimeDomain::ToTimeDomain(const ToTimeDomainData* dbd)
    : DspBlock(dbd) {
  _mydata = dbd;

  auto syni = synth::instance();
  auto impl = _impl[0].makeShared<TO_TD_IMPL>();

}
ToTimeDomain::~ToTimeDomain(){

}

///////////////////////////////////////////////////////////////////////////////

void ToTimeDomain::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;
  auto impl = _impl[0].getShared<TO_TD_IMPL>();
  //impl->compute1(this, dspbuf, ibase, inumframes);

}

///////////////////////////////////////////////////////////////////////////////

void ToTimeDomain::doKeyOn(const KeyOnInfo& koi) // final
{
  auto impl = _impl[0].getShared<TO_TD_IMPL>();
  //impl->clear();
}
} // namespace ork::audio::singularity
