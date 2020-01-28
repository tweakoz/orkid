////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/any.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/util/md5.h>
#include <pkg/ent/ScriptComponent.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/scene.h>
#include <pkg/ent/scene.hpp>

#include <cxxabi.h>
#include <iostream>
#include <sstream>

///////////////////////////////////////////////////////////////////////////////

static const bool kUSEEXECTABUPDATE = false;

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ScriptComponentData, "ScriptComponentData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ScriptComponentInst, "ScriptComponentInst");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ScriptSystemData, "ScriptSystemData");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

constexpr int LUA_STACKINDEX_TOP = -1;

void ScriptComponentData::Describe() {
  ork::reflect::RegisterProperty("ScriptFile", &ScriptComponentData::mScriptPath);
  ork::reflect::annotatePropertyForEditor<ScriptComponentData>("ScriptFile", "editor.class", "ged.factory.filelist");
  ork::reflect::annotatePropertyForEditor<ScriptComponentData>("ScriptFile", "editor.filetype", "lua");
  ork::reflect::annotatePropertyForEditor<ScriptComponentData>("ScriptFile", "editor.filebase", "src://scripts/");
}

///////////////////////////////////////////////////////////////////////////////

ScriptComponentData::ScriptComponentData() {
  printf("ScriptComponentData::ScriptComponentData() this: %p\n", this);
}

///////////////////////////////////////////////////////////////////////////////

ent::ComponentInst* ScriptComponentData::createComponent(ent::Entity* pent) const {
  return new ScriptComponentInst(*this, pent);
}

///////////////////////////////////////////////////////////////////////////////

void ScriptComponentData::DoRegisterWithScene(ork::ent::SceneComposer& sc) {
  sc.Register<ork::ent::ScriptSystemData>();
}

///////////////////////////////////////////////////////////////////////////////

ScriptObject::ScriptObject()
    : mScriptRef(LUA_NOREF) {
  printf("new ScriptObject<%p>\n", this);
}
ScriptObject::~ScriptObject() {
  printf("deleting ScriptObject<%p>\n", this);
  mOnEntLink       = LUA_NOREF;
  mOnEntStart      = LUA_NOREF;
  mOnEntStop       = LUA_NOREF;
  mOnEntActivate   = LUA_NOREF;
  mOnEntDeactivate = LUA_NOREF;
  mOnEntUpdate     = LUA_NOREF;
  mModTabRef       = LUA_NOREF;
  mScriptRef       = LUA_NOREF;
}

///////////////////////////////////////////////////////////////////////////////

void ScriptComponentInst::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

ScriptComponentInst::ScriptComponentInst(const ScriptComponentData& data, ent::Entity* pent)
    : ork::ent::ComponentInst(&data, pent)
    , mCD(data)
    , mScriptObject(nullptr) {
}

///////////////////////////////////////////////////////////////////////////////

