////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/controller.h>
#include <ork/ecs/simulation.h>
#include <ork/ecs/datatable.h>
#include <ork/ecs/scene.h>
///////////////////////////////////////////////////////////////////////////////
using ctx_t               = ork::python::unmanaged_ptr<::ork::lev2::Context>;
///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {

struct SystemPropertyProxy {
  controller_ptr_t _ctrl;
  sys_ref_t _sysref;
};

struct SystemHandle {
  controller_ptr_t _ctrl;
  sys_ref_t _sysref;
};

void pyinit_controller(py::module& module_ecs) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto ctrl_type = py::class_<Controller, controller_ptr_t>(module_ecs, "Controller")
      .def(py::init<>())
      .def(
          "__repr__",
          [](controller_ptr_t arch) -> std::string {
            fxstring<256> fxs;
            fxs.format("ecs::Controller(%p)", (void*)arch.get());
            return fxs.c_str();
          })
      .def("gpuInit", [](controller_ptr_t ctrl, ctx_t ctx) { ctrl->gpuInit(ctx.get()); })
      .def("gpuExit", [](controller_ptr_t ctrl, ctx_t ctx) { ctrl->gpuExit(ctx.get()); })
      .def("bindScene", [](controller_ptr_t ctrl, scenedata_ptr_t scenedata) { ctrl->bindScene(scenedata); })
      .def("createSimulation", [type_codec](controller_ptr_t ctrl, py::kwargs kwargs) {
            varmap::varmap_ptr_t varmap;
            if (kwargs.size() > 0) {
              varmap = std::make_shared<varmap::VarMap>();
              for (auto& [key, value] : kwargs) {
                auto key_str = key.cast<std::string>();
                auto val_obj = py::reinterpret_borrow<py::object>(value);
                auto val_decoded = type_codec->decode(val_obj);
                varmap->setValueForKey(key_str, val_decoded);
              }
            }
            ctrl->createSimulation(varmap);
          })
      .def("stageSimulation", [](controller_ptr_t ctrl) {
        py::gil_scoped_release release;
        ctrl->stageSimulation();
      })
      .def("startSimulation", [](controller_ptr_t ctrl) {
        py::gil_scoped_release release;
        ctrl->startSimulation();
      })
      .def("stopSimulation", [](controller_ptr_t ctrl) {
        // GIL must be RELEASED across every transport op: they block on sim/system
        // locks the update thread holds while it needs the GIL (PythonSystem tick)
        // — holding it here deadlocked a live slider-drag rebuild (2026-07-21).
        py::gil_scoped_release release;
        ctrl->stopSimulation();
      })
      // CLOCK ops, not teardown: the sim mode goes ACTIVE<->PAUSE, events keep
      // servicing and the renderer keeps drawing. Same GIL rule as above — the
      // transport lock is held by the update thread while it needs the GIL.
      .def("pauseSimulation", [](controller_ptr_t ctrl) {
        py::gil_scoped_release release;
        ctrl->pauseSimulation();
      })
      .def("resumeSimulation", [](controller_ptr_t ctrl) {
        py::gil_scoped_release release;
        ctrl->resumeSimulation();
      })
      .def_property_readonly("simulation", [](controller_ptr_t ctrl) -> simulation_ptr_t {
        return ctrl->simulation();
      })
      .def("terminateSimulation", [](controller_ptr_t ctrl) { //
        py::gil_scoped_release release; //
        ctrl->endSimulation();
       })
      .def("updateSimulation", [](controller_ptr_t ctrl) { //
        py::gil_scoped_release release; //
        ctrl->update(); //
        })
      .def("beginWriteTrace", [](controller_ptr_t ctrl, std::string outpath) { //
          ctrl->beginWriteTrace(outpath);
       })
      ///////////////////////////
      .def("renderSimulation", [](controller_ptr_t ctrl,uidrawevent_ptr_t drawev) {
         ctrl->render(drawev);
       })
      .def("gpuRender", [](controller_ptr_t ctrl, ctx_t ctx) {
         ctrl->gpuRender(ctx.get());
       })
      .def("gpuUpdate", [](controller_ptr_t ctrl, ctx_t ctx) {
         ctrl->gpuUpdate(ctx.get());
       })
      .def("updateWithGpu", [](controller_ptr_t ctrl, ctx_t ctx) {
         ctrl->updateWithGpu(ctx.get());
       })
      ///////////////////////////
      .def("installRenderCallbackOnEzApp", [](controller_ptr_t ctrl,lev2::orkezapp_ptr_t ezapp) {
         py::gil_scoped_release release; //
         ctrl->installRenderCallbackOnEzApp(ezapp);
       })
      ///////////////////////////
      .def("installUpdateCallbackOnEzApp", [](controller_ptr_t ctrl,lev2::orkezapp_ptr_t ezapp) {
         py::gil_scoped_release release; //
         ctrl->installUpdateCallbackOnEzApp(ezapp);
       })
      ///////////////////////////
      .def("installGpuUpdateCallbackOnEzApp", [](controller_ptr_t ctrl,lev2::orkezapp_ptr_t ezapp) {
         py::gil_scoped_release release; //
         ctrl->installGpuUpdateCallbackOnEzApp(ezapp);
       })
      ///////////////////////////
      .def("uninstallRenderCallbackOnEzApp", [](controller_ptr_t ctrl,lev2::orkezapp_ptr_t ezapp) {
         py::gil_scoped_release release; //
         ctrl->uninstallRenderCallbackOnEzApp(ezapp);
       })
      ///////////////////////////
      .def("uninstallUpdateCallbackOnEzApp", [](controller_ptr_t ctrl,lev2::orkezapp_ptr_t ezapp) {
         py::gil_scoped_release release; //
         ctrl->uninstallUpdateCallbackOnEzApp(ezapp);
       })
      ///////////////////////////
      .def("uninstallGpuUpdateCallbackOnEzApp", [](controller_ptr_t ctrl,lev2::orkezapp_ptr_t ezapp) {
         py::gil_scoped_release release; //
         ctrl->uninstallGpuUpdateCallbackOnEzApp(ezapp);
       })
      ///////////////////////////
      .def("entBarrier", [](controller_ptr_t ctrl, ent_ref_t eref){
        ctrl->entBarrier(eref);
      })
      ///////////////////////////
      .def(
          "componentNotify",
          [type_codec](
              controller_ptr_t ctrl, //
              comp_ref_t comp,         //
              crcstring_ptr_t evID,
              py::object evdata) {

            evdata_t decoded;
            if (py::isinstance<py::dict>(evdata)){
              auto as_dict = evdata.cast<py::dict>();
              auto dtab = decoded.makeShared<DataTable>();
              DataKey dkey;
              for (auto item : as_dict) {
                auto key = py::cast<crcstring_ptr_t>(item.first);
                auto val = py::reinterpret_borrow<py::object>(item.second);
                auto var_val = type_codec->decode64(val);
                dkey._encoded = *key;
                (*dtab)[dkey] = var_val;
              }
            }
            else{
              decoded = type_codec->decode64(evdata);
            }

            ctrl->componentNotify(comp, *evID, decoded);
          })
                ///////////////////////////
      //
      .def("findSystem", [](controller_ptr_t ctrl, std::string name) -> sys_ref_t {
        return ctrl->findSystemWithClassName(name);
      })
      .def("findSystemHandle", [](controller_ptr_t ctrl, std::string name) -> SystemHandle {
        return SystemHandle{ctrl, ctrl->findSystemWithClassName(name)};
      })
      .def(
          "systemNotify",
          [type_codec](
              controller_ptr_t ctrl, //
              const SystemHandle& sys, //
              crcstring_ptr_t evID,
              py::object evdata) {

            evdata_t decoded;
            if (py::isinstance<py::dict>(evdata)){
              auto as_dict = evdata.cast<py::dict>();
              auto dtab = decoded.makeShared<DataTable>();
              DataKey dkey;
              for (auto item : as_dict) {
                auto key = py::cast<crcstring_ptr_t>(item.first);
                auto val = py::reinterpret_borrow<py::object>(item.second);
                auto var_val = type_codec->decode64(val);
                dkey._encoded = *key;
                (*dtab)[dkey] = var_val;
              }
            }
            else{
              decoded = type_codec->decode64(evdata);
            }

            ctrl->systemNotify(sys._sysref, *evID, decoded);
          })
      .def(
          "systemNotify",
          [type_codec](
              controller_ptr_t ctrl, //
              sys_ref_t sys, //
              crcstring_ptr_t evID,
              py::object evdata) {

            evdata_t decoded;
            if (py::isinstance<py::dict>(evdata)){
              auto as_dict = evdata.cast<py::dict>();
              auto dtab = decoded.makeShared<DataTable>();
              DataKey dkey;
              for (auto item : as_dict) {
                auto key = py::cast<crcstring_ptr_t>(item.first);
                auto val = py::reinterpret_borrow<py::object>(item.second);
                auto var_val = type_codec->decode64(val);
                dkey._encoded = *key;
                (*dtab)[dkey] = var_val;
              }
            }
            else{
              decoded = type_codec->decode64(evdata);
            }

            ctrl->systemNotify(sys, *evID, decoded);
          })
      .def(
          "notifyAllSystems",
          [type_codec](
              controller_ptr_t ctrl,
              crcstring_ptr_t evID,
              py::object evdata) {
            svar64_t decoded;
            if (py::isinstance<py::dict>(evdata)) {
              auto table = std::make_shared<DataTable>();
              for (auto item : py::cast<py::dict>(evdata)) {
                auto key = item.first.cast<crcstring_ptr_t>();
                auto& slot = (*table)[*key];
                auto val = py::reinterpret_borrow<py::object>(item.second);
                slot = type_codec->decode64(val);
              }
              decoded.setShared<DataTable>(table);
            } else {
              decoded = type_codec->decode64(evdata);
            }
            ctrl->notifyAllSystems(*evID, decoded);
          })
      .def(
          "systemRequest",
          [type_codec](
              controller_ptr_t ctrl, //
              const SystemHandle& sys, //
              crcstring_ptr_t evID,
              py::object evdata) -> response_ref_t {
            evdata_t decoded;
            if (evdata.is_none())
              decoded = nullptr;
            else {
              decoded = type_codec->decode64(evdata);
            }
            response_ref_t rval = ctrl->systemRequest(sys._sysref, *evID, decoded);
            return rval;
          })
      .def(
          "systemRequest",
          [type_codec](
              controller_ptr_t ctrl, //
              sys_ref_t sys, //
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
          "systemRequestWithCallback",
          [type_codec](
              controller_ptr_t ctrl,
              const SystemHandle& sys,
              crcstring_ptr_t evID,
              py::object evdata,
              py::object pycallback) -> response_ref_t {
            evdata_t decoded;
            if (evdata.is_none())
              decoded = nullptr;
            else {
              decoded = type_codec->decode64(evdata);
            }
            void_lambda_t callback = nullptr;
            if (!pycallback.is_none()) {
              auto pyfn = std::make_shared<py::function>(pycallback.cast<py::function>());
              callback = [pyfn, type_codec]() {
                py::gil_scoped_acquire acquire;
                try {
                  (*pyfn)();
                } catch (py::error_already_set& e) {
                  printf("\npython exception in systemRequestWithCallback\n");
                  e.restore();
                  PyErr_Print();
                }
              };
            }
            response_ref_t rval = ctrl->systemRequest(sys._sysref, *evID, decoded, callback);
            return rval;
          })
      .def(
          "spawnEntity",
          [type_codec](
              controller_ptr_t ctrl, //
              sad_ptr_t sad) -> ent_ref_t {
            ent_ref_t eref = ctrl->spawnAnonDynamicEntity(sad);
            return eref;
          })
          .def("despawnEntity", [](controller_ptr_t ctrl, ent_ref_t eref) { //
            ctrl->despawnEntity(eref);
          })
      .def("findComponent", [](controller_ptr_t ctrl, ent_ref_t eref, std::string name) -> comp_ref_t { //
        return ctrl->findComponentWithClassName(eref,name); //
      })
      .def("realtimeDelayedOperation", [](controller_ptr_t ctrl, float delay, py::function fn) { //
        auto L = [fn](){
          py::gil_scoped_acquire acquire;
          fn();
        };
        ctrl->realtimeDelayedOperation(delay,L);
      })
      ///////////////////////////
      // State change callback hooks
      ///////////////////////////

// Macro for update-thread hooks: controller.onUpdXxx(fn) where fn(sim)
// PYNAME is the Python method name (e.g. onUpdPreCompose)
// C++ member is _PYNAME (e.g. _onUpdPreCompose)
#define DEF_UPD_HOOK(PYNAME) \
      .def(#PYNAME, [](controller_ptr_t ctrl, py::function fn) { \
        auto pyfn = std::make_shared<py::function>(fn); \
        ctrl->_##PYNAME.push_back([pyfn, ctrl](Simulation* sim) { \
            py::gil_scoped_acquire acquire; \
            try { \
                auto sim_ptr = ctrl->_simulation._unprotected_ref(); \
                (*pyfn)(sim_ptr); \
            } catch (py::error_already_set& e) { \
                printf("\npython exception in " #PYNAME "\n"); \
                e.restore(); PyErr_Print(); OrkAssert(false); \
            } \
        }); \
      })

