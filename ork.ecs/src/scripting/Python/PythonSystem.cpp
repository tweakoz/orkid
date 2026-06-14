////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/any.h>
#include <ork/kernel/environment.h>
#include <ork/pch.h>
#include <ork/reflect/properties/register.h>
#include <ork/util/md5.h>
#include <ork/util/logger.h>

#include <cxxabi.h>
#include <iostream>
#include <sstream>

#include <ork/reflect/properties/registerX.inl>

#include <ork/ecs/pysys/PythonComponent.h>

#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>
#include <ork/ecs/simulation.inl>
#include <ork/kernel/profiler.h>

#include "PythonImpl.h"

namespace py = obind;
using namespace obind::literals;

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecssim {

using namespace ::ork::python;

pb11_typecodec_ptr_t simonly_codec_instance() { // static
  struct TypeCodecFactory : public pb11_typecodec_t {
    TypeCodecFactory()
        : TypeCodec(){};
  };
  static auto _instance = std::make_shared<TypeCodecFactory>();
  return _instance;
}
} // namespace ork::ecssim

///////////////////////////////////////////////////////////////////////////////

static const bool kUSEEXECTABUPDATE = false;

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////
using namespace ork;
using namespace ork::object;
using namespace ork::reflect;

static logchannel_ptr_t logchan_pysys = logger()->configureChannel("ecs.pysys", fvec3(0.9, 0.6, 0.0));

///////////////////////////////////////////////////////////////////////////////

void PythonSystemData::describeX(SystemDataClass* clazz) {
  // E.2-walk: the scene script REFERENCE must round-trip (a serialized scene previously
  // lost it — describeX was empty). Path-form keeps the artifact portable across edits.
  clazz->directProperty("ScriptPath", &PythonSystemData::_sceneScriptPath);
}

///////////////////////////////////////////////////////////////////////////////

PythonSystemData::PythonSystemData() {
}

///////////////////////////////////////////////////////////////////////////////

System* PythonSystemData::createSystem(ork::ecs::Simulation* pinst) const {
  return new PythonSystem(*this, pinst);
}

///////////////////////////////////////////////////////////////////////////////

