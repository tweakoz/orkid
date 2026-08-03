////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "headers/vulkan_ctx.h"
#include <cstdlib>

///////////////////////////////////////////////////////////////////////////////
// ORKID_W17_DESCLIFE : descriptor-set / imageView lifecycle trace.
//
// A draw-time "descriptor is using imageView that has been destroyed" report
// (VUID-vkCmd*-None-08114) names the SET but nothing about how it got that way:
// the bind already returned, so the trap backtrace shows the draw, not the
// capture. These three moments — set WRITE, view DESTROY, cached-set FETCH —
// correlate into the answer. All lines single-line, prefix [DESCLIFE].
//
// Default-off: the FETCH site sits on the per-draw path.
///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2::vulkan {

inline bool desclifeEnabled() {
  static const bool enabled = (nullptr != std::getenv("ORKID_W17_DESCLIFE"));
  return enabled;
}

// Shared clock for all three sites, advanced once per frame by
// VkTextureInterface::_beginFrame. The WRITE->DESTROY frame delta is the
// whole point of the trace, so every site must read the SAME counter.
inline size_t desclifeFrame(vkcontext_rawptr_t ctx) {
  return (ctx and ctx->_txi) ? ctx->_txi->_current_frame : 0;
}

} // namespace ork::lev2::vulkan
