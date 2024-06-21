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

PythonContext::PythonContext(Simulation* psi, PythonSystem* system){
}

PythonContext::~PythonContext(){

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
