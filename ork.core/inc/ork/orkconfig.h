////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

////////////////////////////////////////////////////////////////

#define ORK_CONFIG_OPENGL
#define ORK_CONFIG_QT
#define USE_FCOLLADA
#define INCLUDE_UNIT_TEST

///////////////////////////////////////////////////////////////////////////////

#define ThreadLocal __thread
#define MEMALIGN(x) __attribute__((aligned(x)))
#define ORK_CONFIG_DEFAULT_SERIALIZE_XML

///////////////////////////////////////////////////////////////////////////////

#define ORK_PUSH_SYMVIZ_PUBLIC _Pragma("GCC visibility push(default)")
#define ORK_PUSH_SYMVIZ_PRIVATE _Pragma("GCC visibility push(hidden)")
#define ORK_POP_SYMVIZ _Pragma("GCC visibility pop")

#define ORK_API __attribute__ ((visibility("default")))
