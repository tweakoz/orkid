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

#include "PythonImpl.h"

namespace py = pybind11;
using namespace pybind11::literals;

///////////////////////////////////////////////////////////////////////////////
namespace ork::python {
void init_crcstring(py::module& module_core, python::typecodec_ptr_t type_codec);
}
///////////////////////////////////////////////////////////////////////////////

namespace ork::ecssim {

using namespace ::ork::python;

typecodec_ptr_t simonly_codec_instance() { // static
  struct TypeCodecFactory : public TypeCodec {
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

std::shared_ptr<pysys::EcsGlobalState> getGlobalState() {
  static auto gstate = std::make_shared<pysys::EcsGlobalState>();
  return gstate;
}

///////////////////////////////////////////////////////////////////////////////

PythonSystem::PythonSystem(const PythonSystemData& data, ork::ecs::Simulation* pinst)
    : ork::ecs::System(&data, pinst) {
  //, mScriptRef(LUA_NOREF) {

  //_onSystemUpdate = data._onSystemUpdate;

  logchan_pysys->log("PythonSystem::PythonSystem() <%p>", this);
  auto ecsgstate = getGlobalState();
  auto gstate = std::make_shared<::ork::python::GlobalState>();
  gstate->_mainInterpreter = ecsgstate->_mainInterpreter;
  gstate->_globalInterpreter = ecsgstate->_globalInterpreter;
  _pythonContext = std::make_shared<pyctx_t>(gstate);

  // pybind11::scoped_interpreter guard{};

  ///////////////////////////////////////////////

  auto AppendPath = [&](const char* pth) {
    /*
    lua_getglobal(_pythonContext->mLuaState, "package");
    lua_getfield(_pythonContext->mLuaState, -1, "path");

    //logchan_pysys->log("PythonSystem AppendPath pth<%s>", pth );

    auto orig_path = lua_tostring(_pythonContext->mLuaState, -1);

    //logchan_pysys->log("PythonSystem AppendPath orig_path<%s>", orig_path );

    fxstring<1024> lua_path;
    lua_path.format("%s;%s", orig_path, pth);

    lua_pop(_pythonContext->mLuaState, 1);
    lua_pushstring(_pythonContext->mLuaState, lua_path.c_str());
    lua_setfield(_pythonContext->mLuaState, -2, "path");
    lua_pop(_pythonContext->mLuaState, 1);

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
  auto path      = data._sceneScriptPath;
  auto abspath   = path.toAbsolute();

  _pythonContext->bindSubInterpreter();
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

    try {
      // Execute the script in the subinterpreter context and capture the module
      _systemScript                  = pybind11::module_::import("__main__");
      _systemScript.attr("__file__") = abspath;
      pybind11::dict globals         = _systemScript.attr("__dict__");
      globals["__file__"]            = abspath;

      pybind11::module_ sys            = pybind11::module_::import("sys");
      pybind11::list original_sys_path = sys.attr("path");
      pybind11::module_ math           = pybind11::module_::import("math");
      globals["MATH"]                  = math;
      //pybind11::module_ ecssim         = pybind11::module_::import("orkengine.ecssim");
      //globals["ECS"]                   = ecssim;
      // from orkengine.core import CrcString
      //pybind11::module_ core = pybind11::module_::import("orkengine.core");
      //globals["CORE"]        = core; 
      //pybind11::module_ OPENCL = pybind11::module_::import("pyopencl");
      //globals["CL"]        = OPENCL;
      
      // globals["CrcStringProxy"] = core.attr("CrcStringProxy");

      // auto module_core = py::module::create_extension_module(
      //"core", nullptr, new py::module_::module_def );
      //::ork::python::init_crcstring(module_core, ecssim::simonly_codec_instance());
      //sys.attr("path") = pybind11::list();

      if (1) {
        pybind11::exec(mScriptText.c_str(), globals, globals);
      } else {

        pybind11::object compile = pybind11::module_::import("builtins").attr("compile");
        pybind11::object code    = compile(mScriptText, (std::string)abspath.c_str(), "exec");

        // Execute the compiled code object
        pybind11::exec(code, globals, globals);
      }

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

      if (_pymethodOnSystemInit) {
        __pcallargs(logchan_pysys, _pymethodOnSystemInit, pinst);
      }

    } catch (pybind11::error_already_set& e) {
      logchan_pysys->log("Error executing Python script: %s\n", abspath.c_str());
      e.restore();
      PyErr_Print();
      OrkAssert(false);
    } catch (const std::exception& e) {
      logchan_pysys->log("Error executing Python script: %s\n", e.what());
      OrkAssert(false);
    }
  }
  _pythonContext->unbindSubInterpreter();
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
}

///////////////////////////////////////////////////////////////////////////////

void PythonSystem::_onActivateComponent(PythonComponent* component) {

  /*
  auto L = _pythonContext->mLuaState;

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

  auto L = _pythonContext->mLuaState;

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
  // LuaProtectedCallByName( _pythonContext->mLuaState, mScriptRef, "OnSceneLink");
  if (_pymethodOnSystemLink) {
    _pythonContext->bindSubInterpreter();
    __pcallargs(logchan_pysys, _pymethodOnSystemLink, psi);
    _pythonContext->unbindSubInterpreter();
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////

void PythonSystem::_onUnLink(Simulation* psi) // final
{
  logchan_pysys->log("_onUnLink() ");
  // printf("PythonSystem::DoUnLink()\n");
  // LuaProtectedCallByName( _pythonContext->mLuaState, mScriptRef, "OnSceneUnLink");
}

///////////////////////////////////////////////////////////////////////////////

bool PythonSystem::_onActivate(Simulation* psi) // final
{
  logchan_pysys->log("_onActivate() ");
  // LuaProtectedCallByName( _pythonContext->mLuaState, mScriptRef, "OnSceneStart");
  if (_pymethodOnSystemActivate) {
    _pythonContext->bindSubInterpreter();
    __pcallargs(logchan_pysys, _pymethodOnSystemActivate, psi);
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
  logchan_pysys->log("_onStage() ");
  if (_pymethodOnSystemStage) {
    _pythonContext->bindSubInterpreter();
    __pcallargs(logchan_pysys, _pymethodOnSystemStage, psi);
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
  bool has_notify = bool(_pymethodOnSystemNotify);
  if (has_notify) {
    // logchan_pysys->log("_onNotify() %d", int(has_notify));
    auto evIDcrc = std::make_shared<CrcString>();
    (*evIDcrc)   = evID;
    //_pythonContext->bindSubInterpreter();
    auto table = data.getShared<DataTable>();
    __pcallargs(logchan_pysys, _pymethodOnSystemNotify, simulation(), evIDcrc, table);
    //_pythonContext->unbindSubInterpreter();
  }
}

///////////////////////////////////////////////////////////////////////////////

void PythonSystem::_onUpdate(Simulation* psi) // final
{
  double dt = psi->deltaTime();
  double gt = psi->gameTime();

  if (_pymethodOnSystemUpdate) {
    __pcallargs(logchan_pysys, _pymethodOnSystemUpdate, psi);
  }

  if (_onSystemUpdate) {
    auto controller = psi->controller();
    _onSystemUpdate(nullptr);
    // controller->_simulation.atomicOp([this](simulation_ptr_t& unlocked){
    // });
  }
  /*

  auto lstate = _pythonContext->mLuaState;

  //logchan_pysys->log( "_onUpdate() ");

  // LuaProtectedCallByName( _pythonContext->mLuaState, mScriptRef, "OnSceneUpdate", ldt,lgt);

  if (kUSEEXECTABUPDATE) {
    // luabind::object o(_pythonContext->mLuaState, dt );
    // LuaProtectedCallByName( _pythonContext->mLuaState, mScriptRef, "UpdateSceneEntities",ldt);
  } else {

    auto L = _pythonContext->mLuaState;
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

  auto luast = _pythonContext->mLuaState;

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
