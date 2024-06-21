////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/any.h>
#include <ork/pch.h>
#include <ork/reflect/properties/register.h>
#include <ork/util/md5.h>

#include <cxxabi.h>
#include <iostream>
#include <sstream>

#include <ork/reflect/properties/registerX.inl>

#include <ork/ecs/lua/LuaComponent.h>

#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>
#include <ork/ecs/simulation.inl>

#include "LuaIntf/LuaIntf.h"
#include "LuaImpl.h"
#include <ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::ecs::LuaComponentData, "LuaComponentData");
ImplementReflectionX(ork::ecs::LuaComponent, "LuaComponent");
ImplementReflectionX(ork::ecs::LuaSystemData, "LuaSystemData");

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////
using namespace ork;
using namespace ork::object;
using namespace ork::reflect;

static logchannel_ptr_t logchan_luacomp = logger()->createChannel("ecs.luacomp",fvec3(0.9,0.8,0.0));

void LuaComponentData::describeX(ComponentDataClass* clazz) {
  clazz->directProperty("ScriptFile", &LuaComponentData::mScriptPath)
      ->annotate("editor.class", "ged.factory.filelist")
      ->annotate("editor.filetype", "lua")
      ->annotate("editor.filebase", "src://scripts/");
}

///////////////////////////////////////////////////////////////////////////////

LuaComponentData::LuaComponentData() {
  // printf("LuaComponentData::LuaComponentData() this: %p\n", this);
}

///////////////////////////////////////////////////////////////////////////////

Component* LuaComponentData::createComponent(ecs::Entity* pent) const {
  return new LuaComponent(*this, pent);
}

///////////////////////////////////////////////////////////////////////////////

void LuaComponentData::DoRegisterWithScene(ork::ecs::SceneComposer& sc) const {
  sc.Register<ork::ecs::LuaSystemData>();
}

object::ObjectClass* LuaComponentData::componentClass() {
  return LuaComponent::GetClassStatic();
}

///////////////////////////////////////////////////////////////////////////////

ScriptObject::ScriptObject()
    : mScriptRef(LUA_NOREF) {
  // printf("new ScriptObject<%p>\n", this);
}
ScriptObject::~ScriptObject() {
  // printf("deleting ScriptObject<%p>\n", this);
  mOnEntInitialize   = LUA_NOREF;
  mOnEntUninitialize = LUA_NOREF;
  mOnEntLink         = LUA_NOREF;
  mOnEntActivate     = LUA_NOREF;
  mOnEntDeactivate   = LUA_NOREF;
  mOnEntStage        = LUA_NOREF;
  mOnEntUnstage      = LUA_NOREF;
  mOnEntUpdate       = LUA_NOREF;
  mModTabRef         = LUA_NOREF;
  mScriptRef         = LUA_NOREF;
}

///////////////////////////////////////////////////////////////////////////////

