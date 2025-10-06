#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/targetinterfaces.h>

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

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

struct MovieCaptureContext {

  MovieCaptureContext();

  void init(int width, int height);
  /////////////////////////////////////////////////////////////////////////////////////////
  ~MovieCaptureContext();
  /////////////////////////////////////////////////////////////////////////////////////////
  bool encodeFrame(AVFrame* frame); // returns true if encoded and stream end reached
  void writeFrame(captureasync_ptr_t capbuf);
  void terminate();
  /////////////////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