// Macro for GPU-thread hooks: controller.onGpuXxx(fn) where fn(sim, ctx)
#define DEF_GPU_HOOK(PYNAME) \
      .def(#PYNAME, [](controller_ptr_t ctrl, py::function fn) { \
        auto pyfn = std::make_shared<py::function>(fn); \
        ctrl->_##PYNAME.push_back([pyfn, ctrl](Simulation* sim, lev2::Context* ctx) { \
            py::gil_scoped_acquire acquire; \
            try { \
                auto sim_ptr = ctrl->_simulation._unprotected_ref(); \
                (*pyfn)(sim_ptr, ctx_t(ctx)); \
            } catch (py::error_already_set& e) { \
                printf("\npython exception in " #PYNAME "\n"); \
                e.restore(); PyErr_Print(); OrkAssert(false); \
            } \
        }); \
      })

      // Update thread hooks
      DEF_UPD_HOOK(onUpdPreCompose)
      DEF_UPD_HOOK(onUpdPostCompose)
      DEF_UPD_HOOK(onUpdPreLink)
      DEF_UPD_HOOK(onUpdPostLink)
      DEF_UPD_HOOK(onUpdPreStage)
      DEF_UPD_HOOK(onUpdPostStage)
      DEF_UPD_HOOK(onUpdPreActivate)
      DEF_UPD_HOOK(onUpdPostActivate)
      DEF_UPD_HOOK(onUpdPreDeactivate)
      DEF_UPD_HOOK(onUpdPostDeactivate)
      DEF_UPD_HOOK(onUpdPreUnstage)
      DEF_UPD_HOOK(onUpdPostUnstage)
      // GPU thread hooks
      DEF_GPU_HOOK(onGpuPostInit)
      DEF_GPU_HOOK(onGpuPostLink)
      // Clear all
      .def("clearStateCallbacks", [](controller_ptr_t ctrl) {
        ctrl->clearStateCallbacks();
      })

