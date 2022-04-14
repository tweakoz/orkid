////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#define EGL_EGLEXT_PROTOTYPES
extern "C" {
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <cstdio>
}

template <typename T> T getEglMethod(const char* named) {
  return reinterpret_cast<T>(eglGetProcAddress(named));
}
