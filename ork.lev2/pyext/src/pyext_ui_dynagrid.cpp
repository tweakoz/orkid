////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/ui/dynagrid.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_ui_dynagrid(py::module& uimodule) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  auto dynagrid_type = //
      py::class_<ui::DynaGrid, ui::Group, ui::dynagrid_ptr_t>(uimodule, "DynaGrid")
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto layoutitem   = lg->makeChild<ui::DynaGrid>(name);
                return layoutitem.as_shared();
              })
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::dynagrid_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto grid         = std::make_shared<ui::DynaGrid>(name);
                return grid;
              })
          .def(
              "makeChild",
              [](ui::dynagrid_ptr_t grid, py::kwargs kwargs) -> ui::widget_ptr_t { //
                ui::widget_ptr_t rval;
                if (kwargs) {
                  py::list args;
                  py::object wfactory;
                  int args_parsed = 0;
                  for (auto item : kwargs) {
                    auto key = py::cast<std::string>(item.first);
                    if (key == "uiclass") {
                      auto uiclass_obj  = py::cast<py::object>(item.second);
                      bool has_wfactory = py::hasattr(uiclass_obj, "wfactory");
                      OrkAssert(has_wfactory);
                      wfactory = uiclass_obj.attr("wfactory");
                      args_parsed++;
                    } else if (key == "args") {
                      args = py::cast<py::list>(item.second);
                      args_parsed++;
                    }
                  }
                  OrkAssert(args_parsed == 2);
                  rval = py::cast<ui::widget_ptr_t>(wfactory(args));
                  grid->addChild(rval);
                }
                return rval;
              })
          .def_property(
              "margin",
              [](ui::dynagrid_ptr_t grid) -> int { //
                return grid->margin();
              },
              [](ui::dynagrid_ptr_t grid, int m) { //
                grid->setMargin(m);
              })
          .def_readwrite("aspect_min", &ui::DynaGrid::_aspect_min)
          .def_readwrite("aspect_max", &ui::DynaGrid::_aspect_max);
  type_codec->registerStdCodec<ui::dynagrid_ptr_t>(dynagrid_type);
}

} // namespace ork::lev2
