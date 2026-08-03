////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/file/fileenv.h>
#include <ork/rtti/RTTIX.inl>

#include "../component.h"
#include "../system.h"
#include <Python.h>
#include <ork/python/pycodec.h>
#include <ork/python/context.h>
#include <ork/util/fast_set.inl>

#include <ork/ecs/SceneGraphComponent.h>

namespace ork { namespace ecs {

///////////////////////////////////////////////////////////////////////////////

struct PythonComponentData : public ecs::ComponentData {
  DeclareConcreteX(PythonComponentData, ecs::ComponentData);

public:
  PythonComponentData();

  const file::Path& GetPath() const {
    return mScriptPath;
  }
  void SetPath(const file::Path& pth) {
    mScriptPath = pth;
  }

  ecs::Component* createComponent(ecs::Entity* pent) const final;
  static object::ObjectClass* componentClass();
  void DoRegisterWithScene(ork::ecs::SceneComposer& sc) const final;

  file::Path mScriptPath;

  // AUTHOR->SCRIPT PAYLOAD, reflected: an opaque string the scene declares
  // alongside the script path and the behavior script reads as
  // comp.script_data. It is reflected precisely so it rides the .ecs; a script
  // whose only declared surface was a PATH forced every other authored value
  // out-of-band into the process environment, which does NOT survive the
  // two-process author->player recipe (ork.scene.tojson.py then
  // ork.ecs.player.exe) and left the player reading an empty table.
  std::string _scriptData;

  lev2::scenegraph::node_instance_data_ptr_t _INSTANCEDATA;
};

///////////////////////////////////////////////////////////////////////////////

struct PythonSystemData : public ork::ecs::SystemData {
  DeclareConcreteX(PythonSystemData, ork::ecs::SystemData);

public:
  ///////////////////////////////////////////////////////
  PythonSystemData();
  ///////////////////////////////////////////////////////

  file::Path _sceneScriptPath;            // the PRIMARY system script (back-compat, single)
  std::string _sceneScriptPathsCSV;       // ADDITIONAL system scripts, ';'-joined absolute paths.
                                          // Each runs as its OWN system-scoped script (own namespace,
                                          // all onSystem* hooks) — lets a scene COMPOSE input/behavior
                                          // (e.g. walk_input + a VR head-pose script) on one PythonSystem.
  ork::ecs::System* createSystem(ork::ecs::Simulation* pinst) const final;
};

///////////////////////////////////////////////////////////////////////////////
// One ADDITIONAL system-scoped script: its own globals namespace + the onSystem* hooks it defines.
// The PythonSystem fans every hook to the primary script AND each of these, so InputKey/update/etc.
// reach all of them. Scripts are isolated (separate namespaces) and communicate only via the sim.
///////////////////////////////////////////////////////////////////////////////
struct PythonSysScript {
  obind::object _ns;               // the script's globals dict (keeps the fns alive)
  obind::object _onInit;
  obind::object _onLink;
  obind::object _onActivate;
  obind::object _onStage;
  obind::object _onNotify;
  obind::object _onUpdate;
  obind::object _onGpuUpdate;
  obind::object _onCompActivate;
  obind::object _onCompDeactivate;
};

struct PythonComponent : public ecs::Component {
  DeclareAbstractX(PythonComponent, ecs::Component);

public:
  PythonComponent(const PythonComponentData& cd, ork::ecs::Entity* pent);
  const PythonComponentData& GetCD() const {
    return mCD;
  }

  void _onUninitialize(Simulation* psi) final;
  bool _onLink(Simulation* psi) final;
  void _onUnlink(Simulation* psi) final;
  bool _onStage(Simulation* psi) final;
  void _onUnstage(Simulation* psi) final;
  bool _onActivate(Simulation* psi) final;
  void _onDeactivate(Simulation* psi) final;
  void _onNotify(Simulation* psi, token_t evID, evdata_t data ) final;

  const PythonComponentData& mCD;

  any<64> mPythonData;
  lev2::instanceddrawinstancedata_ptr_t _idata;
  SceneGraphComponent* _mySGcomponentForInstancing = nullptr;
  int _sginstance_id = -1;

