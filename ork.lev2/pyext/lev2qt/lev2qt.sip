// Define the SIP wrapper to the (actual) fib library.

%Module(name=_lev2qt, language="C++")

%Import QtCore/QtCoremod.sip
%Import cppapi.sip

%ModuleCode
#include "test.h"
%End
Z
