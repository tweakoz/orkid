////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once
#include <string>
#include <mutex>

extern "C" {
  #include <Python.h>
}
#include <pybind11/pybind11.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <pybind11/embed.h>

namespace ork::python {

void init();

// Register a Python-aware assert-traceback printer with ork.core's
// OrkAssertFunction. Safe to call multiple times; should be called once
// Python is up and orkid has been imported (e.g. from EzApp mainThreadLoop
// bindings). A no-op if already installed.
void installAssertTraceback();

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
  // Serializes bind→(script)→unbind so the SINGLE _subPrimaryThreadState +
  // _saveInterpreter scratch are never raced across OS threads. Required now that
  // the sub-interpreter is entered from BOTH the update thread (PythonSystem::_onUpdate)
  // AND the render thread (PythonSystem::_onGpuUpdate): without this, the render-thread
  // PyEval_RestoreThread(_subPrimaryThreadState) attaches a tstate the update thread
  // still owns → CPython "_PyThreadState_Attach: non-NULL old thread state" abort. The
  // sub-interp has its OWN GIL (OWN_GIL), so under the GIL-OFF invariant this machinery
  // was built for the two threads could never execute Python concurrently anyway — this
  // only ORDERS the attach/detach the GIL already serializes (no parallelism lost).
  // NOT TRUE under PYTHON_GIL=1: there the sub GIL is a real lock, and CPython's
  // first-import-of-a-C-extension rule cross-attaches the sub-interp thread to the MAIN
  // GIL (import_run_extension -> switch_to_main_interpreter) — AB-BA against a thread
  // holding this mutex and waiting on the sub GIL. The launcher forces PYTHON_GIL=0
  // (don't-clobber) for exactly that reason; see test_gil_ecs_regression.py.
  // recursive_: tolerate a same-thread nested bind (PythonSystem _onActivateComponent).
  std::recursive_mutex _subInterpMutex;
};

using context2_ptr_t = std::shared_ptr<Context2>;

} // namespace ork::python

#include "wraprawpointer.inl"

namespace pybind11::detail {
  template <typename base>
  struct is_holder_type<base, ork::python::unmanaged_ptr<base>> : std::true_type {};

  template <typename base>
  struct is_holder_type<base, ork::python::unmanaged_const_ptr<base>> : std::true_type {};

} // namespace pybind11::detail