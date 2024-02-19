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
#include <ork/util/endian.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/modulation.h>
#include <ork/lev2/aud/singularity/spectral.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/lev2/aud/singularity/fft.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

SpectralImpulseResponse::SpectralImpulseResponse() {
  size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);
  _realL.resize(complex_size);
  _imagL.resize(complex_size);
  _realR.resize(complex_size);
  _imagR.resize(complex_size);
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

void SpectralImpulseResponse::setX(
    floatvect_t& impulseL, //
    floatvect_t& impulseR) {
  _impulseL = impulseL;
  _impulseR = impulseR;
}

///////////////////////////////////////////////////////////////////////////////

void SpectralImpulseResponse::setFromFrequencyBins( //
    const floatvect_t& frequencyBinsL,              // gainDB per frequency bin (left)
    const floatvect_t& frequencyBinsR,              // gainDB per frequency bin (right)
    float samplerate                                // sample rate of the frequency bin data
) {
  size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);
  OrkAssert(frequencyBinsL.size() == frequencyBinsR.size());

  float nyquist  = samplerate / 2.0f;
  float binwidth = nyquist / float(frequencyBinsL.size());

  auto applyFrequencyBinGains = [&](floatvect_t& real,               // Real part of the spectral data to modify
                                    const floatvect_t& frequencyBins // Gain in dB per frequency bin
                                ) {
    std::fill(real.begin(), real.end(), 1.0f); // Reset to unity gain

    for (size_t i = 0; i < frequencyBins.size() - 1; ++i) {
      float linearGainStart = std::pow(10.0f, frequencyBins[i] / 20.0f);
      float linearGainEnd   = std::pow(10.0f, frequencyBins[i + 1] / 20.0f);

      size_t startBin = static_cast<size_t>((i * binwidth) / nyquist * complex_size);
      size_t endBin   = static_cast<size_t>(((i + 1) * binwidth) / nyquist * complex_size);

      for (size_t bin = startBin; bin < endBin && bin < complex_size; ++bin) {
        // Calculate the interpolation factor (0.0 at startBin, 1.0 at endBin)
        float t = static_cast<float>(bin - startBin) / static_cast<float>(endBin - startBin);
        // Linearly interpolate the gain for the current bin
        real[bin] = (1.0f - t) * linearGainStart + t * linearGainEnd;
      }
    }
    // Handle the last bin by extending the last gain value
    if (!frequencyBins.empty()) {
      float linearGainLast = std::pow(10.0f, frequencyBins.back() / 20.0f);
      for (size_t bin = static_cast<size_t>((frequencyBins.size() - 1) * binwidth / nyquist * complex_size); bin < complex_size;
           ++bin) {
        real[bin] = linearGainLast;
      }
    }
  };

  applyFrequencyBinGains(_realL, frequencyBinsL);
  applyFrequencyBinGains(_realR, frequencyBinsR);
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

void SpectralImpulseResponse::loadAudioFile(const std::string& path) {
  auto sample = _impl.makeShared<SampleData>();
  sample->loadFromAudioFile(path, false);
  const s16* data = sample->_sampleBlock;
  OrkAssert(sample->_blk_start == 0);

  floatvect_t impulse(kSPECTRALSIZE, 0.0f);

  int numframes = sample->_blk_end;
  printf("numframes<%d>\n", numframes);
  size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);
  size_t max_ir_size  = 512;
  if (numframes > max_ir_size) {
    numframes = max_ir_size;
    for (int i = 0; i < numframes; i++) {
      float window = (float(i) / float(max_ir_size - 1));
      window       = 1.0f - window;
      window       = powf(window, 0.5f);
      // window = 0.5f * (1.0f - cosf(2.0f * M_PI * window));
      // scalar = 0.5+sinf(scalar * PI)*0.5;
      // impulse[i] = scalar * float(data[i]) / 32768.0f;
      impulse[i] = window * float(data[i]) / 32768.0f;
      // printf( "impulse<%d> %g\n", i, impulse[i] );
    }
  } else {
    for (int i = 0; i < numframes; i++) {
      impulse[i] = float(data[i]) / 32768.0f;
      printf("impulse<%d> %g\n", i, impulse[i]);
    }
  }
  set(impulse, impulse);
}

///////////////////////////////////////////////////////////////////////////////

