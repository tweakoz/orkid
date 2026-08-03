////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <sndfile.h>
#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <chrono>
#include <atomic>
#include <deque>

#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/lev2/aud/singularity/audiotest.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/reflect/properties/registerX.inl>
#include "streaming_cimpl.inl"
#include "streaming_simpl.inl"

ImplementReflectionX(ork::audio::singularity::STREAMING_OSCILLATOR_DATA, "DspStreamingOscillator");

namespace ork::audio::singularity {

static logchannel_ptr_t logchan_streaming = logger()->configureChannel("PERF", fvec3(1, 0.6, .8), true);

void STREAMING_OSCILLATOR_DATA::describeX(class_t* clazz) {
  // Add configurable parameters
  clazz->directProperty("LowWatermark", &STREAMING_OSCILLATOR_DATA::_low_watermark);
  clazz->directProperty("HighWatermark", &STREAMING_OSCILLATOR_DATA::_high_watermark);
  clazz->directProperty("TargetLatencyMs", &STREAMING_OSCILLATOR_DATA::_target_latency_ms);
  clazz->directProperty("InterpolateDropouts", &STREAMING_OSCILLATOR_DATA::_interpolate_dropouts);
  clazz->directProperty("AdaptiveBuffering", &STREAMING_OSCILLATOR_DATA::_adaptive_buffering);
}

STREAMING_OSCILLATOR_DATA::STREAMING_OSCILLATOR_DATA(std::string name)
    : _low_watermark(24000)      // ~500ms at 48kHz
    , _high_watermark(36000)     // ~750ms at 48kHz
    , _target_latency_ms(750.0f) // Target 750ms latency for network streaming
    , _interpolate_dropouts(false)
    , _adaptive_buffering(true) {
}

dspblk_ptr_t STREAMING_OSCILLATOR_DATA::createInstance() const {
  auto instance = createDspInstance<StreamingOscillatorBlock>(this);
  return instance;
}

StreamingOscillatorBlock::StreamingOscillatorBlock(const DspBlockData* dbd)
    : DspBlock(dbd)
    , _ringBuffer(96000)   // 2 seconds at 48kHz (L channel / mono)
    , _ringBuffer_R(96000) // 2 seconds at 48kHz (R channel for stereo)
    , _dynamic_low_watermark(0)
    , _dynamic_high_watermark(0)
    , _dynamic_target_level(0)
    , _underrun_count(0)
    , _total_samples_processed(0)
    , _total_samples_dropped(0)
    , _last_sample(0.0f)
    , _fade_samples(128)
    , _is_priming(true)
    , _was_underrun(false)
    , _playback_rate(1.0f)
    , _chunks_received_total(0)
    , _last_chunk_time(0.0) {

  _streamingdata = dynamic_cast<const STREAMING_OSCILLATOR_DATA*>(dbd);
  _num_channels = _streamingdata->_num_channels;

  //==================================================================
  // INITIALIZE SIMPLE IMPL FOR DEBUGGING
  //==================================================================
  // auto sei_impl = _enhancement_impl.makeShared<StreamingEnhancementImpl>(this);
  auto sei_impl = _enhancement_impl.makeShared<SimpleImpl>(this);

  // Calculate dynamic watermarks based on sample rate when available
  float sample_rate = 48000.0f; // default
  if (synth::instance()) {
    sample_rate = synth::instance()->_sampleRate;
  }
  size_t samples_per_ms = size_t(sample_rate / 1000.0f);
  _dynamic_target_level = samples_per_ms * _streamingdata->_target_latency_ms;

  // Initialize timing
  _stats_timer  = std::chrono::steady_clock::now();
  _startup_time = _stats_timer;

  logchan_streaming->log(
      "STREAMING_OSC: Initialized with target latency %.1fms (%zu samples)",
      _streamingdata->_target_latency_ms,
      _dynamic_target_level);
  logchan_streaming->log(
      "STREAMING_OSC: Watermarks - Low: %zu, Target: %zu, High: %zu",
      _dynamic_low_watermark,
      _dynamic_target_level,
      _dynamic_high_watermark);
  logchan_streaming->log(
      "STREAMING_OSC: Ring buffer capacity: %zu samples (%.1f seconds)",
      _ringBuffer.capacity(),
      float(_ringBuffer.capacity()) / sample_rate);
}

void StreamingOscillatorBlock::compute(DspBuffer& dspbuf) {

  if (1) {
    auto sei = _enhancement_impl.getShared<SimpleImpl>();
    // auto sei = _enhancement_impl.getShared<ComplexImpl>();
    sei->processInput();
    sei->generateOutput(dspbuf);
  } else { // test tone
    size_t inumframes   = _layer->_dspwritecount;
    float* outputchan   = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
    static double phase = 0.0;
    for (size_t i = 0; i < inumframes; ++i) {
      outputchan[i] = sinf(phase) * 0.25f;
      phase += 0.01f; // Adjust frequency as needed
    }
  }
}

void StreamingOscillatorBlock::doKeyOn(const KeyOnInfo& koi) {
  // Reset state on key-on
  _is_priming              = true;
  _underrun_count          = 0;
  _last_sample             = 0.0f;
  _total_samples_processed = 0;
  _total_samples_dropped   = 0;
  _chunks_received_total   = 0;
  _playback_rate           = 1.0f;
  _chunk_intervals.clear();
  _last_chunk_time = 0.0;
  _startup_time    = std::chrono::steady_clock::now();

  // Clear ring buffers (L and R)
  float dummy;
  while (_ringBuffer.try_pop(dummy)) {
  }
  while (_ringBuffer_R.try_pop(dummy)) {
  }

  logchan_streaming->log("SimpleImpl: KeyOn - starting priming phase (channels=%d)", _num_channels);
}

void StreamingOscillatorBlock::doKeyOff() {
  // Could implement fade-out here if needed
  logchan_streaming->log("STREAMING_OSC: KeyOff - processed %zu samples total", _total_samples_processed.load());
}

prgdata_ptr_t createStreamingOscillatorProgramFromSource(lev2::audiostreaminginputchunk_source_ptr_t src, //
                                                         float target_latency_ms,
                                                         int num_channels) { //
  auto prgdata     = std::make_shared<ProgramData>();
  prgdata->_name   = "StreamingOscillatorProgram";
  auto layer       = prgdata->newLayer();
  layer->_panmode  = 4;
  layer->_floatPan = 0.0f; // center pan
  auto dspstage    = layer->appendStage("DSP");
  auto ampstage    = layer->appendStage("AMP");

  // Configure IO for mono or stereo
  if (num_channels == 2) {
    // Stereo: 2 inputs, 2 outputs
    dspstage->setNumIos(2, 2);
    ampstage->setNumIos(2, 2);
    dspstage->_ioconfig->_inputs  = {0, 1};
    dspstage->_ioconfig->_outputs = {0, 1};
    ampstage->_ioconfig->_inputs  = {0, 1};
    ampstage->_ioconfig->_outputs = {0, 1};
  } else {
    // Mono: 1 input panned to stereo output
    dspstage->setNumIos(1, 1);
    ampstage->setNumIos(1, 2);
    dspstage->_ioconfig->_inputs  = {0};
    dspstage->_ioconfig->_outputs = {0};
    ampstage->_ioconfig->_inputs  = {0};
    ampstage->_ioconfig->_outputs = {0, 1};
  }

  auto pchblock                 = dspstage->appendTypedBlock<PITCH>("Pitch");
  layer->_pchBlock              = pchblock;
  auto soscil                   = dspstage->appendTypedBlock<StreamingOscillatorBlock>("Oscil");
  soscil->_target_latency_ms    = target_latency_ms;
  soscil->_low_watermark        = size_t((target_latency_ms * 48.0f)); // 1/2 target at 48kHz
  soscil->_source               = src;
  soscil->_num_channels         = num_channels;  // Store channel count
  auto ampblock                 = ampstage->appendTypedBlock<AMP_ADAPTIVE>("amp");
  return prgdata;
}

} // namespace ork::audio::singularity