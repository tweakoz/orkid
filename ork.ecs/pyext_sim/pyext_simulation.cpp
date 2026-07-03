////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/simulation.inl>
#include <ork/ecs/datatable.h>
#include <ork/event/Event.h> // ui::UpdateData == the component-script "updinfo"

/////////////////////////////////////////////////////////////////////////////////


namespace nb = obind;

namespace ork::ecssim {
using eref_ptr_t = std::shared_ptr<EntityRef>;

// a RESOLVED spawner handle (Simulation::findSpawner at link time = the only
// string lookup); spawn() through it does no name resolution. Bound BY VALUE
// (a bare shared_ptr<T> binding collides with nanobind's shared_ptr caster);
// falsy when the spawner was not found.
struct ScriptSpawner {
  Simulation* _sim = nullptr;
  spawndata_constptr_t _rec;
};

void register_simulation(nb::module_& module_ecssim,python::obind_typecodec_ptr_t type_codec) {
  using namespace ::ork::python;
  /////////////////////////////////////////////////////////////////////////////////
  auto spawner_type = clazz<nanobindadapter, ScriptSpawner>(module_ecssim, "Spawner")
      // pos overrides the spawner transform; vel/avel ride the entity varmap
      // as "initialVelocity"/"initialAngularVelocity" — the ONE velocity
      // channel (the FSM's scheduled spawns write the same linear key;
      // BulletObjectComponent consumes both at activate, which is
      // queue-deferred, so the post-spawn writes land in time). avel in
      // rad/s about the given world axis (e.g. topspin for a projectile).
      // Update-thread context (notify/update hooks). Returns the Entity.
      .def("spawn", [](const ScriptSpawner& spwn, fvec3 pos, fvec3 vel, float scale, fvec3 avel) -> pyentity_ptr_t {
        if (not spwn._rec)
          return pyentity_ptr_t(nullptr);
        auto sad         = std::make_shared<SpawnAnonDynamic>();
        sad->_overridexf = std::make_shared<DecompTransform>();
        sad->_overridexf->_translation  = pos;
        sad->_overridexf->_uniformScale = scale;
        auto ent = spwn._sim->spawnDynamic(spwn._rec, sad);
        if (ent and vel.magnitudeSquared() > 0.0f)
          ent->_varmap->makeValueForKey<fvec3>("initialVelocity") = vel;
        if (ent and avel.magnitudeSquared() > 0.0f)
          ent->_varmap->makeValueForKey<fvec3>("initialAngularVelocity") = avel;
        return pyentity_ptr_t(ent);
      }, py::arg("pos"), py::arg("vel") = fvec3(0, 0, 0), py::arg("scale") = 1.0f, py::arg("avel") = fvec3(0, 0, 0))
      .def("__bool__", [](const ScriptSpawner& spwn) -> bool { return spwn._rec != nullptr; })
      .def("__repr__", [](const ScriptSpawner& spwn) -> std::string {
        fxstring<256> fxs;
        fxs.format("ecssim::Spawner(%s)", spwn._rec ? spwn._rec->GetName().c_str() : "null");
        return fxs.c_str();
      });
  type_codec->registerStdCodec<ScriptSpawner>(spawner_type);
  /////////////////////////////////////////////////////////////////////////////////
  // updinfo — the per-frame timing handed to a PythonComponent's onUpdate
  // (onUpdate(component, updinfo)). Read-only; reuses ork::ui::UpdateData. Bound
  // BY VALUE (the ScriptSpawner pattern — a bare shared_ptr<T> binding collides
  // with nanobind's shared_ptr caster).
  auto updinfo_type = clazz<nanobindadapter, ::ork::ui::UpdateData>(module_ecssim, "UpdateInfo")
      .prop_ro("dt",      [](const ::ork::ui::UpdateData& u) -> double { return u._dt; })
      .prop_ro("abstime", [](const ::ork::ui::UpdateData& u) -> double { return u._abstime; })
      .prop_ro("counter", [](const ::ork::ui::UpdateData& u) -> int { return u._counter; })
      .def("__repr__", [](const ::ork::ui::UpdateData& u) -> std::string {
        fxstring<128> fxs;
        fxs.format("UpdateInfo(dt=%g abstime=%g counter=%d)", u._dt, u._abstime, u._counter);
        return fxs.c_str();
      });
  type_codec->registerStdCodec<::ork::ui::UpdateData>(updinfo_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto sim_type = clazz<nanobindadapter,pysim_ptr_t>(module_ecssim, "Simulation")
       .prop_ro("vars", [](pysim_ptr_t simptr) -> varmap::varmap_ptr_t { return simptr->varmap(); })
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
      })
      // resolve a scene-declared entity BY NAME (init/link-time lookup — resolve
      // once, hold the handle). Loose match; None-ish (null) when absent.
      .def("findEntityByName", [](pysim_ptr_t simptr, const std::string& name) -> pyentity_ptr_t {
        auto ent = simptr->findEntityLoose(AddPooledString(name.c_str()));
        return pyentity_ptr_t(ent);
      })
      // resolve a scene-declared spawner ONCE (init/link time — the only string
      // lookup); spawn through the returned handle at event rate. The handle is
      // FALSY when the scene declares no such spawner.
      .def("findSpawner", [](pysim_ptr_t simptr, const std::string& name) -> ScriptSpawner {
        ScriptSpawner spwn;
        spwn._sim = simptr.get();
        spwn._rec = simptr->findSpawner(name);
        return spwn;
      })
      .def("despawn", [](pysim_ptr_t simptr, pyentity_ptr_t ent) {
        if (ent.get())
          simptr->enqueueDespawnEntity(ent.get());
      });
  type_codec->registerStdCodec<pysim_ptr_t>(sim_type);
}
} // namespace ork::ecssim