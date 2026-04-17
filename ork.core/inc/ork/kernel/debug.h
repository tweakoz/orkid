////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once
#include <string>

namespace ork {
std::string get_backtrace(bool with_color = false);

// Optional callback invoked by OrkAssertFunction before the C++ backtrace
// is printed. Lets the Python layer (ork.core/src/python/context.cpp)
// print a Python traceback when one is available. nullptr when running
// in a non-Python build/context — the assert handler no-ops the call.
using python_stack_printer_t = void(*)();
extern python_stack_printer_t _python_stack_printer;
};

