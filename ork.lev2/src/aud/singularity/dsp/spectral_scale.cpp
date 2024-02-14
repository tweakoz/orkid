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

ImplementReflectionX(ork::audio::singularity::SpectralScaleData, "DspSpectralScale");

namespace ork::audio::singularity {

void SpectralScaleData::describeX(class_t* clazz) {}

///////////////////////////////////////////////////////////////////////////////

SpectralScaleData::SpectralScaleData(std::string name, float fb)
    : DspBlockData(name) {
  _blocktype       = "SpectralScale";
  //auto mix_param   = addParam();
  //auto pitch_param = addParam();
  //mix_param->useDefaultEvaluator();
  //pitch_param->useDefaultEvaluator();
}
///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t SpectralScaleData::createInstance() const { // override
  return std::make_shared<SpectralScale>(this);
}

///////////////////////////////////////////////////////////////////////////////

SpectralScale::SpectralScale(const SpectralScaleData* dbd)
    : DspBlock(dbd) {
  _mydata = dbd;
  auto syni = synth::instance();
}

///////////////////////////////////////////////////////////////////////////////

SpectralScale::~SpectralScale(){

}

///////////////////////////////////////////////////////////////////////////////

void SpectralScale::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;
  size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);
  OrkAssert(dspbuf._real.size()==complex_size);
  OrkAssert(dspbuf._imag.size()==complex_size);
  // scale frequencies by 2, 
  // maintaining phase and total energy

  auto copy_real = dspbuf._real;
  auto copy_imag = dspbuf._imag;

  for (int i = 0; i < complex_size; i++) {
    dspbuf._real[i] = 0.0f;
    dspbuf._imag[i] = 0.0f;
  }
  size_t half_size = complex_size/2;

  /*for (int i = 0; i < complex_size; i++) {
    if((i&1)==0){
      dspbuf._real[i] = copy_real[i/2];
      dspbuf._imag[i] = copy_imag[i/2];
    }
  }
  for (int i = 0; i < half_size; i++) {
      dspbuf._real[i] = copy_real[i*2];
      dspbuf._imag[i] = copy_imag[i*2];
  }
  */
  for (int i = 0; i < half_size; i++) {
    float fj = float(i)/float(half_size);
    //float scale = sinf(fj*3.14159f);
    float scale = fj;
      //dspbuf._real[i] = copy_real[i]*scale;
      //dspbuf._imag[i] = copy_imag[i];
  }
}

///////////////////////////////////////////////////////////////////////////////

void SpectralScale::doKeyOn(const KeyOnInfo& koi) { // final
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
