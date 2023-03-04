#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"


extern "C" {
//#include <x264.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavutil/mathematics.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

namespace ork::lev2 {

#if defined(__APPLE__)
extern bool _macosUseHIDPI;
#endif

struct MovieContext {

  MovieContext() {
    _filename = "GeneratedVideo.mp4";
  }

  void init(int width, int height) {

    _width  = 2560;
    _height = 1440;

    //av_register_all();
    //avcodec_register_all();

    ////////////////////////
    // setup scaler
    ////////////////////////

    _swscontext = sws_getContext(
        _width,
        _height,
        AV_PIX_FMT_RGB24,
        _width,
        _height,
        AV_PIX_FMT_YUV420P,
        SWS_FAST_BILINEAR,
        NULL,
        NULL,
        NULL); // Preparing to convert my generated RGB images to YUV frames.

    // Preparing the data concerning the format and codec in order to write properly the header, frame data and end of file.


    _format = (AVOutputFormat *) av_guess_format(nullptr, _filename.c_str(), nullptr);

    //////////////////////////////////////////
    // setup muxer
    //////////////////////////////////////////

    avformat_alloc_output_context2(&_muxer, _format, nullptr, _filename.c_str());

    //////////////////////////////////////////
    // setup encoder
    //////////////////////////////////////////

    //auto codec = avcodec_find_encoder_by_name("libx264");
    auto codec = avcodec_find_encoder(_format->video_codec);
    _codec = codec; //avcodec_find_encoder(  );

    _encoder               = avcodec_alloc_context3(_codec);

    //////////////////////////////////////////
    // setup stream
    //////////////////////////////////////////

    _stream            = avformat_new_stream(_muxer, codec);
    _stream->time_base = AVRational({1,_fps});
    _stream->r_frame_rate = AVRational({_fps,1});

    auto codecpar = _stream->codecpar;

    int bitrate = 16000;
    codecpar->codec_id = _format->video_codec;
    codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    codecpar->width = _width;
    codecpar->height = _height;
    codecpar->format = AV_PIX_FMT_YUV420P;
    codecpar->bit_rate = bitrate * 1000;

    //////////////////////////////////////////
    // Setting up the encoder
    //////////////////////////////////////////

    avcodec_parameters_to_context(_encoder, codecpar);

    //_encoder->bit_rate     = _width * _height;
    //_encoder->width        = width;
    //_encoder->height       = height;
    //_encoder->pix_fmt      = AV_PIX_FMT_YUV420P;
    _encoder->framerate    = AVRational({_fps,1});
    _encoder->time_base    = AVRational({1, _fps});
    _encoder->gop_size     = _fps; // have at least 1 I-frame per second
    _encoder->max_b_frames = 1;

    //if (codecpar->codec_id == AV_CODEC_ID_H264) {
      //  av_opt_set(_encoder, "preset", "ultrafast", 0);
    //}

    avcodec_parameters_from_context(_stream->codecpar, _encoder);

    //if (_muxer->oformat->flags & AVFMT_GLOBALHEADER) // Some formats require a global header.
//      _encoder->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    avcodec_open2(_encoder, _codec, nullptr);

    _muxer->video_codec_id = _codec->id;
    _muxer->video_codec    = codec;


    //////////////////////////////////////////

    avio_open(&_muxer->pb, _filename.c_str(), AVIO_FLAG_WRITE);
    int ret = avformat_write_header(_muxer, nullptr);

    av_dump_format(_muxer, 0, _filename.c_str(), 1);

    // Allocating memory for each RGB frame, which will be lately converted to YUV:
    _rgb_pic         = av_frame_alloc();
    _rgb_pic->format = AV_PIX_FMT_RGB24;
    _rgb_pic->width  = _width;
    _rgb_pic->height = _height;
    ret              = av_frame_get_buffer(_rgb_pic, 1);

    // Allocating memory for each conversion output YUV frame:
    _yuv_pic         = av_frame_alloc();
    _yuv_pic->format = AV_PIX_FMT_YUV420P;
    _yuv_pic->width  = _width;
    _yuv_pic->height = _height;
    ret              = av_frame_get_buffer(_yuv_pic, 1);

    // After the format, code and general frame data is set, we write the video in the frame generation loop:
    // std::vector<uint8_t> B(width*height*3);
  }
  /////////////////////////////////////////////////////////////////////////////////////////
  ~MovieContext() {
    terminate();
  }
  /////////////////////////////////////////////////////////////////////////////////////////
  bool encodeFrame(AVFrame* frame) // returns if encoded stream end reached
  {
    bool was_received = false;
    int ret = (avcodec_send_frame(_encoder, frame)>=0);
    OrkAssert(ret>=0);

    bool done = false;
    ret = avcodec_receive_packet(_encoder, &_avpacket);
    if(0==ret){
        if((_frame%_fps)==0)
          _avpacket.flags |= AV_PKT_FLAG_KEY;

        _avpacket.pts = av_rescale_q(_avpacket.pts, _encoder->time_base, _stream->time_base);
        _avpacket.dts = av_rescale_q(_avpacket.dts, _encoder->time_base, _stream->time_base);
        _avpacket.duration = av_rescale_q(_avpacket.duration, _encoder->time_base, _stream->time_base);
        av_interleaved_write_frame(_muxer, &_avpacket); // Write the encoded frame to the mp4 file.
        av_packet_unref(&_avpacket);
        was_received = true;
    }
    return was_received;
  }
  /////////////////////////////////////////////////////////////////////////////////////////
  void writeFrame(CaptureBuffer& capbuf) {

    if(_terminated){
      return;
    }

    bool downsample_2x2 = false;

#if defined(__APPLE__)
    if (capbuf.miW != _width*2) {
      return;
    }
    if (capbuf.miH != _height*2) {
      return;
    }
    downsample_2x2 = true;
#else
    OrkAssert(capbuf.miW == _width);
    OrkAssert(capbuf.miH == _height);
#endif
    ///////////////////////////////////////
    // blit into scaling buffer
    ///////////////////////////////////////

    auto src_pixels = (const uint8_t*)capbuf._data;
    size_t src_pixcount = capbuf.miW*capbuf.miH;
    size_t src_pixsize = src_pixcount*3;
    OrkAssert(capbuf.meFormat == EBufferFormat::RGB8);

    auto dest_buffer   = _rgb_pic->data[0];
    auto dest_linesize = _rgb_pic->linesize[0];

    if(downsample_2x2){
      size_t row_stride_src = capbuf.miW * 3;
      for (int y = 0; y < _height; y++) {
        size_t y2 = y*2;
        size_t ry2 = (capbuf.miH - 2) - y2;
        size_t src_row_baseA = ry2 * row_stride_src;
        size_t src_row_baseB = src_row_baseA + row_stride_src;
        for (int x = 0; x < _width; x++) {
          size_t src_pix_baseA = src_row_baseA + ((x*2) * 3);
          size_t src_pix_baseB = src_row_baseB + ((x*2) * 3);

          uint16_t AR0 = src_pixels[src_pix_baseA + 2];
          uint16_t AR1 = src_pixels[src_pix_baseA + 5];
          uint16_t AG0 = src_pixels[src_pix_baseA + 1];
          uint16_t AG1 = src_pixels[src_pix_baseA + 4];
          uint16_t AB0 = src_pixels[src_pix_baseA + 0];
          uint16_t AB1 = src_pixels[src_pix_baseA + 3];

          uint16_t BR0 = src_pixels[src_pix_baseB + 2];
          uint16_t BR1 = src_pixels[src_pix_baseB + 5];
          uint16_t BG0 = src_pixels[src_pix_baseB + 1];
          uint16_t BG1 = src_pixels[src_pix_baseB + 4];
          uint16_t BB0 = src_pixels[src_pix_baseB + 0];
          uint16_t BB1 = src_pixels[src_pix_baseB + 3];

          dest_buffer[y * dest_linesize + 3 * x]     = (AR0+AR1+BR0+BR1)>>2; // B
          dest_buffer[y * dest_linesize + 3 * x + 1] = (AG0+AG1+BG0+BG1)>>2; // G
          dest_buffer[y * dest_linesize + 3 * x + 2] = (AB0+AB1+BB0+BB1)>>2; // R
        }
      }
    }
    else {
      for (int y = 0; y < _height; y++) {
        size_t src_row_base = ((_height - 1) - y) * _width * 3;
        for (int x = 0; x < _width; x++) {
          size_t src_pix_base = src_row_base + (x * 3);

          dest_buffer[y * dest_linesize + 3 * x]     = src_pixels[src_pix_base + 2]; // B
          dest_buffer[y * dest_linesize + 3 * x + 1] = src_pixels[src_pix_base + 1]; // G
          dest_buffer[y * dest_linesize + 3 * x + 2] = src_pixels[src_pix_base + 0]; // R
        }
      }

    }

    ///////////////////////////////////////
    // RGB->YUV
    ///////////////////////////////////////

    sws_scale(
        _swscontext,
        _rgb_pic->data,
        _rgb_pic->linesize,
        0,
        _height,
        _yuv_pic->data,
        _yuv_pic->linesize);

    //////////////////////////////////
    // set up packet
    ///////////////////////////////////////

    av_init_packet(&_avpacket);
    
    //double pts = double(_encoder->time_base.num) / double(_encoder->time_base.den) * 90 * _frame;

    double pts = (1.0 / _fps) * 90 * _frame;

    _avpacket.data = nullptr;
    _avpacket.size = 0;
    _avpacket.stream_index = _stream->index;
    _yuv_pic->pts = _frame; // set presentation time stamp
    _avpacket.pts = _frame;
    _avpacket.dts = AV_NOPTS_VALUE;

    ///////////////////////////////////////
    // encode
    ///////////////////////////////////////

    encodeFrame(_yuv_pic);
    _frame++;

  }
  void terminate() {
    
    if(_terminated){
      return;
    }

    _terminated = true;
    
    printf( "terminating movie stream\n");

    bool end_of_stream = false;

    ////////////////////////////
    // write pending data
    ////////////////////////////

    while (not end_of_stream) {
      end_of_stream = encodeFrame(nullptr);
    }

    ////////////////////////////
    // close the movie file
    ////////////////////////////

    av_write_trailer(_muxer); // Writing the end of the file.
    if (!(_format->flags & AVFMT_NOFILE))
      avio_closep(&_muxer->pb);
    avcodec_close(_encoder);
    sws_freeContext(_swscontext);
    av_frame_free(&_rgb_pic);
    av_frame_free(&_yuv_pic);
    avformat_free_context(_muxer);

    ////////////////////////////

    printf("Wrote %d frames\n", _frame);
    printf("Movie generation complete..\n");
  }

  int _width  = 0;
  int _height = 0;
  int _frame  = 0;
  std::string _filename;
  struct SwsContext* _swscontext = nullptr;
  AVOutputFormat* _format  = nullptr;
  const AVCodec* _codec          = nullptr;
  AVCodecContext* _encoder       = nullptr;
  AVFormatContext* _muxer        = nullptr;
  AVStream* _stream              = nullptr;
  AVFrame* _rgb_pic              = nullptr;
  AVFrame* _yuv_pic              = nullptr;
  int _got_output                = 0;
  int _fps                       = 60;
  AVPacket _avpacket;
  bool _terminated = false;
};

} // namespace ork::lev2
