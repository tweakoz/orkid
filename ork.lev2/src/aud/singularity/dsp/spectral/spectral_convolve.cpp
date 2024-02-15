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
#include <ork/lev2/aud/singularity/fft.h>

ImplementReflectionX(ork::audio::singularity::SpectralConvolveData, "DspSpectralConvolve");

namespace ork::audio::singularity {

void SpectralConvolveData::describeX(class_t* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

SpectralImpulseResponse::SpectralImpulseResponse() {
}

///////////////////////////////////////////////////////////////////////////////

SpectralImpulseResponse::SpectralImpulseResponse(floatvect_t& impulseL, floatvect_t& impulseR) {
  set(impulseL, impulseR);
}

///////////////////////////////////////////////////////////////////////////////

float unwrapPhase(float phase) {
  while (phase < -M_PI)
    phase += 2 * M_PI;
  while (phase > M_PI)
    phase -= 2 * M_PI;
  return phase;
}

///////////////////////////////////////////////////////////////////////////////

void SpectralImpulseResponse::set(
    floatvect_t& impulseL, //
    floatvect_t& impulseR) {

  _impulseL           = impulseL;
  _impulseR           = impulseR;
  size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);

  floatvect_t irPaddedL(kSPECTRALSIZE, 0.0f);
  floatvect_t irPaddedR(kSPECTRALSIZE, 0.0f);
  std::copy(impulseL.begin(), impulseL.end(), irPaddedL.begin());
  std::copy(impulseR.begin(), impulseR.end(), irPaddedR.begin());

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

///////////////////////////////////////////////////////////////////////////////

void SpectralImpulseResponse::blend(  //
    const SpectralImpulseResponse& A, //
    const SpectralImpulseResponse& B, //
    float index) {                    //

  size_t size = std::min(A._realL.size(), B._realL.size()); // Assuming both spectrums are the same size
  _realL.resize(size);
  _imagL.resize(size);

  for (size_t i = 0; i < size; ++i) {
    // Convert to magnitude and phase for both spectrums
    float mag1   = std::sqrt(A._realL[i] * A._realL[i] + A._imagL[i] * A._imagL[i]);
    float phase1 = std::atan2(A._imagL[i], A._realL[i]);
    float mag2   = std::sqrt(B._realL[i] * B._realL[i] + B._imagL[i] * B._imagL[i]);
    float phase2 = std::atan2(B._imagL[i], B._realL[i]);

    // Unwrap phases
    phase1 = unwrapPhase(phase1);
    phase2 = unwrapPhase(phase2);

    // Interpolate magnitude and phase
    float blendedMag   = (1 - index) * mag1 + index * mag2;
    float blendedPhase = (1 - index) * phase1 + index * phase2;

    // Ensure smooth transition by handling phase wrapping
    blendedPhase = unwrapPhase(blendedPhase);

    // Convert back to real and imaginary
    _realL[i] = blendedMag * std::cos(blendedPhase);
    _imagL[i] = blendedMag * std::sin(blendedPhase);
  }
}

///////////////////////////////////////////////////////////////////////////////

void SpectralImpulseResponse::mirror() {
  size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);
  for (int i = complex_size / 2; i < complex_size; ++i) {
    int mirrorIndex = complex_size - i;
    _realL[i]       = _realL[mirrorIndex];
    _imagL[i]       = -_imagL[mirrorIndex]; // Negate for complex conjugate
    _realR[i]       = _realR[mirrorIndex];
    _imagR[i]       = -_imagR[mirrorIndex];
  }
}

///////////////////////////////////////////////////////////////////////////////

static floatvect_t _createCombFilterIR(int sampleRate, int notchSpacing, int maxFrequency) {
  // Calculate delay in samples for the given notch spacing
  int delayInSamples = sampleRate / notchSpacing;

  // Calculate the number of notches within the given frequency range
  int numberOfNotches = maxFrequency / notchSpacing;

  // The length of the IR could be determined by the last notch position
  // However, for a comb filter, we only need to define the initial delay
  floatvect_t ir(delayInSamples + 1, 0.0f); // Initialize with zeros

  // Set the first sample to 1 (the impulse)
  ir[0] = 1.0f;

  // Set the sample at the delay position to -1 to create a notch
  // In a basic comb filter, this represents the feedback or feedforward level
  ir[delayInSamples] = -1.0f; // Adjust this value as needed for your filter design

  return ir;
}

///////////////////////////////////////////////////////////////////////////////

void SpectralImpulseResponse::combFilter(float frequency, float top) {
  auto syn        = synth::instance();
  auto sampleRate = syn->sampleRate();
  auto impulseL   = _createCombFilterIR(sampleRate, frequency, top);
  auto impulseR   = _createCombFilterIR(sampleRate, frequency, top);
  set(impulseL, impulseR);
}

///////////////////////////////////////////////////////////////////////////////

void SpectralImpulseResponse::lowShelf(float frequency, float gain) {
  auto syn            = synth::instance();
  auto sampleRate     = syn->sampleRate();
  size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);

