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

  ///////////////////////////////////////////////////////////////////////////////
  // MovieBackend enum - select video decode backend
  ///////////////////////////////////////////////////////////////////////////////
  py::enum_<MovieBackend>(module_lev2, "MovieBackend")
      .value("FFMPEG", MovieBackend::FFMPEG, "CPU decode (cross-platform, default)")
      .value("VIDEOTOOLBOX", MovieBackend::VIDEOTOOLBOX, "macOS: Hardware decode → IOSurface → Vulkan")
      .value("VAAPI", MovieBackend::VAAPI, "Linux AMD: Hardware decode → DMA-BUF → Vulkan")
      .value("NVDEC", MovieBackend::NVDEC, "Linux NVIDIA: Hardware decode → CUDA → Vulkan")
      .export_values();

  ///////////////////////////////////////////////////////////////////////////////
  // MoviePixelFormat enum - select output pixel format
  ///////////////////////////////////////////////////////////////////////////////
  py::enum_<MoviePixelFormat>(module_lev2, "MoviePixelFormat")
      .value("AUTO", MoviePixelFormat::AUTO, "Backend decides optimal format")
      .value("YCBCR_NV12", MoviePixelFormat::YCBCR_NV12, "4:2:0 YCbCr 8-bit (hardware native)")
      .value("YCBCR_P010", MoviePixelFormat::YCBCR_P010, "4:2:0 YCbCr 10-bit (HDR)")
      .value("RGB_RGBA8", MoviePixelFormat::RGB_RGBA8, "RGB conversion at decode time")
      .export_values();

  ///////////////////////////////////////////////////////////////////////////////
  // MoviePlaybackContext
  ///////////////////////////////////////////////////////////////////////////////
  auto movieplayback_type = //
      py::class_<MoviePlaybackContext, movieplayback_ptr_t>(module_lev2, "MoviePlaybackContext")
          .def(py::init<>())
          // Legacy init - defaults to FFMPEG backend
          .def("init", [](movieplayback_ptr_t ctx, py::object filename) { //
            auto as_str = py::cast<py::str>(filename);
            ctx->init(as_str.cast<std::string>());
          })
          // New init with backend selection
          .def("init", [](movieplayback_ptr_t ctx,
                         py::object filename,
                         MovieBackend backend,
                         MoviePixelFormat format) {
            auto as_str = py::cast<py::str>(filename);
            ctx->init(as_str.cast<std::string>(), backend, format);
          },
          py::arg("filename"),
          py::arg("backend") = MovieBackend::FFMPEG,
          py::arg("format") = MoviePixelFormat::AUTO,
          "Initialize movie playback with backend selection") 
          .def("play", [](movieplayback_ptr_t ctx) { ctx->play(); })
          .def("pause", [](movieplayback_ptr_t ctx) { ctx->pause(); })
          .def("stop", [](movieplayback_ptr_t ctx) { ctx->stop(); })
          .def("restart", [](movieplayback_ptr_t ctx) { ctx->restart(); })
          .def("createImageProvider", [](movieplayback_ptr_t ctx) -> image_provider_ptr_t { return ctx->createImageProvider(); })
          .def_property_readonly("image_provider", [](movieplayback_ptr_t ctx) -> image_provider_ptr_t { return ctx->createImageProvider(); })
          .def("createTextureProvider", [](movieplayback_ptr_t ctx) -> texture_provider_ptr_t { return ctx->createTextureProvider(); })
          .def_property_readonly("texture_provider", [](movieplayback_ptr_t ctx) -> texture_provider_ptr_t { return ctx->createTextureProvider(); })
          .def_property_readonly("texture", [](movieplayback_ptr_t ctx) -> texture_ptr_t {
            return ctx->_backend_impl ? ctx->_backend_impl->texture() : nullptr;
          })
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
                return ctx->_backend_impl ? ctx->_backend_impl->bitRate() : 0;
              })
          .def_property_readonly(
              "format_name",
              [](movieplayback_ptr_t ctx) -> std::string {
                return ctx->_backend_impl ? ctx->_backend_impl->formatName() : "";
              })
          .def_property_readonly(
              "format_long_name",
              [](movieplayback_ptr_t ctx) -> std::string {
                return ctx->_backend_impl ? ctx->_backend_impl->formatLongName() : "";
              })
          // Video codec metadata
          .def_property_readonly(
              "video_codec_name",
              [](movieplayback_ptr_t ctx) -> std::string {
                return ctx->_backend_impl ? ctx->_backend_impl->videoCodecName() : "";
              })
          .def_property_readonly("video_bit_rate", [](movieplayback_ptr_t ctx) -> int64_t {
            return ctx->_backend_impl ? ctx->_backend_impl->videoBitRate() : 0;
          })
          // Audio/video sync adjustment (in seconds)
          // Positive = audio lags video, Negative = audio leads video
          .def_property(
              "audio_timeshift",
              [](movieplayback_ptr_t ctx) -> double {
                return ctx->_audio_timeshift;
              },
              [](movieplayback_ptr_t ctx, double timeshift) {
                ctx->_audio_timeshift = timeshift;
              })
          // Audio mono mixdown (default false = stereo output)
          // When true, stereo/surround audio is mixed down to mono
          .def_property(
              "mono_mixdown",
              [](movieplayback_ptr_t ctx) -> bool {
                return ctx->_mono_mixdown;
              },
              [](movieplayback_ptr_t ctx, bool mono) {
                ctx->_mono_mixdown = mono;
              });
  type_codec->registerStdCodec<movieplayback_ptr_t>(movieplayback_type);
  ///////////////////////////////////////////////////////////////////////////////
  auto moviecapcontext_type = //
      py::class_<MovieCaptureContext, moviecapcontext_ptr_t>(module_lev2, "MovieCaptureContext");
  type_codec->registerStdCodec<moviecapcontext_ptr_t>(moviecapcontext_type);
  ///////////////////////////////////////////////////////////////////////////////
}
} // namespace ork::lev2
