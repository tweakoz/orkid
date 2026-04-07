////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/audiotest.h>
#include <ork/lev2/aud/singularity/fft.h>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <vector>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

static uint64_t _nowEpochMs() {
  auto now      = std::chrono::system_clock::now();
  auto duration = now.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

///////////////////////////////////////////////////////////////////////////////
// TestPatternGenerator
///////////////////////////////////////////////////////////////////////////////

TestPatternGenerator::TestPatternGenerator(float sample_rate)
    : _pattern(TestPattern::CHIRP)
    , _sample_rate(sample_rate)
    , _sample_count(0)
    , _phase(0.0) {
  _epoch_ref_ms = _nowEpochMs();
}

void TestPatternGenerator::setPattern(TestPattern p) { _pattern = p; }
void TestPatternGenerator::setSweepConfig(const SweepConfig& cfg) { _sweep_cfg = cfg; }
void TestPatternGenerator::setChirpConfig(const ChirpConfig& cfg) { _chirp_cfg = cfg; }
void TestPatternGenerator::setMarkerConfig(const MarkerConfig& cfg) { _marker_cfg = cfg; }
void TestPatternGenerator::setStepToneConfig(const StepToneConfig& cfg) { _steptone_cfg = cfg; }
void TestPatternGenerator::setDiracConfig(const DiracConfig& cfg) { _dirac_cfg = cfg; }

uint64_t TestPatternGenerator::_epochMsForSample(uint64_t sample_index) const {
  double seconds = double(sample_index) / double(_sample_rate);
  return _epoch_ref_ms + uint64_t(seconds * 1000.0);
}

double TestPatternGenerator::currentEncodedTimeMs() const {
  double t_seconds  = double(_sample_count) / double(_sample_rate);
  double epoch_secs = double(_epoch_ref_ms) / 1000.0 + t_seconds;
  double period     = _chirp_cfg.period;
  return std::fmod(epoch_secs, period) * 1000.0;
}

void TestPatternGenerator::generate(float* output, size_t num_samples) {
  switch (_pattern) {
    case TestPattern::SWEEP:    _generateSweep(output, num_samples); break;
    case TestPattern::CHIRP:    _generateChirp(output, num_samples); break;
    case TestPattern::MARKER:   _generateMarker(output, num_samples); break;
    case TestPattern::STEPTONE: _generateStepTone(output, num_samples); break;
    case TestPattern::DIRAC:    _generateDirac(output, num_samples); break;
  }
}

///////////////////////////////////////////////////////////////////////////////
// SWEEP: simple smooth repeating frequency sweep (sample-counter based, no epoch)
///////////////////////////////////////////////////////////////////////////////

void TestPatternGenerator::_generateSweep(float* output, size_t num_samples) {
  const double dt      = 1.0 / double(_sample_rate);
  const double f_start = double(_sweep_cfg.f_start);
  const double f_end   = double(_sweep_cfg.f_end);
  const double period  = _sweep_cfg.period;
  const double f_range = f_end - f_start;

  for (size_t i = 0; i < num_samples; i++) {
    double t          = double(_sample_count + i) * dt;
    double t_mod      = std::fmod(t, period);
    double normalized = t_mod / period;
    double f_inst     = f_start + f_range * normalized;
    _phase += 2.0 * M_PI * f_inst * dt;
    if (_phase > 2.0 * M_PI * 65536.0)
      _phase -= 2.0 * M_PI * 65536.0;
    output[i] = float(std::sin(_phase));
  }
  _sample_count += num_samples;
}

///////////////////////////////////////////////////////////////////////////////
// CHIRP: epoch-time-encoded frequency sweep (for latency probing)
///////////////////////////////////////////////////////////////////////////////

void TestPatternGenerator::_generateChirp(float* output, size_t num_samples) {
  const double dt        = 1.0 / double(_sample_rate);
  const double f_start   = double(_chirp_cfg.f_start);
  const double f_end     = double(_chirp_cfg.f_end);
  const double period    = _chirp_cfg.period;
  const double f_range   = f_end - f_start;
  const double epoch_ref = double(_epoch_ref_ms) / 1000.0;

  for (size_t i = 0; i < num_samples; i++) {
    double t_seconds  = double(_sample_count + i) / double(_sample_rate);
    double epoch_secs = epoch_ref + t_seconds;
    double t_mod      = std::fmod(epoch_secs, period);
    double normalized = t_mod / period;
    double f_inst     = f_start + f_range * normalized;
    _phase += 2.0 * M_PI * f_inst * dt;
    if (_phase > 2.0 * M_PI * 65536.0)
      _phase -= 2.0 * M_PI * 65536.0;
    output[i] = float(std::sin(_phase));
  }
  _sample_count += num_samples;
}

///////////////////////////////////////////////////////////////////////////////
// MARKER: periodic tone bursts
///////////////////////////////////////////////////////////////////////////////

void TestPatternGenerator::_generateMarker(float* output, size_t num_samples) {
  const double dt             = 1.0 / double(_sample_rate);
  const double burst_freq     = double(_marker_cfg.burst_freq);
  const double burst_duration = double(_marker_cfg.burst_duration);
  const double period         = double(_marker_cfg.period);
  const float  amplitude      = _marker_cfg.amplitude;

  const double epoch_ref      = double(_epoch_ref_ms) / 1000.0;

  for (size_t i = 0; i < num_samples; i++) {
    double t_seconds   = double(_sample_count + i) / double(_sample_rate);
    double epoch_secs  = epoch_ref + t_seconds;
    double t_in_period = std::fmod(epoch_secs, period);
    if (t_in_period < burst_duration) {
      _phase += 2.0 * M_PI * burst_freq * dt;
      if (_phase > 2.0 * M_PI * 65536.0)
        _phase -= 2.0 * M_PI * 65536.0;
      output[i] = amplitude * float(std::sin(_phase));
    } else {
      output[i] = 0.0f;
    }
  }
  _sample_count += num_samples;
}

///////////////////////////////////////////////////////////////////////////////
// STEPTONE: stepped fixed frequencies
///////////////////////////////////////////////////////////////////////////////

void TestPatternGenerator::_generateStepTone(float* output, size_t num_samples) {
  const double dt            = 1.0 / double(_sample_rate);
  const int    num_freqs     = _steptone_cfg.num_frequencies;
  const double step_duration = double(_steptone_cfg.step_duration);
  const double total_period  = step_duration * num_freqs;

  const double epoch_ref     = double(_epoch_ref_ms) / 1000.0;

  for (size_t i = 0; i < num_samples; i++) {
    double t_seconds  = double(_sample_count + i) / double(_sample_rate);
    double epoch_secs = epoch_ref + t_seconds;
    double t_mod      = std::fmod(epoch_secs, total_period);
    int step_index    = int(t_mod / step_duration);
    step_index        = std::min(step_index, num_freqs - 1);
    double freq       = double(_steptone_cfg.frequencies[step_index]);
    _phase += 2.0 * M_PI * freq * dt;
    if (_phase > 2.0 * M_PI * 65536.0)
      _phase -= 2.0 * M_PI * 65536.0;
    output[i] = float(std::sin(_phase));
  }
  _sample_count += num_samples;
}

///////////////////////////////////////////////////////////////////////////////
// DIRAC: impulse train
///////////////////////////////////////////////////////////////////////////////

void TestPatternGenerator::_generateDirac(float* output, size_t num_samples) {
  const double period    = double(_dirac_cfg.period);
  const float  amplitude = _dirac_cfg.amplitude;
  const double epoch_ref = double(_epoch_ref_ms) / 1000.0;

  for (size_t i = 0; i < num_samples; i++) {
    double t_seconds    = double(_sample_count + i) / double(_sample_rate);
    double epoch_secs   = epoch_ref + t_seconds;
    uint64_t slot       = uint64_t(epoch_secs / period);
    double prev_secs    = (i > 0 || _sample_count > 0)
                          ? epoch_ref + double(_sample_count + i - 1) / double(_sample_rate)
                          : epoch_secs;
    uint64_t prev_slot  = uint64_t(prev_secs / period);
    output[i] = (slot != prev_slot) ? amplitude : 0.0f;
  }
  _sample_count += num_samples;
}

///////////////////////////////////////////////////////////////////////////////
// TestPatternProbe — pimpl
///////////////////////////////////////////////////////////////////////////////

struct TestPatternProbe::Impl {
  float _sample_rate;
  size_t _fft_size;
  ChirpConfig _chirp_cfg;

  std::vector<float> _ring;
  size_t _write_pos  = 0;
  bool _buffer_full  = false;
  mutable float _last_freq = 0.0f;

  mutable audiofft::AudioFFT _fft;

  Impl(float sr, size_t fft_size)
      : _sample_rate(sr)
      , _fft_size(fft_size)
      , _ring(fft_size, 0.0f) {
    _fft.init(_fft_size);
  }

  void write(const float* samples, size_t n) {
    for (size_t i = 0; i < n; i++) {
      _ring[_write_pos] = samples[i];
      _write_pos++;
      if (_write_pos >= _fft_size) {
        _write_pos = 0;
        _buffer_full = true;
      }
    }
  }

  float detectPeakFrequency() const {
    if (!_buffer_full) return -1.0f;

    size_t N = _fft_size;
    size_t complex_size = audiofft::AudioFFT::ComplexSize(N);

    std::vector<float> windowed(N);
    for (size_t i = 0; i < N; i++) {
      size_t idx = (_write_pos + i) % N;
      double w   = 0.42 - 0.5 * std::cos(2.0 * M_PI * i / (N - 1))
                        + 0.08 * std::cos(4.0 * M_PI * i / (N - 1));
      windowed[i] = _ring[idx] * float(w);
    }

    std::vector<float> real(complex_size), imag(complex_size);
    _fft.fft(windowed.data(), real.data(), imag.data());

    // find peak bin (skip DC)
    size_t peak_bin = 1;
    float peak_mag  = 0.0f;
    for (size_t i = 1; i < complex_size; i++) {
      float mag = real[i] * real[i] + imag[i] * imag[i];
      if (mag > peak_mag) {
        peak_mag = mag;
        peak_bin = i;
      }
    }

    // parabolic interpolation for sub-bin accuracy
    float peak_freq;
    if (peak_bin > 0 && peak_bin < complex_size - 1) {
      float mag_l = std::sqrt(real[peak_bin - 1] * real[peak_bin - 1]
                            + imag[peak_bin - 1] * imag[peak_bin - 1]);
      float mag_c = std::sqrt(peak_mag);
      float mag_r = std::sqrt(real[peak_bin + 1] * real[peak_bin + 1]
                            + imag[peak_bin + 1] * imag[peak_bin + 1]);
      float denom = mag_l - 2.0f * mag_c + mag_r;
      float delta = (std::fabs(denom) > 1e-12f)
                    ? 0.5f * (mag_l - mag_r) / denom
                    : 0.0f;
      peak_freq = (float(peak_bin) + delta) * _sample_rate / float(N);
    } else {
      peak_freq = float(peak_bin) * _sample_rate / float(N);
    }

    _last_freq = peak_freq;
    return peak_freq;
  }
};

///////////////////////////////////////////////////////////////////////////////

TestPatternProbe::TestPatternProbe(float sample_rate, size_t fft_size)
    : _impl(std::make_unique<Impl>(sample_rate, fft_size)) {
}

TestPatternProbe::~TestPatternProbe() = default;

void TestPatternProbe::setChirpConfig(const ChirpConfig& cfg) {
  _impl->_chirp_cfg = cfg;
}

void TestPatternProbe::write(const float* samples, size_t num_samples) {
  _impl->write(samples, num_samples);
}

bool TestPatternProbe::ready() const {
  return _impl->_buffer_full;
}

float TestPatternProbe::lastDetectedFrequency() const {
  return _impl->_last_freq;
}

double TestPatternProbe::detectEncodedTimeMs() const {
  float freq = _impl->detectPeakFrequency();
  if (freq < 0.0f) return -1.0;

  const auto& cfg  = _impl->_chirp_cfg;
  double period_ms = cfg.period * 1000.0;
  double f_range   = double(cfg.f_end) - double(cfg.f_start);
  if (f_range <= 0.0) return -1.0;

  double normalized = (double(freq) - double(cfg.f_start)) / f_range;
  normalized        = std::max(0.0, std::min(normalized, 1.0));
  return normalized * period_ms;
}

double TestPatternProbe::measureLatencyMs() const {
  double encoded_ms = detectEncodedTimeMs();
  if (encoded_ms < 0.0) return -1.0;

  uint64_t now_epoch = _nowEpochMs();
  double period_ms   = _impl->_chirp_cfg.period * 1000.0;
  double now_mod     = std::fmod(double(now_epoch), period_ms);

  double latency = now_mod - encoded_ms;
  if (latency < 0.0) latency += period_ms;
  if (latency > period_ms * 0.5) return -1.0;

  return latency;
}

double TestPatternProbe::measureLatencyMs(const TestPatternGenerator& source) const {
  double encoded_ms = detectEncodedTimeMs();
  if (encoded_ms < 0.0) return -1.0;

  double source_ms = source.currentEncodedTimeMs();
  double period_ms = _impl->_chirp_cfg.period * 1000.0;

  double latency = source_ms - encoded_ms;
  if (latency < 0.0) latency += period_ms;
  if (latency > period_ms * 0.5) return -1.0;

  return latency;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