  // Initialize complex spectrum data
  _realL.assign(complex_size, 1.0f);
  _imagL.assign(complex_size, 0.0f);
  _realR.assign(complex_size, 1.0f);
  _imagR.assign(complex_size, 0.0f);

  // Calculate the cutoff index in the frequency domain
  int cutoffIndex = static_cast<int>((frequency / sampleRate) * kSPECTRALSIZE);

  float linearGain = decibel_to_linear_amp_ratio(gain);

  for (int i = 0; i < cutoffIndex; ++i) {
    _realL[i] = linearGain;
    _realR[i] = linearGain;
  }

  // mirror();
}

///////////////////////////////////////////////////////////////////////////////

void SpectralImpulseResponse::highShelf(float frequency, float gain) {
  auto syn            = synth::instance();
  auto sampleRate     = syn->sampleRate();
  size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);

  // Initialize complex spectrum data with zeros
  _realL.assign(complex_size, 1.0f);
  _imagL.assign(complex_size, 0.0f);
  _realR.assign(complex_size, 1.0f);
  _imagR.assign(complex_size, 0.0f);

  // Calculate the cutoff index in the frequency domain
  int cutoffIndex = static_cast<int>((frequency / sampleRate) * kSPECTRALSIZE);

  float linearGain = decibel_to_linear_amp_ratio(gain);

  for (int i = cutoffIndex; i < complex_size / 2; ++i) {
    // Processing half due to symmetry
    _realL[i] = linearGain;
    _realR[i] = linearGain;
  }

  // mirror();
}

///////////////////////////////////////////////////////////////////////////////

