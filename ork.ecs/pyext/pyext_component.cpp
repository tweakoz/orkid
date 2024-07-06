////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {
void pyinit_component(py::module& module_ecs) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<ComponentData,componentdata_ptr_t>(module_ecs, "ComponentData", py::module_local())
      .def(
          "__repr__",
          [](componentdata_ptr_t cdata) -> std::string {
            fxstring<256> fxs;
            auto clazz = cdata->objectClass();
            fxs.format("ecs::ComponentData(%p) class<%s>", cdata.get(), clazz->Name().c_str());
            return fxs.c_str();
          });
  /////////////////////////////////////////////////////////////////////////////////
} // void pyinit_component(py::module& module_ecs) {
/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs {