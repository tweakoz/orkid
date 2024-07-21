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