void SpectralImpulseResponse::lowRolloff(float frequency, float slope) {
  // roll off the high frequencies at a given slope (dB per octave)
  auto syn        = synth::instance();
  auto sampleRate = syn->sampleRate();
  size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);
  _realL.assign(complex_size, 1.0f);
  _imagL.assign(complex_size, 0.0f);
  size_t cutoffBin = static_cast<size_t>((frequency / sampleRate) * complex_size);
  for (size_t bin = 0; bin < complex_size; ++bin) {
    float binFrequency = static_cast<float>(bin) / complex_size * sampleRate;
    if (binFrequency > frequency) {
      float octavesAboveCutoff = log2(binFrequency / frequency);
      float gainDB             = -abs(slope) * octavesAboveCutoff;
      float lineargain         = decibel_to_linear_amp_ratio(gainDB);
      _realL[bin] *= lineargain;
      _imagL[bin] *= lineargain;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void SpectralImpulseResponse::highRolloff(float frequency, float slope) {
  // roll off the low frequencies at a given slope (dB per octave)
  auto syn        = synth::instance();
  auto sampleRate = syn->sampleRate();
  size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);
  _realL.assign(complex_size, 1.0f);
  _imagL.assign(complex_size, 0.0f);
  size_t cutoffBin = static_cast<size_t>((frequency / sampleRate) * complex_size);
  for (size_t bin = 0; bin < complex_size; ++bin) {
    float binFrequency = static_cast<float>(bin) / complex_size * sampleRate;
    if (binFrequency < frequency) {
      float octavesBelowCutoff = log2(frequency / binFrequency);
      float gainDB             = -abs(slope) * octavesBelowCutoff;
      float lineargain         = decibel_to_linear_amp_ratio(gainDB);
      _realL[bin] *= lineargain;
      _imagL[bin] *= lineargain;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

float _calculateParametricEQResponse( float binFrequency, //
                                      float centerFrequency, // 
                                      float linearGain, // 
                                      float qValue, // 
                                      float sampleRate) { //
  // Avoid division by zero

  if (binFrequency == 0.0f || centerFrequency == 0.0f || qValue == 0.0f )
    return 1.0f;

  // Bandwidth calculation
  float bandwidth          = centerFrequency / qValue;
  float omega              = 2 * M_PI * binFrequency / sampleRate;
  float omegaC             = 2 * M_PI * centerFrequency / sampleRate;
  float bandwidthInRadians = 2 * M_PI * bandwidth / sampleRate;

  // Simplified calculation for the peaking EQ gain adjustment factor
  // This is an approximation for educational purposes
  float theta          = (omega - omegaC) / bandwidthInRadians;
  float gainAdjustment = 1.0f + (linearGain - 1.0f) * std::exp(-theta * theta);

  return gainAdjustment;
}

///////////////////////////////////////////////////////////////////////////////

void SpectralImpulseResponse::parametricEQ4(
    fvec4 frequencies, //
    fvec4 gains,       //
    fvec4 qvals) {
  auto syn        = synth::instance();
  auto sampleRate = syn->sampleRate();
  size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);

  // Initialize or ensure spectral data is ready for processing
  _realL.assign(complex_size, 1.0f); // Start with unity gain for simplicity
  _imagL.assign(complex_size, 0.0f); // No initial phase change

  // Process each EQ band
  for (size_t band = 0; band < 4; ++band) {
    float frequency = frequencies.asArray()[band];
    float gain      = gains.asArray()[band];
    float qval      = qvals.asArray()[band];

    // Convert gain from dB to linear scale
    float linearGain = decibel_to_linear_amp_ratio(gain);

    // Loop through the spectrum and apply the EQ adjustments
    for (size_t bin = 0; bin < complex_size; ++bin) {
      float binFrequency = float(bin) / float(kSPECTRALSIZE) * sampleRate;

      // Calculate the frequency response of the parametric EQ for this bin
      float response = _calculateParametricEQResponse(  binFrequency, //
                                                        frequency, // 
                                                        linearGain, // 
                                                        qval, // 
                                                        sampleRate);

      //printf( "binF<%g> ctrF<%g> Q<%g> response<%g>\n", binFrequency, frequency, qval, response);
      // Apply the response to the spectral data
      _realL[bin] *= response;
      _imagL[bin] *= response;
    }
  }
}

struct Formant {
    float frequency; // Formant frequency in Hz
    float bandwidth; // Bandwidth of the formant in Hz
};

using VowelFormants = std::vector<Formant>;

// Example vowel formants for 'A', 'E', 'I', 'O', 'U' (simplified, for demonstration)
static std::map<char, VowelFormants> _vowelFormantsMap = {
    {'A', {{700, 110}, {1220, 110}, {2600, 110}}},  // Example formants for 'A'
    {'E', {{500, 110}, {1750, 110}, {2450, 110}}},  // Example formants for 'E'
    {'I', {{300, 110}, {2200, 110}, {3000, 110}}},  // Example formants for 'I'
    {'O', {{400, 110}, {800, 110},  {2600, 110}}},  // Example formants for 'O'
    {'U', {{350, 110}, {600, 110},  {2700, 110}}}   // Example formants for 'U'
};

void SpectralImpulseResponse::vowelFormant(char vowel, float strength) {
  auto syn = synth::instance();
  auto sampleRate = syn->sampleRate();
  _realL.assign(kSPECTRALSIZE, 1.0f/strength); // Initialize to unity gain
  _imagL.assign(kSPECTRALSIZE, 0.0f); // No initial phase change

  auto formants = _vowelFormantsMap[toupper(vowel)];
  for (const auto& formant : formants) {
      // Here, you would calculate and apply the band-pass filter for each formant.
      // This requires DSP knowledge to implement correctly.
      // For demonstration, we'll simply boost frequencies around the formant frequency.
      int centerBin = static_cast<int>((formant.frequency / sampleRate) * kSPECTRALSIZE);
      int bandwidthBins = static_cast<int>((formant.bandwidth / sampleRate) * kSPECTRALSIZE);

      for (int bin = centerBin - bandwidthBins; bin <= centerBin + bandwidthBins; ++bin) {
          if (bin >= 0 && bin < kSPECTRALSIZE) {
              _realL[bin] = 2.5; // Simplified example of boosting the magnitude
              // No change to _imagL[bin] to keep the example simple
          }
      }
  }
}
///////////////////////////////////////////////////////////////////////////////

SpectralConvolveData::SpectralConvolveData(std::string name, float fb)
    : DspBlockData(name) {
  _blocktype = "SpectralConvolve";

  _impulse_dataset = std::make_shared<SpectralImpulseResponseDataSet>();
  for (int i = 0; i < 256; i++) {
    auto imp = std::make_shared<SpectralImpulseResponse>();
    imp->combFilter(50 + i, 10000);
    _impulse_dataset->_impulses.push_back(imp);
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
  _mydata             = dbd;
  auto syni           = synth::instance();
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
  if ((_layer->_sampleindex & 0x001f) == 0) {
    float fi  = _layer->_layerTime;
    fi        = sin(fi*3.0f);
    int index = 128 + int(fi * 127.0f);
    auto dset = _mydata->_impulse_dataset;
    auto imp  = dset->_impulses[index];
    _realL    = imp->_realL;
    _imagL    = imp->_imagL;
  }
#endif
  // Assuming dspbuf._real and dspbuf._imag contain the FFT of the incoming signal
  // Perform element-wise multiplication for convolution in the frequency domain
  for (size_t i = 0; i < complex_size; i++) {
    float tempReal  = dspbuf._real[i] * _realL[i] - dspbuf._imag[i] * _imagL[i];
    float tempImag  = dspbuf._real[i] * _imagL[i] + dspbuf._imag[i] * _realL[i];
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
