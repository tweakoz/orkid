////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <pkg/ent/component.h>
#include <pkg/ent/componenttable.h>
#include <ork/math/TransformNode.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include "LuaBindings.h"

// using namespace LuaIntf;

namespace ork { namespace ent {

///////////////////////////////////////////////////////////////////////////////

struct ScriptComponentData : public ent::ComponentData {
  ScriptComponentData();

  const file::Path& GetPath() const {
    return mScriptPath;
  }
  void SetPath(const file::Path& pth) {
    mScriptPath = pth;
  }

private:
  RttiDeclareConcrete(ScriptComponentData, ent::ComponentData);
  ent::ComponentInst* createComponent(ent::Entity* pent) const final;
  void DoRegisterWithScene(ork::ent::SceneComposer& sc) final;

  file::Path mScriptPath;
};

typedef ork::FixedString<256> script_funcname_t;

struct ScriptObject {
  ScriptObject();
  ~ScriptObject();

  std::string mScriptPath;
  std::string mScriptText;
  std::string mMD5Digest;
  int mOnEntLink       = LUA_NOREF;
  int mOnEntStart      = LUA_NOREF;
  int mOnEntStop       = LUA_NOREF;
  int mOnEntActivate   = LUA_NOREF;
  int mOnEntDeactivate = LUA_NOREF;
  int mOnEntUpdate     = LUA_NOREF;
  int mModTabRef       = LUA_NOREF;
  int mScriptRef       = LUA_NOREF;
};

///////////////////////////////////////////////////////////////////////////////

struct ScriptComponentInst : public ent::ComponentInst {
  ScriptComponentInst(const ScriptComponentData& cd, ork::ent::Entity* pent);
  const ScriptComponentData& GetCD() const {
    return mCD;
  }

private:
  RttiDeclareAbstract(ScriptComponentInst, ent::ComponentInst);
  void DoUpdate(ent::Simulation* sinst) final;
  bool DoLink(Simulation* psi) final;
  void DoUnLink(Simulation* psi) final;
  bool DoStart(Simulation* psi, const fmtx4& world) final;
  void DoStop(Simulation* psi) final;
  void onActivate(Simulation* psi) final;
  void onDeactivate(Simulation* psi) final;
  const ScriptComponentData& mCD;
  ScriptObject* mScriptObject;

  any<64> mLuaData;
  LuaIntf::LuaRef _luaentity; // its a table
};

///////////////////////////////////////////////////////////////////////////////

class ScriptSystemData : public ork::ent::SystemData {
  RttiDeclareConcrete(ScriptSystemData, ork::ent::SystemData);

public:
  ///////////////////////////////////////////////////////
  ScriptSystemData();
  ///////////////////////////////////////////////////////

private:
  ork::ent::System* createSystem(ork::ent::Simulation* pinst) const final;
};

///////////////////////////////////////////////////////////////////////////////

class ScriptSystem : public ork::ent::System {

public:
  static constexpr systemkey_t SystemType = "ScriptSystem";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }

  ScriptSystem(const ScriptSystemData& data, ork::ent::Simulation* pinst);

  anyp GetLuaManager() {
    return mLuaManager;
  }

  ScriptObject* FlyweightScriptObject(const ork::file::Path& key);

private:
  ~ScriptSystem() final;

  bool DoLink(Simulation* psi) final;
  void DoUnLink(Simulation* psi) final;
  void DoUpdate(Simulation* inst) final;
  void DoStart(Simulation* psi) final;
  void DoStop(Simulation* inst) final;

  anyp mLuaManager;
  std::string mScriptText;
  std::map<ork::file::Path, ScriptObject*> mScriptObjects;
  int mScriptRef;
};

}} // namespace ork::ent
