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
#include <ork/lev2/aud/singularity/spectral.h>
#include <ork/lev2/aud/singularity/fft.h>

ImplementReflectionX(ork::audio::singularity::ToFrequencyDomainData, "DspToFrequencyDomain");

namespace ork::audio::singularity {

void ToFrequencyDomainData::describeX(class_t* clazz) {}

struct TO_FD_IMPL{
  TO_FD_IMPL(){
    _input.resize(kSPECTRALSIZE);
  }
  ~TO_FD_IMPL(){
  }
  void compute(ToFrequencyDomain* tfd, DspBuffer& dspbuf, int ibase, int inumframes){
    auto ibuf = tfd->getInpBuf(dspbuf, 0) + ibase;
    auto obuf = tfd->getOutBuf(dspbuf, 0) + ibase;
    OrkAssert(kSPECTRALSIZE%inumframes==0);
    //  
    if(dspbuf._spectrum_size!=kSPECTRALSIZE){
      size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);
      dspbuf._real.resize(complex_size);
      dspbuf._imag.resize(complex_size);
      dspbuf._spectrum_size = kSPECTRALSIZE;
    }

    for( int i=0; i<inumframes; i++ ){
      int j = _frames_in+i;
      float fj = float(j)/float(kSPECTRALSIZE);
      float window = 0.54 - 0.46 * cos(PI2 * j / (kSPECTRALSIZE - 1));
      _input[j] = ibuf[i]*window;
    }
    _frames_in += inumframes;

    if(_frames_in>=kSPECTRALSIZE){
      _fft.init(kSPECTRALSIZE);
      if( dspbuf._spectrum_size != kSPECTRALSIZE ){
      }
      printf( "run fft\n");
      _fft.fft(_input.data(), dspbuf._real.data(), dspbuf._imag.data());
      _frames_in = 0;
    }
  }
  audiofft::AudioFFT _fft;
  std::vector<float> _input;
  size_t _frames_in = 0;
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
  impl->compute(this, dspbuf, ibase, inumframes);

}

///////////////////////////////////////////////////////////////////////////////

void ToFrequencyDomain::doKeyOn(const KeyOnInfo& koi) // final
{
  auto impl = _impl[0].getShared<TO_FD_IMPL>();
  //impl->clear();
}
} // namespace ork::audio::singularity
