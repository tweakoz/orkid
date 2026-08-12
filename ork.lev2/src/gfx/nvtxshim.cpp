////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////
// NVTX shim implementation — see nvtxshim.h.
//
// The two signatures are declared locally (C ABI, stable since NVTX v1) rather than
// pulled from nvToolsExt.h, so no CUDA toolkit is needed to BUILD the engine. Loading
// the runtime is what makes ranges visible to an attached profiler: the NVTX runtime
// honours the tool's injection environment, so nothing here needs to know which tool
// (or whether any) is listening.
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/nvtxshim.h>

#if defined(__linux__)
#include <dlfcn.h>
#define ORK_NVTX_ENABLED 1
#endif

namespace ork::lev2 {

namespace {

using pfn_push_t = int (*)(const char*);
using pfn_pop_t  = int (*)(void);

struct NvtxProcs {
  pfn_push_t _push = nullptr;
  pfn_pop_t  _pop  = nullptr;

  NvtxProcs() {
#if defined(ORK_NVTX_ENABLED)
    // RTLD_GLOBAL: the NVTX runtime's own injection handshake resolves symbols across
    // the process; loading it privately has been known to leave a tool half-attached.
    void* lib = dlopen("libnvToolsExt.so.1", RTLD_NOW | RTLD_GLOBAL);
    if (not lib)
      return;
    auto push = (pfn_push_t)dlsym(lib, "nvtxRangePushA");
    auto pop  = (pfn_pop_t)dlsym(lib, "nvtxRangePop");
    if (not push or not pop) {
      dlclose(lib);
      return;
    }
    // The library is intentionally NEVER unloaded: ranges are pushed from every engine
    // thread, and a dlclose racing a live range stack would take the process with it.
    _push = push;
    _pop  = pop;
#endif
  }
};

// function-local static: thread-safe one-time load, and no static-init-order coupling to
// the gfx globals that emit the first ranges.
const NvtxProcs& procs() {
  static NvtxProcs _p;
  return _p;
}

} // namespace

////////////////////////////////////////////////////////////////

bool nvtxAvailable() {
  return procs()._push != nullptr;
}

void nvtxPush(const char* name) {
  const auto& p = procs();
  if (p._push)
    p._push(name);
}

void nvtxPop() {
  const auto& p = procs();
  if (p._pop)
    p._pop();
}

////////////////////////////////////////////////////////////////
} // namespace ork::lev2
