////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/controller.h>
#include <ork/ecs/datatable.h>
///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {
void pyinit_controller(py::module& module_ecs) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<Controller, controller_ptr_t>(module_ecs, "Controller")
      .def(py::init<>())
      .def(
          "__repr__",
          [](controller_ptr_t arch) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::Controller(%p)", (void*)arch.get());
            return fxs.c_str();
          })
      .def("bindScene", [](controller_ptr_t ctrl, scenedata_ptr_t scenedata) { ctrl->bindScene(scenedata); })
      .def("createSimulation", [](controller_ptr_t ctrl) { ctrl->createSimulation(); })
      .def("startSimulation", [](controller_ptr_t ctrl) { ctrl->startSimulation(); })
      .def("stopSimulation", [](controller_ptr_t ctrl) { ctrl->stopSimulation(); })
      .def("updateSimulation", [](controller_ptr_t ctrl) { ctrl->update(); })
      .def("beginWriteTrace", [](controller_ptr_t ctrl, std::string outpath) { //
          ctrl->beginWriteTrace(outpath);
       })
      ///////////////////////////
      .def("renderSimulation", [](controller_ptr_t ctrl,uidrawevent_ptr_t drawev) {
         ctrl->render(drawev);
       })
      ///////////////////////////
      .def("installRenderCallbackOnEzApp", [](controller_ptr_t ctrl,lev2::orkezapp_ptr_t ezapp) {
         ctrl->installRenderCallbackOnEzApp(ezapp);
       })
      ///////////////////////////
      //
      .def("findSystem", [](controller_ptr_t ctrl, std::string name) -> sys_ref_t { return ctrl->findSystemWithClassName(name); })
      .def(
          "systemNotify",
          [type_codec](
              controller_ptr_t ctrl, //
              sys_ref_t sys,         //
              crcstring_ptr_t evID,
              py::object evdata) {

            evdata_t decoded;
            if (py::isinstance<py::dict>(evdata)){
              auto as_dict = evdata.cast<py::dict>();
              auto& dtab = decoded.make<DataTable>();
              DataKey dkey;
              for (auto item : as_dict) {
                auto key = py::cast<crcstring_ptr_t>(item.first);
                auto val = py::reinterpret_borrow<py::object>(item.second);
                auto var_val = type_codec->decode64(val);
                dkey._encoded = *key;
                dtab[dkey] = var_val;
              }
            }
            else{
              decoded = type_codec->decode64(evdata);
            }

            ctrl->systemNotify(sys, *evID, decoded);
          })
      .def(
          "systemRequest",
          [type_codec](
              controller_ptr_t ctrl, //
              sys_ref_t sys,         //
              crcstring_ptr_t evID,
              py::object evdata) -> response_ref_t {
            evdata_t decoded;
            if (evdata.is_none())
              decoded = nullptr;
            else {
              decoded = type_codec->decode64(evdata);
            }
            response_ref_t rval = ctrl->systemRequest(sys, *evID, decoded);
            return rval;
          })
      .def(
          "spawnEntity",
          [type_codec](
              controller_ptr_t ctrl, //
              const SpawnAnonDynamic& sad) -> ent_ref_t {
            ent_ref_t eref = ctrl->spawnAnonDynamicEntity(sad);
            return eref;
          });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<SystemRef>(module_ecs, "SystemRef").def("__repr__", [](const sys_ref_t& sys) -> std::string {
    fxstring<256> fxs;
    fxs.format("ecs::SystemRef id(0x%zx)", sys._sysID);
    return fxs.c_str();
  });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<EntityRef>(module_ecs, "EntityRef").def("__repr__", [](const ent_ref_t& sys) -> std::string {
    fxstring<256> fxs;
    fxs.format("ecs::EntityRef id(0x%zx)", sys._entID);
    return fxs.c_str();
  });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<ComponentRef>(module_ecs, "ComponentRef").def("__repr__", [](const comp_ref_t& sys) -> std::string {
    fxstring<256> fxs;
    fxs.format("ecs::ComponentRef id(0x%zx)", sys._compID);
    return fxs.c_str();
  });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<ResponseRef>(module_ecs, "ResponseRef").def("__repr__", [](const response_ref_t& sys) -> std::string {
    fxstring<256> fxs;
    fxs.format("ecs::ResponseRef id(0x%zx)", sys._responseID);
    return fxs.c_str();
  });
  /////////////////////////////////////////////////////////////////////////////////
} // void pyinit_archetype(py::module& module_ecs) {
/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs