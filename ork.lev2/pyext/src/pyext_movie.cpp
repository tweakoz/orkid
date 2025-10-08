////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/gfx/util/movie.inl>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/sampler.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
audio::singularity::prgdata_ptr_t createStreamingOscillatorFromMoviePlayback( movieplayback_ptr_t movie_playback,             
                                                                              audio::singularity::synth_ptr_t audio_synth);

///////////////////////////////////////////////////////////////////////////////
void pyinit_movie(py::module& module_lev2) {
  auto type_codec         = python::pb11_typecodec_t::instance();
  auto movieplayback_type = //
      py::class_<MoviePlaybackContext, movieplayback_ptr_t>(module_lev2, "MoviePlaybackContext")
          .def(py::init<>())
          .def("init", [](movieplayback_ptr_t ctx, const std::string& filename) { ctx->init(filename); })
          .def("play", [](movieplayback_ptr_t ctx) { ctx->play(); })
          .def("pause", [](movieplayback_ptr_t ctx) { ctx->pause(); })
          .def("stop", [](movieplayback_ptr_t ctx) { ctx->stop(); })
          .def("restart", [](movieplayback_ptr_t ctx) { ctx->restart(); })
          .def("createImageProvider", [](movieplayback_ptr_t ctx) -> image_provider_ptr_t { return ctx->createImageProvider(); })
          .def(
              "createAudioProgram",
              [](movieplayback_ptr_t ctx, audio::singularity::synth_ptr_t synth) -> audio::singularity::prgdata_ptr_t {
                return createStreamingOscillatorFromMoviePlayback(ctx, synth);
              })
          .def_property_readonly(
              "state",
              [](movieplayback_ptr_t ctx) -> crcstring_ptr_t {
                auto crc = std::make_shared<CrcString>(uint64_t(ctx->_state));
                return crc;
              })
          .def_property_readonly("fps", [](movieplayback_ptr_t ctx) -> double { return ctx->_fps; })
          .def_property_readonly(
              "width",
              [](movieplayback_ptr_t ctx) -> int {
                if (ctx->_format_ctx && ctx->_video_stream_idx >= 0) {
                  return ctx->_format_ctx->streams[ctx->_video_stream_idx]->codecpar->width;
                }
                return 0;
              })
          .def_property_readonly("height", [](movieplayback_ptr_t ctx) -> int {
            if (ctx->_format_ctx && ctx->_video_stream_idx >= 0) {
              return ctx->_format_ctx->streams[ctx->_video_stream_idx]->codecpar->height;
            }
            return 0;
          });
  type_codec->registerStdCodec<movieplayback_ptr_t>(movieplayback_type);
  ///////////////////////////////////////////////////////////////////////////////
}
} // namespace ork::lev2
