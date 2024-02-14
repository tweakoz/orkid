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
  size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);
  OrkAssert(dspbuf._real.size()==complex_size);
  OrkAssert(dspbuf._imag.size()==complex_size);

  auto copy_real = dspbuf._real;
  auto copy_imag = dspbuf._imag;

// Define the cut-off for the low-pass filter
    size_t cutoff = complex_size / 8; // For example, keep the lowest eighth
    size_t transition_width = cutoff / 4; // Define a transition width for smoother attenuation

    for (size_t i = 0; i < complex_size; i++) {
        if (i < cutoff) {
            // Keep frequencies below the cutoff unchanged
            continue;
        } else if (i >= cutoff && i < cutoff + transition_width) {
            // Apply a linear attenuation in the transition zone
            float factor = 1.0f - static_cast<float>(i - cutoff) / transition_width;
            dspbuf._real[i] = copy_real[i] * factor;
            dspbuf._imag[i] = copy_imag[i] * factor;
        } else {
            // Attenuate frequencies beyond the transition zone
            dspbuf._real[i] = 0.0f;
            dspbuf._imag[i] = 0.0f;
        }
    }

    // Mirror the attenuation for the symmetric part of the spectrum if it's not inherently managed by your FFT library
    for (size_t i = complex_size - cutoff - transition_width; i < complex_size - cutoff; i++) {
        float factor = static_cast<float>(complex_size - i - cutoff) / transition_width;
        dspbuf._real[i] = copy_real[i] * factor;
        dspbuf._imag[i] = copy_imag[i] * factor;
    }
}

///////////////////////////////////////////////////////////////////////////////

void SpectralScale::doKeyOn(const KeyOnInfo& koi) { // final
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
