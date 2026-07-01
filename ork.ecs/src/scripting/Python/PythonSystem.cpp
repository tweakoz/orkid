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
#include <ork/event/Event.h> // ui::UpdateData — the per-component onUpdate's updinfo

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
  // ADDITIONAL composable system scripts (';'-joined absolute paths) — must round-trip too.
  clazz->directProperty("ScriptPaths", &PythonSystemData::_sceneScriptPathsCSV);
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
  bool any       = false;

  //////////////////////////////////////////////////////////////////////////////
  // PRIMARY system script (single; back-compat). A PythonSystem may host ONLY
  // PythonComponents and/or extra scripts and carry NO primary script — legitimate
  // (component-only system). A DECLARED-but-missing primary is an error.
  //////////////////////////////////////////////////////////////////////////////
  auto path = _systemData._sceneScriptPath;
  if (not path.empty()) {
    // KNOWN TRAP: a bare toAbsolute() can drop the leading slash of an already-absolute path.
    auto abspath = path.isAbsolute() ? path : path.toAbsolute();
    if (not abspath.doesPathExist()) {
      printf("PythonSystem::_reload() script<%s> NOT FOUND (declared by the scene — refusing "
             "to run mute)\n", abspath.c_str());
      OrkAssert(false);
    } else {
      logchan_pysys->log("PythonSystem::_reload() loading primary script<%s>", abspath.c_str());
      File scriptfile(abspath, EFM_READ);
      size_t filesize = 0;
      scriptfile.GetLength(filesize);
      char* scripttext = (char*)malloc(filesize + 1);
      scriptfile.Read(scripttext, filesize);
      scripttext[filesize] = 0;
      mScriptText          = scripttext;
      free(scripttext);
      // Primary runs in __main__ (historical). Extra scripts get fresh namespaces (below).
      py::object scope = py::module_::import_("__main__").attr("__dict__");
      auto globals     = py::cast<py::dict>(scope);
      py::exec(py::str(mScriptText.c_str()), scope);
      if (globals.contains("onSystemUpdate"))        _pymethodOnSystemUpdate        = globals["onSystemUpdate"];
      if (globals.contains("onSystemGpuUpdate"))     _pymethodOnSystemGpuUpdate     = globals["onSystemGpuUpdate"];
      if (globals.contains("onSystemInit"))          _pymethodOnSystemInit          = globals["onSystemInit"];
      if (globals.contains("onSystemLink"))          _pymethodOnSystemLink          = globals["onSystemLink"];
      if (globals.contains("onSystemActivate"))      _pymethodOnSystemActivate      = globals["onSystemActivate"];
      if (globals.contains("onSystemStage"))         _pymethodOnSystemStage         = globals["onSystemStage"];
      if (globals.contains("onSystemNotify"))        _pymethodOnSystemNotify        = globals["onSystemNotify"];
      if (globals.contains("onComponentActivate"))   _pymethodOnComponentActivate   = globals["onComponentActivate"];
      if (globals.contains("onComponentDeactivate")) _pymethodOnComponentDeactivate = globals["onComponentDeactivate"];
      any = true;
    }
  } else {
    logchan_pysys->log("PythonSystem::_reload() no primary scene script — component/extra-only system");
  }

  //////////////////////////////////////////////////////////////////////////////
  // ADDITIONAL system scripts (composable). Each runs as its OWN system-scoped
  // script (own namespace, all onSystem* hooks); dispatch fans to all of them.
  //////////////////////////////////////////////////////////////////////////////
  _loadExtraScripts();
  if (not _extraScripts.empty())
    any = true;

  return any;
}

///////////////////////////////////////////////////////////////////////////////