bool ScriptComponentInst::DoLink(ork::ent::Simulation* psi) {
  auto path = mCD.GetPath();

  auto scm = psi->findSystem<ScriptSystem>();

  if (nullptr == scm)
    return true;

  auto asluasys = scm->GetLuaManager().Get<LuaSystem*>();
  OrkAssert(asluasys);
  auto L = asluasys->mLuaState;

  if (scm) {
    mScriptObject = scm->FlyweightScriptObject(path);

    if (mScriptObject) {
      auto ent = this->GetEntity();

      LuaIntf::LuaState lua = L;
      _luaentity            = LuaIntf::LuaRef::createTable(L);
      _luaentity["ent"]     = ent;

      lua.getRef(mScriptObject->mOnEntLink);
      assert(lua.isFunction(LUA_STACKINDEX_TOP));
      lua.push(_luaentity);
      int iret = lua.pcall(1, 0, 0);
      OrkAssert(iret == 0);
    }
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void ScriptComponentInst::DoUnLink(ork::ent::Simulation* psi) {
  auto scm = psi->findSystem<ScriptSystem>();

  if (scm) {
    auto asluasys = scm->GetLuaManager().Get<LuaSystem*>();
    OrkAssert(asluasys);
  }
}

///////////////////////////////////////////////////////////////////////////////

bool ScriptComponentInst::DoStart(Simulation* psi, const fmtx4& world) {
  auto scm = psi->findSystem<ScriptSystem>();

  if (scm && mScriptObject) {
    auto asluasys = scm->GetLuaManager().Get<LuaSystem*>();
    OrkAssert(asluasys);
    auto L = asluasys->mLuaState;

    auto ent  = this->GetEntity();
    auto name = ent->name().c_str();

    // printf("Starting SCRIPTCOMPONENT<%p> of ent<%p:%s> into Lua exec list\n", this, ent, name);
    // printf("start ScriptObject<%p:%s>\n", mScriptObject, mScriptObject->mScriptPath.c_str());

    LuaIntf::LuaState lua = L;
    lua.getRef(mScriptObject->mOnEntStart);
    assert(lua.isFunction(LUA_STACKINDEX_TOP));
    lua.push(_luaentity);
    int iret = lua.pcall(1, 0, 0);
    OrkAssert(iret == 0);
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void ScriptComponentInst::onActivate(Simulation* psi) {

  auto scm = psi->findSystem<ScriptSystem>();

  if (scm && mScriptObject) {
    auto asluasys = scm->GetLuaManager().Get<LuaSystem*>();
    OrkAssert(asluasys);
    auto L = asluasys->mLuaState;

    auto ent  = this->GetEntity();
    auto name = ent->name().c_str();

    // printf("Activating SCRIPTCOMPONENT<%p> of ent<%p:%s> into Lua exec list\n", this, ent, name);
    // printf("activate ScriptObject<%p:%s>\n", mScriptObject, mScriptObject->mScriptPath.c_str());

    LuaIntf::LuaState lua = L;
    lua.getRef(mScriptObject->mOnEntActivate);
    assert(lua.isFunction(LUA_STACKINDEX_TOP));
    lua.push(_luaentity);
    int iret = lua.pcall(1, 0, 0);
    OrkAssert(iret == 0);
  }
}

///////////////////////////////////////////////////////////////////////////////

void ScriptComponentInst::onDeactivate(Simulation* psi) {
  auto scm = psi->findSystem<ScriptSystem>();

  if (scm && mScriptObject) {
    auto asluasys = scm->GetLuaManager().Get<LuaSystem*>();
    OrkAssert(asluasys);
    auto L = asluasys->mLuaState;

    auto ent  = this->GetEntity();
    auto name = ent->name().c_str();

    // printf("Deactivating SCRIPTCOMPONENT<%p> of ent<%p:%s> into Lua exec list\n", this, ent, name);

    LuaIntf::LuaState lua = L;
    lua.getRef(mScriptObject->mOnEntDeactivate);
    assert(lua.isFunction(LUA_STACKINDEX_TOP));
    lua.push(_luaentity);
    int iret = lua.pcall(1, 0, 0);
    OrkAssert(iret == 0);
  }
}

///////////////////////////////////////////////////////////////////////////////

void ScriptComponentInst::DoStop(Simulation* psi) {
  auto scm = psi->findSystem<ScriptSystem>();

  if (scm && mScriptObject) {
    auto asluasys = scm->GetLuaManager().Get<LuaSystem*>();
    OrkAssert(asluasys);
    auto L = asluasys->mLuaState;

    auto ent  = this->GetEntity();
    auto name = ent->name().c_str();

    LuaIntf::LuaState lua = L;
    lua.getRef(mScriptObject->mOnEntStop);
    assert(lua.isFunction(LUA_STACKINDEX_TOP));
    lua.push(_luaentity);
    int iret = lua.pcall(1, 0, 0);
    OrkAssert(iret == 0);
    //////////////////////////////////////////

    // printf( "LINKING SCRIPTCOMPONENT<%p> of ent<%s> into Lua exec list\n", this, name );
  }
}

///////////////////////////////////////////////////////////////////////////////

void ScriptComponentInst::DoUpdate(ork::ent::Simulation* psi) {

  if (false == kUSEEXECTABUPDATE) {
    auto scm = psi->findSystem<ScriptSystem>();

    if (scm && mScriptObject) {
      auto asluasys = scm->GetLuaManager().Get<LuaSystem*>();
      OrkAssert(asluasys);
      auto L    = asluasys->mLuaState;
      auto ent  = this->GetEntity();
      double dt = psi->GetDeltaTime();
      double gt = psi->GetGameTime();

      // printf("update ScriptObject<%p:%s>\n", mScriptObject, mScriptObject->mScriptPath.c_str());

      LuaIntf::LuaState lua = L;
      lua.getRef(mScriptObject->mOnEntUpdate);
      OrkAssert(lua.isFunction(LUA_STACKINDEX_TOP));
      lua.push(_luaentity);
      lua.push(dt);
      int iret = lua.pcall(2, 0, 0);
      OrkAssert(iret == 0);
    }
  }
  // NOP (scriptmanager will execute)
}

///////////////////////////////////////////////////////////////////////////////

void ScriptSystemData::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

ScriptSystemData::ScriptSystemData() {
}

///////////////////////////////////////////////////////////////////////////////

System* ScriptSystemData::createSystem(ork::ent::Simulation* pinst) const {
  return new ScriptSystem(*this, pinst);
}

///////////////////////////////////////////////////////////////////////////////

ScriptSystem::ScriptSystem(const ScriptSystemData& data, ork::ent::Simulation* pinst)
    : ork::ent::System(&data, pinst)
    , mScriptRef(LUA_NOREF) {
  printf("SCMI<%p>\n", this);
  auto luasys = new LuaSystem(pinst);
  mLuaManager.Set<LuaSystem*>(luasys);

  auto luapath = getenv("LUA_PATH");
  assert(luapath != nullptr);

  ///////////////////////////////////////////////

  auto AppendPath = [&](const char* pth) {
    lua_getglobal(luasys->mLuaState, "package");
    lua_getfield(luasys->mLuaState, -1, "path");

    fxstring<256> lua_path;
    lua_path.format("%s;%s", lua_tostring(luasys->mLuaState, -1), pth);

    lua_pop(luasys->mLuaState, 1);
    lua_pushstring(luasys->mLuaState, lua_path.c_str());
    lua_setfield(luasys->mLuaState, -2, "path");
    lua_pop(luasys->mLuaState, 1);
  };

  ///////////////////////////////////////////////
  // Set Lua Search Path
  ///////////////////////////////////////////////

  auto searchpath  = file::Path("src://scripts/");
  auto abssrchpath = searchpath.ToAbsolute();

  if (abssrchpath.DoesPathExist()) {
    fxstring<256> lua_path;
    lua_path.format("%s?.lua", abssrchpath.c_str());
    AppendPath(lua_path.c_str());
  }

  ///////////////////////////////////////////////
  // find & init scene file
  ///////////////////////////////////////////////

  const auto& scenedata = pinst->GetData();
  auto path             = scenedata._sceneScriptPath;
  auto abspath          = path.ToAbsolute();

  if (abspath.DoesPathExist()) {
    File scriptfile(abspath, EFM_READ);
    size_t filesize = 0;
    scriptfile.GetLength(filesize);
    char* scripttext = (char*)malloc(filesize + 1);
    scriptfile.Read(scripttext, filesize);
    scripttext[filesize] = 0;
    mScriptText          = scripttext;
    // printf( "%s\n", scripttext);
    free(scripttext);

    auto asluasys = mLuaManager.Get<LuaSystem*>();
    OrkAssert(asluasys);

    int ret = luaL_loadstring(asluasys->mLuaState, mScriptText.c_str());

    mScriptRef = luaL_ref(asluasys->mLuaState, LUA_REGISTRYINDEX);
    // printf( "mScriptRef<%d>\n", mScriptRef );
    // lua_pop(asluasys->mLuaState, 1); // dont call, just reference

    // LuaProtectedCallByRef( asluasys->mLuaState, mScriptRef );

    // LuaProtectedCallByName( asluasys->mLuaState, mScriptRef, "OnSceneCompose");
  }
}

///////////////////////////////////////////////////////////////////////////////

ScriptSystem::~ScriptSystem() {
  //////////////////////////////
  // delete flyweighted scriptobjects
  //////////////////////////////

  for (auto item : mScriptObjects) {
    auto so = item.second;
    delete so;
  }

  //////////////////////////////
  // delete lua context
  //////////////////////////////
  auto asluasys = mLuaManager.Get<LuaSystem*>();
  OrkAssert(asluasys);
  delete asluasys;
}

///////////////////////////////////////////////////////////////////////////////

bool ScriptSystem::DoLink(Simulation* psi) // final
{
  printf("ScriptSystem::DoLink()\n");
  auto asluasys = mLuaManager.Get<LuaSystem*>();
  OrkAssert(asluasys);
  // LuaProtectedCallByName( asluasys->mLuaState, mScriptRef, "OnSceneLink");

  return true;
}

///////////////////////////////////////////////////////////////////////////////

void ScriptSystem::DoUnLink(Simulation* psi) // final
{
  printf("ScriptSystem::DoUnLink()\n");
  auto asluasys = mLuaManager.Get<LuaSystem*>();
  OrkAssert(asluasys);
  // LuaProtectedCallByName( asluasys->mLuaState, mScriptRef, "OnSceneUnLink");
}

///////////////////////////////////////////////////////////////////////////////

void ScriptSystem::DoStart(Simulation* psi) // final
{
  printf("ScriptSystem::DoStart()\n");
  auto asluasys = mLuaManager.Get<LuaSystem*>();
  OrkAssert(asluasys);
  // LuaProtectedCallByName( asluasys->mLuaState, mScriptRef, "OnSceneStart");
}

///////////////////////////////////////////////////////////////////////////////

void ScriptSystem::DoStop(Simulation* inst) // final
{
  printf("ScriptSystem::DoStop()\n");
  auto asluasys = mLuaManager.Get<LuaSystem*>();
  OrkAssert(asluasys);
  // LuaProtectedCallByName( asluasys->mLuaState, mScriptRef, "OnSceneStop");
}

///////////////////////////////////////////////////////////////////////////////

void ScriptSystem::DoUpdate(Simulation* psi) // final
{
  auto asluasys = mLuaManager.Get<LuaSystem*>();
  OrkAssert(asluasys);
  auto lstate = asluasys->mLuaState;

  double dt = psi->GetDeltaTime();
  double gt = psi->GetGameTime();

  // LuaProtectedCallByName( asluasys->mLuaState, mScriptRef, "OnSceneUpdate", ldt,lgt);

  if (kUSEEXECTABUPDATE) {
    // luabind::object o(asluasys->mLuaState, dt );
    // LuaProtectedCallByName( asluasys->mLuaState, mScriptRef, "UpdateSceneEntities",ldt);
  }
}

///////////////////////////////////////////////////////////////////////////////
// FlyweightScriptObject - load every script file only once
//  share across different entity instances
///////////////////////////////////////////////////////////////////////////////

ScriptObject* ScriptSystem::FlyweightScriptObject(const ork::file::Path& pth) {
  auto abspath  = pth.ToAbsolute();
  auto asluasys = GetLuaManager().Get<LuaSystem*>();
  OrkAssert(asluasys);
  auto luast = asluasys->mLuaState;

  ScriptObject* rval = nullptr;

  auto it = mScriptObjects.find(pth);
  if (it == mScriptObjects.end()) {
    if (abspath.DoesPathExist()) {
      rval = new ScriptObject;

      //////////////////////////////////////////
      // load script text
      //////////////////////////////////////////

      File scriptfile(abspath, EFM_READ);
      size_t filesize = 0;
      scriptfile.GetLength(filesize);
      char* scripttext = (char*)malloc(filesize + 1);
      scriptfile.Read(scripttext, filesize);
      scripttext[filesize] = 0;
      rval->mScriptText    = scripttext;
      rval->mScriptPath    = abspath.c_str();

      //////////////////////////////////////////
      // prefix global method names to scope them
      //////////////////////////////////////////

      int script_index = mScriptObjects.size();
      script_funcname_t postfix;
      postfix.format("_%04x", script_index);

      // printf( "\n%s\n", rval->mScriptText.c_str() );

      //////////////////////////////////////////
      // load chunk into lua and reference it
      //////////////////////////////////////////

      // int ret = luaL_loadstring(luast,rval->mScriptText.c_str());
      auto script_text = rval->mScriptText.c_str();
      auto script_len  = rval->mScriptText.length();
      int ret          = luaL_loadbuffer(luast, script_text, script_len, pth.c_str());

      rval->mScriptRef = luaL_ref(luast, LUA_REGISTRYINDEX);

      assert(rval->mScriptRef != LUA_NOREF);
      lua_rawgeti(luast, LUA_REGISTRYINDEX, rval->mScriptRef);
      // execute it
      ret = lua_pcall(luast, 0, 1, 0);
      if (ret) {
        printf("\n%s\n", rval->mScriptText.c_str());
        printf("LUAERRCODE<%d>\n", ret);
        printf("LUAERR<%s>\n", lua_tostring(luast, -1));
        assert(false);
      }
      rval->mModTabRef = luaL_ref(luast, LUA_REGISTRYINDEX);

      auto getMethodRef = [luast, rval](const char* methodname) -> int {
        lua_rawgeti(luast, LUA_REGISTRYINDEX, rval->mModTabRef);
        lua_pushstring(luast, methodname);
        lua_gettable(luast, -2);
        assert(lua_type(luast, -1) == LUA_TFUNCTION);
        int rval = luaL_ref(luast, LUA_REGISTRYINDEX);
        return rval;
      };

      rval->mOnEntLink       = getMethodRef("OnEntityLink");
      rval->mOnEntStart      = getMethodRef("OnEntityStart");
      rval->mOnEntActivate   = getMethodRef("OnEntityActivate");
      rval->mOnEntDeactivate = getMethodRef("OnEntityDeactivate");
      rval->mOnEntStop       = getMethodRef("OnEntityStop");
      rval->mOnEntUpdate     = getMethodRef("OnEntityUpdate");

      // LuaProtectedCallByRef( luast, rval->mScriptRef );

      // lua_getfenv(luast,-1);
      // assert(lua_setfenv(luast, -1) != 0);

      printf("Script<%s> Loaded\n", abspath.c_str());

      //////////////////////////////////////////
      // flyweight it
      //////////////////////////////////////////

      mScriptObjects[pth] = rval;
    }

  } else {
    rval = it->second;
  }

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::ent
