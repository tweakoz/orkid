////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// Low-level audio test pattern generator and latency probe.
// These are plain C++ utility classes — not singularity DSP modules.
// Instantiable anywhere: sensors, test harnesses, probes in a DSP chain.
//
// The CHIRP pattern encodes wall-clock epoch time (milliseconds) into
// instantaneous frequency. Any probe with a clock can decode the audio,
// recover the generation timestamp, and compute absolute latency without
// needing a reference to the generator.
//
////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <memory>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
// Pattern configurations
///////////////////////////////////////////////////////////////////////////////

struct SweepConfig {
  float f_start = 300.0f;   // Hz
  float f_end   = 8000.0f;  // Hz
  double period = 10.0;     // seconds — sweep repeats after this
};

struct ChirpConfig {
  float f_start  = 300.0f;  // Hz — lower bound (above codec low-freq rolloff)
  float f_end    = 8000.0f; // Hz — upper bound (below 12kHz Nyquist after 48->24->48 codec)
  double period  = 10.0;    // seconds — sweep period, must exceed max expected latency
};

struct MarkerConfig {
  float burst_freq     = 5000.0f; // Hz
  float burst_duration = 0.1f;    // seconds
  float period         = 2.0f;    // seconds between burst onsets
  float amplitude      = 0.8f;
};

struct StepToneConfig {
  float frequencies[6] = {300, 600, 1200, 2400, 4800, 8000};
  int   num_frequencies = 6;
  float step_duration   = 1.0f; // seconds per tone
};

struct DiracConfig {
  float period    = 0.5f; // seconds between impulses
  float amplitude = 1.0f;
};

///////////////////////////////////////////////////////////////////////////////

enum class TestPattern {
  SWEEP,    // simple smooth repeating frequency sweep (no time encoding)
  CHIRP,    // epoch-time-encoded frequency sweep (for latency probing)
  MARKER,   // periodic tone bursts for onset detection
  STEPTONE, // stepped fixed frequencies for frequency response measurement
  DIRAC,    // impulse train for impulse response measurement
};

///////////////////////////////////////////////////////////////////////////////
// TestPatternGenerator
///////////////////////////////////////////////////////////////////////////////

struct TestPatternGenerator {

  TestPatternGenerator(float sample_rate = 48000.0f);

  void setPattern(TestPattern p);
  void setSweepConfig(const SweepConfig& cfg);
  void setChirpConfig(const ChirpConfig& cfg);
  void setMarkerConfig(const MarkerConfig& cfg);
  void setStepToneConfig(const StepToneConfig& cfg);
  void setDiracConfig(const DiracConfig& cfg);

  void generate(float* output, size_t num_samples);

  uint64_t sampleCount() const { return _sample_count; }
  double currentEncodedTimeMs() const;
  const ChirpConfig& chirpConfig() const { return _chirp_cfg; }

private:
  void _generateSweep(float* output, size_t num_samples);
  void _generateChirp(float* output, size_t num_samples);
  void _generateMarker(float* output, size_t num_samples);
  void _generateStepTone(float* output, size_t num_samples);
  void _generateDirac(float* output, size_t num_samples);
  uint64_t _epochMsForSample(uint64_t sample_index) const;

  TestPattern _pattern = TestPattern::SWEEP;
  float _sample_rate;
  uint64_t _sample_count = 0;
  double _phase = 0.0;
  uint64_t _epoch_ref_ms = 0;

  SweepConfig _sweep_cfg;
  ChirpConfig _chirp_cfg;
  MarkerConfig _marker_cfg;
  StepToneConfig _steptone_cfg;
  DiracConfig _dirac_cfg;
};

///////////////////////////////////////////////////////////////////////////////
// TestPatternProbe
///////////////////////////////////////////////////////////////////////////////

struct TestPatternProbe {

  TestPatternProbe(float sample_rate = 48000.0f, size_t fft_size = 4096);
  ~TestPatternProbe();

  void setChirpConfig(const ChirpConfig& cfg);
  void write(const float* samples, size_t num_samples);

  double detectEncodedTimeMs() const;
  double measureLatencyMs() const;
  double measureLatencyMs(const TestPatternGenerator& source) const;

  float lastDetectedFrequency() const;
  bool ready() const;

private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
