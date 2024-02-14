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

ImplementReflectionX(ork::audio::singularity::SpectralTestData, "DspSpectralTest");

namespace ork::audio::singularity {

void SpectralTestData::describeX(class_t* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

SpectralTestData::SpectralTestData(std::string name, float fb)
    : DspBlockData(name) {
  _blocktype = "SpectralTest";
  // auto mix_param   = addParam();
  // auto pitch_param = addParam();
  // mix_param->useDefaultEvaluator();
  // pitch_param->useDefaultEvaluator();
}
///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t SpectralTestData::createInstance() const { // override
  return std::make_shared<SpectralTest>(this);
}

///////////////////////////////////////////////////////////////////////////////

SpectralTest::SpectralTest(const SpectralTestData* dbd)
    : DspBlock(dbd) {
  _mydata   = dbd;
  auto syni = synth::instance();
}

///////////////////////////////////////////////////////////////////////////////

SpectralTest::~SpectralTest() {
}

///////////////////////////////////////////////////////////////////////////////

void SpectralTest::compute(DspBuffer& dspbuf) {
    size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);
    OrkAssert(dspbuf._real.size()==complex_size);
    OrkAssert(dspbuf._imag.size()==complex_size);

    auto syn = synth::instance();
    float elapsedTime = _layer->_layerTime;
    float sampleRate = syn->sampleRate(); 
    float modulationFrequency = 0.1; // 0.1Hz sine wave modulation

    // Calculate the modulation factor based on a 0.1Hz sine wave
    float modulationFactor = sin(2 * M_PI * modulationFrequency * elapsedTime);

    // Sweep from 60Hz to 10000Hz over 10 seconds, modulated by the sine wave
    float centerFrequency = 8000.0f+modulationFactor*7800.0f;
    float octaveBandwidth = centerFrequency/2.0f; // 1 octave bandwidth

    // Convert center frequency and bandwidth to bin numbers
    int centerBin = centerFrequency / (sampleRate / kSPECTRALSIZE);
    int bandwidthBins = octaveBandwidth / (sampleRate / kSPECTRALSIZE);

    // Calculate lower and upper bounds of the notch
    int lowerBound = centerBin - bandwidthBins;
    int upperBound = centerBin + bandwidthBins;
    int bin_count = (upperBound-lowerBound)+1;

    float maxAttenuation = decibel_to_linear_amp_ratio(-72);

    for (int i = lowerBound; i <= upperBound; i++) {
        float distance = abs(i-centerBin);
        float normalizedDistance = distance / bandwidthBins;
        // Corrected attenuation calculation
        float attenuationFactor = maxAttenuation + (1.0 - maxAttenuation) * (1.0 - normalizedDistance);
        attenuationFactor = (1.0-attenuationFactor);
        attenuationFactor = pow(attenuationFactor, 8.0);
        //printf( "normalizedDistance<%f> atten<%g>\n", normalizedDistance, attenuationFactor);
        dspbuf._real[i] *= attenuationFactor;
        dspbuf._imag[i] *= attenuationFactor;
    }
}


///////////////////////////////////////////////////////////////////////////////

void SpectralTest::doKeyOn(const KeyOnInfo& koi) { // final
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
