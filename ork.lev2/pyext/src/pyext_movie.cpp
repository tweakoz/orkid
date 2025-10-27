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
audio::singularity::prgdata_ptr_t
createStreamingOscillatorFromMoviePlayback(movieplayback_ptr_t movie_playback, audio::singularity::synth_ptr_t audio_synth);

///////////////////////////////////////////////////////////////////////////////
void pyinit_movie(py::module& module_lev2) {
  auto type_codec         = python::pb11_typecodec_t::instance();
  auto movieplayback_type = //
      py::class_<MoviePlaybackContext, movieplayback_ptr_t>(module_lev2, "MoviePlaybackContext")
          .def(py::init<>())
          .def("init", [](movieplayback_ptr_t ctx, py::object filename) { //
            auto as_str    = py::cast<py::str>(filename);
            ctx->init(as_str.cast<std::string>());
          }) 
          .def("play", [](movieplayback_ptr_t ctx) { ctx->play(); })
          .def("pause", [](movieplayback_ptr_t ctx) { ctx->pause(); })
          .def("stop", [](movieplayback_ptr_t ctx) { ctx->stop(); })
          .def("restart", [](movieplayback_ptr_t ctx) { ctx->restart(); })
          .def("createImageProvider", [](movieplayback_ptr_t ctx) -> image_provider_ptr_t { return ctx->createImageProvider(); })
          .def_property_readonly("image_provider", [](movieplayback_ptr_t ctx) -> image_provider_ptr_t { return ctx->createImageProvider(); })
          .def(
              "createAudioProgram",
              [](movieplayback_ptr_t ctx, audio::singularity::synth_ptr_t synth) -> audio::singularity::prgdata_ptr_t {
                return createStreamingOscillatorFromMoviePlayback(ctx, synth);
              })
          .def_property_readonly(
              "state",
              [](movieplayback_ptr_t ctx) -> std::string {
                switch (ctx->_state) {
                  case MoviePlaybackContext::State::STOPPED:
                    return "STOPPED";
                  case MoviePlaybackContext::State::PLAYING:
                    return "PLAYING";
                  case MoviePlaybackContext::State::PAUSED:
                    return "PAUSED";
                  default:
                    return "UNKNOWN";
                }
              })
          .def_property_readonly("fps", [](movieplayback_ptr_t ctx) -> double { return ctx->_fps; })
          .def_property_readonly("width", [](movieplayback_ptr_t ctx) -> int { return ctx->_video_width; })
          .def_property_readonly("height", [](movieplayback_ptr_t ctx) -> int { return ctx->_video_height; })
          // Basic playback info
          .def_property_readonly("filename", [](movieplayback_ptr_t ctx) -> std::string { return ctx->_filename; })
          .def_property_readonly("frame_duration", [](movieplayback_ptr_t ctx) -> double { return ctx->_frame_duration; })
          .def_property_readonly(
              "current_frame_index", [](movieplayback_ptr_t ctx) -> int64_t { return ctx->_current_frame_index; })
          .def_property_readonly("max_queue_size", [](movieplayback_ptr_t ctx) -> size_t { return ctx->_max_queue_size; })
          // Stream indices
          .def_property_readonly("video_stream_index", [](movieplayback_ptr_t ctx) -> int { return ctx->_video_stream_idx; })
          .def_property_readonly("audio_stream_index", [](movieplayback_ptr_t ctx) -> int { return ctx->_audio_stream_idx; })
          // Audio metadata
          .def_property_readonly(
              "audio_sample_rate",
              [](movieplayback_ptr_t ctx) -> int {
                if (ctx->_audio_config && ctx->_audio_config->_valid) {
                  return ctx->_audio_config->_sample_rate;
                }
                return 0;
              })
          .def_property_readonly(
              "audio_channels",
              [](movieplayback_ptr_t ctx) -> int {
                if (ctx->_audio_config && ctx->_audio_config->_valid) {
                  return ctx->_audio_config->_num_channels;
                }
                return 0;
              })
          .def_property_readonly(
              "audio_codec_name",
              [](movieplayback_ptr_t ctx) -> std::string {
                if (ctx->_audio_config && ctx->_audio_config->_valid) {
                  return ctx->_audio_config->_codec_name;
                }
                return "";
              })
          .def_property_readonly(
              "has_audio", [](movieplayback_ptr_t ctx) -> bool { return ctx->_audio_config && ctx->_audio_config->_valid; })
          // Format metadata
          .def_property_readonly("duration", [](movieplayback_ptr_t ctx) -> double { return ctx->_duration; })
          .def_property_readonly(
              "bit_rate",
              [](movieplayback_ptr_t ctx) -> int64_t {
                if (ctx->_format_ctx) {
                  return ctx->_format_ctx->bit_rate;
                }
                return 0;
              })
          .def_property_readonly(
              "nb_streams",
              [](movieplayback_ptr_t ctx) -> unsigned int {
                if (ctx->_format_ctx) {
                  return ctx->_format_ctx->nb_streams;
                }
                return 0;
              })
          .def_property_readonly(
              "format_name",
              [](movieplayback_ptr_t ctx) -> std::string {
                if (ctx->_format_ctx && ctx->_format_ctx->iformat && ctx->_format_ctx->iformat->name) {
                  return ctx->_format_ctx->iformat->name;
                }
                return "";
              })
          .def_property_readonly(
              "format_long_name",
              [](movieplayback_ptr_t ctx) -> std::string {
                if (ctx->_format_ctx && ctx->_format_ctx->iformat && ctx->_format_ctx->iformat->long_name) {
                  return ctx->_format_ctx->iformat->long_name;
                }
                return "";
              })
          // Video codec metadata
          .def_property_readonly(
              "video_codec_name",
              [](movieplayback_ptr_t ctx) -> std::string {
                if (ctx->_video_codec && ctx->_video_codec->name) {
                  return ctx->_video_codec->name;
                }
                return "";
              })
          .def_property_readonly("video_bit_rate", [](movieplayback_ptr_t ctx) -> int64_t {
            if (ctx->_video_codec_ctx) {
              return ctx->_video_codec_ctx->bit_rate;
            }
            return 0;
          });
  type_codec->registerStdCodec<movieplayback_ptr_t>(movieplayback_type);
  ///////////////////////////////////////////////////////////////////////////////
  auto moviecapcontext_type = //
      py::class_<MovieCaptureContext, moviecapcontext_ptr_t>(module_lev2, "MovieCaptureContext");
  type_codec->registerStdCodec<moviecapcontext_ptr_t>(moviecapcontext_type);
  ///////////////////////////////////////////////////////////////////////////////
}
} // namespace ork::lev2
