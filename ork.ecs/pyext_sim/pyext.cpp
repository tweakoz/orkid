////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/ui/event.h>
#include <ork/ecs/ecs.h>
#include <ork/profiling.inl>
#include <iostream>

///////////////////////////////////////////////////////////////////////////////

namespace ork {
namespace python {
  void init_math(py::module& module_ecssim,python::typecodec_ptr_t type_codec);
}
}

namespace ork::ecssim {

void register_simulation(py::module& module_ecssim);

} // namespace ork::ecs

////////////////////////////////////////////////////////////////////////////////

PYBIND11_MODULE(_ecssim, module_ecssim) {
  auto type_codec = ork::ecssim::simonly_codec_instance();
  //module_ecs.attr("__name__") = "ecs";
  //////////////////////////////////////////////////////////////////////////////
  module_ecssim.doc() = "Orkid Ecs Internal (Simulation only) Library";
  //////////////////////////////////////////////////////////////////////////////
  //pyinit_entity(module_ecs);
  //pyinit_component(module_ecs);
  //pyinit_system(module_ecs);
  ::ork::ecssim::register_simulation(module_ecssim);
  //::ork::python::init_math(module_ecssim, type_codec);


  //pyinit_pysys(module_ecs);
  //////////////////////////////////////////////////////////////////////////////
}
