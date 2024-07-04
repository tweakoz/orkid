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

namespace ork::ecssim {
void register_simulation(py::module& module_ecssim) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto sim_type = py::class_<Simulation, simulation_ptr_t>(module_ecssim, "Simulation")
      ////////////////////////////////////////////////////////////////////
      // rval->_vars.makeValueForKey<py::function>("uievfn") = uievfn;
      // rval->onUiEvent([=](ork::ui::event_constptr_t ev) -> ui::HandlerResult { //
      // return sim->findSystem<SceneGraphSystem>();
      .def("__repr__", [](const simulation_ptr_t& sdata) -> std::string {
        fxstring<256> fxs;
        fxs.format("ecssim::Simulation(%p)", sdata.get());
        return fxs.c_str();
      });
  type_codec->registerStdCodec<simulation_ptr_t>(sim_type);
}
} // namespace ork::ecssim