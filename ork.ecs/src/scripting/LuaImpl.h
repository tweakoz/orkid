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
#include "LuaBindings.h"

namespace ork::ecs {

constexpr int LUA_STACKINDEX_TOP = -1;

typedef ork::FixedString<256> script_funcname_t;

struct ScriptObject {
  ScriptObject();
  ~ScriptObject();

  std::string mScriptPath;
  std::string mScriptText;
  std::string mMD5Digest;
  int mOnEntInitialize       = LUA_NOREF;
  int mOnEntUninitialize       = LUA_NOREF;
  int mOnEntLink       = LUA_NOREF;
  int mOnEntUnlink       = LUA_NOREF;
  int mOnEntStage      = LUA_NOREF;
  int mOnEntUnstage    = LUA_NOREF;
  int mOnEntActivate   = LUA_NOREF;
  int mOnEntDeactivate = LUA_NOREF;
  int mOnEntUpdate     = LUA_NOREF;
  int mOnNotify        = LUA_NOREF;
  int mModTabRef       = LUA_NOREF;
  int mScriptRef       = LUA_NOREF;
};

///////////////////////////////////////////////////////////////////////////////

struct LuaComponent : public ecs::Component {
  DeclareAbstractX(LuaComponent, ecs::Component);

public:
  LuaComponent(const LuaComponentData& cd, ork::ecs::Entity* pent);
  const LuaComponentData& GetCD() const {
    return mCD;
  }

  void _onUninitialize(Simulation* psi) final;
  bool _onLink(Simulation* psi) final;
  void _onUnlink(Simulation* psi) final;
  bool _onStage(Simulation* psi) final;
  void _onUnstage(Simulation* psi) final;
  bool _onActivate(Simulation* psi) final;
  void _onDeactivate(Simulation* psi) final;
  void _onNotify(Simulation* psi, token_t evID, svar64_t data ) final;

  const LuaComponentData& mCD;
  ScriptObject* mScriptObject;

  any<64> mLuaData;
  LuaIntf::LuaRef _luaentity; // its a table
};

///////////////////////////////////////////////////////////////////////////////

struct LuaSystem final : public ork::ecs::System {

public:
  static constexpr systemkey_t SystemType = "LuaSystem";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }

  LuaSystem(const LuaSystemData& data, ork::ecs::Simulation* pinst);

  anyp GetLuaManager() {
    return mLuaManager;
  }

  ScriptObject* FlyweightScriptObject(const ork::file::Path& key);

private:

  friend struct LuaComponent;
  
  ~LuaSystem();

  void _onActivateComponent(LuaComponent* component);
  void _onDeactivateComponent(LuaComponent* component);

  bool _onLink(Simulation* psi) override;
  void _onUnLink(Simulation* psi) override;
  void _onUpdate(Simulation* inst) override;
  bool _onStage(Simulation* psi) override;
  void _onUnstage(Simulation* inst) override;
  bool _onActivate(Simulation* psi) override;
  void _onDeactivate(Simulation* inst) override;

  anyp mLuaManager;
  std::string mScriptText;
  std::map<ork::file::Path, ScriptObject*> mScriptObjects;
  std::unordered_set<LuaComponent*> _activeComponents;
  int mScriptRef;
};

} //namespace ork::ecs {
