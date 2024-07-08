////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/ui/widget.h>
#include <ork/lev2/ui/group.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/ui/surface.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/ged/ged_surface.h>
#include <ork/lev2/ui/ged/ged.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
using namespace ged;
///////////////////////////////////////////////////////////////////////////////

void pyinit_ui_ged(py::module& module_ui) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto objmodel_type = //
      py::class_<ObjModel, objectmodel_ptr_t>(module_ui, "ObjModel")
          .def(py::init<>())
          .def("attach", [](objectmodel_ptr_t mdl, 
                            object_ptr_t obj_to_attach,
                            bool clear_stack) { //
            return mdl->attach(obj_to_attach,clear_stack);
            });
  type_codec->registerStdCodec<objectmodel_ptr_t>(objmodel_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto gedsurace_type = //
      py::class_<GedSurface, ui::Surface, gedsurface_ptr_t>(module_ui, "GedSurface")
        .def_static(
            "uifactory", [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
              auto decoded_args = type_codec->decodeList(py_args);
              auto name         = decoded_args[0].get<std::string>();
              auto model        = decoded_args[1].get<objectmodel_ptr_t>();
              return lg->makeChild2<GedSurface>(name, model);
            })
        .def(py::init<std::string,objectmodel_ptr_t>());
  type_codec->registerStdCodec<gedsurface_ptr_t>(gedsurace_type);}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
