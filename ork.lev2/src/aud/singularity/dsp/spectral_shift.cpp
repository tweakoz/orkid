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

ImplementReflectionX(ork::audio::singularity::SpectralShiftData, "DspSpectralShift");

namespace ork::audio::singularity {

void SpectralShiftData::describeX(class_t* clazz) {}

///////////////////////////////////////////////////////////////////////////////

SpectralShiftData::SpectralShiftData(std::string name, float fb)
    : DspBlockData(name) {
  _blocktype       = "SpectralShift";
  auto mix_param   = addParam();
  auto pitch_param = addParam();

  mix_param->useDefaultEvaluator();
  pitch_param->useDefaultEvaluator();
}
///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t SpectralShiftData::createInstance() const { // override
  return std::make_shared<SpectralShift>(this);
}

///////////////////////////////////////////////////////////////////////////////

SpectralShift::SpectralShift(const SpectralShiftData* dbd)
    : DspBlock(dbd) {
  _mydata = dbd;

  auto syni = synth::instance();
  //auto impl = _impl[0].makeShared<TO_TD_IMPL>();

}
SpectralShift::~SpectralShift(){

}

///////////////////////////////////////////////////////////////////////////////

void SpectralShift::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;
  //auto impl = _impl[0].getShared<TO_TD_IMPL>();
  //impl->compute(this, dspbuf, ibase, inumframes);
  if(0){
    size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);
    OrkAssert(dspbuf._real.size()==complex_size);
    OrkAssert(dspbuf._imag.size()==complex_size);
    float last_real = dspbuf._real[0];
    float last_imag = dspbuf._imag[0];
    for(size_t i=0; i<complex_size-1; ++i){
      dspbuf._real[i] = dspbuf._real[i+1];
      dspbuf._imag[i] = dspbuf._imag[i+1];
    }
    dspbuf._real[complex_size-1] = last_real;
    dspbuf._imag[complex_size-1] = last_imag;
  }
}

///////////////////////////////////////////////////////////////////////////////

void SpectralShift::doKeyOn(const KeyOnInfo& koi) // final
{
  //auto impl = _impl[0].getShared<TO_TD_IMPL>();
  //impl->clear();
}
} // namespace ork::audio::singularity
