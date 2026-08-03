////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/aud/audiodevice.h>
#include "audiodevice_pa.h"
#include <ork/file/file.h>
#include <ork/util/endian.h>
#include <ork/kernel/orklut.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/Array.h>
#include <ork/kernel/Array.hpp>
#include <ork/kernel/string/string.h>
#include <ork/application/application.h>
#include <portaudio.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <sstream>
#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/lev2/aud/singularity/keyon_prof.h>
#include <ork/lev2/aud/singularity/spike_diag.h>
#include <ork/util/logger.h>
#include <set>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <chrono>
#include <ctime>
#include <cstdlib>
#if defined(__linux__)
#include <alsa/asoundlib.h>
#include <dlfcn.h>
#include <sys/resource.h>
#endif

#if defined(ENABLE_PORTAUDIO)

using namespace ork::audio::singularity;

template class ork::orklut<ork::Char8, float>;

namespace ork::lev2 {
static logchannel_ptr_t logchan_portaudio = logger()->configureChannel("audio.PA", fvec3(1, 0.6, .8), true);

///////////////////////////////////////////////////////////////////////////////

#if defined(__linux__)
static void _null_alsa_error_handler(const char* /*file*/, int /*line*/, const char* /*function*/, int /*err*/, const char* /*fmt*/, ...) {
}
static void _null_jack_msg_handler(const char* /*msg*/) {
}
#endif

static void _silence_host_audio_logging() {
  static std::once_flag _once;
  std::call_once(_once, []() {
#if defined(__linux__)
    // ALSA: install a null error handler so PCM probe failures don't hit stderr.
    snd_lib_error_set_handler(&_null_alsa_error_handler);

    // JACK: dlopen libjack and install null error/info callbacks so failed
    // server connections don't spam stderr. dlopen avoids adding a hard
    // link dependency on libjack.
    void* jack_handle = dlopen("libjack.so.0", RTLD_NOW | RTLD_NOLOAD);
    if (!jack_handle) {
      jack_handle = dlopen("libjack.so.0", RTLD_NOW | RTLD_GLOBAL);
    }
    if (jack_handle) {
      using jack_set_msg_fn = void (*)(void (*)(const char*));
      auto set_err  = reinterpret_cast<jack_set_msg_fn>(dlsym(jack_handle, "jack_set_error_function"));
      auto set_info = reinterpret_cast<jack_set_msg_fn>(dlsym(jack_handle, "jack_set_info_function"));
      if (set_err)  set_err(&_null_jack_msg_handler);
      if (set_info) set_info(&_null_jack_msg_handler);
    }
#endif
  });
}

///////////////////////////////////////////////////////////////////////////////
PaStream* pa_stream      = nullptr;
const bool ENABLE_OUTPUT = true; // allow disabling for long debug sessions
#if defined(__APPLE__)
const int DESIRED_NUMFRAMES = 128;
#else
const int DESIRED_NUMFRAMES = 256;
#endif
///////////////////////////////////////////////////////////////////////////////

struct PaImpl {
  PaDeviceIndex _input_override = -1;
  PaDeviceIndex _output_override = -1;
  PaStream* _stream = nullptr;
  int _actual_input_channels = 0;  // actual device channel count (may differ from requested)
};

///////////////////////////////////////////////////////////////////////////////

// Thread CPU time, paired with wall time in the diagnostics: wall >> cpu means
// the audio thread was DESCHEDULED (a scheduling problem), wall ~= cpu means
// the compute itself overran the deadline (a workload problem). Without this
// pairing a wall-clock-only overrun cannot tell the two apart.
static inline uint64_t _threadCpuNanos() {
  timespec ts{};
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
  return uint64_t(ts.tv_sec) * 1000000000ull + uint64_t(ts.tv_nsec);
}

// Minor page faults charged to THIS thread. the third possibility the wall/cpu
// pairing above cannot separate on its own: kernel memory work (fault-in of
// fresh anonymous pages, THP collapse) is billed as the thread's SYSTEM cpu
// time, so it reads as "compute" in both clocks. zero on platforms with no
// per-thread rusage.
static inline uint64_t _threadMinorFaults() {
#if defined(__linux__)
  rusage ru{};
  getrusage(RUSAGE_THREAD, &ru);
  return uint64_t(ru.ru_minflt);
#else
  return 0;
#endif
}

static int patestCallback(
    const void* inputBuffer,
    void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData) {

  auto padev = (AudioDevicePa*) userData;

  auto paimpl = padev->_impl.getShared<PaImpl>();
  auto the_synth = padev->_the_synth;
  auto aid = padev->_appinitdata.lock();

  auto& diagctrs = audioDiagCounters();

  ///////////////////////////////////////////////////////////////////////////
  // First invocation on this (host-api owned) thread: claim the realtime
  // scheduling band and publish the stream geometry. The band sits above the
  // singularity job-pool workers (40) which this thread joins on.
  ///////////////////////////////////////////////////////////////////////////
  static const bool _rt_claimed = [&]() -> bool {
    elevateAudioThread("orkid.audio.portaudio", 70);
    diagctrs._frames_per_buffer.store(uint32_t(framesPerBuffer), std::memory_order_relaxed);
    diagctrs._sample_rate.store(float(getSampleRate()), std::memory_order_relaxed);
    return true;
  }();
  (void) _rt_claimed;

  ///////////////////////////////////////////////////////////////////////////
  // xrun telemetry is ALWAYS counted (a relaxed atomic add) so any run can be
  // scored; ORKID_PA_DIAG=1 additionally prints compute-headroom windows from
  // the audio thread (diagnostic mode only — the printf perturbs timing).
  ///////////////////////////////////////////////////////////////////////////
  diagctrs._callbacks.fetch_add(1, std::memory_order_relaxed);
  if (statusFlags & paOutputUnderflow)
    diagctrs._underflows.fetch_add(1, std::memory_order_relaxed);

  static const bool diag_enabled = (getenv("ORKID_PA_DIAG") != nullptr);
  static uint64_t diag_frames_window = 0;
  static uint64_t diag_max_ns_window = 0;
  static uint64_t diag_cpu_at_max_ns = 0;
  static uint64_t diag_underflows_reported = 0;
  static uint64_t diag_faults_at_max = 0;
  static uint64_t diag_faults_window = 0;
  if (diag_enabled and (diag_frames_window == 0)) {
    printf("[PA_DIAG] framesPerBuffer<%lu> budget<%gus> SR<%g>\n",
           framesPerBuffer, 1e6 * double(framesPerBuffer) / getSampleRate(), getSampleRate());
  }
  auto diag_t0 = diag_enabled ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
  auto diag_cpu0 = diag_enabled ? _threadCpuNanos() : 0;
  auto diag_flt0 = diag_enabled ? _threadMinorFaults() : 0;

  // per-callback hardware-counter trace: the frequency/occupancy history any
  // spike inside this callback is read against (see spike_diag.h).
  using namespace ork::audio::singularity;
  const bool spike_enabled = spikeDiagEnabled();
  uint64_t spike_t0        = 0;
  RtPmu spike_pmu0;
  if (spike_enabled) {
    spikeDiagReadPmu(spike_pmu0);
    spike_t0 = spikeDiagNanos();
  }
  // the region-marked twin of the diag window above: same span, but whichever
  //  interior region ate the callback is named by the mark timeline. raise
  //  ORKID_SPIKE_US above the nominal callback cost or every callback reports.
  SpikeCallbackScope cbscope("pacallback");

  if(inputBuffer and padev->_input_handler){
    static auto chunk = std::make_shared<AudioInputChunk>(padev->_num_input_channels);
    chunk->_num_frames = framesPerBuffer;
    chunk->_chunk_index++;
    OrkAssert(padev->_num_input_channels >= 1);
    auto& chan0 = chunk->_channels[0];
    const int16_t* in = (const int16_t*)inputBuffer;
    chan0.resize(framesPerBuffer);

    constexpr float scale = 1.0f / 32768.0f;
    int actual_channels = paimpl->_actual_input_channels;

    if (actual_channels == 2 && padev->_num_input_channels == 1) {
      // Mix stereo to mono (int16 -> float)
      for (size_t i = 0; i < framesPerBuffer; i++) {
        float L = float(in[i * 2]) * scale;
        float R = float(in[i * 2 + 1]) * scale;
        chan0[i] = (L + R) * 0.5f;
      }
    } else if (actual_channels == 1) {
      // Mono input (int16 -> float)
      for (size_t i = 0; i < framesPerBuffer; i++) {
        chan0[i] = float(in[i]) * scale;
      }
    } else {
      // Fallback: just take first channel (int16 -> float)
      for (size_t i = 0; i < framesPerBuffer; i++) {
        chan0[i] = float(in[i * actual_channels]) * scale;
      }
    }
    padev->_input_handler(chunk.get());
  }

  if(the_synth and outputBuffer){
    auto out = (float*)outputBuffer;

    // convert inputBuffer to float
    static auto inputBufferFloat = new float[framesPerBuffer];
    const int16_t* inpbuf_int = nullptr;
    if(aid->_enable_audio_input and padev->_num_input_channels == 1){
      inpbuf_int = (const int16_t*)inputBuffer;
    }
    else{
      static std::vector<uint16_t> zeros(framesPerBuffer, 0);
      inpbuf_int = (const int16_t*)zeros.data(); // no input, fill with zeros
    }

    for (size_t i = 0; i < framesPerBuffer; i++) {
      int16_t j = inpbuf_int[i];
      // endian swap 
      ork::swapbytes(j);
      inputBufferFloat[i] = float(j) * (1.0f / 32768.0f); // convert to float
    }



    spikeDiagMark(); // mark 0: input conversion done
    the_synth->compute(framesPerBuffer, inputBufferFloat);
    spikeDiagMark(); // mark N-1: compute returned
    the_synth->_cpuload = Pa_GetStreamCpuLoad(pa_stream);

    if (diag_enabled) {
      auto diag_ns = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now() - diag_t0).count());
      auto diag_cpu_ns = _threadCpuNanos() - diag_cpu0;
      auto diag_flt    = _threadMinorFaults() - diag_flt0;
      diag_faults_window += diag_flt;
      if (diag_ns > diag_max_ns_window) {
        diag_max_ns_window = diag_ns;
        diag_cpu_at_max_ns = diag_cpu_ns;
        diag_faults_at_max = diag_flt;
      }
      diag_frames_window += framesPerBuffer;
      if (diag_frames_window >= uint64_t(getSampleRate() * 5.0)) {
        uint64_t uf = diagctrs._underflows.load(std::memory_order_relaxed);
        printf("[PA_DIAG] window_max_compute<%gus> cpu_at_max<%gus> faults_at_max<%lu> faults_window<%lu> budget<%gus> underflows_total<%lu>%s\n",
               double(diag_max_ns_window) * 1e-3,
               double(diag_cpu_at_max_ns) * 1e-3,
               diag_faults_at_max,
               diag_faults_window,
               1e6 * double(framesPerBuffer) / getSampleRate(),
               uf,
               (uf != diag_underflows_reported) ? " <<< NEW UNDERFLOWS" : "");
        keyonProfReport();
        diag_underflows_reported = uf;
        diag_frames_window = 0;
        diag_max_ns_window = 0;
        diag_cpu_at_max_ns = 0;
        diag_faults_at_max = 0;
        diag_faults_window = 0;
      }
    }

    if (spike_enabled) {
      uint64_t spike_t1 = spikeDiagNanos();
      RtPmu spike_pmu1;
      spikeDiagReadPmu(spike_pmu1);
      RtPmu d;
      d._insn = spike_pmu1._insn - spike_pmu0._insn;
      d._cyc  = spike_pmu1._cyc - spike_pmu0._cyc;
      d._ref   = spike_pmu1._ref - spike_pmu0._ref;
      d._kinsn = spike_pmu1._kinsn - spike_pmu0._kinsn;
      uint64_t spike_cpu = diag_enabled ? (_threadCpuNanos() - diag_cpu0) : 0;
      uint64_t spike_flt = diag_enabled ? (_threadMinorFaults() - diag_flt0) : 0;
      spikeDiagCallback(spike_t0, spike_t1 - spike_t0, spike_cpu, spike_flt, d, orkaud_getcpu());
    }

    if (false) { // test tone ?
      static int64_t _testtoneph = 0;
      for (size_t i = 0; i < framesPerBuffer; i++) {
        double phase = 440.0 * pi2 * double(_testtoneph) / getSampleRate();
        //printf( "phase<%g>\n", phase );
        float samp   = sinf(phase) * 1.0;
        *out++       = samp; // interleaved
        *out++       = samp; // interleaved
        _testtoneph++;
      }
    } else if (ENABLE_OUTPUT) {
      const auto& obuf = the_synth->_obuf;
      for (size_t i = 0; i < framesPerBuffer; i++) {
        *out++ = obuf._leftBuffer[i];  // interleaved
        *out++ = obuf._rightBuffer[i]; // interleaved
      }
    } else {
      for (size_t i = 0; i < framesPerBuffer; i++) {
        *out++ = 0.0f; // interleaved
        *out++ = 0.0f; // interleaved
      }

    }
  }
  else if(outputBuffer) { // no synth
    auto out = (float*)outputBuffer;
    for (size_t i = 0; i < framesPerBuffer; i++) {
      *out++ = 0.0f; // interleaved
      *out++ = 0.0f; // interleaved
    }
  }
  return 0;
}

 static void _startupAudio(AudioDevicePa* padev) {

  logchan_portaudio->log("starting audio");

  // Resolve short IDs (4-char hash) to full device names
  // Short IDs are 4 uppercase alphanumeric chars, e.g. "G6PQ"
  auto isShortId = [](const std::string& name) -> bool {
    if (name.length() != 4) return false;
    for (char c : name) {
      if (!std::isalnum(c)) return false;
    }
    return true;
  };

  auto aid         = padev->_appinitdata.lock();
  bool want_input  = aid->_enable_audio_input;
  bool want_output = aid->_enable_audio_output;

  // Resolve short ids for the ENABLED directions only. An unresolvable id for an
  // active direction is a loud, clean failure (AudioDeviceException) which the
  // caller degrades to the NULL device — never a bogus stream, never an assert.
  if (want_input and isShortId(padev->_inp_dev_name)) {
    auto dev = findAudioDeviceByShortId(padev->_inp_dev_name);
    if (not dev) {
      dev = retryAndDiagnoseShortId(padev->_inp_dev_name, /*want_output*/ false);
    }
    if (dev) {
      logchan_portaudio->log("resolved input short id '%s' to '%s' @ %gHz",
                             padev->_inp_dev_name.c_str(), dev->_name.c_str(), dev->_sample_rate);
      padev->_inp_dev_name = dev->_name;
    } else {
      logerrchannel()->log("unknown input short id '%s' - run ork.devicelist.audio.py to see available IDs",
                           padev->_inp_dev_name.c_str());
      throw AudioDeviceException(
          FormatString("unresolvable input audio device short id '%s'", padev->_inp_dev_name.c_str()));
    }
  }
  if (want_output and isShortId(padev->_out_dev_name)) {
    auto dev = findAudioDeviceByShortId(padev->_out_dev_name);
    if (not dev) {
      dev = retryAndDiagnoseShortId(padev->_out_dev_name, /*want_output*/ true);
    }
    if (dev) {
      logchan_portaudio->log("resolved output short id '%s' to '%s' @ %gHz",
                             padev->_out_dev_name.c_str(), dev->_name.c_str(), dev->_sample_rate);
      padev->_out_dev_name = dev->_name;
    } else {
      logerrchannel()->log("unknown output short id '%s' - run ork.devicelist.audio.py to see available IDs",
                           padev->_out_dev_name.c_str());
      throw AudioDeviceException(
          FormatString("unresolvable output audio device short id '%s'", padev->_out_dev_name.c_str()));
    }
  }

  float SR = getSampleRate();

  if(padev->_the_synth){
    padev->_the_synth->setSampleRate(SR);
    printf("SingularitySynth<%p> SR<%g>\n", (void*) padev->_the_synth.get(), SR);
    // loadPrograms();
  }

  auto paimpl = padev->_impl.makeShared<PaImpl>();
  _silence_host_audio_logging();
  auto err = Pa_Initialize();
  OrkAssert(err == paNoError);
  int num_inputs = 0;
  int num_outputs = 0;
  if( aid->_enable_audio_input ) {
    num_inputs = padev->_num_input_channels;
  }
  if( aid->_enable_audio_output ) {
    num_outputs = padev->_num_output_channels;
  }

  logchan_portaudio->log("desired input device name<%s>", padev->_inp_dev_name.c_str());
  logchan_portaudio->log("desired output device name<%s>", padev->_out_dev_name.c_str());
  logchan_portaudio->log("req num_inp<%zu> num_out<%zu>", num_inputs, num_outputs);

  size_t num_devices = Pa_GetDeviceCount();
  logchan_portaudio->log("num devices<%zu>", num_devices);

  bool input_default = padev->_inp_dev_name == "default";
  bool output_default = padev->_out_dev_name == "default";
  bool got_input = false;
  bool got_output = false;


  logchan_portaudio->log("input_default<%s> output_default<%s>",
                         input_default ? "true" : "false",
                         output_default ? "true" : "false");

  for(size_t c=0; c<num_devices; c++){
    auto devinfo = Pa_GetDeviceInfo(c);
    std::string devname = devinfo->name;
    size_t num_inp = devinfo->maxInputChannels;
    size_t num_out = devinfo->maxOutputChannels;
    logchan_portaudio->log("device<%zu> name<%s> num_inp<%zu> num_out<%zu>", c, devinfo->name, num_inp, num_out);


    if( (num_inputs>0) and (num_inp >= num_inputs) and paimpl->_input_override == -1 ){
      bool substr_matched = (devname.find(padev->_inp_dev_name)==0);
      if(substr_matched or input_default){
        logchan_portaudio->log("using device<%s> for input (device has %zu channels, requested %d)",
                               devname.c_str(), num_inp, num_inputs);
        paimpl->_input_override = c;
        paimpl->_actual_input_channels = num_inp;
        got_input = true;
      }
    }
    if( (num_outputs>0) and (num_out == num_outputs) and paimpl->_output_override == -1 ){
      bool substr_matched = (devname.find(padev->_out_dev_name)==0);
      if(substr_matched or output_default){
        logchan_portaudio->log("using device<%s> for output", devname.c_str());
        paimpl->_output_override = c;
        got_output = true;
      }
    }
  }

  PaStreamParameters inp_params, out_params;
  if(num_inputs>0){
    if(not got_input){
      logerrchannel()->log("could not open input device<%s>", padev->_inp_dev_name.c_str());
      Pa_Terminate();
      throw AudioDeviceException(
          FormatString("no usable input audio device matched '%s'", padev->_inp_dev_name.c_str()));
    }
    inp_params.device = paimpl->_input_override;
    inp_params.channelCount = paimpl->_actual_input_channels;  // use actual device channels
    inp_params.sampleFormat = paInt16;
    inp_params.suggestedLatency = Pa_GetDeviceInfo(inp_params.device)->defaultLowInputLatency;
    inp_params.hostApiSpecificStreamInfo = nullptr;
    logchan_portaudio->log("opening input with %d channels (will mix to %d)",
                           paimpl->_actual_input_channels, num_inputs);
  }

  if(num_outputs>0){
    if(not got_output){
      logerrchannel()->log("could not open output device<%s>", padev->_out_dev_name.c_str());
      Pa_Terminate();
      throw AudioDeviceException(
          FormatString("no usable output audio device matched '%s'", padev->_out_dev_name.c_str()));
    }
    out_params.device = paimpl->_output_override;
    out_params.channelCount = num_outputs;
    out_params.sampleFormat = paFloat32;
    out_params.suggestedLatency = Pa_GetDeviceInfo(out_params.device)->defaultLowOutputLatency;
    out_params.hostApiSpecificStreamInfo = nullptr;
  }

  if( (num_inputs>0) and (num_outputs>0) ){
    OrkAssert(got_input);
    err = Pa_OpenStream(
        &pa_stream,
        &inp_params,
        &out_params,
        SR,
        DESIRED_NUMFRAMES,
        paClipOff|paDitherOff,
        patestCallback,
        (void*) padev );
    OrkAssert(err == paNoError);
  }
  else if( (num_outputs>0) ){
    OrkAssert(got_output);
    err = Pa_OpenStream(
        &pa_stream,
        nullptr,
        &out_params,
        SR,
        DESIRED_NUMFRAMES,
        paClipOff,
        patestCallback,
        (void*) padev );
    OrkAssert(err == paNoError);
  }
  else if( (num_inputs>0) ){
    OrkAssert(got_input);
    err = Pa_OpenStream(
        &pa_stream,
        &inp_params,
        nullptr,
        SR,
        DESIRED_NUMFRAMES,
        paClipOff,
        patestCallback,
        (void*) padev );
    OrkAssert(err == paNoError);
  }
  else{
    OrkAssert(false);
  }

  // the synth learns its buffer geometry from the frame count it is handed,
  // and GROWING it walks all 512 pooled layers x 32 stage slots plus every
  // bus - ~18ms of allocation. left to the callback that is the first thing
  // the audio thread ever does, inside a 5.3ms budget. prime it here, off the
  // audio thread, with the same count Pa_OpenStream was asked for; a device
  // that hands back a larger buffer still grows on demand in the callback.
  if(padev->_the_synth){
    padev->_the_synth->resize(DESIRED_NUMFRAMES);
  }

  err = Pa_StartStream(pa_stream);
  OrkAssert(err == paNoError);

  logchan_portaudio->log("have default stream<%p>", pa_stream);

  if(padev->_the_synth){
    padev->_the_synth->resetFenables();
  }
}