bool PythonSystem::_reload(Simulation* psi) {

  auto scenedata = psi->GetData();
  auto path      = _systemData._sceneScriptPath;
  // KNOWN TRAP: a bare toAbsolute() can drop the leading slash of an already-absolute
  // path (see the HeightFieldGenData::materialize comment) — keep absolute paths as-is.
  auto abspath = path.isAbsolute() ? path : path.toAbsolute();

  if (not abspath.doesPathExist()) {
    // a scene that DECLARES a script must get it — soft-continuing leaves a mute
    // PythonSystem (notifies silently dropped) and a very confusing black box.
    printf("PythonSystem::_reload() script<%s> NOT FOUND (declared by the scene — refusing "
           "to run mute)\n", abspath.c_str());
    OrkAssert(false);
    return false;
  }
  logchan_pysys->log("PythonSystem::_reload() loading script<%s>", abspath.c_str());

  File scriptfile(abspath, EFM_READ);
  size_t filesize = 0;
  scriptfile.GetLength(filesize);
  char* scripttext = (char*)malloc(filesize + 1);
  scriptfile.Read(scripttext, filesize);
  scripttext[filesize] = 0;
  mScriptText          = scripttext;
  free(scripttext);

  // Execute the script in the subinterpreter context and capture the module
  py::object scope = py::module_::import_("__main__").attr("__dict__");
  auto globals     = py::cast<py::dict>(scope);
  //_systemScript                  = py::module_::import_("__main__");
  //_systemScript.attr("__file__") = std::string(abspath.c_str());
  // auto globals         = py::cast<py::dict>(_systemScript.attr("__dict__"));
  // globals["__file__"]            = std::string(abspath.c_str());

  // py::module_ sys            = py::module_::import_("sys");
  // py::list original_sys_path = sys.attr("path");
  // py::module_ math           = py::module_::import("math");
  // globals["MATH"]                  = math;
  // py::module_ ecssim         = py::module_::import("orkengine.ecssim");
  // globals["ECS"]                   = ecssim;
  //  from orkengine.core import CrcString
  // py::module_ core = py::module_::import("orkengine.core");
  // globals["CORE"]        = core;

  // py::module_ OPENCL = py::module_::import("pyopencl");
  // globals["CL"]        = OPENCL;

  // globals["CrcStringProxy"] = core.attr("CrcStringProxy");

  // auto module_core = py::module::create_extension_module(
  //"core", nullptr, new py::module_::module_def );
  //::ork::python::init_crcstring(module_core, ecssim::simonly_codec_instance());
  // sys.attr("path") = py::list();

  py::exec(py::str(mScriptText.c_str()), scope);

  // find global onSystemUpdate() and assign to _pymethodOnSystemUpdate
  if (globals.contains("onSystemUpdate")) {
    _pymethodOnSystemUpdate = globals["onSystemUpdate"];
  }
  if (globals.contains("onSystemInit")) {
    _pymethodOnSystemInit = globals["onSystemInit"];
  }
  if (globals.contains("onSystemLink")) {
    _pymethodOnSystemLink = globals["onSystemLink"];
  }
  if (globals.contains("onSystemActivate")) {
    _pymethodOnSystemActivate = globals["onSystemActivate"];
  }
  if (globals.contains("onSystemStage")) {
    _pymethodOnSystemStage = globals["onSystemStage"];
  }
  if (globals.contains("onSystemNotify")) {
    _pymethodOnSystemNotify = globals["onSystemNotify"];
  }
  if (globals.contains("onComponentActivate")) {
    _pymethodOnComponentActivate = globals["onComponentActivate"];
  }
  if (globals.contains("onComponentDeactivate")) {
    _pymethodOnComponentDeactivate = globals["onComponentDeactivate"];
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////

PythonSystem::PythonSystem(const PythonSystemData& data, ork::ecs::Simulation* psi)
    : ork::ecs::System(&data, psi)
    , _systemData(data) {

  logchan_pysys->log("PythonSystem::PythonSystem() <%p>", this);
  // E.2-walk diagnostics/self-defense: a host that did NOT embed the interpreter
  // (zero-python C++ host with a python-declaring scene) must fail FACTUALLY here,
  // not wedge the update thread into a black screen.
  if (not Py_IsInitialized()) {
    printf("PythonSystem: the MAIN interpreter is NOT initialized — this host did not embed "
           "python. A scene declaring PythonSystem requires an embedding host "
           "(ork.ecs.player.exe inits it when the scene declares PythonSystemData).\n");
    OrkAssert(false);
  }
  logchan_pysys->log("PythonSystem: script<%s>", _systemData._sceneScriptPath.c_str());
  _pythonContext = std::make_shared<pyctx_t>();

  _varmap->makeSharedForKey<ComponentArray>("components", _activeComponents._linear);

  ///////////////////////////////////////////////
  // Set Python Search Path
  ///////////////////////////////////////////////

  /*std::string orkdirstr;
  genviron.get("ORKID_WORKSPACE_DIR", orkdirstr);
  OrkAssert(orkdirstr != "");
  auto orkidWorkspaceDir = file::Path(orkdirstr);
  auto searchpath        = (orkidWorkspaceDir / "ork.data" / "src" / "scripts");
  auto abssrchpath       = searchpath.toAbsolute();
  OrkAssert(abssrchpath.doesPathExist());

  if (abssrchpath.doesPathExist()) {
  }*/

  // logchan_pysys->log("PythonSystem LUA_PATH <%s>", abssrchpath.c_str() );

  ///////////////////////////////////////////////
  // find & init scene file
  ///////////////////////////////////////////////

  // todo figure out how to remove GIL
  // the update thread should not need
  // the primary interpreter GIL at all...
  // pybind11::gil_scoped_acquire acq;

  _pythonContext->bindSubInterpreter();

  try {
    if (_reload(psi)) {

      if (_pymethodOnSystemInit) {
        auto wrapped = pysim_ptr_t(psi);
        __pcallargs(_pymethodOnSystemInit, wrapped);
      }
    }
  } catch (const std::exception& e) {
    logchan_pysys->log("Error executing Python script: %s\n", e.what());
    // e.restore();
    PyErr_Print();
    OrkAssert(false);
  }
  /*} catch (py::error_already_set& e) {
    logchan_pysys->log("Error executing Python script: %s\n", abspath.c_str());
    e.restore();
    PyErr_Print();
    OrkAssert(false);*/
  _pythonContext->unbindSubInterpreter();
}

///////////////////////////////////////////////////////////////////////////////

PythonSystem::~PythonSystem() {

  printf("PythonSystem::~PythonSystem()\n");
}

///////////////////////////////////////////////////////////////////////////////

void PythonSystem::_onActivateComponent(PythonComponent* component) {

  if (_pymethodOnComponentActivate) {
    _pythonContext->bindSubInterpreter();
    auto wrapped_c = pycomponent_ptr_t(component);
    auto wrapped_s = pysim_ptr_t(simulation());
    __pcallargs(_pymethodOnComponentActivate, wrapped_s, wrapped_c);
    _pythonContext->unbindSubInterpreter();
  }

  if (component->_mySGcomponentForInstancing) {
    component->_mySGcomponentForInstancing->_onInstanceCreated = [=]() {
      // at this point this entity's SG component should be staged
      // and therefore SG component's _INSTANCE should be already created
      auto instance = component->_mySGcomponentForInstancing->_INSTANCE;
      OrkAssert(instance);
      auto idata                = instance->_idata;
      component->_sginstance_id = instance->_instance_index;
      component->_idata         = idata;
    };
  }

  _activeComponents.insert(component);
}
void PythonSystem::_onDeactivateComponent(PythonComponent* component) {

  if (_pymethodOnComponentDeactivate) {
    _pythonContext->bindSubInterpreter();
    auto wrapped = pycomponent_ptr_t(component);
    __pcallargs(_pymethodOnComponentDeactivate, wrapped);
    _pythonContext->unbindSubInterpreter();
  }

  _activeComponents.remove(component);
}

///////////////////////////////////////////////////////////////////////////////

bool PythonSystem::_onLink(Simulation* psi) // final
{
  // todo figure out how to remove GIL
  // the update thread should not need
  // the primary interpreter GIL at all...
  // pybind11::gil_scoped_acquire acq;
  logchan_pysys->log("_onLink() ");
  if (_pymethodOnSystemLink) {
    _pythonContext->bindSubInterpreter();
    auto wrapped = pysim_ptr_t(psi);
    __pcallargs(_pymethodOnSystemLink, wrapped);
    _pythonContext->unbindSubInterpreter();
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////

void PythonSystem::_onUnLink(Simulation* psi) // final
{
  logchan_pysys->log("_onUnLink() ");
}

///////////////////////////////////////////////////////////////////////////////

bool PythonSystem::_onActivate(Simulation* psi) // final
{
  // todo figure out how to remove GIL
  // the update thread should not need
  // the primary interpreter GIL at all...
  // pybind11::gil_scoped_acquire acq;
  logchan_pysys->log("_onActivate() ");
  // LuaProtectedCallByName( _pythonContext->mLuaState, mScriptRef, "OnSceneStart");
  if (_pymethodOnSystemActivate) {
    _pythonContext->bindSubInterpreter();
    auto wrapped = pysim_ptr_t(psi);
    __pcallargs(_pymethodOnSystemActivate, wrapped);
    _pythonContext->unbindSubInterpreter();
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void PythonSystem::_onDeactivate(Simulation* psi) // final
{
  logchan_pysys->log("_onDeactivate() ");
  // LuaProtectedCallByName( _pythonContext->mLuaState, mScriptRef, "OnSceneStop");
}

///////////////////////////////////////////////////////////////////////////////

bool PythonSystem::_onStage(Simulation* psi) {
  // todo figure out how to remove GIL
  // the update thread should not need
  // the primary interpreter GIL at all...
  // pybind11::gil_scoped_acquire acq;
  logchan_pysys->log("_onStage() ");
  if (_pymethodOnSystemStage) {
    _pythonContext->bindSubInterpreter();
    auto wrapped = pysim_ptr_t(psi);
    __pcallargs(_pymethodOnSystemStage, wrapped);
    _pythonContext->unbindSubInterpreter();
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void PythonSystem::_onUnstage(Simulation* psi) {
  logchan_pysys->log("_onUnstage() ");
}

///////////////////////////////////////////////////////////////////////////////

void PythonSystem::_onNotify(token_t evID, evdata_t data) {

  switch (evID.hashed()) {
    case "RELOAD"_crcu:
      _reload(simulation());
      break;
    default: {
      bool has_notify = bool(_pymethodOnSystemNotify);
      if (has_notify) {
        // logchan_pysys->log("_onNotify() %d", int(has_notify));
        auto evIDcrc = std::make_shared<CrcString>();
        (*evIDcrc)   = evID;
        _pythonContext->bindSubInterpreter();
        auto table   = data.getShared<DataTable>();
        auto wrapped = pysim_ptr_t(simulation());
        __pcallargs(_pymethodOnSystemNotify, wrapped, evIDcrc, table);
        _pythonContext->unbindSubInterpreter();
      }
      break;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void PythonSystem::_onUpdate(Simulation* psi) // final
{
  OrkProfilerSampleScope(CHANNEL_UPDATE, "PythonSystem::_onUpdate");
  // todo figure out how to remove GIL
  // the update thread should not need
  // the primary interpreter GIL at all...
  // pybind11::gil_scoped_acquire acq;

  double dt = psi->deltaTime();
  double gt = psi->gameTime();

  if (_pymethodOnSystemUpdate) {
    auto wrapped = pysim_ptr_t(psi);
    _pythonContext->bindSubInterpreter();
    __pcallargs(_pymethodOnSystemUpdate, wrapped);
    _pythonContext->unbindSubInterpreter();
  }

  if (_onSystemUpdate) {
    // auto controller = psi->controller();
    //_onSystemUpdate(nullptr);
    //  controller->_simulation.atomicOp([this](simulation_ptr_t& unlocked){
    //  });
  }

  // if instancing active
  //  apply instances
  for (auto c : _activeComponents._linear) {
    if (c->_idata) {
      fmtx4 mtx                                    = c->GetEntity()->transform()->composed();
      c->_idata->_worldmatrices[c->_sginstance_id] = mtx;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
