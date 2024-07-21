////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/gfx/image.h>
#include <iostream>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

void pyinit_gfx_image(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();
  auto image_type = //
      py::class_<Image, image_ptr_t>(module_lev2, "Image")
      .def_static("createFromFile", [](const std::string& inpath) -> image_ptr_t {
        auto datablock = ::ork::File::loadDatablock(inpath);
        auto img = std::make_shared<Image>();
        img->initFromDataBlock(datablock);
        return img;
      })
      .def_static("createRGB8FromColor", [](int w, int h, fvec3 color) -> image_ptr_t {
        auto img = std::make_shared<Image>();
        img->initRGB8WithColor(w,h,color,EBufferFormat::RGB8);
        return img;
      })
      .def_static("createRGBA8FromColor", [](int w, int h, fvec4 color) -> image_ptr_t {
        auto img = std::make_shared<Image>();
        img->initRGBA8WithColor(w,h,color,EBufferFormat::RGBA8);
        return img;
      })
      .def_static("createImageFromBuffer", [](int w, int h, crcstring_ptr_t fmt, py::buffer data) -> image_ptr_t {
         py::buffer_info info  = data.request();
         auto format_code = EBufferFormat(fmt->hashed());
        //img->initRGBA8WithColor(w,h,color,EBufferFormat::RGBA8);
         size_t bytes_per_item = 0;
         if (info.format == py::format_descriptor<uint8_t>::format()) {
           //bytes_per_item = 1;
         } else if (info.format == py::format_descriptor<int>::format()) {
           //bytes_per_item = sizeof(int);
         } else if (info.format == py::format_descriptor<long>::format()) {
           //bytes_per_item = sizeof(long);
         } else if (info.format == py::format_descriptor<float>::format()) {
           int num_components = info.size;
           printf( "float :: num_components<%d>\n", num_components );
           //bytes_per_item = sizeof(float);
         } else if (info.format == py::format_descriptor<double>::format()) {
           //bytes_per_item = sizeof(double);
         }
        auto img = std::make_shared<Image>();
        return img;
      })
      .def_property_readonly("width", [](image_ptr_t img) -> int { return img->_width; })
      .def_property_readonly("height", [](image_ptr_t img) -> int { return img->_height; })
      .def_property_readonly("depth", [](image_ptr_t img) -> int { return img->_depth; })
      .def_property_readonly("numcomponents", [](image_ptr_t img) -> int { return img->_numcomponents; })
      .def_property_readonly("bytesPerChannel", [](image_ptr_t img) -> int { return img->_bytesPerChannel; })
      .def_property_readonly("format", [](image_ptr_t img) -> int { return int(img->_format); })
      .def_property_readonly("data", [](image_ptr_t img) -> datablock_ptr_t { return img->_data; })
      ;
  type_codec->registerStdCodec<image_ptr_t>(image_type);      

}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
