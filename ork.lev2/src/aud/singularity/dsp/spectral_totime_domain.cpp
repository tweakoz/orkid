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

ImplementReflectionX(ork::audio::singularity::ToTimeDomainData, "DspToTimeDomain");

namespace ork::audio::singularity {

void ToTimeDomainData::describeX(class_t* clazz) {}

struct TO_TD_IMPL{
  TO_TD_IMPL(){
    _fft.init(kSPECTRALSIZE);
    _output.resize(kSPECTRALSIZE);
    _overlapBuffer.resize(kSPECTRALSIZE / 2, 0);
  }
  ~TO_TD_IMPL(){
  }
  void compute(ToTimeDomain* ttd, DspBuffer& dspbuf, int ibase, int inumframes){

    auto ibuf = ttd->getInpBuf(dspbuf, 0) + ibase;
    auto obufL = ttd->getOutBuf(dspbuf, 0) + ibase;
    auto obufR = ttd->getOutBuf(dspbuf, 1) + ibase;

    OrkAssert(kSPECTRALSIZE%inumframes==0);
    size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);
        
    OrkAssert(dspbuf._real.size()==complex_size);
    OrkAssert(dspbuf._imag.size()==complex_size);

    if(dspbuf._didFFT){
      printf( "run ifft\n");
      _fft.ifft(_output.data(), dspbuf._real.data(), dspbuf._imag.data());
      _frames_out = 0;
    }

    // output the time domain data
    for( int i=0; i<inumframes; i++ ){
      int j = _frames_out+i;
      float fj = float(j)/float(kSPECTRALSIZE-1);
      float window = 0.54 - 0.46 * cos(PI2 * fj);
      obufL[i] = _output[j]*window;
      obufR[i] = _output[j]*window;
    }
    _frames_out += inumframes;
  }
  audiofft::AudioFFT _fft;
  std::vector<float> _output;
  std::vector<float> _overlapBuffer;
  size_t _frames_out = 0;
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
  impl->compute(this, dspbuf, ibase, inumframes);

}

///////////////////////////////////////////////////////////////////////////////

void ToTimeDomain::doKeyOn(const KeyOnInfo& koi) // final
{
  auto impl = _impl[0].getShared<TO_TD_IMPL>();
  //impl->clear();
}
} // namespace ork::audio::singularity