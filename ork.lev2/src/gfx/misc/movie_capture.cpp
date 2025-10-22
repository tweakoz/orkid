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
  _width = width;
  _height = height;
  _audio_device = audio_dev;

  logchan_moviecap->log("MovieCaptureContext::init w=%d h=%d fps=%d", _width, _height, _fps);

  // Initialize video stream
  _initVideoStream();

  // Initialize audio stream (if we have an audio device)
  if (_audio_device) {
    _initAudioStream();
  }

  // Open output file and write header
  int ret = avio_open(&_muxer->pb, _filename.c_str(), AVIO_FLAG_WRITE);
  if (ret < 0) {
    logchan_moviecap->log("ERROR: Failed to open output file: %s", _filename.c_str());
    return;
  }

  ret = avformat_write_header(_muxer, nullptr);
  if (ret < 0) {
    logchan_moviecap->log("ERROR: Failed to write format header");
    return;
  }

  av_dump_format(_muxer, 0, _filename.c_str(), 1);

  // Start encoding thread
  _startEncodingThread();

  logchan_moviecap->log("MovieCaptureContext initialized successfully");
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Video Stream Initialization
///////////////////////////////////////////////////////////////////////////////////////////////

void MovieCaptureContext::_initVideoStream() {
  logchan_moviecap->log("Initializing video stream...");

  // Setup scaler for RGB->YUV conversion
  _swscontext = sws_getContext(
      _width, _height, AV_PIX_FMT_RGB24,
      _width, _height, AV_PIX_FMT_YUV420P,
      SWS_FAST_BILINEAR, NULL, NULL, NULL);

  // Guess output format
  _format = (AVOutputFormat*)av_guess_format(nullptr, _filename.c_str(), nullptr);
  if (!_format) {
    logchan_moviecap->log("ERROR: Could not guess output format");
    return;
  }

  // Allocate muxer context
  avformat_alloc_output_context2(&_muxer, _format, nullptr, _filename.c_str());
  if (!_muxer) {
    logchan_moviecap->log("ERROR: Could not allocate output context");
    return;
  }

  // Find video codec
  _video_codec = avcodec_find_encoder(_format->video_codec);
  if (!_video_codec) {
    logchan_moviecap->log("ERROR: Video codec not found");
    return;
  }

  // Create video stream
  _video_stream = avformat_new_stream(_muxer, _video_codec);
  if (!_video_stream) {
    logchan_moviecap->log("ERROR: Could not create video stream");
    return;
  }

  _video_stream->time_base = AVRational{1, _fps};
  _video_stream->r_frame_rate = AVRational{_fps, 1};

  // Configure codec parameters
  auto codecpar = _video_stream->codecpar;
  codecpar->codec_id = _format->video_codec;
  codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
  codecpar->width = _width;
  codecpar->height = _height;
  codecpar->format = AV_PIX_FMT_YUV420P;
  codecpar->bit_rate = 8000000;  // 8 Mbps

  // Allocate encoder context
  _video_encoder = avcodec_alloc_context3(_video_codec);
  avcodec_parameters_to_context(_video_encoder, codecpar);

  _video_encoder->framerate = AVRational{_fps, 1};
  _video_encoder->time_base = AVRational{1, _fps};
  _video_encoder->gop_size = _fps;  // One I-frame per second
  _video_encoder->max_b_frames = 1;

  // Copy back to stream
  avcodec_parameters_from_context(_video_stream->codecpar, _video_encoder);

  // Open encoder
  int ret = avcodec_open2(_video_encoder, _video_codec, nullptr);
  if (ret < 0) {
    logchan_moviecap->log("ERROR: Could not open video encoder");
    return;
  }

  // Allocate RGB frame
  _rgb_pic = av_frame_alloc();
  _rgb_pic->format = AV_PIX_FMT_RGB24;
  _rgb_pic->width = _width;
  _rgb_pic->height = _height;
  av_frame_get_buffer(_rgb_pic, 1);

  // Allocate YUV frame
  _yuv_pic = av_frame_alloc();
  _yuv_pic->format = AV_PIX_FMT_YUV420P;
  _yuv_pic->width = _width;
  _yuv_pic->height = _height;
  av_frame_get_buffer(_yuv_pic, 1);

  logchan_moviecap->log("Video stream initialized");
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Audio Stream Initialization (PCM codec for simplicity)
///////////////////////////////////////////////////////////////////////////////////////////////

void MovieCaptureContext::_initAudioStream() {
  logchan_moviecap->log("Initializing audio stream...");

  // Use PCM float codec (simpler than AAC, no frame size constraints)
  _audio_codec = avcodec_find_encoder(AV_CODEC_ID_PCM_F32LE);
  if (!_audio_codec) {
    logchan_moviecap->log("ERROR: Audio codec not found");
    return;
  }

  // Create audio stream
  _audio_stream = avformat_new_stream(_muxer, _audio_codec);
  if (!_audio_stream) {
    logchan_moviecap->log("ERROR: Could not create audio stream");
    return;
  }

  _audio_stream->time_base = AVRational{1, _audio_sample_rate};

  // Allocate encoder context
  _audio_encoder = avcodec_alloc_context3(_audio_codec);
  _audio_encoder->sample_fmt = AV_SAMPLE_FMT_FLT;  // PCM uses FLT not FLTP
  _audio_encoder->sample_rate = _audio_sample_rate;

  // Use newer FFmpeg channel API
  av_channel_layout_default(&_audio_encoder->ch_layout, _audio_channels);

  _audio_encoder->time_base = AVRational{1, _audio_sample_rate};

  // Open encoder
  int ret = avcodec_open2(_audio_encoder, _audio_codec, nullptr);
  if (ret < 0) {
    logchan_moviecap->log("ERROR: Could not open audio encoder");
    return;
  }

  // Copy params to stream
  avcodec_parameters_from_context(_audio_stream->codecpar, _audio_encoder);

  logchan_moviecap->log("Audio stream initialized (PCM F32LE, %d Hz, %d ch)",
                        _audio_sample_rate, _audio_channels);
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

  logchan_moviecap->log("Encoding thread running");

  while (_encoding_running || !_frame_queue.empty()) {
    CapturedMovieFrame frame_data;

    //=========================================
    // Pop from queue (blocking)
    //=========================================
    {
      std::unique_lock<std::mutex> lock(_queue_mutex);

      logchan_moviecap->log("_encodingThreadFunc: Waiting for frame (queue_size=%zu, running=%d)...",
                            _frame_queue.size(), _encoding_running.load());

      _queue_cv.wait(lock, [this]{
        return !_frame_queue.empty() || !_encoding_running;
      });

      if (!_encoding_running && _frame_queue.empty()) {
        logchan_moviecap->log("_encodingThreadFunc: Thread stopping (not running, queue empty)");
        break;  // Exit thread
      }

      if (_frame_queue.empty()) {
        logchan_moviecap->log("_encodingThreadFunc: Spurious wakeup, continuing...");
        continue;  // Spurious wakeup
      }

      frame_data = _frame_queue.front();
      _frame_queue.pop_front();

      logchan_moviecap->log("_encodingThreadFunc: Dequeued frame %d, queue size now: %zu",
                            frame_data.frame_number, _frame_queue.size());

      _queue_cv.notify_all();  // Notify queueFrame if it was waiting
    }

    //=========================================
    // Wait for GPU capture completion
    //=========================================
    auto future = frame_data.capture_future;
    int timeout_ms = 5000;
    int waited_ms = 0;

    logchan_moviecap->log("_encodingThreadFunc: Waiting for GPU capture completion for frame %d...",
                          frame_data.frame_number);

    while (!future->_completed && waited_ms < timeout_ms) {
      usleep(1000);  // 1ms
      waited_ms++;
    }

    if (!future->_completed) {
      logchan_moviecap->log("ERROR: Frame %d capture timeout!", frame_data.frame_number);
      continue;
    }

    logchan_moviecap->log("_encodingThreadFunc: Frame %d capture complete (waited %d ms)",
                          frame_data.frame_number, waited_ms);

    //=========================================
    // Extract audio samples from audio device
    //=========================================
    if (_audio_device && _audio_stream) {
      auto str_audio = std::dynamic_pointer_cast<StrAudioDevice>(_audio_device);
      if (str_audio) {
        int available = str_audio->availableSamples();
        int to_extract = std::min(available, frame_data.expected_audio_samples);

        logchan_moviecap->log("_encodingThreadFunc: Frame %d - audio available=%d, expected=%d, extracting=%d",
                              frame_data.frame_number, available,
                              frame_data.expected_audio_samples, to_extract);

        if (to_extract > 0) {
          auto audio_capture = str_audio->extractSamples(to_extract);

          if (audio_capture && audio_capture->_num_samples > 0) {
            logchan_moviecap->log("_encodingThreadFunc: Frame %d - writing %d audio samples",
                                  frame_data.frame_number, audio_capture->_num_samples);
            _writeAudioSamples(
              audio_capture->_left.data(),
              audio_capture->_right.data(),
              audio_capture->_num_samples
            );
          }
        }
      }
    }

    //=========================================
    // Encode video frame
    //=========================================
    logchan_moviecap->log("_encodingThreadFunc: Frame %d - encoding video...", frame_data.frame_number);
    _writeVideoFrame(frame_data.capture_buffer);
    logchan_moviecap->log("_encodingThreadFunc: Frame %d - video encoded", frame_data.frame_number);

    //=========================================
    // Progress logging
    //=========================================
    if ((frame_data.frame_number % 60) == 0) {
      logchan_moviecap->log("Encoded frame %d @ vtime %.2fs",
                            frame_data.frame_number,
                            frame_data.virtual_time);
    }
  }

  //=========================================
  // Flush encoders on thread exit
  //=========================================
  _flushEncoders();

  logchan_moviecap->log("Encoding thread exiting");
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Write Video Frame
///////////////////////////////////////////////////////////////////////////////////////////////

void MovieCaptureContext::_writeVideoFrame(capturebuffer_ptr_t buffer) {
  auto img = buffer->_image;

  if (!img) {
    logchan_moviecap->log("ERROR: No captured image in buffer!");
    return;
  }

  OrkAssert(img->_format == EBufferFormat::RGB8);
  OrkAssert(img->_width == _width);
  OrkAssert(img->_height == _height);

  auto src_pixels = (const uint8_t*)img->_data->data();
  auto dest_buffer = _rgb_pic->data[0];
  auto dest_linesize = _rgb_pic->linesize[0];

  //=========================================
  // Copy RGB to AVFrame (flip Y)
  //=========================================
  for (int y = 0; y < _height; y++) {
    size_t src_row_base = ((_height - 1) - y) * _width * 3;
    for (int x = 0; x < _width; x++) {
      size_t src_pix_base = src_row_base + (x * 3);

      dest_buffer[y * dest_linesize + 3 * x + 0] = src_pixels[src_pix_base + 2]; // B
      dest_buffer[y * dest_linesize + 3 * x + 1] = src_pixels[src_pix_base + 1]; // G
      dest_buffer[y * dest_linesize + 3 * x + 2] = src_pixels[src_pix_base + 0]; // R
    }
  }

  //=========================================
  // RGB → YUV conversion
  //=========================================
  sws_scale(_swscontext,
            _rgb_pic->data, _rgb_pic->linesize,
            0, _height,
            _yuv_pic->data, _yuv_pic->linesize);

  //=========================================
  // Set PTS and encode
  //=========================================
  _yuv_pic->pts = _frame;

  int ret = avcodec_send_frame(_video_encoder, _yuv_pic);
  if (ret < 0) {
    logchan_moviecap->log("ERROR: Error sending video frame to encoder");
    return;
  }

  //=========================================
  // Receive and write packets
  //=========================================
  AVPacket pkt;
  av_init_packet(&pkt);
  pkt.data = nullptr;
  pkt.size = 0;

  while (avcodec_receive_packet(_video_encoder, &pkt) == 0) {
    pkt.stream_index = _video_stream->index;
    pkt.pts = av_rescale_q(pkt.pts, _video_encoder->time_base, _video_stream->time_base);
    pkt.dts = av_rescale_q(pkt.dts, _video_encoder->time_base, _video_stream->time_base);
    pkt.duration = av_rescale_q(pkt.duration, _video_encoder->time_base, _video_stream->time_base);

    av_interleaved_write_frame(_muxer, &pkt);
    av_packet_unref(&pkt);
  }

  _frame++;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Write Audio Samples
///////////////////////////////////////////////////////////////////////////////////////////////

void MovieCaptureContext::_writeAudioSamples(
    const float* left,
    const float* right,
    int num_samples) {

  if (_terminated || num_samples == 0) return;

  //=========================================
  // Allocate AVFrame
  //=========================================
  AVFrame* frame = av_frame_alloc();
  frame->format = _audio_encoder->sample_fmt;
  av_channel_layout_copy(&frame->ch_layout, &_audio_encoder->ch_layout);
  frame->sample_rate = _audio_encoder->sample_rate;
  frame->nb_samples = num_samples;

  av_frame_get_buffer(frame, 0);

  //=========================================
  // Interleave samples (PCM FLT is interleaved LRLRLR...)
  //=========================================
  float* dest = (float*)frame->data[0];
  for (int i = 0; i < num_samples; i++) {
    dest[i * 2 + 0] = left[i];
    dest[i * 2 + 1] = right[i];
  }

  //=========================================
  // Set PTS
  //=========================================
  frame->pts = _audio_samples_written;
  _audio_samples_written += num_samples;

  //=========================================
  // Encode and write
  //=========================================
  int ret = avcodec_send_frame(_audio_encoder, frame);
  if (ret < 0) {
    logchan_moviecap->log("ERROR: Error sending audio frame to encoder");
    av_frame_free(&frame);
    return;
  }

  //=========================================
  // Receive and write packets
  //=========================================
  AVPacket pkt;
  av_init_packet(&pkt);
  pkt.data = nullptr;
  pkt.size = 0;

  while (avcodec_receive_packet(_audio_encoder, &pkt) == 0) {
    pkt.stream_index = _audio_stream->index;
    pkt.pts = av_rescale_q(pkt.pts, _audio_encoder->time_base, _audio_stream->time_base);
    pkt.dts = av_rescale_q(pkt.dts, _audio_encoder->time_base, _audio_stream->time_base);
    pkt.duration = av_rescale_q(pkt.duration, _audio_encoder->time_base, _audio_stream->time_base);

    av_interleaved_write_frame(_muxer, &pkt);
    av_packet_unref(&pkt);
  }

  av_frame_free(&frame);
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Flush Encoders (send nullptr to get remaining packets)
///////////////////////////////////////////////////////////////////////////////////////////////

void MovieCaptureContext::_flushEncoders() {
  logchan_moviecap->log("Flushing encoders...");

  // Flush video encoder
  avcodec_send_frame(_video_encoder, nullptr);

  AVPacket pkt;
  av_init_packet(&pkt);

  while (avcodec_receive_packet(_video_encoder, &pkt) == 0) {
    pkt.stream_index = _video_stream->index;
    av_interleaved_write_frame(_muxer, &pkt);
    av_packet_unref(&pkt);
  }

  // Flush audio encoder (if exists)
  if (_audio_encoder) {
    avcodec_send_frame(_audio_encoder, nullptr);

    while (avcodec_receive_packet(_audio_encoder, &pkt) == 0) {
      pkt.stream_index = _audio_stream->index;
      av_interleaved_write_frame(_muxer, &pkt);
      av_packet_unref(&pkt);
    }
  }

  logchan_moviecap->log("Encoders flushed");
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

  // Write file trailer
  if (_muxer) {
    av_write_trailer(_muxer);
  }

  // Close file
  if (_muxer && !(_format->flags & AVFMT_NOFILE)) {
    avio_closep(&_muxer->pb);
  }

  // Cleanup
  if (_swscontext) {
    sws_freeContext(_swscontext);
    _swscontext = nullptr;
  }

  if (_rgb_pic) {
    av_frame_free(&_rgb_pic);
  }

  if (_yuv_pic) {
    av_frame_free(&_yuv_pic);
  }

  if (_audio_frame) {
    av_frame_free(&_audio_frame);
  }

  if (_video_encoder) {
    avcodec_free_context(&_video_encoder);
  }

  if (_audio_encoder) {
    avcodec_free_context(&_audio_encoder);
  }

  if (_muxer) {
    avformat_free_context(_muxer);
    _muxer = nullptr;
  }

  logchan_moviecap->log("Wrote %d frames to %s", _frame, _filename.c_str());
  logchan_moviecap->log("Movie generation complete");
}

} // namespace ork::lev2