void PythonSystem::_loadExtraScripts() {
  _extraScripts.clear();
  const std::string& csv = _systemData._sceneScriptPathsCSV;
  size_t start = 0;
  while (start < csv.size()) {
    size_t sep      = csv.find(';', start);
    std::string one = (sep == std::string::npos) ? csv.substr(start) : csv.substr(start, sep - start);
    start           = (sep == std::string::npos) ? csv.size() : sep + 1;
    if (one.empty())
      continue;
    file::Path xp = one.c_str();
    auto xabs     = xp.isAbsolute() ? xp : xp.toAbsolute();
    if (not xabs.doesPathExist()) {
      printf("PythonSystem::_reload() extra system script<%s> NOT FOUND\n", xabs.c_str());
      OrkAssert(false);
      continue;
    }
    File xf(xabs, EFM_READ);
    size_t xsz = 0;
    xf.GetLength(xsz);
    char* xtxt = (char*)malloc(xsz + 1);
    xf.Read(xtxt, xsz);
    xtxt[xsz]         = 0;
    std::string xtext = xtxt;
    free(xtxt);
    py::dict ns; // fresh namespace per script (isolation — same pattern as per-component scripts)
    py::exec(py::str(xtext.c_str()), ns);
    PythonSysScript ss;
    ss._ns = ns;
    if (ns.contains("onSystemInit"))          ss._onInit          = ns["onSystemInit"];
    if (ns.contains("onSystemLink"))          ss._onLink          = ns["onSystemLink"];
    if (ns.contains("onSystemActivate"))      ss._onActivate      = ns["onSystemActivate"];
    if (ns.contains("onSystemStage"))         ss._onStage         = ns["onSystemStage"];
    if (ns.contains("onSystemNotify"))        ss._onNotify        = ns["onSystemNotify"];
    if (ns.contains("onSystemUpdate"))        ss._onUpdate        = ns["onSystemUpdate"];
    if (ns.contains("onSystemGpuUpdate"))     ss._onGpuUpdate     = ns["onSystemGpuUpdate"];
    if (ns.contains("onComponentActivate"))   ss._onCompActivate  = ns["onComponentActivate"];
    if (ns.contains("onComponentDeactivate")) ss._onCompDeactivate = ns["onComponentDeactivate"];
    logchan_pysys->log("PythonSystem::_reload() +extra system script<%s>", xabs.c_str());
    _extraScripts.push_back(ss);
  }
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
      for (auto& es : _extraScripts)
        if (es._onInit) {
          auto wrapped = pysim_ptr_t(psi);
          __pcallargs(es._onInit, wrapped);
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

void PythonSystem::_loadComponentScript(PythonComponent* component) {
  // ONE-shot per component (subinterpreter must already be bound by the caller).
  // A PythonComponent's ScriptFile is its OWN behavior script, separate from the
  // PythonSystem's scene script — execed in a fresh namespace so component
  // scripts never clobber each other or the system's __main__.
  if (component->_scriptLoaded)
    return;
  component->_scriptLoaded = true; // mark first — never re-read per frame, even on failure

  auto path = component->mCD.GetPath();
  if (path.empty())
    return; // no ScriptFile: an instancing-only PythonComponent — nothing to run

  // mirror _reload's absolute-path trap handling
  auto abspath = path.isAbsolute() ? path : path.toAbsolute();
  if (not abspath.doesPathExist()) {
    // self-defense (mirrors PythonSystem::_reload): a declared script that's
    // missing = a mute component. Fail factually, don't run a black box.
    printf("PythonComponent script<%s> NOT FOUND (declared by the scene)\n", abspath.c_str());
    OrkAssert(false);
    return;
  }

  File scriptfile(abspath, EFM_READ);
  size_t filesize = 0;
  scriptfile.GetLength(filesize);
  char* scripttext = (char*)malloc(filesize + 1);
  scriptfile.Read(scripttext, filesize);
  scripttext[filesize] = 0;

  py::dict ns; // fresh per-component namespace (CPython injects __builtins__ on exec)
  py::exec(py::str(scripttext), ns);
  free(scripttext);

  component->_scriptNamespace = ns;
  if (ns.contains("onActivate"))
    component->_pyOnActivate = ns["onActivate"];
  if (ns.contains("onUpdate"))
    component->_pyOnUpdate = ns["onUpdate"];

  logchan_pysys->log(
      "PythonComponent loaded script<%s> (onUpdate=%d onActivate=%d)",
      abspath.c_str(),
      int(bool(component->_pyOnUpdate)),
      int(bool(component->_pyOnActivate)));
}

///////////////////////////////////////////////////////////////////////////////

void PythonSystem::_onActivateComponent(PythonComponent* component) {

  bool has_script = not component->mCD.GetPath().empty();
  bool needs_py   = has_script or bool(_pymethodOnComponentActivate);

  if (needs_py) {
    _pythonContext->bindSubInterpreter();
    // load this component's own behavior script (idempotent), then onActivate()
    if (has_script) {
      _loadComponentScript(component);
      if (component->_pyOnActivate) {
        auto wrapped_c = pycomponent_ptr_t(component);
        __pcallargs(component->_pyOnActivate, wrapped_c);
      }
    }
    // system-level onComponentActivate hook (the scene script's, if declared)
    if (_pymethodOnComponentActivate) {
      auto wrapped_c = pycomponent_ptr_t(component);
      auto wrapped_s = pysim_ptr_t(simulation());
      __pcallargs(_pymethodOnComponentActivate, wrapped_s, wrapped_c);
    }
    for (auto& es : _extraScripts)
      if (es._onCompActivate) {
        auto wrapped_c = pycomponent_ptr_t(component);
        auto wrapped_s = pysim_ptr_t(simulation());
        __pcallargs(es._onCompActivate, wrapped_s, wrapped_c);
      }
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

  if (_pymethodOnComponentDeactivate or not _extraScripts.empty()) {
    _pythonContext->bindSubInterpreter();
    auto wrapped = pycomponent_ptr_t(component);
    if (_pymethodOnComponentDeactivate)
      __pcallargs(_pymethodOnComponentDeactivate, wrapped);
    for (auto& es : _extraScripts)
      if (es._onCompDeactivate) __pcallargs(es._onCompDeactivate, wrapped);
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
  if (_pymethodOnSystemLink or not _extraScripts.empty()) {
    _pythonContext->bindSubInterpreter();
    auto wrapped = pysim_ptr_t(psi);
    if (_pymethodOnSystemLink)
      __pcallargs(_pymethodOnSystemLink, wrapped);
    for (auto& es : _extraScripts)
      if (es._onLink) __pcallargs(es._onLink, wrapped);
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
  if (_pymethodOnSystemActivate or not _extraScripts.empty()) {
    _pythonContext->bindSubInterpreter();
    auto wrapped = pysim_ptr_t(psi);
    if (_pymethodOnSystemActivate)
      __pcallargs(_pymethodOnSystemActivate, wrapped);
    for (auto& es : _extraScripts)
      if (es._onActivate) __pcallargs(es._onActivate, wrapped);
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
  if (_pymethodOnSystemStage or not _extraScripts.empty()) {
    _pythonContext->bindSubInterpreter();
    auto wrapped = pysim_ptr_t(psi);
    if (_pymethodOnSystemStage)
      __pcallargs(_pymethodOnSystemStage, wrapped);
    for (auto& es : _extraScripts)
      if (es._onStage) __pcallargs(es._onStage, wrapped);
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
      // Fan the notify (e.g. InputKey) to the primary script AND every extra script that
      // defines onSystemNotify — this is the composition keystone (multiple input owners).
      if (_pymethodOnSystemNotify or not _extraScripts.empty()) {
        auto evIDcrc = std::make_shared<CrcString>();
        (*evIDcrc)   = evID;
        _pythonContext->bindSubInterpreter();
        auto table   = data.getShared<DataTable>();
        auto wrapped = pysim_ptr_t(simulation());
        if (_pymethodOnSystemNotify)
          __pcallargs(_pymethodOnSystemNotify, wrapped, evIDcrc, table);
        for (auto& es : _extraScripts)
          if (es._onNotify) __pcallargs(es._onNotify, wrapped, evIDcrc, table);
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

  // onSystemUpdate runs HERE, on the UPDATE thread — always. (Render-tick work goes
  // in onSystemGpuUpdate, dispatched from PythonSystem::_onGpuUpdate; the two scripts
  // are thread-distinct by contract.)
  if (_pymethodOnSystemUpdate or not _extraScripts.empty()) {
    auto wrapped = pysim_ptr_t(psi);
    _pythonContext->bindSubInterpreter();
    if (_pymethodOnSystemUpdate)
      __pcallargs(_pymethodOnSystemUpdate, wrapped);
    for (auto& es : _extraScripts)
      if (es._onUpdate) __pcallargs(es._onUpdate, wrapped);
    _pythonContext->unbindSubInterpreter();
  }

  if (_onSystemUpdate) {
    // auto controller = psi->controller();
    //_onSystemUpdate(nullptr);
    //  controller->_simulation.atomicOp([this](simulation_ptr_t& unlocked){
    //  });
  }

  // ---- per-component behavior scripts: onUpdate(component, updinfo) ----
  // updinfo carries dt + abstime (game time). This runs BEFORE the instancing
  // world-matrix copy below, so a script that mutates its entity transform
  // (e.g. a spin: entity.orientation = quat(vec3(0,1,0), updinfo.abstime*w)) is
  // visible THIS frame on the instanced path; the non-instanced SceneGraph sync
  // reads the same entity transform downstream. Bind the subinterpreter once
  // around the whole sweep, and only when some component actually has a script
  // (script-less PythonSystems — e.g. walk_input — pay nothing).
  bool any_comp_update = false;
  for (auto c : _activeComponents._linear)
    if (c->_pyOnUpdate) {
      any_comp_update = true;
      break;
    }
  if (any_comp_update) {
    ork::ui::UpdateData updinfo;
    updinfo._dt      = dt;
    updinfo._abstime = gt;
    updinfo._counter = _updateCounter;
    _pythonContext->bindSubInterpreter();
    for (auto c : _activeComponents._linear) {
      if (c->_pyOnUpdate) {
        auto wrapped_c = pycomponent_ptr_t(c);
        __pcallargs(c->_pyOnUpdate, wrapped_c, updinfo);
      }
    }
    _pythonContext->unbindSubInterpreter();
  }
  _updateCounter++;

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

void PythonSystem::_onGpuUpdate(Simulation* psi, lev2::Context* ctx) {
  // RENDER-tick (render thread, once per rendered frame). Runs ONLY the dedicated
  // onSystemGpuUpdate hook — render-rate work (e.g. VR head pose). onSystemUpdate is
  // UPDATE-THREAD ONLY and is NEVER invoked here. ctx is intentionally NOT forwarded
  // to the script yet (script takes only the sim).
  OrkProfilerSampleScope(CHANNEL_UPDATE, "PythonSystem::_onGpuUpdate");
  // ONLY the dedicated render-tick hook runs here. onSystemUpdate is UPDATE-THREAD
  // ONLY (it runs in _onUpdate) — NEVER call it from the gpu/render thread.
  if (_pymethodOnSystemGpuUpdate or not _extraScripts.empty()) {
    auto wrapped = pysim_ptr_t(psi);
    _pythonContext->bindSubInterpreter();
    if (_pymethodOnSystemGpuUpdate)
      __pcallargs(_pymethodOnSystemGpuUpdate, wrapped);
    for (auto& es : _extraScripts)
      if (es._onGpuUpdate) __pcallargs(es._onGpuUpdate, wrapped);
    _pythonContext->unbindSubInterpreter();
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
