////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/simulation.inl>
#include <ork/ecs/datatable.h>

///////////////////////////////////////////////////////////////////////////////
using ctx_t = ork::python::unmanaged_ptr<::ork::lev2::Context>;
///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {
void pyinit_simulation(py::module& module_ecs) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto sim_type = py::class_<Simulation, simulation_ptr_t>(module_ecs, "Simulation")
      //.def(py::init([](scenedata_ptr_t scene, pyapplication_ptr_t app){
      // return std::make_shared<Simulation>(scene,app.get());
      //}))
      .def(
          "update",
          [](simulation_ptr_t sim) {
            OrkAssert(false);
            // need controller bindings !
            // sim->updateThreadTick();
          })
      .def("render", [](simulation_ptr_t sim, ui::drawevent_constptr_t drwev) { sim->render(drwev); })
      .def("sceneGraphSystem", [](simulation_ptr_t sim) -> pysgsystem_ptr_t { return pysgsystem_ptr_t(sim->findSystem<SceneGraphSystem>()); })
      /////////////////////////////////////////////////////////////////////////////////
      // SceneGraphSystem convenience accessors (avoid returning unmanaged_ptr to Python)
      /////////////////////////////////////////////////////////////////////////////////
      .def_property_readonly("sceneGraphScene", [](simulation_ptr_t sim) -> lev2::scenegraph::scene_ptr_t {
        auto sg_sys = sim->findSystem<SceneGraphSystem>();
        return sg_sys ? sg_sys->_scene : nullptr;
      })
      .def_property_readonly("sceneGraphDefaultLayer", [](simulation_ptr_t sim) -> lev2::scenegraph::layer_ptr_t {
        auto sg_sys = sim->findSystem<SceneGraphSystem>();
        return sg_sys ? sg_sys->_default_layer : nullptr;
      })
      .def("sceneGraphInitForEditMode", [](simulation_ptr_t sim, ctx_t ctx) {
        auto sg_sys = sim->findSystem<SceneGraphSystem>();
        if (sg_sys) sg_sys->initializeForEditMode(ctx.get());
      })
      .def("sceneGraphProcessRenderOps", [](simulation_ptr_t sim) {
        auto sg_sys = sim->findSystem<SceneGraphSystem>();
        if (sg_sys) sg_sys->processRenderOps();
      })
      .def("sceneGraphReloadDrawableData", [](simulation_ptr_t sim, lev2::drawabledata_ptr_t data) {
        auto sg_sys = sim->findSystem<SceneGraphSystem>();
        if (sg_sys) sg_sys->reloadDrawableData(data);
      })
      /////////////////////////////////////////////////////////////////////////////////
      .def(
          "start",
          [](simulation_ptr_t sim) {
            // sim->SetSimulationMode(ecs::ESimulationMode::EDIT);
            // sim->SetSimulationMode(ecs::ESimulationMode::RUN);
          })
      .def(
          "onLink",
          [](simulation_ptr_t sim, py::object onlink_lambda) {
            sim->setOnLinkLambda([=]() {
              py::gil_scoped_acquire acquire;
              auto as_fn = py::cast<py::function>(onlink_lambda);
              as_fn();
            });
          })
      ////////////////////////////////////////////////////////////////////
      // rval->_vars.makeValueForKey<py::function>("uievfn") = uievfn;
      // rval->onUiEvent([=](ork::ui::event_constptr_t ev) -> ui::HandlerResult { //
      // return sim->findSystem<SceneGraphSystem>();
      .def("findEntityByRef", [](simulation_ptr_t sim, uint64_t ref_id) -> py::object {
        EntityRef eref;
        eref._entID = ref_id;
        auto ent = sim->_findEntityFromRef(eref);
        if (ent) {
          return py::cast(pyentity_ptr_t(ent));
        }
        return py::none();
      })
      .def("__repr__", [](const simulation_ptr_t& sdata) -> std::string {
        fxstring<256> fxs;
        fxs.format("ecs::Simulation(%p)", sdata.get());
        return fxs.c_str();
      });
  type_codec->registerStdCodec<simulation_ptr_t>(sim_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto sad_type = py::class_<SpawnAnonDynamic, sad_ptr_t>(module_ecs, "SpawnAnonDynamic")
                    .def(py::init([](std::string name) -> sad_ptr_t {
                      auto rval         = std::make_shared<SpawnAnonDynamic>();
                      rval->_edataname  = AddPooledString(name.c_str());
                      rval->_overridexf = std::make_shared<DecompTransform>();
                      return rval;
                    }))
                    .def_property_readonly("name", [](sad_ptr_t self) { return self->_edataname; })
                    .def_property_readonly("overridexf", [](sad_ptr_t self) { return self->_overridexf; })
                    .def_property(
                        "table",
                        [](sad_ptr_t self) -> datatable_ptr_t { return self->_table; },
                        [](sad_ptr_t self, datatable_ptr_t table) { self->_table = table; });
  type_codec->registerStdCodec<sad_ptr_t>(sad_type);
  /////////////////////////////////////////////////////////////////////////////////
} // void pyinit_simulation(py::module& module_ecs) {
/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs