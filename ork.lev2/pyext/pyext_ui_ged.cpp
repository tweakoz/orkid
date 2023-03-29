////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/ui/widget.h>
#include <ork/lev2/ui/group.h>
#include <ork/lev2/ui/surface.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/ged/ged_surface.h>
#include <ork/lev2/ui/ged/ged.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
using namespace ged;
///////////////////////////////////////////////////////////////////////////////

void pyinit_ui_ged(py::module& module_ui) {
  auto type_codec = python::TypeCodec::instance();
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
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
