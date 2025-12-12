////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/filters.h>
#include <cmath>
#include <algorithm>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
// Utility functions
///////////////////////////////////////////////////////////////////////////////

static inline float linearToDb(float linear) {
  constexpr float MIN_DB = -120.0f;
  if (linear <= 0.0f) return MIN_DB;
  return 20.0f * std::log10(linear);
}

static inline float dbToLinear(float dB) {
  return std::pow(10.0f, dB / 20.0f);
}

// Time constant: attempt to reach ~63% in given time (ms)
static inline float timeToCoeff(float ms) {
  if (ms <= 0.0f) return 1.0f;
  return 1.0f - std::exp(-1.0f / (ms * 0.001f * getSampleRate()));
}

///////////////////////////////////////////////////////////////////////////////
// Compressor Implementation
///////////////////////////////////////////////////////////////////////////////

Compressor::Compressor() {
  setupMedium();
}

void Compressor::setThreshold(float dB) {
  _threshold_dB = dB;
}

void Compressor::setRatio(float ratio) {
  _ratio = std::max(1.0f, ratio);
}

void Compressor::setAttack(float ms) {
  _attack_coeff = timeToCoeff(ms);
}

void Compressor::setRelease(float ms) {
  _release_coeff = timeToCoeff(ms);
}

void Compressor::setMakeupGain(float dB) {
  _makeup_linear = dbToLinear(dB);
}

void Compressor::setKnee(float dB) {
  _knee_dB = std::max(0.0f, dB);
}

float Compressor::getMakeupGain() const {
  return linearToDb(_makeup_linear);
}

///////////////////////////////////////////////////////////////////////////////
// Presets
///////////////////////////////////////////////////////////////////////////////

void Compressor::setupForVoice(float threshold_dB, float ratio) {
  setThreshold(threshold_dB);
  setRatio(ratio);
  setAttack(5.0f);      // fast attack for transients
  setRelease(50.0f);    // moderate release
  setKnee(6.0f);        // soft knee for transparency
  setMakeupGain(threshold_dB*0.3);  // no makeup (preserve dynamics)
}

void Compressor::setupGentle() {
  setThreshold(-18.0f);
  setRatio(2.0f);
  setAttack(20.0f);
  setRelease(200.0f);
  setKnee(12.0f);
  setMakeupGain(3.0f);
}

void Compressor::setupMedium() {
  setThreshold(-16.0f);
  setRatio(4.0f);
  setAttack(10.0f);
  setRelease(100.0f);
  setKnee(6.0f);
  setMakeupGain(4.0f);
}

void Compressor::setupHeavy() {
  setThreshold(-12.0f);
  setRatio(10.0f);
  setAttack(1.0f);
  setRelease(50.0f);
  setKnee(3.0f);
  setMakeupGain(6.0f);
}

///////////////////////////////////////////////////////////////////////////////
// Processing
///////////////////////////////////////////////////////////////////////////////

void Compressor::clear() {
  _envelope = 0.0f;
  _gain_reduction_dB = 0.0f;
}

float Compressor::computeGain(float input_dB) {
  // Soft knee compression curve
  float overshoot = input_dB - _threshold_dB;

  if (_knee_dB > 0.0f && overshoot > -_knee_dB * 0.5f && overshoot < _knee_dB * 0.5f) {
    // In the knee region - quadratic interpolation
    float knee_factor = (overshoot + _knee_dB * 0.5f) / _knee_dB;
    float compressed = overshoot * knee_factor * (1.0f - 1.0f / _ratio);
    return -compressed;
  } else if (overshoot <= -_knee_dB * 0.5f) {
    // Below threshold
    return 0.0f;
  } else {
    // Above threshold - apply ratio
    return -(overshoot - _knee_dB * 0.5f) * (1.0f - 1.0f / _ratio);
  }
}

float Compressor::compute(float input) {
  // Get input level in dB (using absolute value for envelope)
  float input_abs = std::fabs(input);
  float input_dB = linearToDb(input_abs);

  // Compute desired gain reduction
  float target_gr = computeGain(input_dB);

  // Smooth envelope with attack/release
  float coeff = (target_gr < _gain_reduction_dB) ? _attack_coeff : _release_coeff;
  _gain_reduction_dB += coeff * (target_gr - _gain_reduction_dB);

  // Apply gain reduction and makeup
  float gain = dbToLinear(_gain_reduction_dB) * _makeup_linear;
  return input * gain;
}

void Compressor::computeBlock(float* samples, size_t count) {
  for (size_t i = 0; i < count; i++) {
    samples[i] = compute(samples[i]);
  }
}

///////////////////////////////////////////////////////////////////////////////
// NoiseGate Implementation
///////////////////////////////////////////////////////////////////////////////

NoiseGate::NoiseGate() {
  setupForVoice();
}

void NoiseGate::setThreshold(float threshold_dB) {
  // Convert dB threshold to linear energy threshold
  // e.g., -40dB -> 0.0001, -60dB -> 0.000001
  _threshold_dB = threshold_dB;
  float linear = dbToLinear(threshold_dB);
  _threshold = linear * linear;  // energy is amplitude squared
}

void NoiseGate::setAttack(float ms) {
  // Time-based attack coefficient (ms)
  _attack_ms = ms;
  _attack_coeff = timeToCoeff(ms);
}

void NoiseGate::setRelease(float ms) {
  // Time-based release coefficient (ms)
  _release_ms = ms;
  _release_coeff = timeToCoeff(ms);
}

void NoiseGate::setMaxAttenuation(float dB) {
  // Max attenuation when gate is closed (e.g., -40dB means signal reduced to -40dB, not fully muted)
  // Use very negative value (e.g., -80) for near-full mute
  _max_atten_dB = std::min(0.0f, dB);
  _min_gain = dbToLinear(_max_atten_dB);
}

void NoiseGate::setInputGain(float gain) {
  _input_gain = std::max(0.0f, gain);
}

void NoiseGate::setOutputGain(float gain) {
  _output_gain = std::max(0.0f, gain);
}

void NoiseGate::setupForVoice() {
  setThreshold(-40.0f);       // -40dB threshold
  setAttack(5.0f);            // 5ms attack (fast open)
  setRelease(200.0f);         // 200ms release (slow close, avoid chatter)
  setMaxAttenuation(-40.0f);  // -40dB max attenuation (not full mute)
  _input_gain = 1.0f;
  _output_gain = 1.0f;
}

void NoiseGate::clear() {
  _envelope = 0.0f;
  _energy = 0.0f;
}

float NoiseGate::compute(float input) {
  // Apply input gain
  float sample = input * _input_gain;

  // Measure energy with exponential smoothing (~10ms window at 48kHz)
  constexpr float energy_coeff = 0.9995f;
  _energy = _energy * energy_coeff + sample * sample * (1.0f - energy_coeff);

  // Determine target envelope (gate open or closed)
  float target = (_energy > _threshold) ? 1.0f : 0.0f;

  // Smooth envelope with attack/release
  float coeff = (target > _envelope) ? _attack_coeff : _release_coeff;
  _envelope += coeff * (target - _envelope);

  // Apply envelope with max attenuation floor and output gain
  // envelope=1 -> gain=1, envelope=0 -> gain=_min_gain
  float gain = _min_gain + _envelope * (1.0f - _min_gain);
  return sample * gain * _output_gain;
}

void NoiseGate::computeBlock(float* samples, size_t count) {
  for (size_t i = 0; i < count; i++) {
    samples[i] = compute(samples[i]);
  }
}

} // namespace ork::audio::singularity
