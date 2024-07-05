////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/simulation.inl>
#include <ork/ecs/datatable.h>

/////////////////////////////////////////////////////////////////////////////////


namespace py = pybind11;

namespace ork::ecssim {
using sim_raw_ptr_t = ork::python::unmanaged_ptr<::ork::ecs::Simulation>;
void register_simulation(py::module& module_ecssim,python::typecodec_ptr_t type_codec) {
  /////////////////////////////////////////////////////////////////////////////////
  auto sim_type = py::class_<sim_raw_ptr_t>(module_ecssim, "Simulation")
      ////////////////////////////////////////////////////////////////////
      // rval->_vars.makeValueForKey<py::function>("uievfn") = uievfn;
      // rval->onUiEvent([=](ork::ui::event_constptr_t ev) -> ui::HandlerResult { //
      // return sim->findSystem<SceneGraphSystem>();
      .def("__repr__", [](sim_raw_ptr_t simptr) -> std::string {
        fxstring<256> fxs;
        fxs.format("ecssim::Simulation(%p)", simptr.get() );
        return fxs.c_str();
      })
      .def_property_readonly("deltaTime", [](sim_raw_ptr_t simptr) -> float {
        return simptr->deltaTime();
      })
      .def_property_readonly("gameTime", [](sim_raw_ptr_t simptr) -> float {
        return simptr->gameTime();
      })
      .def("findSystemByName", [](sim_raw_ptr_t simptr, const std::string& name) -> pysystem_ptr_t {
        return simptr->_findSystemFromName(name.c_str());
      });
  type_codec->registerRawPtrCodec<sim_raw_ptr_t, Simulation*>(sim_type);
  //type_codec->registerStdCodec<sim_raw_ptr_t>(sim_type);
}
} // namespace ork::ecssim