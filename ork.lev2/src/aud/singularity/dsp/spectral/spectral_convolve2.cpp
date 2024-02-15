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

ImplementReflectionX(ork::audio::singularity::SpectralConvolve2Data, "DspSpectralConvolve2");

namespace ork::audio::singularity {

void SpectralConvolve2Data::describeX(class_t* clazz) {
}


///////////////////////////////////////////////////////////////////////////////

SpectralConvolve2Data::SpectralConvolve2Data(std::string name, float fb)
    : DspBlockData(name) {
  _blocktype = "SpectralConvolve2";

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

dspblk_ptr_t SpectralConvolve2Data::createInstance() const { // override
  return std::make_shared<SpectralConvolve2>(this);
}

///////////////////////////////////////////////////////////////////////////////

SpectralConvolve2::SpectralConvolve2(const SpectralConvolve2Data* dbd)
    : DspBlock(dbd) {
  _mydata             = dbd;
  auto syni           = synth::instance();
}

///////////////////////////////////////////////////////////////////////////////

SpectralConvolve2::~SpectralConvolve2() {
}

///////////////////////////////////////////////////////////////////////////////

void SpectralConvolve2::compute(DspBuffer& dspbuf) {
  size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);
  OrkAssert(dspbuf._real.size() == complex_size);
  OrkAssert(dspbuf._imag.size() == complex_size);
  auto dset = _mydata->_impulse_dataset;
  float fi = _param[0].eval();
  float fmax = dset->_impulses.size()-1;
  fi = fi < 0.0f ? 0.0f : fi;
  fi = fi > fmax ? fmax : fi;
  int index = int(fi*fmax);
  auto imp  = dset->_impulses[index];
}

///////////////////////////////////////////////////////////////////////////////

void SpectralConvolve2::doKeyOn(const KeyOnInfo& koi) { // final
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
