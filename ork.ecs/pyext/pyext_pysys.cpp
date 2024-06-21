////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/pysys/PythonComponent.h>
#include <ork/ecs/datatable.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {
void pyinit_pysys(py::module& module_ecs) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto pyc_type =
      py::class_<PythonComponentData, ComponentData, pycompdata_ptr_t>(module_ecs, "PythonComponentData")
          .def(
              "__repr__",
              [](pycompdata_ptr_t physc) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::PythonComponentData(%p)", physc.get());
                return fxs.c_str();
              });
  type_codec->registerStdCodec<pycompdata_ptr_t>(pyc_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto pysys_type = py::class_<PythonSystemData, SystemData, pysysdata_ptr_t>(module_ecs, "PythonSystemData")
                          .def(
                              "__repr__",
                              [](const pysysdata_ptr_t& sysdata) -> std::string {
                                fxstring<256> fxs;
                                fxs.format("ecs::PythonSystemData(%p)", sysdata.get());
                                return fxs.c_str();
                              });
  type_codec->registerStdCodec<pysysdata_ptr_t>(pysys_type);
  /////////////////////////////////////////////////////////////////////////////////
} // void pyinit_system(py::module& module_ecs) {
/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
