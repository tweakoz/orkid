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

// #include "LuaIntf/LuaIntf.h"
#include "PythonImpl.h"

///////////////////////////////////////////////////////////////////////////////

static const bool kUSEEXECTABUPDATE = false;

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////
using namespace ork;
using namespace ork::object;
using namespace ork::reflect;

static logchannel_ptr_t logchan_pysys = logger()->createChannel("ecs.pysys", fvec3(0.9, 0.6, 0.0));

///////////////////////////////////////////////////////////////////////////////

void PythonSystemData::describeX(SystemDataClass* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

PythonSystemData::PythonSystemData() {
}

///////////////////////////////////////////////////////////////////////////////

System* PythonSystemData::createSystem(ork::ecs::Simulation* pinst) const {
  return new PythonSystem(*this, pinst);
}

///////////////////////////////////////////////////////////////////////////////

PythonSystem::PythonSystem(const PythonSystemData& data, ork::ecs::Simulation* pinst)
    : ork::ecs::System(&data, pinst) {
  //, mScriptRef(LUA_NOREF) {

  _onSystemUpdate = data._onSystemUpdate;

  logchan_pysys->log("PythonSystem::PythonSystem() <%p>", this);
  //auto pyctx = new pysys::PythonContext(pinst, this);
  //mPythonManager.set<pysys::PythonContext*>(pyctx);

  //pyctx->bindSubInterpreter();
  // pybind11::scoped_interpreter guard{};
  // pyctx->unbindSubInterpreter();

  ///////////////////////////////////////////////

  auto AppendPath = [&](const char* pth) {
    /*
    lua_getglobal(pyctx->mLuaState, "package");
    lua_getfield(pyctx->mLuaState, -1, "path");

    //logchan_pysys->log("PythonSystem AppendPath pth<%s>", pth );

    auto orig_path = lua_tostring(pyctx->mLuaState, -1);

    //logchan_pysys->log("PythonSystem AppendPath orig_path<%s>", orig_path );

    fxstring<1024> lua_path;
    lua_path.format("%s;%s", orig_path, pth);

    lua_pop(pyctx->mLuaState, 1);
    lua_pushstring(pyctx->mLuaState, lua_path.c_str());
    lua_setfield(pyctx->mLuaState, -2, "path");
    lua_pop(pyctx->mLuaState, 1);

    logchan_pysys->log("PythonSystem AppendPath lua_path<%s>", lua_path.c_str() );
    */
  };

  ///////////////////////////////////////////////
  // Set Lua Search Path
  ///////////////////////////////////////////////

  std::string orkdirstr;
  genviron.get("ORKID_WORKSPACE_DIR", orkdirstr);
  OrkAssert(orkdirstr != "");
  auto orkidWorkspaceDir = file::Path(orkdirstr);
  auto searchpath        = (orkidWorkspaceDir / "ork.data" / "src" / "scripts");
  auto abssrchpath       = searchpath.toAbsolute();
  OrkAssert(abssrchpath.doesPathExist());

  if (abssrchpath.doesPathExist()) {
    fxstring<1024> lua_path;
    lua_path.format("%s/?.lua", abssrchpath.c_str());
    AppendPath(lua_path.c_str());
  }

  // logchan_pysys->log("PythonSystem LUA_PATH <%s>", abssrchpath.c_str() );

  ///////////////////////////////////////////////
  // find & init scene file
  ///////////////////////////////////////////////

  auto scenedata = pinst->GetData();
  auto path      = scenedata->_sceneScriptPath;
  auto abspath   = path.toAbsolute();

  if (abspath.doesPathExist()) {
    File scriptfile(abspath, EFM_READ);
    size_t filesize = 0;
    scriptfile.GetLength(filesize);
    char* scripttext = (char*)malloc(filesize + 1);
    scriptfile.Read(scripttext, filesize);
    scripttext[filesize] = 0;
    mScriptText          = scripttext;
    // printf( "%s\n", scripttext);
    free(scripttext);

    //auto as_context = mPythonManager.get<pysys::PythonContext*>();
    //OrkAssert(as_context);

    //as_context->bindSubInterpreter();
    //pybind11::scoped_interpreter guard{};
    //as_context->unbindSubInterpreter();

    // int ret = luaL_loadstring(as_context->mLuaState, mScriptText.c_str());

    // mScriptRef = luaL_ref(as_context->mLuaState, LUA_REGISTRYINDEX);
    // logchan_pysys->log( "mScriptRef<%d>", mScriptRef );
    //   lua_pop(as_context->mLuaState, 1); // dont call, just reference

    // LuaProtectedCallByRef( as_context->mLuaState, mScriptRef );

    // LuaProtectedCallByName( as_context->mLuaState, mScriptRef, "OnSceneCompose");
  }
}

///////////////////////////////////////////////////////////////////////////////

PythonSystem::~PythonSystem() {
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
  //auto as_context = mPythonManager.get<pysys::PythonContext*>();
  //OrkAssert(as_context);
  //delete as_context;
}

///////////////////////////////////////////////////////////////////////////////

void PythonSystem::_onActivateComponent(PythonComponent* component) {

  //auto as_context = this->getManager().get<pysys::PythonContext*>();
  //OrkAssert(as_context);
  /*
  auto L = as_context->mLuaState;

  auto ent  = component->GetEntity();
  auto name = ent->name().c_str();

  // printf("Starting PythonComponent<%p> of ent<%p:%s> into Lua exec list\n", this, ent, name);
  // printf("start ScriptObject<%p:%s>\n", mScriptObject, mScriptObject->mScriptPath.c_str());

  if (component->mScriptObject->mOnEntActivate >= 0) {
    LuaIntf::LuaState lua = L;
    lua.getRef(component->mScriptObject->mOnEntActivate);
    assert(lua.isFunction(LUA_STACKINDEX_TOP));
    lua.push(component->_luaentity);
    // printf( "CALL mOnEntStart\n");
    int iret = lua.pcall(1, 0, 0);
    OrkAssert(iret == 0);
  }
  */
  _activeComponents.insert(component);
}
void PythonSystem::_onDeactivateComponent(PythonComponent* component) {

  auto it = _activeComponents.find(component);
  _activeComponents.erase(component);
  /*
  auto as_context = this->getManager().get<pysys::PythonContext*>();
  OrkAssert(as_context);
  auto L = as_context->mLuaState;

  if (component->mScriptObject->mOnEntDeactivate >= 0) {

    auto ent  = component->GetEntity();
    auto name = ent->name().c_str();

    // printf("Activating PythonComponent<%p> of ent<%p:%s> into Lua exec list\n", this, ent, name);
    // printf("activate ScriptObject<%p:%s>\n", mScriptObject, mScriptObject->mScriptPath.c_str());
    // printf("mOnEntActivate<%d>\n", mScriptObject->mOnEntActivate);

    LuaIntf::LuaState lua = L;
    lua.getRef(component->mScriptObject->mOnEntDeactivate);
    assert(lua.isFunction(LUA_STACKINDEX_TOP));
    lua.push(component->_luaentity);
    // printf( "CALL mOnEntActivate\n");
    int iret = lua.pcall(1, 0, 0);
    OrkAssert(iret == 0);
  }
  */
}

///////////////////////////////////////////////////////////////////////////////

bool PythonSystem::_onLink(Simulation* psi) // final
{
  logchan_pysys->log("_onLink() ");
  // printf("PythonSystem::DoLink()\n");
  //auto as_context = mPythonManager.get<pysys::PythonContext*>();
  //OrkAssert(as_context);
  // LuaProtectedCallByName( as_context->mLuaState, mScriptRef, "OnSceneLink");

  return true;
}

///////////////////////////////////////////////////////////////////////////////

void PythonSystem::_onUnLink(Simulation* psi) // final
{
  logchan_pysys->log("_onUnLink() ");
  // printf("PythonSystem::DoUnLink()\n");
  //auto as_context = mPythonManager.get<pysys::PythonContext*>();
  //OrkAssert(as_context);
  // LuaProtectedCallByName( as_context->mLuaState, mScriptRef, "OnSceneUnLink");
}

///////////////////////////////////////////////////////////////////////////////

bool PythonSystem::_onActivate(Simulation* psi) // final
{
  logchan_pysys->log("_onActivate() ");
  //auto as_context = mPythonManager.get<pysys::PythonContext*>();
  //OrkAssert(as_context);
  // LuaProtectedCallByName( as_context->mLuaState, mScriptRef, "OnSceneStart");
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void PythonSystem::_onDeactivate(Simulation* inst) // final
{
  logchan_pysys->log("_onDeactivate() ");
  //auto as_context = mPythonManager.get<pysys::PythonContext*>();
 // OrkAssert(as_context);
  // LuaProtectedCallByName( as_context->mLuaState, mScriptRef, "OnSceneStop");
}

///////////////////////////////////////////////////////////////////////////////

bool PythonSystem::_onStage(Simulation* inst) {
  logchan_pysys->log("_onStage() ");
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void PythonSystem::_onUnstage(Simulation* inst) {
  logchan_pysys->log("_onUnstage() ");
}

///////////////////////////////////////////////////////////////////////////////

void PythonSystem::_onUpdate(Simulation* psi) // final
{
  //auto as_context = mPythonManager.get<pysys::PythonContext*>();
  //OrkAssert(as_context);

  double dt = psi->deltaTime();
  double gt = psi->gameTime();

  if (_onSystemUpdate) {
    /*
    as_context->bindSubInterpreter();
    auto controller = psi->controller();
    as_context->eval(
        "print('Hello')\n"
        "print('world!');"
        );
    //_onSystemUpdate(nullptr);
    // controller->_simulation.atomicOp([this](simulation_ptr_t& unlocked){
    // });
    as_context->unbindSubInterpreter();*/
  }
  /*

  auto lstate = as_context->mLuaState;

  //logchan_pysys->log( "_onUpdate() ");

  // LuaProtectedCallByName( as_context->mLuaState, mScriptRef, "OnSceneUpdate", ldt,lgt);

  if (kUSEEXECTABUPDATE) {
    // luabind::object o(as_context->mLuaState, dt );
    // LuaProtectedCallByName( as_context->mLuaState, mScriptRef, "UpdateSceneEntities",ldt);
  } else {
    auto as_context = this->getManager().get<pysys::PythonContext*>();
    OrkAssert(as_context);
    auto L = as_context->mLuaState;
    for (auto c : _activeComponents) {
      if (c->mScriptObject) {
        if (c->mScriptObject->mOnEntUpdate >= 0) {
          auto ent              = c->GetEntity();
          LuaIntf::LuaState lua = L;
          lua.getRef(c->mScriptObject->mOnEntUpdate);
          OrkAssert(lua.isFunction(LUA_STACKINDEX_TOP));
          lua.push(c->_luaentity);
          lua.push(dt);
          //printf( "CALL mOnEntUpdate c<%p> dt<%g>\n", c, dt);
          int iret = lua.pcall(2, 0, 0);
          OrkAssert(iret == 0);
        }
      }
    }
  }
  */
}

///////////////////////////////////////////////////////////////////////////////
// FlyweightScriptObject - load every script file only once
//  share across different entity instances
///////////////////////////////////////////////////////////////////////////////

/*
ScriptObject* PythonSystem::FlyweightScriptObject(const ork::file::Path& pth) {
  auto abspath  = pth.toAbsolute();
  auto as_context = getManager().get<pysys::PythonContext*>();
  OrkAssert(as_context);
  auto luast = as_context->mLuaState;

  ScriptObject* rval = nullptr;

  //printf("##### FlyweightScriptObject %s\n", abspath.c_str());

  auto it = mScriptObjects.find(pth);
  if (it == mScriptObjects.end()) {
    if (abspath.doesPathExist()) {
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
      // printf( "CALL mScriptRef\n");
      ret = lua_pcall(luast, 0, 1, 0);
      // printf( "CALL mScriptRef ret<%d>\n", ret);
      if (ret) {
        printf("\n%s\n", rval->mScriptText.c_str());
        printf("LUAERRCODE<%d>\n", ret);
        printf("LUAERR<%s>\n", lua_tostring(luast, -1));
        assert(false);
      }
      ///////////////////////////////
      // if you get a crash in the following lua ref
      //  make sure the script is returning a function table!
      ///////////////////////////////
      rval->mModTabRef = luaL_ref(luast, LUA_REGISTRYINDEX);
      // printf( "rval->mModTabRef<%d>\n", rval->mModTabRef);
      ///////////////////////////////
      auto getMethodRef = [luast, rval](const char* methodname) -> int {
        lua_rawgeti(luast, LUA_REGISTRYINDEX, rval->mModTabRef);
        lua_pushstring(luast, methodname);
        lua_gettable(luast, -2);
        // assert(lua_type(luast, -1) == LUA_TFUNCTION);
        int rval = luaL_ref(luast, LUA_REGISTRYINDEX);
        // printf("getMethodRef<%s> rval<%d>\n", methodname, rval);
        return rval;
      };

      rval->mOnEntInitialize   = getMethodRef("OnEntityInitialize");
      rval->mOnEntUninitialize = getMethodRef("OnEntityUninitialize");
      rval->mOnEntLink         = getMethodRef("OnEntityLink");
      rval->mOnEntUnlink       = getMethodRef("OnEntitiyUnlink");
      rval->mOnEntStage        = getMethodRef("OnEntityStage");
      rval->mOnEntUnstage      = getMethodRef("OnEntityUnstage");
      rval->mOnEntActivate     = getMethodRef("OnEntityActivate");
      rval->mOnEntDeactivate   = getMethodRef("OnEntityDeactivate");
      rval->mOnEntUpdate       = getMethodRef("OnEntityUpdate");
      rval->mOnNotify          = getMethodRef("OnNotify");

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
*/
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