  // ----- per-entity component script (PythonComponentData ScriptFile) -----
  // The PER-COMPONENT script — distinct from the PythonSystem scene script.
  // Runs in its OWN namespace; onUpdate(component, updinfo) fires each frame
  // from PythonSystem::_onUpdate (before the SG world-matrix sync). Lets a
  // single entity carry declarative behavior (e.g. a spin) right in the .ecs.
  obind::object _scriptNamespace; // the script's globals dict (keeps the fns alive)
  obind::object _pyOnActivate;    // onActivate(component)        [optional]
  obind::object _pyOnUpdate;      // onUpdate(component, updinfo)  [optional]
  bool _scriptLoaded = false;
};

///////////////////////////////////////////////////////////////////////////////

struct PythonSystem final : public ork::ecs::System {

public:
  static constexpr systemkey_t SystemType = "PythonSystem";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }

  PythonSystem(const PythonSystemData& data, ork::ecs::Simulation* pinst);

  friend struct PythonComponent;
  
  ~PythonSystem();

  void _onActivateComponent(PythonComponent* component);
  void _onDeactivateComponent(PythonComponent* component);
  void _loadComponentScript(PythonComponent* component); // idempotent; subinterp must be bound

  bool _onLink(Simulation* psi) final;
  void _onUnLink(Simulation* psi) final;
  void _onUpdate(Simulation* inst) final;
  void _onGpuUpdate(Simulation* psi, lev2::Context* ctx) final;  // render-tick: runs onSystemGpuUpdate
  bool _onStage(Simulation* psi) final;
  void _onUnstage(Simulation* inst) final;
  bool _onActivate(Simulation* psi) final;
  void _onDeactivate(Simulation* inst) final;
  void _onNotify(token_t evID, evdata_t data) final;

  bool _reload(Simulation* psi);
  void _loadExtraScripts(); // load PythonSystemData._sceneScriptPathsCSV -> _extraScripts (own namespaces)

  template <typename Arg>
  auto process_arg(Arg&& arg) {
      //auto type_codec = ecssim::simonly_codec_instance();
        auto type_codec = ork::python::obind_typecodec_t::instance();

      return type_codec->encode(std::forward<Arg>(arg));
  }

  template <typename... A> void __pcallargs(obind::object fn_object,A&&... args){
    try {

        auto process_args = [&](auto&&... processed_args) {
            fn_object(std::forward<decltype(processed_args)>(processed_args)...);
        };

        // Process each argument and expand them
        process_args(process_arg(std::forward<A>(args))...);

    } catch (const std::exception& e) {
      printf("Error executing Python script: %s\n", e.what());
      // FLUSH BEFORE THE ASSERT. Redirected stdout is fully buffered, and the
      // assert aborts without unwinding, so the traceback that says WHY the
      // script died never reached any log file — every captured crash read as a
      // bare SIGTRAP. A diagnostic that only survives on a tty is not loud.
      fflush(stdout);
      OrkAssert(false);
    }
  }
  using pyctx_t = ::ork::python::Context2;
  std::shared_ptr<pyctx_t> _pythonContext;
  std::string mScriptText;
  fast_set<PythonComponent*> _activeComponents;
  system_update_lambda_t _onSystemUpdate;
  obind::object _systemScript;
  obind::object _pymethodOnSystemUpdate;
  obind::object _pymethodOnSystemGpuUpdate;   // optional render-tick hook: onSystemGpuUpdate(sim)
  obind::object _pymethodOnSystemInit;
  obind::object _pymethodOnSystemLink;
  obind::object _pymethodOnSystemActivate;
  obind::object _pymethodOnSystemStage;
  obind::object _pymethodOnSystemNotify;

  obind::object _pymethodOnComponentActivate;
  obind::object _pymethodOnComponentDeactivate;

  // ADDITIONAL system-scoped scripts (PythonSystemData._sceneScriptPathsCSV). Each runs alongside
  // the primary _pymethod* script; every dispatch site fans to the primary AND all of these.
  std::vector<PythonSysScript> _extraScripts;

  const PythonSystemData& _systemData;
  int _updateCounter = 0; // frame counter fed to component-script updinfo

  //int mScriptRef;
};


using pypythonsystem_ptr_t = ::ork::python::unmanaged_ptr<PythonSystem>;

///////////////////////////////////////////////////////////////////////////////
struct ComponentArray {
  inline ComponentArray(std::vector<PythonComponent*>& components)
    : _components(components) {
  }
  inline pycomponent_ptr_t get(int idx) const {
    return pycomponent_ptr_t(_components[idx]);
  }
  inline size_t size() const {
    return _components.size();
  }
  std::vector<PythonComponent*>& _components;
};
using pycomponentarray_ptr_t = std::shared_ptr<ComponentArray>;

}} // namespace ork::ecs