#undef DEF_UPD_HOOK
#undef DEF_GPU_HOOK

      ///////////////////////////
      // Import system
      ///////////////////////////
      .def_property_readonly("importedScenes", [](controller_ptr_t ctrl) -> py::dict {
        py::dict result;
        for (auto& [ns, scene] : ctrl->importedScenes()) {
          result[py::cast(ns)] = std::const_pointer_cast<SceneData>(scene);
        }
        return result;
      })
      .def("findImportedScene", [](controller_ptr_t ctrl, std::string ns) -> scenedata_ptr_t {
        auto scene = ctrl->findImportedScene(ns);
        return scene ? std::const_pointer_cast<SceneData>(scene) : nullptr;
      })
      ;

  type_codec->registerStdCodec<controller_ptr_t>(ctrl_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto sref_t = py::class_<SystemRef>(module_ecs, "SystemRef").def("__repr__", [](const sys_ref_t& sys) -> std::string {
    fxstring<256> fxs;
    fxs.format("ecs::SystemRef id(0x%zx)", sys._sysID);
    return fxs.c_str();
  });
  type_codec->registerStdCodec<SystemRef>(sref_t);
  /////////////////////////////////////////////////////////////////////////////////
  // SystemPropertyProxy: write-only proxy — __setattr__ sends SetProperty event
  /////////////////////////////////////////////////////////////////////////////////
  auto sysprop_type = py::class_<SystemPropertyProxy>(module_ecs, "SystemPropertyProxy")
      .def("__setattr__", [type_codec](SystemPropertyProxy& self, const std::string& name, py::object value) {
        evdata_t evdata;
        auto& dtab = *evdata.makeShared<DataTable>();
        CrcString name_crc(name.c_str());
        dtab[name_crc] = type_codec->decode64(value);
        static CrcString SetProperty("SetProperty");
        self._ctrl->systemNotify(self._sysref, SetProperty, evdata);
      });
  /////////////////////////////////////////////////////////////////////////////////
  // SystemHandle: wraps controller + sys_ref, exposes system_properties proxy
  /////////////////////////////////////////////////////////////////////////////////
  auto syshandle_type = py::class_<SystemHandle>(module_ecs, "SystemHandle")
      .def("__repr__", [](const SystemHandle& h) -> std::string {
        fxstring<256> fxs;
        fxs.format("ecs::SystemHandle id(0x%zx)", h._sysref._sysID);
        return fxs.c_str();
      })
      .def_property_readonly("ref", [](const SystemHandle& h) -> sys_ref_t { return h._sysref; })
      .def_property_readonly("system_properties", [](const SystemHandle& h) -> SystemPropertyProxy {
        return SystemPropertyProxy{h._ctrl, h._sysref};
      });
  /////////////////////////////////////////////////////////////////////////////////
  auto eref_t = py::class_<EntityRef>(module_ecs, "EntityRef").def("__repr__", [](const ent_ref_t& sys) -> std::string {
    fxstring<256> fxs;
    fxs.format("ecs::EntityRef id(0x%zx)", sys._entID);
    return fxs.c_str();
  })
  .def_property_readonly("id",[](ent_ref_t eref) -> uint64_t {
    return eref._entID;
  });
  type_codec->registerStdCodec<EntityRef>(eref_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto cref_t = py::class_<ComponentRef>(module_ecs, "ComponentRef").def("__repr__", [](const comp_ref_t& sys) -> std::string {
    fxstring<256> fxs;
    fxs.format("ecs::ComponentRef id(0x%zx)", sys._compID);
    return fxs.c_str();
  });
  type_codec->registerStdCodec<ComponentRef>(cref_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto rref_t = py::class_<ResponseRef>(module_ecs, "ResponseRef").def("__repr__", [](const response_ref_t& sys) -> std::string {
    fxstring<256> fxs;
    fxs.format("ecs::ResponseRef id(0x%zx)", sys._responseID);
    return fxs.c_str();
  });
  type_codec->registerStdCodec<ResponseRef>(rref_t);

  /////////////////////////////////////////////////////////////////////////////////
} // void pyinit_archetype(py::module& module_ecs) {
/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