///////////////////////////////////////////////////////////////////////////////

AudioDevicePa::AudioDevicePa(appinitdata_wkptr_t appinitd)
    : AudioDevice(appinitd) {
  
  
  /////////////////////////////////////

  _num_input_channels = 0;
  if(appinitd.lock()->_enable_audio_input){
    _num_input_channels = _appinitdata.lock()->_audio_input_numchannels;
  }
  _num_output_channels = _appinitdata.lock()->_audio_output_numchannels;

  _inp_dev_name = _appinitdata.lock()->_audio_input_devname;
  _out_dev_name = _appinitdata.lock()->_audio_output_devname;

}

///////////////////////////////////////////////////////////////////////////////

AudioDevicePa::~AudioDevicePa(){
}

///////////////////////////////////////////////////////////////////////////////

void AudioDevicePa::startup(){
  if(_appinitdata.lock()->_enable_audio_synth){
    _the_synth = synth::instance();
  }
  // A null synth is a legitimate config here — the PA callback feeds silence
  // (or input-only) when _the_synth is null, so guard rather than deref.
  if(_the_synth){
    _the_synth->waitUntilReady();
  }
  _startupAudio(this);

}

///////////////////////////////////////////////////////////////////////////////

void AudioDevicePa::shutdown(){
  logchan_portaudio->log("tearing down audio");
  auto err = Pa_StopStream(pa_stream);
  OrkAssert(err == paNoError);
  err = Pa_Terminate();
  OrkAssert(err == paNoError);
  if(_the_synth){
    synth::tearDown();
  }
}