void LuaComponent::describeX(ObjectClass* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

LuaComponent::LuaComponent(const LuaComponentData& data, ecs::Entity* pent)
    : ork::ecs::Component(&data, pent)
    , mCD(data)
    , mScriptObject(nullptr) {
}

///////////////////////////////////////////////////////////////////////////////

void LuaComponent::_onUninitialize(ork::ecs::Simulation* psi) {
  if (mScriptObject) {
    auto scm      = psi->findSystem<LuaSystem>();
    auto asluasys = scm->GetLuaManager().get<LuaContext*>();
    OrkAssert(asluasys);
    auto L                = asluasys->mLuaState;
    LuaIntf::LuaState lua = L;
    auto ent              = this->GetEntity();
    if (mScriptObject->mOnEntUninitialize >= 0) {
      lua.getRef(mScriptObject->mOnEntUninitialize);
      assert(lua.isFunction(LUA_STACKINDEX_TOP));
      lua.push(_luaentity);
      // printf( "CALL mOnEntUninitialize\n");
      int iret = lua.pcall(1, 0, 0);
      OrkAssert(iret == 0);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

bool LuaComponent::_onLink(ork::ecs::Simulation* sim) {

  auto ent = this->GetEntity();
  auto scm = sim->findSystem<LuaSystem>();

  auto path = mCD.GetPath();

  logchan_luacomp->log( "LuaComponent::_onLink: scm<%p> path<%s>", scm, path.toAbsolute().c_str() );
  
  if (nullptr == scm){
    logerrchannel()->log( "LuaComponent::_onLink: scm is nullptr!!!!" );
    return false;
  }

  auto asluasys = scm->GetLuaManager().get<LuaContext*>();
  OrkAssert(asluasys);
  auto L                = asluasys->mLuaState;
  LuaIntf::LuaState lua = L;

  if (scm) {
    mScriptObject = scm->FlyweightScriptObject(path);

    if (mScriptObject) {
      auto ent = this->GetEntity();

      _luaentity        = LuaIntf::LuaRef::createTable(L);
      _luaentity["ent"] = ent;

      if (mScriptObject->mOnEntInitialize >= 0) {
        lua.getRef(mScriptObject->mOnEntInitialize);
        assert(lua.isFunction(LUA_STACKINDEX_TOP));
        lua.push(_luaentity);
        // printf( "CALL mOnEntInitialize\n");
        int iret = lua.pcall(1, 0, 0);
        OrkAssert(iret == 0);
      }
    }
  }

  if (mScriptObject) {
    auto asluasys = scm->GetLuaManager().get<LuaContext*>();
    OrkAssert(asluasys);
    auto L                = asluasys->mLuaState;
    LuaIntf::LuaState lua = L;
    auto ent              = this->GetEntity();
    if (mScriptObject->mOnEntLink >= 0) {
      lua.getRef(mScriptObject->mOnEntLink);
      assert(lua.isFunction(LUA_STACKINDEX_TOP));
      lua.push(_luaentity);
      int iret = lua.pcall(1, 0, 0);
      if (iret != 0) {
        logchan_luacomp->log("PCALL-ERROR: LuaComponent::_onLink: scm<%p> path<%s>", scm, path.toAbsolute().c_str());
        // printf( "CALL mOnEntLink\n");
      }
      OrkAssert(iret == 0);
    }
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void LuaComponent::_onUnlink(ork::ecs::Simulation* psi) {
  if (mScriptObject) {
    auto scm      = psi->findSystem<LuaSystem>();
    auto asluasys = scm->GetLuaManager().get<LuaContext*>();
    OrkAssert(asluasys);
    auto L                = asluasys->mLuaState;
    LuaIntf::LuaState lua = L;
    auto ent              = this->GetEntity();
    if (mScriptObject->mOnEntUnlink >= 0) {
      lua.getRef(mScriptObject->mOnEntUnlink);
      assert(lua.isFunction(LUA_STACKINDEX_TOP));
      lua.push(_luaentity);
      // printf( "CALL mOnEntUnlink\n");
      int iret = lua.pcall(1, 0, 0);
      OrkAssert(iret == 0);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

bool LuaComponent::_onStage(ork::ecs::Simulation* sim) {
  logchan_luacomp->log("LuaComponent::_onStage: mScriptObject<%p>", mScriptObject);
  if (mScriptObject) {
    auto scm      = sim->findSystem<LuaSystem>();
    auto asluasys = scm->GetLuaManager().get<LuaContext*>();
    OrkAssert(asluasys);
    auto L                = asluasys->mLuaState;
    LuaIntf::LuaState lua = L;
    auto ent              = this->GetEntity();
    if (mScriptObject->mOnEntStage >= 0) {
      lua.getRef(mScriptObject->mOnEntStage);
      assert(lua.isFunction(LUA_STACKINDEX_TOP));
      lua.push(_luaentity);
      // printf( "CALL mOnEntStage\n");
      int iret = lua.pcall(1, 0, 0);
      OrkAssert(iret == 0);
    }
  }
  return true;
}
///////////////////////////////////////////////////////////////////////////////

void LuaComponent::_onUnstage(ork::ecs::Simulation* psi) {
  if (mScriptObject) {
    auto scm      = psi->findSystem<LuaSystem>();
    auto asluasys = scm->GetLuaManager().get<LuaContext*>();
    OrkAssert(asluasys);
    auto L                = asluasys->mLuaState;
    LuaIntf::LuaState lua = L;
    auto ent              = this->GetEntity();
    if (mScriptObject->mOnEntUnstage >= 0) {
      lua.getRef(mScriptObject->mOnEntUnstage);
      assert(lua.isFunction(LUA_STACKINDEX_TOP));
      lua.push(_luaentity);
      // printf( "CALL mOnEntUnstage\n");
      int iret = lua.pcall(1, 0, 0);
      OrkAssert(iret == 0);
    }
  }
}
///////////////////////////////////////////////////////////////////////////////

bool LuaComponent::_onActivate(Simulation* psi) {
  auto scm = psi->findSystem<LuaSystem>();
  if (scm && mScriptObject) {
    scm->_onActivateComponent(this);
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void LuaComponent::_onDeactivate(Simulation* psi) {

  auto scm = psi->findSystem<LuaSystem>();
  if (scm && mScriptObject) {
    scm->_onDeactivateComponent(this);
  }
}

///////////////////////////////////////////////////////////////////////////////

void LuaComponent::_onNotify(Simulation* psi, token_t evID, evdata_t data) {

  ScriptVar luaID;
  luaID._encoded = evID;

  ScriptVar luadata;
  luadata._encoded = data;

  auto scm = psi->findSystem<LuaSystem>();

  //printf("LuaComponent<%p> scm<%p> mScriptObject<%p>\n", this, scm, (void*)mScriptObject);

  if (scm && mScriptObject) {

    //printf("  _onNotify<%p>\n", mScriptObject->mOnNotify);

    if (mScriptObject->mOnNotify >= 0) {
      auto asluactx = scm->GetLuaManager().get<LuaContext*>();
      OrkAssert(asluactx);
      auto L   = asluactx->mLuaState;
      auto ent = this->GetEntity();

      LuaIntf::LuaState lua = L;
      lua.getRef(mScriptObject->mOnNotify);
      OrkAssert(lua.isFunction(LUA_STACKINDEX_TOP));
      lua.push(_luaentity);
      luaID.pushToLua(L);
      luadata.pushToLua(L);
      int iret = lua.pcall(3, 0, 0);
      OrkAssert(iret == 0);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
