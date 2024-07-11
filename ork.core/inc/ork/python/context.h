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

bool isPythonEnabled();

PyThreadState* fetchPyThreadState(PyInterpreterState* interp);
PyInterpreterState* fetchPyInterpreterState(PyThreadState* tstate);
bool ensureGILonInterpreterForThisThread(PyInterpreterState* interp);
bool releaseGILonInterpreterForThisThread(PyInterpreterState* interp);
bool hasGILonInterpreterForThisThread(PyInterpreterState* interp);
void deleteInterpreter(PyInterpreterState* interp_to_delete, PyInterpreterState* interp_next );

struct GlobalState {

  PyInterpreterState* _mainInterpreter   = nullptr;
};

using globalstate_ptr_t = std::shared_ptr<GlobalState>;

struct Context2 {
  
  Context2(globalstate_ptr_t ptr);
  ~Context2();

  PyInterpreterState* _subInterpreter = nullptr;
  PyInterpreterState* _mainInterpreter = nullptr;
  PyInterpreterState* _saveInterpreter = nullptr;

  void bindSubInterpreter(bool ensure,bool save);
  void unbindSubInterpreter();
	globalstate_ptr_t _gstate;

};

using context2_ptr_t = std::shared_ptr<Context2>;

} // namespace ork::python

