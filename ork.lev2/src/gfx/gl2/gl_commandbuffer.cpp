////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/memcpy.inl>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/gfx/dds.h>
#include "gl.h"
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/ui/ui.h>
#include <ork/file/file.h>
#include <ork/math/misc_math.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/debug.h>

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

void ContextGL::_doPushCommandBuffer(commandbuffer_ptr_t cmdbuf, rtgroup_ptr_t rtg) {
    OrkAssert(false);
}
void ContextGL::_doPopCommandBuffer() {
    OrkAssert(false);
}
void ContextGL::_doEnqueueSecondaryCommandBuffer(commandbuffer_ptr_t cmdbuf) {
    OrkAssert(false);
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
