////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/util/movie.inl>
#include <ork/lev2/aud/stream/audiodevice_stream.h>
#include <ork/kernel/string/deco.inl>
#include <ork/util/logger.h>
#include "_ffmpeg_enc.inl"

namespace ork::lev2 {

static logchannel_ptr_t logchan_moviecap =
    logger()->configureChannel("MOVIECAP", fvec3(1.0, 0.8, 0.3), true);

///////////////////////////////////////////////////////////////////////////////////////////////
// Constructor / Destructor
///////////////////////////////////////////////////////////////////////////////////////////////

MovieCaptureContext::MovieCaptureContext(moviecapsettings_ptr_t settings)
    : _settings(settings) {
}

MovieCaptureContext::~MovieCaptureContext() {
  terminate();
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Init: Setup video and audio streams, start encoding thread
///////////////////////////////////////////////////////////////////////////////////////////////

void MovieCaptureContext::init() {
  logchan_moviecap->log("MovieCaptureContext::init called with w=%d h=%d", _settings->_width, _settings->_height);

  logchan_moviecap->log("MovieCaptureContext::init _width=%d _height=%d fps=%d", _settings->_width, _settings->_height, _settings->_fps);

  // Start encoding thread
  _startEncodingThread();

  logchan_moviecap->log("MovieCaptureContext initialized successfully");
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Queue Management: Called from render thread
///////////////////////////////////////////////////////////////////////////////////////////////

size_t MovieCaptureContext::enqueueFrame(
    captureasync_ptr_t future,
    capturebuffer_ptr_t buffer,
    int frame_num,
    int expected_samples) {
  //return 0; // benchmark without the encoding..
  if (_terminated) {
    logchan_moviecap->log("enqueueFrame: TERMINATED, ignoring frame %d", frame_num);
    return 0;
  }

  // Wait if queue is full (backpressure)
  {
    std::unique_lock<std::mutex> lock(_queue_mutex);
    while (_frame_queue.size() >= _settings->_max_queue_size && _encoding_running) {
      if(1)logchan_moviecap->log("enqueueFrame: Queue full (%zu), waiting...", _frame_queue.size());
      _queue_cv.wait(lock);
    }
  }

  // Create frame data
  CapturedMovieFrame frame_data;
  frame_data.capture_future = future;
  frame_data.capture_buffer = buffer;
  frame_data.frame_number = frame_num;
  frame_data.expected_audio_samples = expected_samples;
  frame_data.virtual_time = (double)frame_num / double(_settings->_fps);

  // Add to queue
  size_t queue_size = 0;
  {
    std::lock_guard<std::mutex> lock(_queue_mutex);
    _frame_queue.push_back(frame_data);
    queue_size = _frame_queue.size();
  }

  _queue_cv.notify_one();  // Wake encoding thread

  return queue_size;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Encoding Thread Management
///////////////////////////////////////////////////////////////////////////////////////////////

void MovieCaptureContext::_startEncodingThread() {
  _encoding_running = true;
  _encoding_thread = std::thread([this]() { _encodingThreadFunc(); });
  logchan_moviecap->log("Encoding thread started");
}

void MovieCaptureContext::_stopEncodingThread() {
  logchan_moviecap->log("Stopping encoding thread...");

  _encoding_running = false;
  _queue_cv.notify_all();

  if (_encoding_thread.joinable()) {
    _encoding_thread.join();
  }

  logchan_moviecap->log("Encoding thread stopped");
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Main Encoding Thread Loop
///////////////////////////////////////////////////////////////////////////////////////////////

void MovieCaptureContext::_encodingThreadFunc() {
  ork::SetCurrentThreadName("movie-encode");

  auto encoder = ffmpeg_enc::createEncoder(_settings);
  if (!encoder) {
    logchan_moviecap->log("ERROR: Failed to create encoder");
    return;
  }

  // Query codec's desired audio frame size (e.g., 1024 for AAC)
  int codec_audio_frame_size = encoder->getAudioFrameSize();
  logchan_moviecap->log("Encoding thread running - codec audio frame size: %d", codec_audio_frame_size);
  ork::Timer timer;
  timer.Start();
  size_t total_audio_samples = 0;
  auto str_audio = std::dynamic_pointer_cast<StrAudioDevice>(_settings->_audiodevice);

  // Flush any samples that are in the audio buffer before the capture starts.
  auto _ = str_audio->extractSamples(str_audio->availableSamples());

  bool hold_until_empty = true;
  while (_encoding_running or hold_until_empty) {


    //=========================================
    // 2. Determine what can be encoded (symmetric, like mux.c)
    //=========================================
    bool can_audio = str_audio && (str_audio->availableSamples() >= codec_audio_frame_size);
    bool can_video = false;
    CapturedMovieFrame frame_data;

    {
      std::unique_lock<std::mutex> lock(_queue_mutex);
      hold_until_empty = not _frame_queue.empty();
      if (hold_until_empty) {
        frame_data = _frame_queue.front();
        can_video = frame_data.capture_future->isReady();
      }
    }

    // Calculate PTS for comparison
    double video_pts = can_video ? ((double)frame_data.frame_number / (double)_settings->_fps) : 999999.0;
    double audio_pts = (double)total_audio_samples / 48000.0;

    // Symmetric decision: encode whichever stream is behind
    bool should_video = can_video && (!can_audio || video_pts <= audio_pts);
    bool should_audio = can_audio && (!can_video || audio_pts < video_pts);

    //=========================================
    // 3. Encode video (if it should)
    //=========================================
    if (should_video) {
      std::unique_lock<std::mutex> lock(_queue_mutex);
      _frame_queue.pop_front();
      _queue_cv.notify_all();
      encoder->enqueueFrames(frame_data.capture_buffer, nullptr);
    } else if (should_audio) {
      //=========================================
      // 4. Extract and encode audio directly
      //=========================================
      auto extracted_audio_frames = str_audio->extractSamples(codec_audio_frame_size);
      OrkAssert(extracted_audio_frames->_num_samples == codec_audio_frame_size);

      // Add debug tone if enabled
      if(_settings->_audio_test_tone) {
        static float phaseL0 = 0.0f;
        static float phaseL1 = 0.0f;
        static float phaseR0 = 0.0f;
        static float phaseR1 = 0.0f;
        for( int i=0; i<codec_audio_frame_size; i++ ) {
          float frqL = sinf( phaseL1 * 6.2831853f * 1.0 ) * 220.0f + 220.0f;
          float sampL = sinf( phaseL0 * 6.2831853f ) * 0.1f;
          float frqR = sinf( phaseR1 * 6.2831853f * 1.1 ) * 220.0f + 220.0f;
          float sampR = sinf( phaseR0 * 6.2831853f ) * 0.1f;
          extracted_audio_frames->_left[i] += sampL;
          extracted_audio_frames->_right[i] += sampR;
          phaseL0 += frqL / 48000.0f;
          phaseL1 += 1.0f / 48000.0f;
          phaseR0 += frqR / 48000.0f;
          phaseR1 += 1.0f / 48000.0f;
        }
      }

      extracted_audio_frames->_timestamp = audio_pts;
      total_audio_samples += codec_audio_frame_size;

      encoder->enqueueFrames(nullptr, extracted_audio_frames);
    } else {
      // Neither stream ready - yield CPU to other threads (GPU capture, audio generation)
      usleep(100); // 0.1ms
    }

    ////////////////////////////////////////
    // encoding progress log
    ////////////////////////////////////////

    if(timer.SecsSinceStart()>4.0f){
      std::unique_lock<std::mutex> lock(_queue_mutex);
      size_t queue_size = _frame_queue.size();
      size_t frame_index = frame_data.frame_number;
      size_t num_frames_encoded = encoder->_num_frames_encoded;
      float video_elapsed_secs = float(frame_index) / float(_settings->_fps);
      float audio_elapsed_secs = float(total_audio_samples) / 48000.0f;
      logchan_moviecap->log("_encodingThreadFunc: enqueued<%zu> frameidx<%zu> numencoded<%zu> total_audio_samples<%zu> audio_elapsed<%g> video_elapsed<%g>", //
                            queue_size,  //
                            frame_index,  //
                            num_frames_encoded,
                            total_audio_samples,
                            audio_elapsed_secs,
                            video_elapsed_secs);
      timer.Start();
    }

  } // while (_encoding_running or (not _frame_queue.empty())) {

  encoder = nullptr;
  logchan_moviecap->log("Encoding thread exiting");
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Terminate: Stop thread, flush, cleanup
///////////////////////////////////////////////////////////////////////////////////////////////

void MovieCaptureContext::terminate() {
  while(not _frame_queue.empty()) {
    logchan_moviecap->log("MovieCaptureContext::terminate waiting for encoding to stop...");
    usleep(1<<20);
  }

  if (_terminated.exchange(true)) {
    return;  // Already terminated
  }

  logchan_moviecap->log("MovieCaptureContext::terminate");

  // Stop encoding thread (will flush queue first)
  _stopEncodingThread();

  logchan_moviecap->log("Wrote movie to file: %s", _settings->_filename.c_str());
}

} // namespace ork::lev2
