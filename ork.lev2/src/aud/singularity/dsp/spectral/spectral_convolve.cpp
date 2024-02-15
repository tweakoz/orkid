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

ImplementReflectionX(ork::audio::singularity::SpectralConvolveData, "DspSpectralConvolve");

namespace ork::audio::singularity {

void SpectralConvolveData::describeX(class_t* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

std::vector<float> createCombFilterIR(int sampleRate, int notchSpacing, int maxFrequency) {
    // Calculate delay in samples for the given notch spacing
    int delayInSamples = sampleRate / notchSpacing;

    // Calculate the number of notches within the given frequency range
    int numberOfNotches = maxFrequency / notchSpacing;

    // The length of the IR could be determined by the last notch position
    // However, for a comb filter, we only need to define the initial delay
    std::vector<float> ir(delayInSamples + 1, 0.0f); // Initialize with zeros

    // Set the first sample to 1 (the impulse)
    ir[0] = 1.0f;

    // Set the sample at the delay position to -1 to create a notch
    // In a basic comb filter, this represents the feedback or feedforward level
    ir[delayInSamples] = -1.0f; // Adjust this value as needed for your filter design

    return ir;
}

SpectralImpulseResponse::SpectralImpulseResponse(std::vector<float>& impulseL, std::vector<float>& impulseR)
    : _impulseL(impulseL), _impulseR(impulseR) {

  size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);

  std::vector<float> irPaddedL(kSPECTRALSIZE, 0.0f);
  std::vector<float> irPaddedR(kSPECTRALSIZE, 0.0f);
  std::copy(_impulseL.begin(), _impulseL.end(), irPaddedL.begin());
  std::copy(_impulseR.begin(), _impulseR.end(), irPaddedR.begin());
  

  _realL.resize(complex_size);
  _realR.resize(complex_size);
  _imagL.resize(complex_size);
  _imagR.resize(complex_size);

  audiofft::AudioFFT fftL, fftR;
  fftL.init(kSPECTRALSIZE);
  fftL.fft(irPaddedL.data(), _realL.data(), _imagL.data());  
  fftR.init(kSPECTRALSIZE);
  fftR.fft(irPaddedR.data(), _realR.data(), _imagR.data());  

}
SpectralConvolveData::SpectralConvolveData(std::string name, float fb)
    : DspBlockData(name) {
  _blocktype = "SpectralConvolve";

  for( int i=0; i<100; i++ ){
    float fi = float(i)/100.0f;
    float frq = 500+fi*2000;
    auto impulseL = createCombFilterIR(48000, frq, 20000);
    auto impulseR = createCombFilterIR(48000, frq, 20000);

    auto imp = std::make_shared<SpectralImpulseResponse>(impulseL,impulseR);
    _impulses.push_back(imp);
  }

  // auto mix_param   = addParam();
  // auto pitch_param = addParam();
  // mix_param->useDefaultEvaluator();
  // pitch_param->useDefaultEvaluator();
}
///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t SpectralConvolveData::createInstance() const { // override
  return std::make_shared<SpectralConvolve>(this);
}

///////////////////////////////////////////////////////////////////////////////

SpectralConvolve::SpectralConvolve(const SpectralConvolveData* dbd)
    : DspBlock(dbd) {
  _mydata   = dbd;
  auto syni = synth::instance();
  size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);
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
    size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);
    OrkAssert(dspbuf._real.size() == complex_size);
    OrkAssert(dspbuf._imag.size() == complex_size);

    #if 1
    if((_layer->_sampleindex&0x00ff)==0){
      float fi = _layer->_layerTime;
      fi = sin(fi);
      int index = 50+int(fi*49.0f);
      auto imp = _mydata->_impulses[index];   
      _realL = imp->_realL;
      _imagL = imp->_imagL;   
    }
    #endif
    // Assuming dspbuf._real and dspbuf._imag contain the FFT of the incoming signal
    // Perform element-wise multiplication for convolution in the frequency domain
    for (size_t i = 0; i < complex_size; i++) {
        float tempReal = dspbuf._real[i] * _realL[i] - dspbuf._imag[i] * _imagL[i];
        float tempImag = dspbuf._real[i] * _imagL[i] + dspbuf._imag[i] * _realL[i];
        dspbuf._real[i] = tempReal;
        dspbuf._imag[i] = tempImag;
    }

    // Now dspbuf contains the convolved signal in the frequency domain
    // You'll need to apply IFFT here to get the time-domain signal back
    // Remember to manage overlap-add if your system requires it for continuous streaming
}

///////////////////////////////////////////////////////////////////////////////

void SpectralConvolve::doKeyOn(const KeyOnInfo& koi) { // final
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
