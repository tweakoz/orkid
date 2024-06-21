////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/file/fileenv.h>
#include <ork/rtti/RTTIX.inl>

#include <ork/ecs/component.h>
#include <ork/ecs/system.h>
#include <ork/ecs/pysys/PythonComponent.h>
//#include "LuaBindings.h"

#include <Python.h>
#include <pybind11/embed.h> // everything needed for embedding
#include <pybind11/pybind11.h>

namespace ork::ecs::pysys {

typedef ork::FixedString<256> script_funcname_t;
struct PythonContext;
struct ScriptObject;
}

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {

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
  pysys::ScriptObject* mScriptObject;

  any<64> mPythonData;
  //LuaIntf::LuaRef _luaentity; // its a table
};

///////////////////////////////////////////////////////////////////////////////

struct PythonSystem final : public ork::ecs::System {

public:
  static constexpr systemkey_t SystemType = "PythonSystem";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }

  PythonSystem(const PythonSystemData& data, ork::ecs::Simulation* pinst);

  anyp getManager() {
    return mPythonManager;
  }

  pysys::ScriptObject* FlyweightScriptObject(const ork::file::Path& key);

private:

  friend struct PythonComponent;
  
  ~PythonSystem();

  void _onActivateComponent(PythonComponent* component);
  void _onDeactivateComponent(PythonComponent* component);

  bool _onLink(Simulation* psi) override;
  void _onUnLink(Simulation* psi) override;
  void _onUpdate(Simulation* inst) override;
  bool _onStage(Simulation* psi) override;
  void _onUnstage(Simulation* inst) override;
  bool _onActivate(Simulation* psi) override;
  void _onDeactivate(Simulation* inst) override;

  anyp mPythonManager;
  std::string mScriptText;
  std::map<ork::file::Path, pysys::ScriptObject*> mScriptObjects;
  std::unordered_set<PythonComponent*> _activeComponents;
  //int mScriptRef;
};

} // namespace ork::ecs {

namespace ork::ecs::pysys {

struct GlobalState;
using globalstate_ptr_t = std::shared_ptr<GlobalState>;

struct PythonContext {
  
  PythonContext(Simulation* psi, PythonSystem* system);
  ~PythonContext();
  //lua_State* mLuaState = nullptr;
  Simulation* mSimulation = nullptr;
  PythonSystem* _python_system = nullptr;
  PyThreadState* _subInterpreter = nullptr;

  std::unordered_map<uint64_t,scriptwrapper_t> _tokwrappers;

  void bindSubInterpreter();
  void unbindSubInterpreter();

};

struct ScriptObject {
  ScriptObject();
  ~ScriptObject();

  std::string mScriptPath;
  std::string mScriptText;
  std::string mMD5Digest;
  //int mOnEntInitialize       = LUA_NOREF;
  //int mOnEntUninitialize       = LUA_NOREF;
  //int mOnEntLink       = LUA_NOREF;
  //int mOnEntUnlink       = LUA_NOREF;
  //int mOnEntStage      = LUA_NOREF;
  //int mOnEntUnstage    = LUA_NOREF;
  //int mOnEntActivate   = LUA_NOREF;
  //int mOnEntDeactivate = LUA_NOREF;
  //int mOnEntUpdate     = LUA_NOREF;
  //int mOnNotify        = LUA_NOREF;
  //int mModTabRef       = LUA_NOREF;
  //int mScriptRef       = LUA_NOREF;
};

} // namespace ork::ecs::pysys {
