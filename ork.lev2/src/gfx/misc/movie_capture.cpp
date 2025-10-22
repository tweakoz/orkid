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

namespace ork::lev2 {

static logchannel_ptr_t logchan_moviecap =
    logger()->configureChannel("MOVIECAP", fvec3(1.0, 0.8, 0.3), true);

///////////////////////////////////////////////////////////////////////////////////////////////
// Constructor / Destructor
///////////////////////////////////////////////////////////////////////////////////////////////

MovieCaptureContext::MovieCaptureContext() {
  _filename = "GeneratedVideo.mp4";
}

MovieCaptureContext::~MovieCaptureContext() {
  terminate();
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Init: Setup video and audio streams, start encoding thread
///////////////////////////////////////////////////////////////////////////////////////////////

void MovieCaptureContext::init(int width, int height, audiodevice_ptr_t audio_dev) {
  logchan_moviecap->log("MovieCaptureContext::init called with w=%d h=%d", width, height);

  _width = width;
  _height = height;
  _audio_device = audio_dev;

  logchan_moviecap->log("MovieCaptureContext::init _width=%d _height=%d fps=%d", _width, _height, _fps);

  // ALL FFMPEG CODE REMOVED - keeping only queue/thread management

  // Start encoding thread
  _startEncodingThread();

  logchan_moviecap->log("MovieCaptureContext initialized (no encoding)");
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Video Stream Initialization - REMOVED
///////////////////////////////////////////////////////////////////////////////////////////////

void MovieCaptureContext::_initVideoStream() {
  // ALL FFMPEG CODE REMOVED
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Audio Stream Initialization - REMOVED
///////////////////////////////////////////////////////////////////////////////////////////////

void MovieCaptureContext::_initAudioStream() {
  // ALL FFMPEG CODE REMOVED
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Queue Management: Called from render thread
///////////////////////////////////////////////////////////////////////////////////////////////

void MovieCaptureContext::queueFrame(
    captureasync_ptr_t future,
    capturebuffer_ptr_t buffer,
    int frame_num,
    int expected_samples) {

  if (_terminated) {
    logchan_moviecap->log("queueFrame: TERMINATED, ignoring frame %d", frame_num);
    return;
  }

  logchan_moviecap->log("queueFrame: Queueing frame %d (expected_samples=%d)", frame_num, expected_samples);

  // Wait if queue is full (backpressure)
  {
    std::unique_lock<std::mutex> lock(_queue_mutex);
    while (_frame_queue.size() >= _max_queue_size && _encoding_running) {
      logchan_moviecap->log("queueFrame: Queue full (%zu), waiting...", _frame_queue.size());
      _queue_cv.wait(lock);
    }
  }

  // Create frame data
  CapturedMovieFrame frame_data;
  frame_data.capture_future = future;
  frame_data.capture_buffer = buffer;
  frame_data.frame_number = frame_num;
  frame_data.expected_audio_samples = expected_samples;
  frame_data.virtual_time = (double)frame_num / _fps;

  // Add to queue
  {
    std::lock_guard<std::mutex> lock(_queue_mutex);
    _frame_queue.push_back(frame_data);
    logchan_moviecap->log("queueFrame: Frame %d queued, queue size now: %zu", frame_num, _frame_queue.size());
  }

  _queue_cv.notify_one();  // Wake encoding thread
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

  logchan_moviecap->log("Encoding thread running (NO ENCODING - just consuming queue)");

  while (_encoding_running || !_frame_queue.empty()) {
    CapturedMovieFrame frame_data;

    //=========================================
    // Pop from queue (blocking)
    //=========================================
    {
      std::unique_lock<std::mutex> lock(_queue_mutex);

      _queue_cv.wait(lock, [this]{
        return !_frame_queue.empty() || !_encoding_running;
      });

      if (!_encoding_running && _frame_queue.empty()) {
        logchan_moviecap->log("_encodingThreadFunc: Thread stopping");
        break;
      }

      if (_frame_queue.empty()) {
        continue;
      }

      frame_data = _frame_queue.front();
      _frame_queue.pop_front();

      logchan_moviecap->log("_encodingThreadFunc: Dequeued frame %d, queue size now: %zu",
                            frame_data.frame_number, _frame_queue.size());

      _queue_cv.notify_all();
    }

    //=========================================
    // Wait for GPU capture completion
    //=========================================
    auto future = frame_data.capture_future;
    int timeout_ms = 5000;
    int waited_ms = 0;

    while (!future->_completed && waited_ms < timeout_ms) {
      usleep(1000);
      waited_ms++;
    }

    if (!future->_completed) {
      logchan_moviecap->log("ERROR: Frame %d capture timeout!", frame_data.frame_number);
      continue;
    }

    //=========================================
    // Extract audio samples (structure kept, not used)
    //=========================================
    if (_audio_device) {
      auto str_audio = std::dynamic_pointer_cast<StrAudioDevice>(_audio_device);
      if (str_audio) {
        int available = str_audio->availableSamples();
        int to_extract = std::min(available, frame_data.expected_audio_samples);

        if (to_extract > 0) {
          auto audio_capture = str_audio->extractSamples(to_extract);
          // Would encode here
        }
      }
    }

    //=========================================
    // Progress logging
    //=========================================
    if ((frame_data.frame_number % 60) == 0) {
      logchan_moviecap->log("Processed frame %d @ vtime %.2fs (no encoding)",
                            frame_data.frame_number,
                            frame_data.virtual_time);
    }
  }

  logchan_moviecap->log("Encoding thread exiting (no encoding occurred)");
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Write Video Frame - REMOVED
///////////////////////////////////////////////////////////////////////////////////////////////

void MovieCaptureContext::_writeVideoFrame(capturebuffer_ptr_t buffer) {
  // ALL FFMPEG ENCODING CODE REMOVED
  _frame++;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Write Audio Samples - REMOVED
///////////////////////////////////////////////////////////////////////////////////////////////

void MovieCaptureContext::_writeAudioSamples(
    const float* left,
    const float* right,
    int num_samples) {
  // ALL FFMPEG ENCODING CODE REMOVED
  _audio_samples_written += num_samples;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Flush Encoders - REMOVED
///////////////////////////////////////////////////////////////////////////////////////////////

void MovieCaptureContext::_flushEncoders() {
  // ALL FFMPEG CODE REMOVED
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Terminate: Stop thread, flush, cleanup
///////////////////////////////////////////////////////////////////////////////////////////////

void MovieCaptureContext::terminate() {
  if (_terminated.exchange(true)) {
    return;  // Already terminated
  }

  logchan_moviecap->log("MovieCaptureContext::terminate");

  // Stop encoding thread (will flush queue first)
  _stopEncodingThread();

  // ALL FFMPEG CLEANUP REMOVED

  logchan_moviecap->log("Processed %d frames (no encoding occurred)", _frame);
}

} // namespace ork::lev2
