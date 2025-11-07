#pragma once

#include <ork/kernel/opq.h>

extern "C" {
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

namespace ffmpeg_enc {
constexpr AVPixelFormat STREAM_PIX_FMT = AV_PIX_FMT_YUV420P; /* default pix_fmt */

constexpr uint64_t SCALE_FLAGS = SWS_BICUBIC;

///////////////////////////////////////////////////////////////////////////////////////////////
// a wrapper around a single output AVStream
///////////////////////////////////////////////////////////////////////////////////////////////

struct OutputStream {
  AVStream* st = nullptr;
  AVCodecContext* enc = nullptr;

  /* pts of the next frame that will be generated */
  int64_t next_pts = 0;
  int samples_count = 0;

  AVFrame* frame = nullptr;
  AVFrame* tmp_frame = nullptr;

  AVPacket* tmp_pkt = nullptr;

  SwsContext* sws_ctx = nullptr;
  SwrContext* swr_ctx = nullptr;

};

using outputstream_ptr_t = std::shared_ptr<OutputStream>;

///////////////////////////////////////////////////////////////////////////////////////////////

static AVFrame* _allocAudioFrame(enum AVSampleFormat sample_fmt, const AVChannelLayout* channel_layout, int sample_rate, int nb_samples) {
  AVFrame* frame = av_frame_alloc();
  if (!frame) {
    fprintf(stderr, "Error allocating an audio frame\n");
    exit(1);
  }

  frame->format = sample_fmt;
  av_channel_layout_copy(&frame->ch_layout, channel_layout);
  frame->sample_rate = sample_rate;
  frame->nb_samples  = nb_samples;

  if (nb_samples) {
    if (av_frame_get_buffer(frame, 0) < 0) {
      fprintf(stderr, "Error allocating an audio buffer\n");
      exit(1);
    }
  }

  return frame;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// alloc video frame
///////////////////////////////////////////////////////////////////////////////////////////////

static AVFrame* _allocVideoFrame(AVPixelFormat pix_fmt, //
                                 int width,             //
                                 int height) {          //

  AVFrame* frame = av_frame_alloc();

  if (!frame)
    return nullptr;

  frame->format = pix_fmt;
  frame->width  = width;
  frame->height = height;
                           
  /* allocate the buffers for the frame data */
  int ret = av_frame_get_buffer(frame, 0);
  if (ret < 0) {
    fprintf(stderr, "Could not allocate frame data.\n");
    exit(1);
  }

  return frame;
}

///////////////////////////////////////////////////////////////////////////////////////////////

struct Encoder {

  Encoder(ork::lev2::moviecapsettings_ptr_t settings);
  ~Encoder();

  void enqueueFrames(ork::lev2::capturebuffer_ptr_t video_buffer,
                     ork::lev2::audioframecapture_ptr_t audio_capture);

  int getAudioFrameSize() const {
    if (_audio_stream && _audio_stream->enc) {
      return _audio_stream->enc->frame_size;
    }
    return 0;
  }

  // Returns true if audio is behind the target video frame time
  // target_video_time: the timestamp (in seconds) of the video frame we're about to encode
  bool needsMoreAudio(double target_video_time) const {
    if (!_enable_video || !_enable_audio) {
      return false;
    }

    // Calculate audio's current timestamp in seconds
    double audio_time = (double)_audio_stream->next_pts / (double)_audio_stream->enc->sample_rate;

    // Encode audio if it's behind the target video frame time
    return audio_time < target_video_time;
  }

  void _closeStream(OutputStream* ost);

  void _add_stream( OutputStream* ost,         //
                    const AVCodec** codec,     //
                    enum AVCodecID codec_id);

  void _openVideo(AVDictionary* opt_arg);
  void _openAudio(AVDictionary* opt_arg);

  AVFrame* _getVideoFrame(ork::lev2::capturebuffer_ptr_t external_video);
  int _writeFrame(AVCodecContext* c, AVStream* st, AVFrame* frame, AVPacket* pkt);
  int _writeVideoFrame(ork::lev2::capturebuffer_ptr_t external_video);
  int _writeAudioFrame(ork::lev2::audioframecapture_ptr_t external_audio);
  AVFrame* _getAudioFrame( ork::lev2::audioframecapture_ptr_t external_audio);

  ork::lev2::moviecapsettings_ptr_t _settings;
  outputstream_ptr_t _video_stream = nullptr;
  outputstream_ptr_t _audio_stream = nullptr;
  const AVOutputFormat* fmt  = nullptr;
  AVFormatContext* oc        = nullptr;
  const AVCodec* audio_codec = nullptr;
  const AVCodec* video_codec = nullptr;
  bool _valid                = false;
  bool _enable_video = 0;
  bool _enable_audio = 0;
  size_t _num_frames_encoded = 0;
};
using encoder_ptr_t = std::shared_ptr<Encoder>;

///////////////////////////////////////////////////////////////////////////////////////////////

Encoder::Encoder(ork::lev2::moviecapsettings_ptr_t settings)
  : _settings(settings) {

  _video_stream = std::make_shared<OutputStream>();
  _audio_stream = std::make_shared<OutputStream>();
  AVDictionary* opt = NULL;
  int i;

  // av_dict_set(&opt, argv[i]+1, argv[i+1], 0);

  /* allocate the output media context */
  avformat_alloc_output_context2(&oc, NULL, NULL, _settings->_filename.c_str());
  if (!oc) {
    printf("Could not deduce output format from file extension: using MPEG.\n");
    avformat_alloc_output_context2(&oc, NULL, "mpeg", _settings->_filename.c_str());
  }
  if (!oc)
    return;

  fmt = oc->oformat;

  /* Add the audio and video streams using the default format codecs
   * and initialize the codecs. */
  if (fmt->video_codec != AV_CODEC_ID_NONE) {
    // Force H.264 codec for better quality and compatibility (instead of MPEG4)
    _add_stream(_video_stream.get(), &video_codec, AV_CODEC_ID_H264);
    _enable_video = true;
  }
  if (fmt->audio_codec != AV_CODEC_ID_NONE) {
    _add_stream(_audio_stream.get(), &audio_codec, fmt->audio_codec);
    _enable_audio = true;
  }

  /* Now that all the parameters are set, we can open the audio and
   * video codecs and allocate the necessary encode buffers. */
  if (_enable_video)
    _openVideo(opt);

  if (_enable_audio)
    _openAudio(opt);

  av_dump_format(oc, 0, _settings->_filename.c_str(), 1);

  /* open the output file, if needed */
  if (!(fmt->flags & AVFMT_NOFILE)) {
    int chk = avio_open(&oc->pb, _settings->_filename.c_str(), AVIO_FLAG_WRITE);
    if (chk < 0) {
      fprintf(stderr, "Could not open '%s': %s\n", _settings->_filename.c_str(), av_err2str(chk));
      return;
    }
  }

  /* Write the stream header, if any. */
  int chk = avformat_write_header(oc, &opt);
  if (chk < 0) {
    fprintf(stderr, "Error occurred when opening output file: %s\n", av_err2str(chk));
    return;
  }
  _valid = true;
}

///////////////////////////////////////////////////////////////////////////////////////////////

Encoder::~Encoder() {
  // Cleanup handled in encode() function
  if( nullptr == oc )
    return;

  av_write_trailer(oc);

  /* Close each codec. */
  if (_enable_video)
    _closeStream(_video_stream.get());
  if (_enable_audio)
    _closeStream(_audio_stream.get());
  
  if (!(fmt->flags & AVFMT_NOFILE))
    /* Close the output file. */
    avio_closep(&oc->pb);

  /* free the stream */
  avformat_free_context(oc);
  _video_stream = nullptr;
  _audio_stream = nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////

void Encoder::_closeStream(OutputStream* ost) {
  avcodec_free_context(&ost->enc);
  av_frame_free(&ost->frame);
  av_frame_free(&ost->tmp_frame);
  av_packet_free(&ost->tmp_pkt);
  sws_freeContext(ost->sws_ctx);
  swr_free(&ost->swr_ctx);
}

///////////////////////////////////////////////////////////////////////////////////////////////

void Encoder::_openVideo(AVDictionary* opt_arg) {
  int ret;

  AVCodecContext* c = _video_stream->enc;
  AVDictionary* opt = NULL;

  av_dict_copy(&opt, opt_arg, 0);

  /* open the codec */
  ret = avcodec_open2(c, video_codec, &opt);
  av_dict_free(&opt);

#ifdef __linux__
  // On Linux, fallback to mpeg4 if h264 fails (e.g., no hardware encoder in headless mode)
  if (ret < 0 && c->codec_id == AV_CODEC_ID_H264) {
    fprintf(stderr, "Could not open H.264 video codec: %s\n", av_err2str(ret));
    fprintf(stderr, "Attempting fallback to MPEG4 codec for Linux...\n");

    // Find mpeg4 codec
    const AVCodec* mpeg4_codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if (!mpeg4_codec) {
      fprintf(stderr, "FATAL: MPEG4 codec not found\n");
      abort();
    }

    // Free the h264 codec context and allocate a fresh one for mpeg4
    // (h264-specific options are incompatible with mpeg4)
    int width = c->width;
    int height = c->height;
    AVRational time_base = c->time_base;
    int64_t bit_rate = c->bit_rate;
    int gop_size = c->gop_size;
    AVPixelFormat pix_fmt = c->pix_fmt;
    AVColorRange color_range = c->color_range;
    int flags = c->flags;

    avcodec_free_context(&c);
    c = avcodec_alloc_context3(mpeg4_codec);
    if (!c) {
      fprintf(stderr, "FATAL: Could not allocate MPEG4 codec context\n");
      abort();
    }

    // Restore basic settings
    c->codec_id = AV_CODEC_ID_MPEG4;
    c->width = width;
    c->height = height;
    c->time_base = time_base;
    // MPEG4 is ~2x less efficient than H.264, so double the bitrate to maintain quality
    c->bit_rate = bit_rate * 2;
    c->gop_size = gop_size;
    c->pix_fmt = pix_fmt;
    c->color_range = color_range;
    c->flags = flags;

    // Update encoder and codec
    _video_stream->enc = c;
    video_codec = mpeg4_codec;

    // Try opening with mpeg4
    av_dict_copy(&opt, opt_arg, 0);
    ret = avcodec_open2(c, video_codec, &opt);
    av_dict_free(&opt);

    if (ret >= 0) {
      fprintf(stderr, "Successfully using MPEG4 codec\n");
    }
  }
#endif

  if (ret < 0) {
    fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
    fprintf(stderr, "FATAL: Movie encoding initialization failed - aborting\n");
    abort();
  }

  /* allocate and init a re-usable frame */
  _video_stream->frame = _allocVideoFrame(c->pix_fmt, c->width, c->height);
  if (!_video_stream->frame) {
    fprintf(stderr, "Could not allocate video frame\n");
    exit(1);
  }

  /* If the output format is not YUV420P, then a temporary YUV420P
   * picture is needed too. It is then converted to the required
   * output format. */
  _video_stream->tmp_frame = NULL;
  if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
    _video_stream->tmp_frame = _allocVideoFrame(AV_PIX_FMT_YUV420P, c->width, c->height);
    if (!_video_stream->tmp_frame) {
      fprintf(stderr, "Could not allocate temporary video frame\n");
      exit(1);
    }
  }

  /* copy the stream parameters to the muxer */
  ret = avcodec_parameters_from_context(_video_stream->st->codecpar, c);
  if (ret < 0) {
    fprintf(stderr, "Could not copy the stream parameters\n");
    exit(1);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////

void Encoder::_openAudio(AVDictionary* opt_arg) {
  AVCodecContext* c;
  int nb_samples;
  int ret;
  AVDictionary* opt = NULL;

  c = _audio_stream->enc;

  /* open it */
  av_dict_copy(&opt, opt_arg, 0);
  ret = avcodec_open2(c, audio_codec, &opt);
  av_dict_free(&opt);
  if (ret < 0) {
    fprintf(stderr, "Could not open audio codec: %s\n", av_err2str(ret));
    exit(1);
  }

  if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
    nb_samples = 10000;
  else
    nb_samples = c->frame_size;

  // Allocate frame in codec's native format (FLTP for AAC)
  _audio_stream->frame = _allocAudioFrame(c->sample_fmt, &c->ch_layout, c->sample_rate, nb_samples);

  /* copy the stream parameters to the muxer */
  ret = avcodec_parameters_from_context(_audio_stream->st->codecpar, c);
  if (ret < 0) {
    fprintf(stderr, "Could not copy the stream parameters\n");
    exit(1);
  }

  // No resampler needed - direct float to planar float conversion
}

///////////////////////////////////////////////////////////////////////////////////////////////

void Encoder::enqueueFrames(ork::lev2::capturebuffer_ptr_t video_buffer,
                            ork::lev2::audioframecapture_ptr_t audio_capture) {
  if (!_valid) {
    return;
  }

  // Encode video frame if provided
  if (_enable_video && video_buffer) {
    _writeVideoFrame(video_buffer);
  }

  // Encode audio if provided
  if (_enable_audio && audio_capture) {
    _writeAudioFrame(audio_capture);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////

AVFrame* Encoder::_getVideoFrame(ork::lev2::capturebuffer_ptr_t external_video) {
  AVCodecContext* c = _video_stream->enc;

  /* If no external buffer provided, return NULL (no more frames) */
  if (!external_video)
    return NULL;

  /* when we pass a frame to the encoder, it may keep a reference to it
   * internally; make sure we do not overwrite it here */
  if (av_frame_make_writable(_video_stream->frame) < 0)
    exit(1);

  // Extract RGB image from external buffer
  auto img = external_video->_image;
  if (!img) {
    fprintf(stderr, "No image in capture buffer\n");
    return NULL;
  }

  OrkAssert(img->_format == ork::lev2::EBufferFormat::RGBA8);
  auto as_rgb8 = std::make_shared<ork::lev2::Image>();
  as_rgb8->convertFromImageToFormat(*img, ork::lev2::EBufferFormat::RGB8);
  auto src_pixels = (const uint8_t*)as_rgb8->_data->data();
  int width = as_rgb8->_width;
  int height = as_rgb8->_height;
  //static int frame_count = 0;
  //img->writeToFile(ork::FormatString("/tmp/ffmpeg_frame_%04d.png", frame_count++));
  //as_rgb8->writeToFile(ork::FormatString("/tmp/ffmpeg_frame_%04db.png", frame_count++));
  // Setup RGB->YUV conversion context if needed
  if (!_video_stream->sws_ctx) {
    _video_stream->sws_ctx = sws_getContext(
        width, height, AV_PIX_FMT_RGB24,
        c->width, c->height, c->pix_fmt,
        SCALE_FLAGS, NULL, NULL, NULL);
    if (!_video_stream->sws_ctx) {
      fprintf(stderr, "Could not initialize the conversion context\n");
      exit(1);
    }
  }

  // Allocate temporary RGB frame if needed
  if (!_video_stream->tmp_frame) {
    _video_stream->tmp_frame = _allocVideoFrame(AV_PIX_FMT_RGB24, width, height);
    if (!_video_stream->tmp_frame) {
      fprintf(stderr, "Could not allocate temporary RGB frame\n");
      exit(1);
    }
  }

  // Copy RGB data to temp frame (with Y-flip for OpenGL/Vulkan coordinate system)
  auto dest_buffer = _video_stream->tmp_frame->data[0];
  auto dest_linesize = _video_stream->tmp_frame->linesize[0];
  constexpr size_t chunk_size = 32;
  size_t num_chunks = (height + chunk_size - 1) / chunk_size; // Round up division
  std::atomic<int> chunkcounter = num_chunks;
  for (size_t chunk = 0; chunk < num_chunks; chunk++) {
    auto op = [=, &chunkcounter](){
      size_t y_start = chunk * chunk_size;
      size_t y_end = std::min(y_start + chunk_size, size_t(height));
      for (size_t y = y_start; y < y_end; y++) {
        //size_t src_row_base = ((height - 1) - y) * width * 3;
        size_t src_row_base = y * width * 3;
        for (int x = 0; x < width; x++) {
          size_t src_pix_base = src_row_base + (x * 3);
          dest_buffer[y * dest_linesize + 3 * x + 0] = src_pixels[src_pix_base + 0]; // R
          dest_buffer[y * dest_linesize + 3 * x + 1] = src_pixels[src_pix_base + 1]; // G
          dest_buffer[y * dest_linesize + 3 * x + 2] = src_pixels[src_pix_base + 2]; // B
        }
      }
      chunkcounter.fetch_sub(1);
    };
    ::ork::opq::concurrentQueue()->enqueue(op);
  }
  while(chunkcounter.load() > 0) {
    std::this_thread::yield();
  }

  // Convert RGB to YUV
  sws_scale(
      _video_stream->sws_ctx,
      (const uint8_t* const*)_video_stream->tmp_frame->data,
      _video_stream->tmp_frame->linesize,
      0,
      height,
      _video_stream->frame->data,
      _video_stream->frame->linesize);

  _video_stream->frame->pts = _video_stream->next_pts++;

  _video_stream->frame->color_range = AVCOL_RANGE_MPEG;

  return _video_stream->frame;
}

///////////////////////////////////////////////////////////////////////////////////////////////

int Encoder::_writeFrame(AVCodecContext* c, AVStream* st, AVFrame* frame, AVPacket* pkt) {
  int ret;

  // send the frame to the encoder
  ret = avcodec_send_frame(c, frame);
  if (ret < 0) {
    fprintf(stderr, "Error sending a frame to the encoder: %s\n", av_err2str(ret));
    exit(1);
  }

  while (ret >= 0) {
    ret = avcodec_receive_packet(c, pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      break;
    else if (ret < 0) {
      fprintf(stderr, "Error encoding a frame: %s\n", av_err2str(ret));
      exit(1);
    }

    /* rescale output packet timestamp values from codec to stream timebase */
    av_packet_rescale_ts(pkt, c->time_base, st->time_base);
    pkt->stream_index = st->index;

    /* Write the compressed frame to the media file. */

    ret = av_interleaved_write_frame(oc, pkt);
    /* pkt is now blank (av_interleaved_write_frame() takes ownership of
     * its contents and resets pkt), so that no unreferencing is necessary.
     * This would be different if one used av_write_frame(). */
    if (ret < 0) {
      fprintf(stderr, "Error while writing output packet: %s\n", av_err2str(ret));
      exit(1);
    }
  }

  return ret == AVERROR_EOF ? 1 : 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// encode one audio frame and send it to the muxer
// return 1 when encoding is finished, 0 otherwise
///////////////////////////////////////////////////////////////////////////////////////////////

int Encoder::_writeVideoFrame(ork::lev2::capturebuffer_ptr_t external_buffer) {
    auto vfr = _getVideoFrame(external_buffer);
    _num_frames_encoded++;
    return _writeFrame(_video_stream->enc, _video_stream->st, vfr, _video_stream->tmp_pkt);    
}

///////////////////////////////////////////////////////////////////////////////////////////////
// encode one audio frame and send it to the muxer
// return 1 when encoding is finished, 0 otherwise
///////////////////////////////////////////////////////////////////////////////////////////////

int Encoder::_writeAudioFrame(ork::lev2::audioframecapture_ptr_t external_audio) {
  auto frame = _getAudioFrame(external_audio);

  if (frame) {
    _audio_stream->samples_count += frame->nb_samples;
  }

  return _writeFrame(_audio_stream->enc, _audio_stream->st, frame, _audio_stream->tmp_pkt);
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Prepare audio frame from external audio capture data 
///////////////////////////////////////////////////////////////////////////////////////////////

AVFrame* Encoder::_getAudioFrame( ork::lev2::audioframecapture_ptr_t external_audio) { //
  AVFrame* output_frame = _audio_stream->frame;

  ///////////////////////////////////////////////////////
  // If no external audio provided, return nullptr (no more frames)
  ///////////////////////////////////////////////////////

  if (!external_audio || external_audio->_num_samples == 0)
    return nullptr;

  int num_samples = external_audio->_num_samples;
  int num_channels = _audio_stream->enc->ch_layout.nb_channels;
  OrkAssert(num_channels==2);

  // Ensure frame is writable
  if (av_frame_make_writable(output_frame) < 0)
    return nullptr;

  // ensure output_frame has enough samples
  if (output_frame->nb_samples < num_samples) {
    av_frame_free(&output_frame);
    output_frame = _allocAudioFrame(_audio_stream->enc->sample_fmt,
                                    &_audio_stream->enc->ch_layout,
                                    _audio_stream->enc->sample_rate,
                                    num_samples);
    _audio_stream->frame = output_frame;
  }
  ///////////////////////////////////////////////////////
  // Direct copy: float arrays -> planar float (FLTP)
  // data[0] = left channel, data[1] = right channel
  ///////////////////////////////////////////////////////

  auto left_out = (float*)output_frame->data[0];
  auto right_out = (float*)output_frame->data[1];

  for (int i = 0; i < num_samples; i++) {
    left_out[i] = external_audio->_left[i];
    right_out[i] = external_audio->_right[i];
  }

  output_frame->nb_samples = num_samples;
  output_frame->pts = _audio_stream->next_pts;
  _audio_stream->next_pts += num_samples;

  return output_frame;
}

///////////////////////////////////////////////////////////////////////////////////////////////

void Encoder::_add_stream( OutputStream* ost,         //
                           const AVCodec** codec,     //
                           enum AVCodecID codec_id) { //
  AVCodecContext* c;
  int i;

  /////////////////////////////////////////
  // find the encoder 
  /////////////////////////////////////////

  *codec = avcodec_find_encoder(codec_id);
  if (!(*codec)) {
    fprintf(stderr, "Could not find encoder for '%s'\n", avcodec_get_name(codec_id));
    exit(1);
  }

  /////////////////////////////////////////

  ost->tmp_pkt = av_packet_alloc();
  if (!ost->tmp_pkt) {
    fprintf(stderr, "Could not allocate AVPacket\n");
    exit(1);
  }

  ost->st = avformat_new_stream(oc, NULL);
  if (!ost->st) {
    fprintf(stderr, "Could not allocate stream\n");
    exit(1);
  }
  ost->st->id = oc->nb_streams - 1;
  c           = avcodec_alloc_context3(*codec);
  if (!c) {
    fprintf(stderr, "Could not alloc an encoding context\n");
    exit(1);
  }
  ost->enc = c;

  switch ((*codec)->type) {
    ///////////////////////////////////////////////////////
    case AVMEDIA_TYPE_AUDIO: {
    ///////////////////////////////////////////////////////
      c->sample_fmt  = (*codec)->sample_fmts ? (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
      if(_settings->_preset_name=="low"){
        c->bit_rate = 320000;
      } else if(_settings->_preset_name=="medium"){
        c->bit_rate = 360000;
      } else if(_settings->_preset_name=="default"){
        c->bit_rate = 360000;
      } else if(_settings->_preset_name=="high"){
        c->bit_rate = 640000;
      } else if(_settings->_preset_name=="ultra"){
        c->bit_rate = 1280000;
      }
      c->sample_rate = 48000;
      if ((*codec)->supported_samplerates) {
        c->sample_rate = (*codec)->supported_samplerates[0];
        for (i = 0; (*codec)->supported_samplerates[i]; i++) {
          if ((*codec)->supported_samplerates[i] == 48000)
            c->sample_rate = 48000;
        }
      }
      AVChannelLayout layout = AV_CHANNEL_LAYOUT_STEREO;
      av_channel_layout_copy(&c->ch_layout, &layout);
      ost->st->time_base = (AVRational){1, c->sample_rate};
      break;
    }
    ///////////////////////////////////////////////////////
    case AVMEDIA_TYPE_VIDEO: {
    ///////////////////////////////////////////////////////
      c->codec_id = codec_id;

      if(_settings->_preset_name=="fast"){
        c->bit_rate = 1000000;
        #if defined (__APPLE__)
        av_opt_set_int(c->priv_data, "prio_speed", 1, 0);  // prioritize speed
        av_opt_set(c->priv_data, "profile", "baseline", 0);
        #else
        av_opt_set(c->priv_data, "preset", "ultrafast", 0 );
        #endif        
      } else if(_settings->_preset_name=="medium" or _settings->_preset_name=="default" or _settings->_preset_name==""){
        c->bit_rate = 6400000;
        #if defined (__APPLE__)
        av_opt_set(c->priv_data, "profile", "main", 0);
        #else
        av_opt_set(c->priv_data, "preset", "medium", 0 );
        #endif        
      } else if(_settings->_preset_name=="high"){
        c->bit_rate = 12800000;
        #if defined (__APPLE__)
        av_opt_set_int(c->priv_data, "prio_speed", 0, 0);  // prioritize quality
        av_opt_set(c->priv_data, "profile", "high", 0);
        av_opt_set(c->priv_data, "coder", "cabac", 0);  // better compression
        #else
        av_opt_set(c->priv_data, "preset", "high", 0 );
        #endif        
      } else if(_settings->_preset_name=="ultra"){
        c->bit_rate = 128000000;
        #if defined (__APPLE__)
        av_opt_set_int(c->priv_data, "prio_speed", 0, 0);
        av_opt_set(c->priv_data, "profile", "high", 0);
        av_opt_set(c->priv_data, "coder", "cabac", 0);
        #else
        av_opt_set(c->priv_data, "preset", "veryslow", 0 );
        #endif        
      }
      // Resolution must be a multiple of two. 
      OrkAssert((_settings->_width % 2) == 0);
      OrkAssert((_settings->_height % 2) == 0);
      c->width  = _settings->_width;
      c->height = _settings->_height;
      // timebase: This is the fundamental unit of time (in seconds) in terms
      // of which frame timestamps are represented. For fixed-fps content,
      // timebase should be 1/framerate and timestamp increments should be
      // identical to 1.
      ost->st->time_base = (AVRational){1, _settings->_fps};
      c->time_base       = ost->st->time_base;

      c->gop_size = 12; // emit one intra frame every twelve frames at most 
      c->pix_fmt  = STREAM_PIX_FMT;
      if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
        // just for testing, we also add B-frames 
        c->max_b_frames = 2;
      }
      if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
        // Needed to avoid using macroblocks in which some coeffs overflow.
        // This does not happen with normal video, it just happens here as
        // the motion of the chroma plane does not match the luma plane. 
        c->mb_decision = 2;
      }
      c->color_range = AVCOL_RANGE_MPEG;
      break;
    }
    ///////////////////////////////////////////////////////
    default:
      OrkAssert(false);
      break;
  }

  // Some formats want stream headers to be separate.
  if (oc->oformat->flags & AVFMT_GLOBALHEADER)
    c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}

///////////////////////////////////////////////////////////////////////////////////////////////

encoder_ptr_t createEncoder( ork::lev2::moviecapsettings_ptr_t settings ) {
  auto enc = std::make_shared<Encoder>(settings);
  return enc->_valid ? enc : nullptr;
}


} // namespace ffmpeg_enc
