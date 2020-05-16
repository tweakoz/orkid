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
