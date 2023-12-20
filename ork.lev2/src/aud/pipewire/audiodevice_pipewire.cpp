////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

////////////////////////////////////////////
#include <ork/pch.h>
#include <ork/lev2/config.h>
#if defined(ENABLE_PIPEWIRE)
////////////////////////////////////////////
#include <ork/application/application.h>
#include <ork/kernel/thread.h>
#include <ork/kernel/timer.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/util/multi_buffer.h>
////////////////////////////////////////////
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include "audiodevice_pipewire.h"
////////////////////////////////////////////

using namespace ork::audio::singularity;
namespace ork::lev2::pipewire {

///////////////////////////////////////////////////////
static constexpr int DESIRED_NUMFRAMES = 256;
static constexpr int KBUFFERCOUNT      = 4;
static constexpr int KNUMCHANNELS      = 2;
///////////////////////////////////////////////////////

struct PrivateImplementation {

  PrivateImplementation();
  ~PrivateImplementation();
  static void on_pw_process(void* userdata);
  
  template <typename T>
  void _addSpaParam(const T*p) { //
    auto pod = static_cast<const spa_pod*>(p);
    _spa_params.push_back(pod);
  }

  synth_ptr_t _synth;
  Thread _audioThread;
  Thread _synthThread;
  std::atomic<int> _execstate;
  pw_stream* _pipewire_stream       = nullptr;
  pw_main_loop* _pipewire_main_loop = nullptr;
  uint8_t _spa_buffer[DESIRED_NUMFRAMES * KNUMCHANNELS * sizeof(float)];
  std::vector<const spa_pod*> _spa_params;
  float _input_buffer[2048];
  ork::Timer _timer;
  double _timeAccum = 0.0;
  double _framecount = 0.0;

};

///////////////////////////////////////////////////////

void PrivateImplementation::on_pw_process(void* userdata) {

  auto _this = (PrivateImplementation*)userdata;

  double SR = getSampleRate();

  static size_t counter = 0;

  auto buf_obj = pw_stream_dequeue_buffer(_this->_pipewire_stream);
  if (nullptr == buf_obj)
    return;

  auto buffer = buf_obj->buffer;

  auto channel_data = buffer->datas[0];

  size_t stride     = sizeof(float) * KNUMCHANNELS;
  size_t num_frames = channel_data.maxsize / stride;

  channel_data.chunk->offset = 0;
  channel_data.chunk->stride = stride;
  channel_data.chunk->size   = num_frames * stride;

  // printf( "on_pw_process<%zu> num_frames<%zu> stride<%zu>\n", counter++, num_frames, stride );

  OrkAssert(channel_data.data);
  auto output_buffer = (float*)channel_data.data;

  if (false) { // test tone ?
    static int64_t _testtoneph = 0;
    for (int i = 0; i < num_frames; i++) {
      double phase = 440.0 * pi2 * double(_testtoneph) / SR;
      // printf( "phase<%g>\n", phase );
      float samp       = sinf(phase) * .06;
      *output_buffer++ = samp; // interleaved
      *output_buffer++ = samp; // interleaved
      _testtoneph++;
    }
  } else {
    static synth_ptr_t the_synth = synth::instance();

    double start_time = _this->_timer.SecsSinceStart();

    the_synth->compute(num_frames, _this->_input_buffer);

    const auto& obuf = the_synth->_obuf;
    float gain       = the_synth->_masterGain;
    for (size_t i = 0; i < num_frames; i++) {
      *output_buffer++ = obuf._leftBuffer[i]* gain;  // interleaved
      *output_buffer++ = obuf._rightBuffer[i]* gain; // interleaved
    }

    _this->_timeAccum += _this->_timer.SecsSinceStart() - start_time;
    _this->_framecount += double(num_frames);
    double FPS = _this->_framecount/_this->_timeAccum;
    double cpuload = SR / FPS;
    //printf( "_timeAccum<%g> _framecount<%g> FPS<%g> cpuload<%g>\n", _this->_timeAccum, _this->_framecount, FPS, cpuload );
    //double pct = (double(num_frames)/SR) / elapsed;
    if( _this->_timeAccum > 1.0 ){
      _this->_timeAccum = 0.0;
      _this->_framecount = 0.0;
    }
    else if( _this->_timeAccum > 0.25 ){
      the_synth->_cpuload = cpuload;
    }

  }
  

  pw_stream_queue_buffer(_this->_pipewire_stream, buf_obj);
}

///////////////////////////////////////////////////////

PrivateImplementation::PrivateImplementation() {

  _timer.Start();

  for( int i=0; i<2048; i++ ){
    _input_buffer[i] = 0.0f;
  }

  _execstate.store(0);
  float SR = getSampleRate();
  synth::bringUp();
  _synth = synth::instance();
  _synth->setSampleRate(SR);
  _synth->resetFenables();

  ///////////////////////////
  // pipewire setup
  ///////////////////////////

  _audioThread.start([=](anyp data) {
    static pw_stream_events _stream_events = {PW_VERSION_STREAM_EVENTS, .process = on_pw_process};

    static std::vector<char*> _fake_argv = {(char*)"OrkidPipeWireClient"};
    static char** fake_argv              = _fake_argv.data();
    static int fake_argc                 = _fake_argv.size();
    pw_init(&fake_argc, &fake_argv);

    auto pw_props = pw_properties_new(
        PW_KEY_MEDIA_TYPE,
        "Audio", //
        PW_KEY_MEDIA_CATEGORY,
        "Playback", //
        PW_KEY_MEDIA_ROLE,
        "music", //
        //PW_KEY_NODE_MAX_LATENCY,
        //"256/48000", //
        nullptr);

    _pipewire_main_loop = pw_main_loop_new(nullptr);

    _pipewire_stream = pw_stream_new_simple(        //
        pw_main_loop_get_loop(_pipewire_main_loop), //
        "audio-src",                                //
        pw_props,                                   //
        &_stream_events,                            //
        (void*)this);

    static spa_pod_builder _spa_builder = SPA_POD_BUILDER_INIT(_spa_buffer, sizeof(_spa_buffer));

    static auto ainfo = SPA_AUDIO_INFO_RAW_INIT(
            .format   = SPA_AUDIO_FORMAT_F32, //
            .channels = KNUMCHANNELS,         //
            .rate     = uint32_t(SR));

    _addSpaParam( spa_format_audio_raw_build(&_spa_builder, SPA_PARAM_EnumFormat, &ainfo) );

    _addSpaParam( spa_pod_builder_add_object(
        &_spa_builder,
        SPA_TYPE_OBJECT_ParamBuffers,
        SPA_PARAM_Buffers,
        SPA_PARAM_BUFFERS_size,
        SPA_POD_Int(sizeof(float) * KNUMCHANNELS * 256),
        SPA_PARAM_BUFFERS_stride,
        SPA_POD_Int(sizeof(float) * KNUMCHANNELS),
        SPA_PARAM_BUFFERS_blocks,
        SPA_POD_Int(1),
        SPA_PARAM_BUFFERS_blocks,
        SPA_POD_CHOICE_RANGE_Int(1, 1, 10)) );

    auto stream_flags = pw_stream_flags(
        PW_STREAM_FLAG_AUTOCONNECT | //
        PW_STREAM_FLAG_MAP_BUFFERS | //
        PW_STREAM_FLAG_RT_PROCESS);

    pw_stream_connect(
        _pipewire_stream,    //
        PW_DIRECTION_OUTPUT, //
        PW_ID_ANY,           //
        stream_flags,        //
        _spa_params.data(),
        _spa_params.size());

    pw_main_loop_run(_pipewire_main_loop);
    pw_stream_destroy(_pipewire_stream);
    pw_main_loop_destroy(_pipewire_main_loop);
    pw_deinit();
    _execstate++;
  });
}
///////////////////////////////////////////////////////////////////////////////

PrivateImplementation::~PrivateImplementation() {

  pw_main_loop_quit(_pipewire_main_loop);
  // on_process shall not be invoked after pw_deinit();
  _execstate.store(2);
  while (_execstate.load() == 2) {
    ork::usleep(1000);
  }
  synth::tearDown();
}

///////////////////////////////////////////////////////////////////////////////

AudioDevicePipeWire::AudioDevicePipeWire()
    : AudioDevice() {

  _impl.makeShared<PrivateImplementation>();
}

} // namespace ork::lev2::pipewire

#endif
