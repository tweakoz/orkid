#pragma once
////////////////////////////////////////////////////////////////
// NVTX shim — engine phase names, pushed into whatever GPU profiler is attached, with
// NO build dependency on NVTX. A timeline capture that reads "assemble / shadow-maps /
// cull:perview" instead of anonymous submits is the difference between a profile you
// can act on and one you have to guess at.
//
// libnvToolsExt.so.1 (the NVTX runtime, which is what forwards ranges to an attached
// tool) is dlopen'd on first use and exactly two entry points are resolved. Absent
// library — every non-NVIDIA machine, and an NVIDIA one with no toolkit — leaves the
// pointers null and every call costs one branch, which is why the emission sites need no
// gating of their own.
//
// A TOOLING AID, NOT A FEATURE: nothing here is loud and nothing depends on it. Ranges
// are strictly nested per thread (the NVTX push/pop stack is thread-local), so every
// emitter must be scope-shaped.
////////////////////////////////////////////////////////////////

namespace ork::lev2 {

// true iff the NVTX runtime loaded and both entry points resolved.
bool nvtxAvailable();
// thread-local range stack. `name` is copied by the NVTX runtime.
void nvtxPush(const char* name);
void nvtxPop();

// scope form, for the emitters that are not already RAII
struct NvtxScope {
  NvtxScope(const char* name) {
    nvtxPush(name);
  }
  ~NvtxScope() {
    nvtxPop();
  }
};

} // namespace ork::lev2
