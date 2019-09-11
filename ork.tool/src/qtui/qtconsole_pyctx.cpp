////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////
extern "C" {
  #include <Python.h>
}

#include <stdio.h>

#include <orktool/qtui/qtui_tool.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/opq.h>
///////////////////////////////////////////////////////////////////////////////
#include <orktool/qtui/qtconsole.h>
#include <QtWidgets/QScrollBar>
#include <ork/lev2/qtui/qtui.hpp>
#include <ork/util/stl_ext.h>
///////////////////////////////////////////////////////////////////////////////
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
bool gPythonEnabled = true;
FILE* g_orig_stdout = nullptr;
#if 1
///////////////////////////////////////////////////////////////////////////////
static int fd_pty_out_master = -1;
static int fd_pty_err_master = -1;
static int fd_pty_inp_master = -1;
static FILE* fp_pty_out_master = nullptr;
static FILE* fp_pty_err_master = nullptr;
static FILE* fp_pty_inp_master = nullptr;
static int fd_pty_out_slave = -1;
static int fd_pty_err_slave = -1;
static int fd_pty_inp_slave = -1;
static struct termios stored_settings;
extern int Py_NoSiteFlag;
extern int Py_VerboseFlag;
extern char *(*PyOS_ReadlineFunctionPointer)(FILE *sys_stdin, FILE *sys_stdout, char *prompt);
extern "C" char *PyOS_StdioReadline(FILE *sys_stdin, FILE *sys_stdout, char *prompt);
extern "C" char *(*PyOS_ReadlineFunctionPointer)(FILE *sys_stdin, FILE *sys_stdout, char *prompt);
extern "C" int PyRun_InteractiveOneFlags(FILE *fp, const char *filename, PyCompilerFlags *flags);
extern "C" int(*_orkpy_redirect_interactiveloopflags)(FILE *fp, const char *filename, PyCompilerFlags *flags);
static PyCompilerFlags orkpy_cf;

///////////////////////////////////////////////////////////////////////////////
char *orkpy_readline(FILE *sys_stdin, FILE *sys_stdout, char *prompt)
{
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
}
///////////////////////////////////////////////////////////////////////////////
void echo_off(int ifil)
{
    struct termios new_settings;
    tcgetattr(ifil,&stored_settings);
    new_settings = stored_settings;
    new_settings.c_lflag &= (~ECHO);
    tcsetattr(ifil,TCSANOW,&new_settings);
    return;
}
///////////////////////////////////////////////////////////////////////////////
void echo_on(int ifil)
{
    tcsetattr(ifil,TCSANOW,&stored_settings);
    return;
}
///////////////////////////////////////////////////////////////////////////////

