////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/controller.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {
void pyinit_controller(py::module& module_ecs) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<Controller,controller_ptr_t>(module_ecs, "Controller")
      .def(py::init<>())
      .def(
          "__repr__",
          [](controller_ptr_t arch) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::Controller(%p)", (void*) arch.get());
            return fxs.c_str();
          })
     .def("bindScene", [](controller_ptr_t ctrl, scenedata_ptr_t scenedata) {
         ctrl->bindScene(scenedata);
       })
    .def( "createSimulation", [](controller_ptr_t ctrl) {
        ctrl->createSimulation();
      })
    .def("startSimulation", [](controller_ptr_t ctrl) {
        ctrl->startSimulation();
      })
      .def("stopSimulation", [](controller_ptr_t ctrl) {
        ctrl->stopSimulation();
      })
      .def("updateSimulation", [](controller_ptr_t ctrl) {
        ctrl->update();
      })
      ///////////////////////////
      //
      .def("findSystem", [](controller_ptr_t ctrl, std::string name) -> sys_ref_t {
        return ctrl->findSystemWithClassName(name);
      })
      .def("systemNotify", [type_codec](controller_ptr_t ctrl, //
                              sys_ref_t sys, //
                              crcstring_ptr_t evID,
                              py::object evdata) {
        evdata_t decoded = type_codec->decode64(evdata);
        
        ctrl->systemNotify(sys,*evID,decoded);
      })
      ;
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<SystemRef>(module_ecs, "SystemRef")
        .def(
            "__repr__",
            [](const sys_ref_t& sys) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::SystemRef id(0x%zx)", sys._sysID);
                return fxs.c_str();
            });
  /////////////////////////////////////////////////////////////////////////////////
} // void pyinit_archetype(py::module& module_ecs) {
/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs {