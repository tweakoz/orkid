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
using eref_ptr_t = std::shared_ptr<EntityRef>;
void register_simulation(nb::module_& module_ecssim,python::obind_typecodec_ptr_t type_codec) {
  /////////////////////////////////////////////////////////////////////////////////
  auto sim_type = nb::class_<pysim_ptr_t>(module_ecssim, "Simulation")
      ////////////////////////////////////////////////////////////////////
      // rval->_vars.makeValueForKey<nb::function>("uievfn") = uievfn;
      // rval->onUiEvent([=](ork::ui::event_constptr_t ev) -> ui::HandlerResult { //
      // return sim->findSystem<SceneGraphSystem>();
      .def("__repr__", [](pysim_ptr_t simptr) -> std::string {
        fxstring<256> fxs;
        fxs.format("ecssim::Simulation(%p)", simptr.get() );
        return fxs.c_str();
      })
      .def_prop_ro("deltaTime", [](pysim_ptr_t simptr) -> float {
        return simptr->deltaTime();
      })
      .def_prop_ro("gameTime", [](pysim_ptr_t simptr) -> float {
        return simptr->gameTime();
      })
      .def("findSystemByName", [](pysim_ptr_t simptr, const std::string& name) -> pysystem_ptr_t {
        auto sys = simptr->_findSystemFromName(name.c_str());
        auto wrapped = pysystem_ptr_t(sys);
        return wrapped;
      })
      .def("entityByID", [](pysim_ptr_t simptr, EntityRef eref) -> pyentity_ptr_t {
        auto ent = simptr->_findEntityFromRef(eref);
        auto wrapped = pyentity_ptr_t(ent);
        return wrapped;
      });
  //ype_codec->registerRawPtrCodec<sim_raw_ptr_t, Simulation*>(sim_type);
  type_codec->registerStdCodec<pysim_ptr_t>(sim_type);
}
} // namespace ork::ecssim