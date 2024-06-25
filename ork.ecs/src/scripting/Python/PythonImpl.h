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
  system_update_lambda_t _onSystemUpdate;

  //int mScriptRef;
};

} // namespace ork::ecs {

namespace ork::ecs::pysys {


} // namespace ork::ecs::pysys {
