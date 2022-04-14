////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>
#include <ork/math/frustum.h>
#include <ork/math/polar.h>
#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/lev2/gfx/fxstate_instance.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <fstream>

namespace ork::meshutil {

//////////////////////////////////////////////////////////////////////////////
using namespace ork;
struct SplitScreenHorizontalPrimitive {
  //////////////////////////////////////////////////////////////////////////////
  inline void gpuInit(lev2::Context* context) {
    meshutil::submesh __submesh;
    ////////////////////////////////////////////////////////////
    auto addquad = [&](fvec3 vtxa, fvec3 vtxb, fvec3 vtxc, fvec3 vtxd, fvec2 uva, fvec2 uvb, fvec2 uvc, fvec2 uvd) {
      __submesh.addQuad(
          vtxa,
          vtxb,
          vtxc,
          vtxd, // verts
          uva,
          uvb,
          uvc,
          uvd,             // uvs
          fvec4::White()); // vertex color
    };

    addquad(
        fvec3(-1, -1, 0), // vtxa
        fvec3(0, -1, 0),  // vtxb
        fvec3(0, 1, 0),   // vtxc
        fvec3(-1, 1, 0),  // vtxd
        fvec2(0, 0),      // uva
        fvec2(.5, 0),     // uvb
        fvec2(.5, 1),     // uvc
        fvec2(0, 1));     // uvd
    addquad(
        fvec3(0, -1, 1), // vtxa
        fvec3(1, -1, 1), // vtxb
        fvec3(1, 1, 1),  // vtxc
        fvec3(0, 1, 1),  // vtxd
        fvec2(.5, 0),    // uva
        fvec2(1, 0),     // uvb
        fvec2(1, 1),     // uvc
        fvec2(.5, 1));   // uvd
    ////////////////////////////////////////////////////////////

    _primitive = std::make_shared<meshutil::rigidprim_V12T8_t>();
    _primitive->fromSubMesh(__submesh, context);
  }
  //////////////////////////////////////////////////////////////////////////////
  meshutil::rigidprim_V12T8_ptr_t _primitive;
};
using splitscreenhoriz_primitive_ptr_t = std::shared_ptr<SplitScreenHorizontalPrimitive>;

} // namespace ork::meshutil
