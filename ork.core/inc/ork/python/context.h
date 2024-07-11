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

PyInterpreterState* fetchPyInterpreterState(PyThreadState* tstate);
bool ensureGILonInterpreterForThisThread(PyThreadState* interp);
bool releaseGILonInterpreterForThisThread(PyThreadState* interp);
bool hasGILonInterpreterForThisThread(PyThreadState* interp);
void deleteInterpreter(PyThreadState* interp_to_delete, PyInterpreterState* interp_next );

struct GlobalState;
using globalstate_ptr_t = std::shared_ptr<GlobalState>;

struct GlobalState {

  GlobalState();
  PyInterpreterState* _mainInterpreter   = nullptr;
  PyThreadState* _mainInterpreterMainThreadState = nullptr;

  static globalstate_ptr_t instance();
};


struct Context2 {
  
  Context2();
  ~Context2();

  PyInterpreterState* _subInterpreter = nullptr;
  PyThreadState* _subPrimaryThreadState = nullptr;
  PyThreadState* _mainInterpreterMyThreadState = nullptr;


  PyInterpreterState* _mainInterpreter = nullptr;
  PyThreadState* _saveInterpreter = nullptr;

  void bindSubInterpreter();
  void unbindSubInterpreter();
  bool _subGILheld = false;
};

using context2_ptr_t = std::shared_ptr<Context2>;

} // namespace ork::python

