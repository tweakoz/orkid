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

  if (_terminated) {
    logchan_moviecap->log("enqueueFrame: TERMINATED, ignoring frame %d", frame_num);
    return 0;
  }

  // Wait if queue is full (backpressure)
  {
    std::unique_lock<std::mutex> lock(_queue_mutex);
    while (_frame_queue.size() >= _settings->_max_queue_size && _encoding_running) {
      if(0)logchan_moviecap->log("enqueueFrame: Queue full (%zu), waiting...", _frame_queue.size());
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

  while (_encoding_running or (not _frame_queue.empty())) {
    CapturedMovieFrame frame_data;

    //=========================================
    // Pop from queue
    //=========================================
    {
      std::unique_lock<std::mutex> lock(_queue_mutex);

      // If queue is empty, no-op this iteration
      if (_frame_queue.empty()) {
        //logchan_moviecap->log("_frame_queue.empty()");
        continue;
      }

      // Check audio availability BEFORE dequeuing
      if (str_audio) {
        int available_audio = str_audio->availableSamples();
        int expected_audio = _frame_queue.front().expected_audio_samples;

        // Wait if not enough audio samples available
        if (available_audio < expected_audio) {
          logchan_moviecap->log("Waiting for audio: have %d, need %d",
                                available_audio, expected_audio);
          continue;  // Try again on next iteration
        }
      }

      // We have enough audio, dequeue the frame
      frame_data = _frame_queue.front();

      if(frame_data.capture_future->isReady()==false){
        continue;
      }
      _frame_queue.pop_front();

      if(0)logchan_moviecap->log("_encodingThreadFunc: Dequeued frame %d, queue size now: %zu",
                            frame_data.frame_number, _frame_queue.size());

      _queue_cv.notify_all();
    }
    if(timer.SecsSinceStart()>1.0f){
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


    //=========================================
    // Wait for GPU capture completion
    //=========================================
    auto future = frame_data.capture_future;
    int timeout_ms = 10000;
    int waited_ms = 0;

    while (!future->isReady() && waited_ms < timeout_ms) {
      usleep(500);
      waited_ms++;
    }

    if (!future->isReady()) {
      logchan_moviecap->log("ERROR: Frame %d capture timeout!", frame_data.frame_number);
      continue;
    }

    //=========================================
    // Extract and accumulate audio samples
    //=========================================
    if (str_audio && frame_data.expected_audio_samples > 0) {
      auto extracted = str_audio->extractSamples(frame_data.expected_audio_samples);
      OrkAssert(extracted->_num_samples == frame_data.expected_audio_samples);

      // Add debug tone
      if(_settings->_audio_test_tone) {
        static float phaseL0 = 0.0f;
        static float phaseL1 = 0.0f;
        static float phaseR0 = 0.0f;
        static float phaseR1 = 0.0f;
        for( int i=0; i<frame_data.expected_audio_samples; i++ ) {
          float frqL = sinf( phaseL1 * 6.2831853f * 1.0 ) * 220.0f + 220.0f;
          float sampL = sinf( phaseL0 * 6.2831853f ) * 0.1f;
          float frqR = sinf( phaseR1 * 6.2831853f * 1.1 ) * 220.0f + 220.0f;
          float sampR = sinf( phaseR0 * 6.2831853f ) * 0.1f;
          extracted->_left[i] += sampL;
          extracted->_right[i] += sampR;
          phaseL0 += frqL / 48000.0f;
          phaseL1 += 1.0f / 48000.0f;
          phaseR0 += frqR / 48000.0f;
          phaseR1 += 1.0f / 48000.0f;
        }
      }

      // Accumulate into buffer
      _audio_buffer_left.insert(_audio_buffer_left.end(),
                                 extracted->_left.begin(),
                                 extracted->_left.end());
      _audio_buffer_right.insert(_audio_buffer_right.end(),
                                  extracted->_right.begin(),
                                  extracted->_right.end());
    }

    //=========================================
    // Prepare audio for encoding (if we have enough samples)
    //=========================================
    audioframecapture_ptr_t audio_for_encoder = nullptr;

    if (_audio_buffer_left.size() >= codec_audio_frame_size) {
      // We have enough samples - create audio capture with exactly codec frame size
      audio_for_encoder = std::make_shared<AudioFrameCapture>();
      audio_for_encoder->_num_samples = codec_audio_frame_size;
      audio_for_encoder->_sample_rate = 48000;
      audio_for_encoder->_timestamp = frame_data.virtual_time;
      total_audio_samples += codec_audio_frame_size;

      // Copy exactly codec_audio_frame_size samples
      audio_for_encoder->_left.assign(_audio_buffer_left.begin(),
                                       _audio_buffer_left.begin() + codec_audio_frame_size);
      audio_for_encoder->_right.assign(_audio_buffer_right.begin(),
                                        _audio_buffer_right.begin() + codec_audio_frame_size);

      // Remove consumed samples from buffer
      _audio_buffer_left.erase(_audio_buffer_left.begin(),
                                _audio_buffer_left.begin() + codec_audio_frame_size);
      _audio_buffer_right.erase(_audio_buffer_right.begin(),
                                 _audio_buffer_right.begin() + codec_audio_frame_size);

      if((frame_data.frame_number % 60) == 0) {
        logchan_moviecap->log("Encoding frame %d with audio (%d samples), buffer remaining: %zu expected<%zu>",
                              frame_data.frame_number, codec_audio_frame_size, _audio_buffer_left.size(), frame_data.expected_audio_samples);
      }
    } 

    //=========================================
    // Encode video + optional audio
    //=========================================
    encoder->enqueueFrames(frame_data.capture_buffer, audio_for_encoder);

  } // while (_encoding_running or (not _frame_queue.empty())) {

  // Flush any remaining audio in buffer
  if (_audio_buffer_left.size() > 0) {
    logchan_moviecap->log("WARNING: %zu audio samples remaining in buffer at end", _audio_buffer_left.size());
  }

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