///////////////////////////////////////////////////////////////////////////////

audiodeviceinfo_list_t enumerateAudioDevices_portaudio() {
  audiodeviceinfo_list_t result;
  _silence_host_audio_logging();
  Pa_Initialize();

  // Sample rates to probe
  static const double test_rates[] = {44100.0, 48000.0, 88200.0, 96000.0};
  static const int num_test_rates = sizeof(test_rates) / sizeof(test_rates[0]);

  int num_devices = Pa_GetDeviceCount();
  for (int i = 0; i < num_devices; i++) {
    auto pa_info = Pa_GetDeviceInfo(i);

    // Probe supported sample rates for input
    std::vector<double> supported_input_rates;
    if (pa_info->maxInputChannels > 0) {
      PaStreamParameters inp_params;
      inp_params.device = i;
      inp_params.channelCount = 1;
      inp_params.sampleFormat = paInt16;
      inp_params.suggestedLatency = pa_info->defaultLowInputLatency;
      inp_params.hostApiSpecificStreamInfo = nullptr;

      for (int r = 0; r < num_test_rates; r++) {
        if (Pa_IsFormatSupported(&inp_params, nullptr, test_rates[r]) == paFormatIsSupported) {
          supported_input_rates.push_back(test_rates[r]);
        }
      }
    }

    // Probe supported sample rates for output
    std::vector<double> supported_output_rates;
    if (pa_info->maxOutputChannels > 0) {
      PaStreamParameters out_params;
      out_params.device = i;
      out_params.channelCount = std::min(2, pa_info->maxOutputChannels);
      out_params.sampleFormat = paFloat32;
      out_params.suggestedLatency = pa_info->defaultLowOutputLatency;
      out_params.hostApiSpecificStreamInfo = nullptr;

      for (int r = 0; r < num_test_rates; r++) {
        if (Pa_IsFormatSupported(nullptr, &out_params, test_rates[r]) == paFormatIsSupported) {
          supported_output_rates.push_back(test_rates[r]);
        }
      }
    }

    // Create an entry for each supported sample rate
    std::set<double> all_rates;
    for (auto r : supported_input_rates) all_rates.insert(r);
    for (auto r : supported_output_rates) all_rates.insert(r);

    // If no rates probed successfully, use default
    if (all_rates.empty()) {
      all_rates.insert(pa_info->defaultSampleRate);
    }

    for (double rate : all_rates) {
      auto info = std::make_shared<AudioDeviceInfo>();
      info->_name = pa_info->name;
      info->_device_index = i;
      info->_sample_rate = rate;
      info->_supported_input_rates = supported_input_rates;
      info->_supported_output_rates = supported_output_rates;

      // Only set channels if this rate is supported for that direction
      bool rate_supported_input = std::find(supported_input_rates.begin(),
                                            supported_input_rates.end(), rate) != supported_input_rates.end();
      bool rate_supported_output = std::find(supported_output_rates.begin(),
                                             supported_output_rates.end(), rate) != supported_output_rates.end();

      if (rate_supported_input || supported_input_rates.empty()) {
        info->_max_input_channels = pa_info->maxInputChannels;
      }
      if (rate_supported_output || supported_output_rates.empty()) {
        info->_max_output_channels = pa_info->maxOutputChannels;
      }

      result.push_back(info);
    }
  }
  Pa_Terminate();
  return result;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
#endif 
