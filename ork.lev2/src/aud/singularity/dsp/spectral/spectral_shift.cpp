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
  //auto mix_param   = addParam();
  //auto pitch_param = addParam();
  //mix_param->useDefaultEvaluator();
  //pitch_param->useDefaultEvaluator();
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
}

///////////////////////////////////////////////////////////////////////////////

SpectralShift::~SpectralShift(){

}

///////////////////////////////////////////////////////////////////////////////

void SpectralShift::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;
  size_t complex_size = audiofft::AudioFFT::ComplexSize(_mydata->_length);
  OrkAssert(dspbuf._real.size()==complex_size);
  OrkAssert(dspbuf._imag.size()==complex_size);
  float last_real = dspbuf._real[0];
  float last_imag = dspbuf._imag[0];
  size_t lastcplx = complex_size-1;
  for(size_t i=lastcplx; i>0; i--){
    dspbuf._real[i] = dspbuf._real[i-1];
    dspbuf._imag[i] = dspbuf._imag[i-1];
  }
  dspbuf._real[0] = 0.0f;
  dspbuf._imag[0] = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

void SpectralShift::doKeyOn(const KeyOnInfo& koi) { // final
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
