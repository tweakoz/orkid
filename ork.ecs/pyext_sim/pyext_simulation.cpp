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


namespace nb = obind;

namespace ork::ecssim {
using sim_raw_ptr_t = ::ork::ecs::Simulation*;
void register_simulation(nb::module_& module_ecssim,python::pb11_typecodec_ptr_t type_codec) {
  /////////////////////////////////////////////////////////////////////////////////
  auto sim_type = nb::class_<::ork::ecs::Simulation>(module_ecssim, "Simulation")
      ////////////////////////////////////////////////////////////////////
      // rval->_vars.makeValueForKey<nb::function>("uievfn") = uievfn;
      // rval->onUiEvent([=](ork::ui::event_constptr_t ev) -> ui::HandlerResult { //
      // return sim->findSystem<SceneGraphSystem>();
      .def("__repr__", [](sim_raw_ptr_t simptr) -> std::string {
        fxstring<256> fxs;
        fxs.format("ecssim::Simulation(%p)", simptr );
        return fxs.c_str();
      })
      .def_prop_ro("deltaTime", [](sim_raw_ptr_t simptr) -> float {
        return simptr->deltaTime();
      })
      .def_prop_ro("gameTime", [](sim_raw_ptr_t simptr) -> float {
        return simptr->gameTime();
      })
      .def("findSystemByName", [](sim_raw_ptr_t simptr, const std::string& name) -> pysystem_ptr_t {
        return simptr->_findSystemFromName(name.c_str());
      });
  //ype_codec->registerRawPtrCodec<sim_raw_ptr_t, Simulation*>(sim_type);
  //type_codec->registerStdCodec<sim_raw_ptr_t>(sim_type);
}
} // namespace ork::ecssim