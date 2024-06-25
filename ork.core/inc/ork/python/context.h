////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once
#include <string>

extern "C" {
  #include <Python.h>
}
#include <pybind11/pybind11.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <pybind11/embed.h>

namespace ork::python {

void init();

struct Context
{
public:
	Context();
	~Context();
	void call(const std::string& cmdstr);
  pybind11::module orkidModule();
};
Context& context();

struct GlobalState;
using globalstate_ptr_t = std::shared_ptr<GlobalState>;

struct SubContext {
  
  SubContext();
  ~SubContext();
  //lua_State* mLuaState = nullptr;
  //Simulation* mSimulation = nullptr;
  //PythonSystem* _python_system = nullptr;
  PyThreadState* _subState = nullptr;
  PyThreadState* _saveState = nullptr;
  //pybind11::object _scope;
  PyObject* _mainModule = nullptr;
  PyObject* _mainDict = nullptr;

  //std::unordered_map<uint64_t,scriptwrapper_t> _tokwrappers;

  void bindSubInterpreter();
  void unbindSubInterpreter();
  void eval(std::string str);

};

using subcontext_ptr_t = std::shared_ptr<SubContext>;

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

bool isPythonEnabled();

}
