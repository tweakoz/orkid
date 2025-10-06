////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/util/movie.inl>
#include <ork/kernel/memcpy.inl>
#include <iostream>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

void pyinit_gfx_image(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();
  auto image_type = //
      py::class_<Image, image_ptr_t>(module_lev2, "Image")
      .def_static("createFromFile", [](py::object inpath) -> image_ptr_t {
        auto as_str = py::cast<py::str>(inpath);
        auto datablock = ::ork::File::loadDatablock(as_str.cast<std::string>());
        auto img = std::make_shared<Image>();
        img->initFromDataBlock(datablock);
        return img;
      })
      .def_static("createRGB8FromColor", [](int w, int h, fvec3 color) -> image_ptr_t {
        auto img = std::make_shared<Image>();
        img->initRGB8WithColor(w,h,color);
        return img;
      })
      .def_static("createRGBA8FromColor", [](int w, int h, fvec4 color) -> image_ptr_t {
        auto img = std::make_shared<Image>();
        img->initRGBA8WithColor(w,h,color);
        return img;
      })
      .def_static("createFromBuffer", [](int w, int h, crcstring_ptr_t fmt, py::buffer data) -> image_ptr_t {
         py::buffer_info info  = data.request();
         auto format_code = EBufferFormat(fmt->hashed());
          auto img = std::make_shared<Image>();
         switch(format_code){
          case EBufferFormat::RGB8:{
            OrkAssert(info.format == py::format_descriptor<uint8_t>::format());
            int data_len = info.size;
            auto data_ptr = static_cast<uint8_t*>(info.ptr);
            OrkAssert(data_len == (w*h*3));
            img->init(w,h,3,1);
            img->_format = format_code;
            auto data_out = (void*) img->_data->data();
            memcpy_fast(data_out,data_ptr,data_len);
            //printf( "got good rgb8 bufferdata <%p>\n", data_ptr );
            break;
          }
          default:
            OrkAssert(false);

         }
        return img;
      })
      .def_property_readonly("width", [](image_ptr_t img) -> int { return img->_width; })
      .def_property_readonly("height", [](image_ptr_t img) -> int { return img->_height; })
      .def_property_readonly("depth", [](image_ptr_t img) -> int { return img->_depth; })
      .def_property_readonly("numcomponents", [](image_ptr_t img) -> int { return img->_numcomponents; })
      .def_property_readonly("bytesPerChannel", [](image_ptr_t img) -> int { return img->_bytesPerChannel; })
      .def_property_readonly("format", [](image_ptr_t img) -> int { return int(img->_format); })
      .def_property_readonly("data", [](image_ptr_t img) -> datablock_ptr_t { return img->_data; })
      .def("writeToFile", [](image_ptr_t img, const std::string& outpath) {
        img->writeToFile(file::Path(outpath));
      })
      ;
  type_codec->registerStdCodec<image_ptr_t>(image_type);      
  ///////////////////////////////////////////////////////
  auto image_provider_type = //
      py::class_<ImageProvider, image_provider_ptr_t>(module_lev2, "ImageProvider")
      .def("__repr__", [](image_provider_ptr_t ip) {
        return "<lev2.ImageProvider>";
      });
  type_codec->registerStdCodec<image_provider_ptr_t>(image_provider_type);
  ///////////////////////////////////////////////////////
  auto movieplayback_type = //
      py::class_<MoviePlaybackContext, movieplayback_ptr_t>(module_lev2, "MoviePlaybackContext")
      .def(py::init<>())
      .def("init", [](movieplayback_ptr_t ctx, const std::string& filename) {
        ctx->init(filename);
      })
      .def("play", [](movieplayback_ptr_t ctx) {
        ctx->play();
      })
      .def("pause", [](movieplayback_ptr_t ctx) {
        ctx->pause();
      })
      .def("stop", [](movieplayback_ptr_t ctx) {
        ctx->stop();
      })
      .def("restart", [](movieplayback_ptr_t ctx) {
        ctx->restart();
      })
      .def("createImageProvider", [](movieplayback_ptr_t ctx) -> image_provider_ptr_t {
        return ctx->createImageProvider();
      })
      .def_property_readonly("state", [](movieplayback_ptr_t ctx) -> crcstring_ptr_t {
        auto crc = std::make_shared<CrcString>(uint64_t(ctx->_state));
        return crc;
      })
      .def_property_readonly("fps", [](movieplayback_ptr_t ctx) -> double {
        return ctx->_fps;
      })
      .def_property_readonly("width", [](movieplayback_ptr_t ctx) -> int {
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
  ///////////////////////////////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
