////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/filters.h>
#include <rnnoise.h>
#include <cstring>
#include <algorithm>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
// RNNoise Denoiser Implementation
///////////////////////////////////////////////////////////////////////////////

RNNoiseDenoise::RNNoiseDenoise() {
  _state = rnnoise_create(nullptr);  // use default model
  clear();
}

RNNoiseDenoise::~RNNoiseDenoise() {
  if (_state) {
    rnnoise_destroy(static_cast<DenoiseState*>(_state));
    _state = nullptr;
  }
}

void RNNoiseDenoise::clear() {
  _input_pos = 0;
  _output_pos = 0;
  _output_avail = 0;
  _vad_prob = 0.0f;
  std::memset(_input_buffer, 0, sizeof(_input_buffer));
  std::memset(_output_buffer, 0, sizeof(_output_buffer));
}

void RNNoiseDenoise::processFrame() {
  // RNNoise expects input scaled to [-32768, 32767] range
  float scaled_input[FRAME_SIZE];
  float scaled_output[FRAME_SIZE];

  for (size_t i = 0; i < FRAME_SIZE; i++) {
    scaled_input[i] = _input_buffer[i] * 32767.0f;
  }

  _vad_prob = rnnoise_process_frame(
    static_cast<DenoiseState*>(_state),
    scaled_output,
    scaled_input
  );

  // Scale output back to [-1, 1] range
  for (size_t i = 0; i < FRAME_SIZE; i++) {
    _output_buffer[i] = scaled_output[i] / 32767.0f;
  }

  _output_pos = 0;
  _output_avail = FRAME_SIZE;
}

float RNNoiseDenoise::compute(float input) {
  // Add sample to input buffer
  _input_buffer[_input_pos++] = input;

  // When input buffer is full, process frame
  if (_input_pos >= FRAME_SIZE) {
    processFrame();
    _input_pos = 0;
  }

  // Return output sample (or 0 if no output available yet - initial latency)
  if (_output_avail > 0) {
    float out = _output_buffer[_output_pos++];
    _output_avail--;
    return out;
  }

  return 0.0f;  // No output yet (initial latency period)
}

void RNNoiseDenoise::computeBlock(float* samples, size_t count) {
  for (size_t i = 0; i < count; i++) {
    samples[i] = compute(samples[i]);
  }
}

} // namespace ork::audio::singularity