namespace ork {
namespace tool {
char slave_out_name[256];
char slave_err_name[256];
char slave_inp_name[256];

///////////////////////////////////////////////////////////////////////////////
Py& Py::Ctx()
{
	static Py gPY;
	return gPY;
}
void Py::Call(const std::string& cmdstr)
{
	int i = PyRun_SimpleString(cmdstr.c_str());
	//printf( "pycall<%s> : %d\n", cmdstr.c_str(), i );
}
Py::Py()
{
	Py_NoSiteFlag = 1;
	Py_VerboseFlag = 2;

	PyOS_ReadlineFunctionPointer=orkpy_readline;
    orkpy_cf.cf_flags = 0;
	Py_InitializeEx(0);
	PyEval_InitThreads();
	PyOS_ReadlineFunctionPointer=orkpy_readline;
	//_orkpy_redirect_interactiveloopflags = orkpy_redirect_interactiveloopflags;
}
Py::~Py()
{
	Py_Finalize();
}

///////////////////////////////////////////////////////////////////////////////
void orkpy_initst2()
{
    PyObject *v;
    v = PySys_GetObject("ps1");
    if (v == NULL) {
        PySys_SetObject("ps1", v = PyString_FromString(">>> "));
        Py_XDECREF(v);
    }
    v = PySys_GetObject("ps2");
    if (v == NULL) {
        PySys_SetObject("ps2", v = PyString_FromString("... "));
        Py_XDECREF(v);
    }
}
///////////////////////////////////////////////////////////////////////////////
void orkpy_initpty()
{
	int ret = openpty(&fd_pty_inp_master, &fd_pty_inp_slave, slave_inp_name, NULL, NULL);
		ret = openpty(&fd_pty_out_master, &fd_pty_out_slave, slave_out_name, NULL, NULL);
		ret = openpty(&fd_pty_err_master, &fd_pty_err_slave, slave_err_name, NULL, NULL);

	fp_pty_err_master = fdopen( fd_pty_err_master, "w" );
	fp_pty_out_master = fdopen( fd_pty_out_master, "w" );
	fp_pty_inp_master = fdopen( fd_pty_inp_master, "r" );

	setvbuf(fp_pty_out_master, (char*)NULL, _IOFBF, 0); // disable buffering
	setvbuf(fp_pty_err_master, (char*)NULL, _IOFBF, 0); // disable buffering

	printf("master inp fd: %d\n", fd_pty_inp_master);
	printf("master inp fp: %p\n", fp_pty_inp_master);
	printf("slave inp fd: %d\n", fd_pty_inp_slave);
	printf("slave inp <%s>\n", slave_inp_name);

	printf("master out fd: %d\n", fd_pty_out_master);
	printf("master out fp: %p\n", fp_pty_out_master);
	printf("slave out fd: %d\n", fd_pty_out_slave);
	printf("slave out<%s>\n", slave_out_name);

	printf("master err fd: %d\n", fd_pty_err_master);
	printf("master err fp: %p\n", fp_pty_err_master);
	printf("slave err fd: %d\n", fd_pty_err_slave);
	printf("slave err<%s>\n", slave_err_name);

	fflush(stdout);
	fflush(stderr);

	echo_off(fd_pty_out_master);
	echo_off(fd_pty_err_master);

	int flags;
	if ((flags = fcntl(fd_pty_out_master, F_GETFL, 0)) == -1)
		flags = 0;
	if (fcntl(fd_pty_out_master, F_SETFL, flags | O_NONBLOCK) == -1)
		assert(false);
	if ((flags = fcntl(fd_pty_err_master, F_GETFL, 0)) == -1)
		flags = 0;
	if (fcntl(fd_pty_err_master, F_SETFL, flags | O_NONBLOCK) == -1)
		assert(false);

	Py::Ctx().Call("import os");
	Py::Ctx().Call("import sys");
	Py::Ctx().Call("import fcntl");
	Py::Ctx().Call("sys.stdout.flush() # <--- important when redirecting to files\n");
	Py::Ctx().Call("sys.stderr.flush() # <--- important when redirecting to files\n");


	Py::Ctx().Call(CreateFormattedString("fd_inp_master = %d\n",fd_pty_inp_master));
	Py::Ctx().Call(CreateFormattedString("fd_out_master = %d\n",fd_pty_out_master));
	Py::Ctx().Call(CreateFormattedString("fd_err_master = %d\n",fd_pty_err_master));
	//Py::Ctx().Call(CreateFormattedString("fd_inp_master = os.dup(%d)\n",fd_pty_inp_master));
	//Py::Ctx().Call(CreateFormattedString("fd_out_master = os.dup(%d)\n",fd_pty_out_master));
	//Py::Ctx().Call(CreateFormattedString("fd_err_master = os.dup(%d)\n",fd_pty_err_master));
	Py::Ctx().Call("print str(fd_inp_master)\n");
	Py::Ctx().Call("print str(fd_out_master)\n");
	Py::Ctx().Call("print str(fd_err_master)\n");
	//Py::Ctx().Call("fl = fcntl.fcntl(fd_err_master, fcntl.F_GETFL)\n" );
	//Py::Ctx().Call("fl |= os.O_SYNC # or os.O_DSYNC (if you don't care the file timestamp updates)\n");
	//Py::Ctx().Call("fcntl.fcntl(fd_err_master, fcntl.F_SETFL, fl)\n");
	Py::Ctx().Call("sys.pty_inp_master = os.fdopen(fd_inp_master, 'r', 0)\n");
	Py::Ctx().Call("sys.pty_out_master = os.fdopen(fd_out_master, 'w', 0)\n");
	Py::Ctx().Call("sys.pty_err_master = os.fdopen(fd_err_master, 'w', 0)\n");
	Py::Ctx().Call("print 'inp<%s>' % str(sys.pty_inp_master)\n");
	Py::Ctx().Call("print 'out<%s>' % str(sys.pty_out_master)\n");
	Py::Ctx().Call("print 'err<%s>' % str(sys.pty_err_master)\n");

	//PyObject* v = PySys_GetObject("pty_err_master");
	//int ifd_stderr = PyObject_AsFileDescriptor(v);

	Py::Ctx().Call("print 'is_tty<inp> : %s' % str(os.isatty(fd_inp_master))\n" );
	Py::Ctx().Call("print 'is_tty<out> : %s' % str(os.isatty(fd_out_master))\n" );
	Py::Ctx().Call("print 'is_tty<err> : %s' % str(os.isatty(fd_err_master))\n" );

	Py::Ctx().Call("sys.stdin = sys.pty_inp_master\n");
	Py::Ctx().Call("sys.stdout = sys.pty_out_master\n");
	Py::Ctx().Call("sys.stderr = sys.pty_err_master\n");

	g_orig_stdout = stdout;

	stdin = fp_pty_inp_master;
	stderr = fp_pty_err_master;
	//stdout = fp_pty_out_master;

	Py::Ctx().Call("print 'is_tty<inp> : %s' % str(os.isatty(fd_inp_master))\n" );
	Py::Ctx().Call("print 'is_tty<out> : %s' % str(os.isatty(fd_out_master))\n" );
	Py::Ctx().Call("print 'is_tty<err> : %s' % str(os.isatty(fd_err_master))\n" );
	Py::Ctx().Call("print sys.version" );
	Py::Ctx().Call("print dir()" );
	Py::Ctx().Call("print 'Welcome to the machine.'" );
}

void orkpy_initork();

///////////////////////////////////////////////////////////////////////////////
void orkpy_runiter()
{
	fflush(fp_pty_out_master);
	fflush(fp_pty_err_master);
	int ret = PyRun_InteractiveOneFlags(stdin, "<stdin>", & orkpy_cf );
	MainThreadOpQ().push([&]() {
		orkpy_runiter();
	});
}
///////////////////////////////////////////////////////////////////////////////
void InitPython()
{
	gPythonEnabled = true;

	MainThreadOpQ().push([&]() {
		usleep(1500000);
		char* strbuf = "TheMachine";
		Py_SetProgramName(strbuf);
		Py::Ctx();

		PyGILState_STATE gstate = PyGILState_Ensure();
		PyGILState_Release(gstate);
		MainThreadOpQ().push([&]() {orkpy_initst2();});
		MainThreadOpQ().push([&]() {orkpy_initpty();});
		MainThreadOpQ().push([&]() {orkpy_initork();});
		MainThreadOpQ().push([&]() {orkpy_runiter();});

	});
}
///////////////////////////////////////////////////////////////////////////////
} } // namespace ork::tool
#endif
