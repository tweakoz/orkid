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

///////////////////////////////////////////////////////////////////////////////

GlobalState::GlobalState() {
  PyGILState_STATE gstate         = PyGILState_Ensure();
  _mainInterpreterMainThreadState = PyThreadState_Get();
  _mainInterpreter                = PyInterpreterState_Main();
  PyGILState_Release(gstate);
}

///////////////////////////////////////////////////////////////////////////////

globalstate_ptr_t GlobalState::instance() {
  static auto gstate = std::make_shared<GlobalState>();
  return gstate;
}

///////////////////////////////////////////////////////////////////////////////

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
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | Function                     | Description                               | Read State        | Write State       | GIL Access
// | State Location              |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | PyThreadState_Get()          | Retrieves the current thread state        | Current thread    | None              | R
// (specific)| Current thread (TLS)        | | (Python 2.1)                 |                                           | state | |
// |                             |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | PyThreadState_New()          | Creates a new thread state for the        | Specified         | New thread state  | R
// (specific)| Specified interpreter       | | (Python 2.1)                 | specified interpreter                     |
// interpreter state |                   |             |                             |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | PyThreadState_Swap()         | Switches the current thread state         | None              | Current thread    | RW
// (specific)| Global (TLS)               | | (Python 2.1)                 |                                           | | state |
// |                             |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | PyEval_RestoreThread()       | Restores the thread state and acquires    | None              | Current thread    | RW
// (specific)| Specified interpreter      | | (Python 2.1)                 | the GIL                                   | | state,
// GIL        |             |                             |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | PyEval_SaveThread()          | Releases the GIL and saves the current    | Current thread    | None              | RW
// (specific)| Specified interpreter      | | (Python 2.1)                 | thread state                              | state, GIL
// |                   |             |                             |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | Py_EndInterpreter()          | Ends the specified interpreter            | Specified         | Specified         | R
// (specific)| Specified interpreter      | | (Python 2.1)                 |                                           | interpreter
// state | interpreter state,|             |                             | |                              | |                   |
// Thread states     |             |                             |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | PyInterpreterState_New()     | Creates a new interpreter state           | None              | New interpreter   | R (global)
// | Global                      | | (Python 2.1)                 |                                           |                   |
// state             |             |                             |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | PyInterpreterState_Delete()  | Deletes the specified interpreter state   | Specified         | Specified         | R (global)
// | Specified interpreter      | | (Python 2.1)                 |                                           | interpreter state |
// interpreter state |             |                             |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | PyInterpreterState_Head()    | Returns the first interpreter state in    | None              | None              | - | Global |
// | (Python 2.1)                 | the list of all interpreters              |                   |                   | | |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | PyInterpreterState_Next()    | Returns the next interpreter state in the | None              | None              | - | Global |
// | (Python 2.1)                 | list of all interpreters                  |                   |                   | | |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | PyThreadState_Clear()        | Clears the thread state                   | Current thread    | Current thread    | R
// (specific)| Current thread (TLS)       | | (Python 2.1)                 |                                           | state |
// state             |             |                             |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | PyThreadState_Delete()       | Deletes the thread state                  | Current thread    | Current thread    | R
// (specific)| Current thread (TLS)       | | (Python 2.1)                 |                                           | state |
// state             |             |                             |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | PyGILState_Ensure()          | Ensures the current thread holds the GIL  | Current GIL state | Current thread    | RW (global)
// | Global                      | | (Python 2.3)                 |                                           |                   |
// state, GIL        |             |                             |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | PyGILState_Release()         | Releases the GIL acquired by              | Current GIL state | Current thread    | RW (global)
// | Global                      | | (Python 2.3)                 | PyGILState_Ensure                         |                   |
// state, GIL        |             |                             |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | PyInterpreterState_Main()    | Returns the main interpreter state        | None              | None              | - | Global |
// | (Python 2.5)                 |                                           |                   |                   | | |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | PyGILState_Check()           | Checks if the current thread holds the    | Current GIL state | None              | - | Global |
// | (Python 2.7)                 | GIL                                       |                   |                   | | |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | PyThreadState_DeleteCurrent()| Deletes the current thread state          | Current thread    | Current thread    | R
// (specific)| Current thread (TLS)       | | (Python 3.7)                 |                                           | state |
// state             |             |                             |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | PyInterpreterState_Get()     | Retrieves the current interpreter state   | Current interpreter| None              | R
// (specific)| Current thread (TLS)       | | (Python 3.7)                 |                                           | state | |
// |                             |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | PyThreadState_SetAsyncExc()  | Asynchronously raises an exception in the | None              | Current thread    | - | Current
// thread (TLS)       | | (Python 3.7)                 | thread                                    | state             | | | |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | _PyInterpreterState_Lock()   | Locks the interpreter state               | Specified         | Specified         | RW (global)
// | Specified interpreter       | | (Python 3.9)                 |                                           | interpreter state |
// interpreter state |             |                             |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | _PyInterpreterState_Unlock() | Unlocks the interpreter state             | Specified         | Specified         | RW (global)
// | Specified interpreter       | | (Python 3.9)                 |                                           | interpreter state |
// interpreter state |             |                             |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | PyThreadState_GetDict()      | Returns the current thread state's        | Current thread    | None              | R
// (specific)| Current thread (TLS)       | | (Python 3.12)                | dictionary                                | state | |
// |                             |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
// | PyThreadState_GetInterpreter()| Returns the interpreter for the current  | Current thread    | None              | R
// (specific)| Current thread (TLS)       | | (Python 3.12)                 | thread state                             | state | |
// |                             |
// +------------------------------+-------------------------------------------+-------------------+-------------------+-------------+-----------------------------+
///////////////////////////////////////////////////////////////////////////////

PyInterpreterState* fetchPyInterpreterState(PyThreadState* tstate) {
  return PyThreadState_GetInterpreter(tstate);
}

///////////////////////////////////////////////////////////////////////////////

Context2::Context2() {

  // created on update thread
  // for now until deletion the update thread will
  //  only run _subInterpreter
  //  and main thread will run _mainInterpreter

  auto gstate      = GlobalState::instance();
  _mainInterpreter = gstate->_mainInterpreter;

  _mainInterpreterMyThreadState = PyThreadState_New(_mainInterpreter);
  PyGILState_STATE gil_state    = PyGILState_Ensure();

  PyInterpreterConfig pyconfig;
  memset(&pyconfig, 0, sizeof(PyInterpreterConfig));
  pyconfig.gil                           = PyInterpreterConfig_OWN_GIL;
  pyconfig.check_multi_interp_extensions = 1;
  auto status                            = Py_NewInterpreterFromConfig(&_subPrimaryThreadState, &pyconfig);
  OrkAssert(PyStatus_IsError(status) == 0);

  _subGILheld = true; // Py_NewInterpreterFromConfig releases parent GIL and acquires new GIL

  _subInterpreter = fetchPyInterpreterState(_subPrimaryThreadState);

  logchan_pyctx->log("pyctx<%p> _subInterpreter<%p>\n", this, (void*)_subInterpreter);
  logchan_pyctx->log("pyctx<%p> 1...\n", this);
}

///////////////////////////////////////////////////////////////////////////////

Context2::~Context2() {

  // destroy _subInterpreter  on update thread
  //  assume _subInterpreter is active on this thread
  //  and _mainInterpreter is active on main thread
  //  exit this method with _mainInterpreter bound to this thread
  auto gstate = GlobalState::instance();

  PyGILState_STATE gil_state = PyGILState_Ensure();

  logchan_pyctx->log("pyctx<%p> ~Context2\n", this);
  PyThreadState_Swap(_subPrimaryThreadState);
  _subPrimaryThreadState->cframe->current_frame = nullptr;
  Py_EndInterpreter(_subPrimaryThreadState);

  PyThreadState_Swap(_mainInterpreterMyThreadState);

  PyGILState_Release(gil_state);
  logchan_pyctx->log("pyctx<%p> ~Context2 done\n", this);
}

///////////////////////////////////////////////////////////////////////////////

void Context2::bindSubInterpreter() {
  // logchan_pyctx->log("pyctx<%p> binding subinterpreter\n", this);
  _saveInterpreter = PyThreadState_Get();
  if (_saveInterpreter != _subPrimaryThreadState) {
    PyThreadState_Swap(_subPrimaryThreadState);
  }

  //  logchan_pyctx->log("pyctx<%p> bound subinterpreter...\n", this);
}

///////////////////////////////////////////////////////////////////////////////

void Context2::unbindSubInterpreter() {
  PyThreadState_Swap(_saveInterpreter);
}
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::python
