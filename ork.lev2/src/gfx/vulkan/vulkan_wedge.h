////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "headers/vulkan_ctx.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>

///////////////////////////////////////////////////////////////////////////////
// Bounded fence waits: a stalled GPU must FAIL LOUD, never hang silently.
//
// Every fence wait in this backend was vkWaitForFences(..., UINT64_MAX) with
// the result discarded. When the GPU stopped completing work the render thread
// parked in the kernel forever — a process that is alive, silent, produces no
// further output and never dies (observed as a 24-minute park in the offscreen
// submit path). The frame ring cannot wrap into a diagnosable state either:
// the pacing wait sits inside _doEndFrame AHEAD of the CB-pool deallocate, so
// an infinite wait is the ONLY expression a stall has here.
//
// Bound: wait in slices, and past ORKID_VULKAN_WEDGE_TIMEOUT_SECS (default 30)
// abort naming the wait site, the elapsed seconds, the frame counter, the wait
// result and the fence's own vkGetFenceStatus. Below the threshold behavior is
// identical to the unbounded wait.
//
// The 30s default is deliberately generous: legitimate frames DO run multiple
// seconds (cold-start pipeline compiles, bake phases, an 8.6s single frame is
// on record), and a false abort on a slow-but-alive frame is a worse failure
// than a late detection. Set the env lower to tighten, or <=0 to disable the
// bound entirely (restores the old infinite wait).
///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2::vulkan {

inline double wedgeTimeoutSecs() {
  static const double secs = []() -> double {
    const char* e = std::getenv("ORKID_VULKAN_WEDGE_TIMEOUT_SECS");
    return e ? atof(e) : 30.0;
  }();
  return secs;
}

///////////////////////////////////////////////////////////////////////////////
// ORKID_VULKAN_INDUCE_WEDGE=<frame> : fault injection, default-off.
//
// At that frame the offscreen submit path deliberately SKIPS its queueSubmit
// while still resetting and waiting the frame fence — which therefore never
// signals. The only correct outcome is the bounded wait above firing loud, so
// this is how the safety net is proven to work rather than assumed. Pair it
// with a short ORKID_VULKAN_WEDGE_TIMEOUT_SECS so the proof is quick.
///////////////////////////////////////////////////////////////////////////////

inline uint64_t induceWedgeFrame() {
  static const uint64_t frame = []() -> uint64_t {
    const char* e = std::getenv("ORKID_VULKAN_INDUCE_WEDGE");
    return e ? strtoull(e, nullptr, 10) : 0;
  }();
  return frame;
}

// The frame counter every wait site reports, so a wedge line can be lined up
// against the rest of the frame-indexed logging.
inline uint64_t wedgeFrame(vkcontext_rawptr_t ctx) {
  return (ctx and ctx->_fbi and ctx->_fbi->_output) ? ctx->_fbi->_output->_current_frame : 0;
}

///////////////////////////////////////////////////////////////////////////////
// Bounded replacement for vkWaitForFences(..., UINT64_MAX). Returns only when
// the fence is signalled; anything else aborts loud.
///////////////////////////////////////////////////////////////////////////////

inline void waitFenceBounded(
    VkDevice device,   //
    VkFence fence,     //
    const char* site,  //
    uint64_t frame) {  //

  constexpr uint64_t SLICE_NS = 2000000000ull; // 2s slices, so elapsed stays reportable
  const double timeout        = wedgeTimeoutSecs();
  const auto t0               = std::chrono::steady_clock::now();

  while (true) {
    VkResult res = vkWaitForFences(device, 1, &fence, VK_TRUE, SLICE_NS);
    if (res == VK_SUCCESS) {
      return;
    }
    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (res == VK_TIMEOUT) {
      if (timeout <= 0.0) { // bound disabled by env
        continue;
      }
      if (elapsed < timeout) {
        continue;
      }
    }
    // Timed out past the threshold, or the wait itself errored
    // (VK_ERROR_DEVICE_LOST lands here — previously discarded).
    VkResult fence_status = vkGetFenceStatus(device, fence);
    auto msg              = FormatString(
        "GPU WEDGE: fence wait site<%s> frame<%llu> elapsed<%.1f s> waitResult<%d> fenceStatus<%d> threshold<%.1f s>",
        site,
        (unsigned long long)frame,
        elapsed,
        int(res),
        int(fence_status),
        timeout);
    fprintf(stderr, "[VKWEDGE] %s\n", msg.c_str());
    fflush(stderr);
    OrkAssertI(false, msg.c_str());
    return;
  }
}

} // namespace ork::lev2::vulkan
