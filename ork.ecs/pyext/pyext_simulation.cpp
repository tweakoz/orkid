////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {
void pyinit_simulation(py::module& module_ecs) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<Simulation,simulation_ptr_t>(module_ecs, "Simulation")
      //.def(py::init([](scenedata_ptr_t scene, pyapplication_ptr_t app){
        //return std::make_shared<Simulation>(scene,app.get());
      //}))
      .def("update", [](simulation_ptr_t sim){
        OrkAssert(false);
        // need controller bindings !
        //sim->updateThreadTick();
      })
      .def("render", [](simulation_ptr_t sim,ui::drawevent_constptr_t drwev){
        sim->render(drwev);
      })
      .def("sceneGraphSystem", [](simulation_ptr_t sim)->pysgsystem_ptr_t{
        return sim->findSystem<SceneGraphSystem>();
      })
      .def("start", [](simulation_ptr_t sim) {
        //sim->SetSimulationMode(ecs::ESimulationMode::EDIT);
        //sim->SetSimulationMode(ecs::ESimulationMode::RUN);
      })
      .def("onLink", [](simulation_ptr_t sim, py::object onlink_lambda) {

        sim->setOnLinkLambda([=](){
          py::gil_scoped_acquire acquire;
          auto as_fn = py::cast<py::function>(onlink_lambda);
          as_fn();
        });

          })
        ////////////////////////////////////////////////////////////////////
          //rval->_vars.makeValueForKey<py::function>("uievfn") = uievfn;
          //rval->onUiEvent([=](ork::ui::event_constptr_t ev) -> ui::HandlerResult { //
          //return sim->findSystem<SceneGraphSystem>();
      .def(
          "__repr__",
          [](const simulation_ptr_t& sdata) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::Simulation(%p)", sdata.get());
            return fxs.c_str();
          });
  /////////////////////////////////////////////////////////////////////////////////
} // void pyinit_simulation(py::module& module_ecs) {
/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs {