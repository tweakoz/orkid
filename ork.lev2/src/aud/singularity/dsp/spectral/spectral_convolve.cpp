////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <vector>
#include <cmath>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/modulation.h>
#include <ork/lev2/aud/singularity/spectral.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/lev2/aud/singularity/fft.h>

ImplementReflectionX(ork::audio::singularity::SpectralConvolveData, "DspSpectralConvolve");

namespace ork::audio::singularity {

void SpectralConvolveData::describeX(class_t* clazz) {
}


///////////////////////////////////////////////////////////////////////////////

SpectralConvolveData::SpectralConvolveData(std::string name, float fb)
    : DspBlockData(name) {
  _blocktype = "SpectralConvolve";

  _impulse_dataset = std::make_shared<SpectralImpulseResponseDataSet>();
  for (int i = 0; i < 256; i++) {
    auto imp = std::make_shared<SpectralImpulseResponse>();
    //imp->combFilter(50 + i, 10000);
    _impulse_dataset->_impulses.push_back(imp);
  }

  auto index_param   = addParam();
  index_param->useDefaultEvaluator();
}

///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t SpectralConvolveData::createInstance() const { // override
  return std::make_shared<SpectralConvolve>(this);
}

///////////////////////////////////////////////////////////////////////////////

SpectralConvolve::SpectralConvolve(const SpectralConvolveData* dbd)
    : DspBlock(dbd) {
  _mydata             = dbd;
  auto syni           = synth::instance();
  size_t complex_size = audiofft::AudioFFT::ComplexSize(dbd->_length);
  _realL.resize(complex_size);
  _imagL.resize(complex_size);
  _realR.resize(complex_size);
  _imagR.resize(complex_size);
}

///////////////////////////////////////////////////////////////////////////////

SpectralConvolve::~SpectralConvolve() {
}

///////////////////////////////////////////////////////////////////////////////

void SpectralConvolve::compute(DspBuffer& dspbuf) {
  size_t complex_size = audiofft::AudioFFT::ComplexSize(_mydata->_length);
  OrkAssert(dspbuf._real.size() == complex_size);
  OrkAssert(dspbuf._imag.size() == complex_size);
  auto dset = _mydata->_impulse_dataset;
  float fi = _param[0].eval();
  float fmax = dset->_impulses.size()-1;
  fi = fi < 0.0f ? 0.0f : fi;
  fi = fi > fmax ? fmax : fi;
  int index = int(fi*fmax);
  auto imp  = dset->_impulses[index];
  const auto& realL    = imp->_realL;
  const auto& imagL    = imp->_imagL;
  for (size_t i = 0; i < complex_size; i++) {
    float tempReal  = dspbuf._real[i] * realL[i] - dspbuf._imag[i] * imagL[i];
    float tempImag  = dspbuf._real[i] * imagL[i] + dspbuf._imag[i] * realL[i];
    dspbuf._real[i] = tempReal;
    dspbuf._imag[i] = tempImag;
  }
}

///////////////////////////////////////////////////////////////////////////////

void SpectralConvolve::doKeyOn(const KeyOnInfo& koi) { // final
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