void SpectralImpulseResponse::loadAudioFileX(const std::string& path) {
  auto sample = _impl.makeShared<SampleData>();
  sample->loadFromAudioFile(path, false); // Assume loadFromAudioFile has been adjusted as discussed
  const s16* data = sample->_sampleBlock;
  OrkAssert(sample->_blk_start == 0);

  floatvect_t impulseL, impulseR; // Separate vectors for left and right channels
  int numframes   = sample->_blk_end;
  int numchannels = sample->_numChannels;
  printf("numframes<%d> numchannels<%d>\n", numframes, numchannels);

  int lastnonzero = 0;
  for (int i = 0; i < numframes * numchannels; i += numchannels) {
    for (int channel = 0; channel < numchannels; ++channel) {
      if (data[i + channel] != 0)
        lastnonzero = i / numchannels; // Update to consider multi-channel indexing
    }
  }

  int truncated = numframes - lastnonzero;
  printf("lastnonzero<%d> truncated<%d>\n", lastnonzero, truncated);
  numframes = lastnonzero;

  // Adjust the loop to handle stereo or mono files
  for (int i = 0; i < numframes; i++) {
    if (numchannels == 2) {
      // For stereo files, alternate between left and right channels
      impulseL.push_back(float(data[i * 2]) / 32768.0f);     // Left channel
      impulseR.push_back(float(data[i * 2 + 1]) / 32768.0f); // Right channel
    } else if (numchannels == 1) {
      // For mono files, duplicate the sample for both channels
      float V = float(data[i]) / 32768.0f;
      impulseL.push_back(V);
      impulseR.push_back(V);
    }
  }

  // Adjust the call to setX to use impulseL and impulseR
  setX(impulseL, impulseR);
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

  for (int i = cutoffIndex; i < complex_size; ++i) {
    // Processing half due to symmetry
    _realL[i] = linearGain;
    _realR[i] = linearGain;
  }

  // mirror();
}

///////////////////////////////////////////////////////////////////////////////

