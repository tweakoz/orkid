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

TimeToFrequencyDomain::TimeToFrequencyDomain(){
    _fft.init(kSPECTRALSIZE);
    _input.resize(kSPECTRALSIZE);
    for( int i=0; i<kSPECTRALSIZE; i++ ){
      float fj = float(i)/float(kSPECTRALSIZE-1);
      float window = 0.54 - 0.46 * cos(PI2 * fj);
      _window.push_back(window);
    }

}
TimeToFrequencyDomain::~TimeToFrequencyDomain(){

}
bool TimeToFrequencyDomain::compute(const float* inp,  //
                                    floatvect_t& real, //
                                    floatvect_t& imag, //
                                    int inumframes){   //
    bool didFFT = false;
    OrkAssert(kSPECTRALSIZE%inumframes==0);
    size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);
    //  
    if(real.size()!=kSPECTRALSIZE){
      real.resize(complex_size);
      imag.resize(complex_size);
    }

    // input the time domain data for .666mSec chunk
    for( int i=0; i<inumframes; i++ ){
      int j = _frames_in+i;
      _input[j] = inp[i];//*_window[j];
    }
    _frames_in += inumframes;

    // we have enough data to run the fft
    if(_frames_in>=kSPECTRALSIZE){
      _fft.fft(_input.data(), real.data(), imag.data());
      // overlap add 50%
      std::copy(_input.begin()+kSPECTRALSIZE/2, _input.end(), _input.begin());
      _frames_in = kSPECTRALSIZE/2;
      didFFT = true;
    }
    return didFFT;
}

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

  //auto syni = synth::instance();
  auto impl = _impl[0].makeShared<TimeToFrequencyDomain>();
}
ToFrequencyDomain::~ToFrequencyDomain(){

}

///////////////////////////////////////////////////////////////////////////////

void ToFrequencyDomain::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;
  auto impl = _impl[0].getShared<TimeToFrequencyDomain>();
  auto ibuf = this->getInpBuf(dspbuf, 0) + ibase;
  dspbuf._didFFT = impl->compute(ibuf, dspbuf._real, dspbuf._imag, inumframes);
  //impl->compute(this, dspbuf, ibase, inumframes);

}

///////////////////////////////////////////////////////////////////////////////

void ToFrequencyDomain::doKeyOn(const KeyOnInfo& koi) // final
{
  auto impl = _impl[0].getShared<TimeToFrequencyDomain>();
  //impl->clear();
}
} // namespace ork::audio::singularity
