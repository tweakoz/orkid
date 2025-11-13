////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/ui/sdfshape.h>
#include <ork/lev2/ui/layoutgroup.inl>

namespace ork::lev2 {

void pyinit_ui_sdfshape(py::module& uimodule) {
  auto type_codec = python::pb11_typecodec_t::instance();

  auto sdfshape_type = //
      py::class_<ui::SdfShape, ui::Widget, ui::sdfshape_ptr_t>(uimodule, "SdfShape")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::sdfshape_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name = decoded_args[0].get<std::string>();
                auto shape = std::make_shared<ui::SdfShape>(name);
                return shape;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name = decoded_args[0].get<std::string>();
                auto layoutitem = lg->makeChild<ui::SdfShape>(name);
                return layoutitem.as_shared();
              })
          .def_property(
              "shape_type",
              [](ui::sdfshape_ptr_t shape) -> crcstring_ptr_t { //
                return std::make_shared<CrcString>(shape->_shape_type);
              },
              [](ui::sdfshape_ptr_t shape, crcstring_ptr_t st) { //
                shape->_shape_type = st->hashed();
              })
          .def_property(
              "theme",
              [](ui::sdfshape_ptr_t shape) -> crcstring_ptr_t { //
                return std::make_shared<CrcString>(shape->_theme_tag);
              },
              [](ui::sdfshape_ptr_t shape, crcstring_ptr_t c) { //
                shape->_theme_tag = c->hashed();
              })
          .def_readwrite("corner_radii", &ui::SdfShape::_corner_radii)
          .def_readwrite("shape_param", &ui::SdfShape::_shape_param)
          .def_readwrite("horizontal", &ui::SdfShape::_horizontal);

  type_codec->registerStdCodec<ui::sdfshape_ptr_t>(sdfshape_type);
}

} // namespace ork::lev2