void SpectralImpulseResponse::lowRolloff(float frequency, float slope) {
  // roll off the high frequencies at a given slope (dB per octave)
  auto syn            = synth::instance();
  auto sampleRate     = syn->sampleRate();
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
  auto syn            = synth::instance();
  auto sampleRate     = syn->sampleRate();
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

void SpectralImpulseResponse::applyToStream( //
    floatvect_t& realBinsL,                  // streamed frequency bin data (left)
    floatvect_t& realBinsR,                  // streamed frequency bin data (right)
    floatvect_t& imagBinsL,                  // streamed imaginary bin data (left)
    floatvect_t& imagBinsR) const {          // streamed imaginary bin data (right)

  size_t complex_size = audiofft::AudioFFT::ComplexSize(kSPECTRALSIZE);
  OrkAssert(realBinsL.size() == complex_size);
  OrkAssert(realBinsR.size() == complex_size);
  OrkAssert(imagBinsL.size() == complex_size);
  OrkAssert(imagBinsR.size() == complex_size);

  for (size_t i = 0; i < complex_size; ++i) {
    // Complex multiplication: (a+bi)(c+di) = (ac-bd) + (bc+ad)i
    float a = realBinsL[i], b = imagBinsL[i]; // Original complex number (left)
    float c = _realL[i], d = _imagL[i];       // Impulse response complex number (left)
    realBinsL[i] = a * c - b * d;             // Real part after applying impulse response
    imagBinsL[i] = b * c + a * d;             // Imaginary part after applying impulse response

    a = realBinsR[i], b = imagBinsR[i]; // Original complex number (right)
    c = _realR[i], d = _imagR[i];       // Impulse response complex number (right)
    realBinsR[i] = a * c - b * d;       // Real part after applying impulse response
    imagBinsR[i] = b * c + a * d;       // Imaginary part after applying impulse response
  }
}

///////////////////////////////////////////////////////////////////////////////

float _calculateParametricEQResponse(
    float binFrequency,    //
    float centerFrequency, //
    float linearGain,      //
    float qValue,          //
    float sampleRate) {    //
  // Avoid division by zero

  if (binFrequency == 0.0f || centerFrequency == 0.0f || qValue == 0.0f)
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
  auto syn            = synth::instance();
  auto sampleRate     = syn->sampleRate();
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
      float response = _calculateParametricEQResponse(
          binFrequency, //
          frequency,    //
          linearGain,   //
          qval,         //
          sampleRate);

      // printf( "binF<%g> ctrF<%g> Q<%g> response<%g>\n", binFrequency, frequency, qval, response);
      //  Apply the response to the spectral data
      _realL[bin] *= response;
      _imagL[bin] *= response;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

struct Formant {
  float frequency; // Formant frequency in Hz
  float bandwidth; // Bandwidth of the formant in Hz
};

///////////////////////////////////////////////////////////////////////////////

using formants_list = std::vector<Formant>;

// Example vowel formants for 'A', 'E', 'I', 'O', 'U' (simplified, for demonstration)
static std::map<char, formants_list> _vowelFormantsMap = {
    {'A', {{700, 110}, {1220, 110}, {2600, 110}}}, // Example formants for 'A'
    {'E', {{500, 110}, {1750, 110}, {2450, 110}}}, // Example formants for 'E'
    {'I', {{300, 110}, {2200, 110}, {3000, 110}}}, // Example formants for 'I'
    {'O', {{400, 110}, {800, 110}, {2600, 110}}},  // Example formants for 'O'
    {'U', {{350, 110}, {600, 110}, {2700, 110}}}   // Example formants for 'U'
};

void SpectralImpulseResponse::vowelFormant(char vowel, float strength) {
  // auto syn = synth::instance();
  auto sampleRate = 48000.0f; // syn->sampleRate();
  // auto sampleRate = syn->sampleRate();
  _realL.assign(kSPECTRALSIZE, 1.0f / strength); // Initialize to unity gain
  _imagL.assign(kSPECTRALSIZE, 0.0f);            // No initial phase change
  _realR.assign(kSPECTRALSIZE, 1.0f / strength); // Initialize to unity gain
  _imagR.assign(kSPECTRALSIZE, 0.0f);            // No initial phase change

  auto formants = _vowelFormantsMap[toupper(vowel)];
  for (const auto& formant : formants) {
    // Here, you would calculate and apply the band-pass filter for each formant.
    // This requires DSP knowledge to implement correctly.
    // For demonstration, we'll simply boost frequencies around the formant frequency.
    int centerBin     = static_cast<int>((formant.frequency / sampleRate) * kSPECTRALSIZE);
    int bandwidthBins = static_cast<int>((formant.bandwidth / sampleRate) * kSPECTRALSIZE);

    for (int bin = centerBin - bandwidthBins; bin <= centerBin + bandwidthBins; ++bin) {
      if (bin >= 0 && bin < kSPECTRALSIZE) {
        _realL[bin] = 1.0f; // Simplified example of boosting the magnitude
        _realR[bin] = 1.0f; // Simplified example of boosting the magnitude
                            // No change to _imagL[bin] to keep the example simple
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void SpectralImpulseResponse::violinFormant(float strength) {
  auto syn        = synth::instance();
  auto sampleRate = 48000.0f; // syn->sampleRate();

  _realL.assign(kSPECTRALSIZE, 1.0f / strength); // Initialize to unity gain
  _imagL.assign(kSPECTRALSIZE, 0.0f);            // No initial phase change
  _realR.assign(kSPECTRALSIZE, 1.0f / strength); // Initialize to unity gain
  _imagR.assign(kSPECTRALSIZE, 0.0f);            // No initial phase change

  // Define violin resonant frequencies and their bandwidth

  formants_list formants = {
      {280, 100},  // Main air resonance (Helmholtz resonance)
      {450, 100},  // First major wood resonance
      {600, 100},  // Second wood resonance
      {1000, 120}, // Additional body resonance
      {1400, 150}, // Upper body resonance
      {2500, 200}, // Brilliance range start
      {3500, 300}, // Brilliance range peak
      {5000, 400}, // High-end brilliance and projection
  };

  for (auto f : formants) {
    int centerBin     = static_cast<int>((f.frequency / sampleRate) * kSPECTRALSIZE);
    int bandwidthBins = static_cast<int>((f.bandwidth / sampleRate) * kSPECTRALSIZE);
    for (int bin = centerBin - bandwidthBins; bin <= centerBin + bandwidthBins; ++bin) {
      if (bin >= 0 && bin < kSPECTRALSIZE) {
        _realL[bin] = 1.0f; // Simplified example of boosting the magnitude
        _realR[bin] = 1.0f; // Simplified example of boosting the magnitude
                            // No change to _imagL[bin] to keep the example simple
      }
    }
  }
}

} // namespace ork::audio::singularity
