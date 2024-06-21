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

//#include "LuaIntf/LuaIntf.h"
#include "PythonImpl.h"

///////////////////////////////////////////////////////////////////////////////

static const bool kUSEEXECTABUPDATE = false;

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs::pysys {
///////////////////////////////////////////////////////////////////////////////
using namespace ork;
using namespace ork::object;
using namespace ork::reflect;

static logchannel_ptr_t logchan_pyctx = logger()->createChannel("ecs.pyctx",fvec3(0.9,0.6,0.0));

struct GlobalState{

  GlobalState(){
    Py_Initialize();
    _globalInterpreter = PyThreadState_Get();
    _mainInterpreter = Py_NewInterpreter();
    logchan_pyctx->log("global python _mainInterpreter<%p>\n", (void*) _mainInterpreter );
  }
  PyThreadState* _mainInterpreter = nullptr;
  PyThreadState* _globalInterpreter = nullptr;
};

globalstate_ptr_t getGlobalState() {
  static globalstate_ptr_t gstate = std::make_shared<GlobalState>();
  return gstate;
}

PythonContext::PythonContext(Simulation* psi, PythonSystem* system){
  _subInterpreter = Py_NewInterpreter();
    logchan_pyctx->log("pyctx<%p> _subInterpreter<%p>\n", this, (void*) _subInterpreter );
  PyThreadState_Swap(_subInterpreter);
    logchan_pyctx->log("pyctx<%p> 1...\n", this );
   //pybind11::initialize_interpreter();
    //logchan_pyctx->log("pyctx<%p> created...\n", this );
}

PythonContext::~PythonContext(){
  logchan_pyctx->log("~pyctx<%p> _subInterpreter<%p>\n", this, (void*) _subInterpreter );
  PyThreadState_Swap(_subInterpreter);
  logchan_pyctx->log("~pyctx<%p> finalize<%p>\n", this, (void*) _subInterpreter );
  pybind11::finalize_interpreter();
  logchan_pyctx->log("~pyctx<%p> end<%p>\n", this, (void*) _subInterpreter );
  Py_EndInterpreter(_subInterpreter);
  logchan_pyctx->log("~pyctx<%p> renable main\n", this );
  PyThreadState_Swap(getGlobalState()->_mainInterpreter);
  //Py_Finalize();
}

void PythonContext::bindSubInterpreter(){
    logchan_pyctx->log("pyctx<%p> binding subinterpreter\n", this );
  PyThreadState_Swap(_subInterpreter);
    logchan_pyctx->log("pyctx<%p> bound subinterpreter...\n", this );
}
void PythonContext::unbindSubInterpreter(){
    logchan_pyctx->log("pyctx<%p> unbinding subinterpreter\n", this );
  PyThreadState_Swap(getGlobalState()->_mainInterpreter);
    logchan_pyctx->log("pyctx<%p> unbound subinterpreter...\n", this );
}

ScriptObject::ScriptObject() {
    //: mScriptRef(LUA_NOREF) {
  // printf("new ScriptObject<%p>\n", this);
}

///////////////////////////////////////////////////////////////////////////////

ScriptObject::~ScriptObject() {
  // printf("deleting ScriptObject<%p>\n", this);
  /*
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
  */
}
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////
