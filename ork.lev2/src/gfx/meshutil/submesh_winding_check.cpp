////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/math/plane.hpp>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <deque>

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

std::string submeshConvexCheckWindingOrder(const submesh& inpsubmesh) {

  OrkAssert(inpsubmesh.isConvexHull());
  dvec3 C = inpsubmesh.centerOfVertices();

  bool inside_out = false;

  int flipped  = 0;
  int standard = 0;

  inpsubmesh.visitAllPolys([&](poly_const_ptr_t p) {
    dvec3 N1 = p->computeNormal();

    dvec3 poly_c  = p->centerOfMass();
    dvec3 poly_dc = (poly_c - C).normalized();

    float DOT = N1.dotWith(poly_dc);
    // printf( "DOT<%g>\n", DOT);

    if (DOT > 0.0) {
      flipped++;
    } else {
      standard++;
    }
  });

  if (flipped > 0 and standard == 0)
    return std::string("inside-out");
  else if (flipped == 0 and standard > 0)
    return std::string("standard");
  else // mixed
    return std::string("mixed");
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
