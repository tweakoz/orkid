////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/prop.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/fixedstring.hpp>
#include <ork/util/logger.h>
#include <ork/python/context.h>
#include <ork/util/stl_ext.h>
///////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
///////////////////////////////////////////////////////////////////////////////
#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif
///////////////////////////////////////////////////////////////////////////////

namespace ork::python {

static PyCompilerFlags orkpy_cf;
void init_bindings();
bool gPythonEnabled = true;
FILE* g_orig_stdout = nullptr;

static int fd_pty_out_master   = -1;
static int fd_pty_err_master   = -1;
static int fd_pty_inp_master   = -1;
static FILE* fp_pty_out_master = nullptr;
static FILE* fp_pty_err_master = nullptr;
static FILE* fp_pty_inp_master = nullptr;
static int fd_pty_out_slave    = -1;
static int fd_pty_err_slave    = -1;
static int fd_pty_inp_slave    = -1;
static struct termios stored_settings;
int Py_NoSiteFlag;
int Py_VerboseFlag;

extern "C" char* PyOS_StdioReadline(FILE* sys_stdin, FILE* sys_stdout, const char* prompt);
extern "C" int PyRun_InteractiveOneFlags(FILE* fp, const char* filename, PyCompilerFlags* flags);
extern "C" int (*_orkpy_redirect_interactiveloopflags)(FILE* fp, const char* filename, PyCompilerFlags* flags);
char slave_out_name[256];
char slave_err_name[256];
char slave_inp_name[256];

bool isPythonEnabled() {
  return gPythonEnabled;
}

///////////////////////////////////////////////////////////////////////////////
char* orkpy_readline(FILE* sys_stdin, FILE* sys_stdout, char* prompt) {
#if 0
//printf( "prompt<%s>\n", prompt );
	char* pdata = PyOS_StdioReadline( sys_stdin, sys_stdout, prompt );
    //printf( "prompt<%s> pdata<%s>\n", prompt, pdata );
    ork::fxstring<256> proc_line(pdata);
	proc_line.replace_in_place("\r", "");
	int ilen = proc_line.length();
	char* pret = (char*) PyMem_MALLOC(ilen+1);
	strncpy( pret, proc_line.c_str(), ilen );
	pret[ilen]=0;
	PyMem_FREE(pdata);
	return pret;
#else
  return nullptr;
#endif
}
///////////////////////////////////////////////////////////////////////////////
void echo_off(int ifil) {
  struct termios new_settings;
  tcgetattr(ifil, &stored_settings);
  new_settings = stored_settings;
  new_settings.c_lflag &= (~ECHO);
  tcsetattr(ifil, TCSANOW, &new_settings);
  return;
}
///////////////////////////////////////////////////////////////////////////////
void echo_on(int ifil) {
  tcsetattr(ifil, TCSANOW, &stored_settings);
  return;
}
///////////////////////////////////////////////////////////////////////////////
void initst2() {
  PyObject* v;
  v = PySys_GetObject("ps1");
  if (v == NULL) {
    // PySys_SetObject("ps1", v = PyString_FromString(">>> "));
    Py_XDECREF(v);
  }
  v = PySys_GetObject("ps2");
  if (v == NULL) {
    // PySys_SetObject("ps2", v = PyString_FromString("... "));
    Py_XDECREF(v);
  }
}
///////////////////////////////////////////////////////////////////////////////
void runiter() {
  fflush(fp_pty_out_master);
  fflush(fp_pty_err_master);
  int ret = PyRun_InteractiveOneFlags(stdin, "<stdin>", &orkpy_cf);
  opq::mainSerialQueue()->enqueue([&]() { runiter(); });
}
void initpty() {
  int ret = openpty(&fd_pty_inp_master, &fd_pty_inp_slave, slave_inp_name, NULL, NULL);
  ret     = openpty(&fd_pty_out_master, &fd_pty_out_slave, slave_out_name, NULL, NULL);
  ret     = openpty(&fd_pty_err_master, &fd_pty_err_slave, slave_err_name, NULL, NULL);

  fp_pty_err_master = fdopen(fd_pty_err_master, "w");
  fp_pty_out_master = fdopen(fd_pty_out_master, "w");
  fp_pty_inp_master = fdopen(fd_pty_inp_master, "r");

  setvbuf(fp_pty_out_master, (char*)NULL, _IOFBF, 0); // disable buffering
  setvbuf(fp_pty_err_master, (char*)NULL, _IOFBF, 0); // disable buffering

  printf("master inp fd: %d\n", fd_pty_inp_master);
  printf("master inp fp: %p\n", (void*)fp_pty_inp_master);
  printf("slave inp fd: %d\n", fd_pty_inp_slave);
  printf("slave inp <%s>\n", slave_inp_name);

  printf("master out fd: %d\n", fd_pty_out_master);
  printf("master out fp: %p\n", (void*)fp_pty_out_master);
  printf("slave out fd: %d\n", fd_pty_out_slave);
  printf("slave out<%s>\n", slave_out_name);

  printf("master err fd: %d\n", fd_pty_err_master);
  printf("master err fp: %p\n", (void*)fp_pty_err_master);
  printf("slave err fd: %d\n", fd_pty_err_slave);
  printf("slave err<%s>\n", slave_err_name);

  fflush(stdout);
  fflush(stderr);

  // echo_off(fd_pty_out_master);
  // echo_off(fd_pty_err_master);

  int flags;
  if ((flags = fcntl(fd_pty_out_master, F_GETFL, 0)) == -1)
    flags = 0;
  if (fcntl(fd_pty_out_master, F_SETFL, flags | O_NONBLOCK) == -1)
    assert(false);
  if ((flags = fcntl(fd_pty_err_master, F_GETFL, 0)) == -1)
    flags = 0;
  if (fcntl(fd_pty_err_master, F_SETFL, flags | O_NONBLOCK) == -1)
    assert(false);

  context().call("import os");
  context().call("import sys");
  context().call("import fcntl");
  context().call("sys.stdout.flush() # <--- important when redirecting to files\n");
  context().call("sys.stderr.flush() # <--- important when redirecting to files\n");

  context().call(CreateFormattedString("fd_inp_master = %d\n", fd_pty_inp_master));
  context().call(CreateFormattedString("fd_out_master = %d\n", fd_pty_out_master));
  context().call(CreateFormattedString("fd_err_master = %d\n", fd_pty_err_master));
  // context().call(CreateFormattedString("fd_inp_master = os.dup(%d)\n",fd_pty_inp_master));
  // context().call(CreateFormattedString("fd_out_master = os.dup(%d)\n",fd_pty_out_master));
  // context().call(CreateFormattedString("fd_err_master = os.dup(%d)\n",fd_pty_err_master));
  context().call("print str(fd_inp_master)\n");
  context().call("print str(fd_out_master)\n");
  context().call("print str(fd_err_master)\n");
  // context().call("fl = fcntl.fcntl(fd_err_master, fcntl.F_GETFL)\n" );
  // context().call("fl |= os.O_SYNC # or os.O_DSYNC (if you don't care the file timestamp updates)\n");
  // context().call("fcntl.fcntl(fd_err_master, fcntl.F_SETFL, fl)\n");
  context().call("sys.pty_inp_master = os.fdopen(fd_inp_master, 'r', 0)\n");
  context().call("sys.pty_out_master = os.fdopen(fd_out_master, 'w', 0)\n");
  context().call("sys.pty_err_master = os.fdopen(fd_err_master, 'w', 0)\n");
  context().call("print 'inp<%s>' % str(sys.pty_inp_master)\n");
  context().call("print 'out<%s>' % str(sys.pty_out_master)\n");
  context().call("print 'err<%s>' % str(sys.pty_err_master)\n");

  // PyObject* v = PySys_GetObject("pty_err_master");
  // int ifd_stderr = PyObject_AsFileDescriptor(v);

  context().call("print 'is_tty<inp> : %s' % str(os.isatty(fd_inp_master))\n");
  context().call("print 'is_tty<out> : %s' % str(os.isatty(fd_out_master))\n");
  context().call("print 'is_tty<err> : %s' % str(os.isatty(fd_err_master))\n");

  context().call("sys.stdin = sys.pty_inp_master\n");
  context().call("sys.stdout = sys.pty_out_master\n");
  context().call("sys.stderr = sys.pty_err_master\n");

  g_orig_stdout = stdout;

  stdin  = fp_pty_inp_master;
  stderr = fp_pty_err_master;
  // stdout = fp_pty_out_master;

  context().call("print 'is_tty<inp> : %s' % str(os.isatty(fd_inp_master))\n");
  context().call("print 'is_tty<out> : %s' % str(os.isatty(fd_out_master))\n");
  context().call("print 'is_tty<err> : %s' % str(os.isatty(fd_err_master))\n");
  context().call("print sys.version");
  context().call("print dir()");
  context().call("print 'Welcome to the machine.'");
}
///////////////////////////////////////////////////////////////////////////////
void init() {
  gPythonEnabled = true;

  opq::mainSerialQueue()->enqueue([&]() {
    ork::msleep(1500);
    Py_SetProgramName(L"TheMachine");
    auto& ctx = context();

    PyGILState_STATE gstate = PyGILState_Ensure();
    PyGILState_Release(gstate);
    // opq::mainSerialQueue()->enqueue([&]() {initst2();});
    // opq::mainSerialQueue()->enqueue([&]() {initpty();});
    // opq::mainSerialQueue()->enqueue([&]() {init_bindings();});
    // opq::mainSerialQueue()->enqueue([&]() {runiter();});
  });
}

///////////////////////////////////////////////////////////////////////////////
Context& context() {
  static Context gPY;
  return gPY;
}
///////////////////////////////////////////////////////////////////////////////
void Context::call(const std::string& cmdstr) {
  int i = PyRun_SimpleString(cmdstr.c_str());
  // printf( "pycall<%s> : %d\n", cmdstr.c_str(), i );
}
///////////////////////////////////////////////////////////////////////////////
pybind11::module Context::orkidModule() {
  return pybind11::module::import("ork_core");
}
///////////////////////////////////////////////////////////////////////////////
Context::Context() {
  Py_NoSiteFlag  = 1;
  Py_VerboseFlag = 2;

  // PyOS_StdioReadline=orkpy_readline;
  orkpy_cf.cf_flags = 0;
  Py_InitializeEx(0);
  PyEval_InitThreads();
  // PyOS_StdioReadline=orkpy_readline;
  //_orkpy_redirect_interactiveloopflags = orkpy_redirect_interactiveloopflags;
}
///////////////////////////////////////////////////////////////////////////////
Context::~Context() {
  Py_Finalize();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan_pyctx = logger()->createChannel("ork.pyctx", fvec3(0.9, 0.6, 0.0));

///////////////////////////////////////////////////////////////////////////////


PyThreadState* fetchPyThreadState(PyInterpreterState* interp) {
// Manually manage the GIL to ensure it is held
    PyThreadState* tstate = PyGILState_GetThisThreadState();
    if (tstate == nullptr || tstate->interp != interp) {
        PyGILState_STATE gstate = PyGILState_Ensure();
        
        tstate = PyThreadState_Get();
        if (tstate == nullptr || tstate->interp != interp) {
            // Create a new thread state for the specified interpreter
            tstate = PyThreadState_New(interp);
            PyThreadState_Swap(tstate);
        }
        
        PyGILState_Release(gstate);
    }
    return tstate;
    }
PyInterpreterState* fetchPyInterpreterState(PyThreadState* tstate) {
  return tstate->interp;
}

bool ensureGILonInterpreterForThisThread(PyInterpreterState* interp) {
  // check current states and achieve GIL acquisition
  //   regardless of current state...
  //   this means check current state and act accordingly...
  // ensure other thread and interpreter state is untouched !

  bool was_acquired = false;

  // Fetch the current thread state for the specified interpreter
  PyThreadState* currentThreadState = fetchPyThreadState(interp);

  // Check if the current interpreter is the desired one
  if (currentThreadState->interp != interp) {
    // Swap to the thread state for the desired interpreter and acquire the GIL
    PyThreadState* newThreadState = PyThreadState_New(interp);
    PyThreadState_Swap(newThreadState);
    PyEval_AcquireThread(newThreadState);
    was_acquired = true;
  } else {
    // The GIL is already acquired for the desired interpreter
    PyEval_AcquireThread(currentThreadState);
    was_acquired = true;
  }

  return was_acquired;
}
bool releaseGILonInterpreterForThisThread(PyInterpreterState* interp) {
  // check current states and release GIL acquisition
  //   regardless of current state...
  //   this means check current state and act accordingly...
  // ensure other thread and interpreter state is untouched !
  // Fetch the current thread state for the specified interpreter

  bool was_released = false;

  PyThreadState* currentThreadState = fetchPyThreadState(interp);

  // Check if the current interpreter is the desired one
  if (currentThreadState->interp == interp) {
    // Release the GIL for the desired interpreter
    PyEval_ReleaseThread(currentThreadState);
    was_released = true;
  }

  return was_released;
}
bool hasGILonInterpreterForThisThread(PyInterpreterState* interp) {
  // check current states and succeed regardless of current state...
  // ensure all thread and interpreter state is untouched !

  bool has_gil = false;

  // Fetch the current thread state for the specified interpreter
  PyThreadState* currentThreadState = fetchPyThreadState(interp);

  // Check if the current thread state is associated with the given interpreter
  if (currentThreadState->interp == interp) {
    has_gil = (PyGILState_Check() == PyGILState_LOCKED);
  }

  return has_gil;
}

void deleteInterpreter(PyInterpreterState* interp_to_delete, PyInterpreterState* interp_next ) {
  // delete interp_to_delete
  //  after it is deleted, ensure interp_next is active on this thread with GIL acquired

    // Ensure the GIL is acquired for the interpreter to delete
    ensureGILonInterpreterForThisThread(interp_to_delete);

    // Fetch the thread state for the interpreter to delete
    PyThreadState* tstate_to_delete = fetchPyThreadState(interp_to_delete);

    // Delete the interpreter
    PyInterpreterState_Clear(interp_to_delete);
    PyInterpreterState_Delete(interp_to_delete);
    //Py_EndInterpreter(tstate_to_delete);

    ensureGILonInterpreterForThisThread(interp_next);
}

///////////////////////////////////////////////////////////////////////////////

Context2::Context2(globalstate_ptr_t gstate) {

  // created on update thread
  // for now until deletion the update thread will
  //  only run _subInterpreter
  //  and main thread will run _mainInterpreter

  _gstate          = gstate;
  _mainInterpreter = _gstate->_mainInterpreter;

  PyInterpreterConfig pyconfig;
  memset(&pyconfig, 0, sizeof(PyInterpreterConfig));
  pyconfig.gil                           = PyInterpreterConfig_OWN_GIL;
  pyconfig.check_multi_interp_extensions = 1;
  PyThreadState* subts = nullptr;
  auto status                            = Py_NewInterpreterFromConfig(&subts, &pyconfig);
  OrkAssert(PyStatus_IsError(status) == 0);
  _subInterpreter = fetchPyInterpreterState(subts);

  PyEval_ReleaseThread(subts);
  logchan_pyctx->log("pyctx<%p> _subInterpreter<%p>\n", this, (void*)_subInterpreter);
  logchan_pyctx->log("pyctx<%p> 1...\n", this);
}

///////////////////////////////////////////////////////////////////////////////

Context2::~Context2() {

  // destroy _subInterpreter  on update thread
  //  assume _subInterpreter is active on this thread
  //  and _mainInterpreter is active on main thread
  //  exit this method with _mainInterpreter bound to this thread

  logchan_pyctx->log("pyctx<%p> ~Context2\n", this);

  // Ensure the GIL is acquired for the subinterpreter
  ensureGILonInterpreterForThisThread(_subInterpreter);

  // Delete the subinterpreter
  deleteInterpreter(_subInterpreter, _mainInterpreter);

  // Fetch the current thread state for the main interpreter
  PyThreadState* currentThreadState = fetchPyThreadState(_mainInterpreter);

  // Ensure the GIL is acquired for the main interpreter
  ensureGILonInterpreterForThisThread(_mainInterpreter);

  // Release the GIL
  // PyGILState_Release(parentGILState);

  logchan_pyctx->log("pyctx<%p> ~Context2 done\n", this);
}

///////////////////////////////////////////////////////////////////////////////

void Context2::bindSubInterpreter(bool ensure, bool save) {
  // logchan_pyctx->log("pyctx<%p> binding subinterpreter\n", this);

  auto tstate = fetchPyThreadState(_subInterpreter);


  if (save) {
    // Save the current thread state and swap to subinterpreter
    auto prev_ts = PyThreadState_Swap(tstate);
    _saveInterpreter = prev_ts->interp;

    // Release the GIL for the parent interpreter
    if (prev_ts) {
      PyEval_ReleaseThread(prev_ts);
    }
  }

  if (ensure) {
    // Ensure the GIL is acquired for the subinterpreter
    //ensureGILonInterpreterForThisThread(_subInterpreter);
  }


  // Acquire the GIL for the subinterpreter
  bool was_acq = ensureGILonInterpreterForThisThread(_subInterpreter);
  //PyEval_AcquireThread(tstate);
  // logchan_pyctx->log("pyctx<%p> bound subinterpreter...\n", this);
}

///////////////////////////////////////////////////////////////////////////////

void Context2::unbindSubInterpreter() {
  // logchan_pyctx->log("pyctx<%p> unbinding subinterpreter\n", this);
  auto sub_tstate = fetchPyThreadState(_subInterpreter);
  //PyEval_ReleaseThread(sub_tstate);

  bool was_released = releaseGILonInterpreterForThisThread(_subInterpreter);

  // Restore the saved thread state if there was one
  if (_saveInterpreter) {
    bool was_acq = ensureGILonInterpreterForThisThread(_saveInterpreter);
    _saveInterpreter = nullptr;
  }
  // logchan_pyctx->log("pyctx<%p> unbound subinterpreter...\n", this);
}
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::python
