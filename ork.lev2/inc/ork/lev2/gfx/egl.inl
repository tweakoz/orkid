////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
