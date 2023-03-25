////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/math/plane.hpp>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include "xatlas.h"
#include <deque>

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

void submeshWithTextureUnwrap(const submesh& inpsubmesh, submesh& outsubmesh) {
  int inump = inpsubmesh.GetNumPolys();

  xatlas::ChartOptions chart_options;
  xatlas::PackOptions pack_options;
  xatlas::Atlas* atlas = xatlas::Create();

  for (int ip = 0; ip < inump; ip++) {
    const poly& ply = *inpsubmesh._orderedPolys[ip];
    fvec3 N         = ply.ComputeNormal();

    int inumv = ply.GetNumSides();
    std::vector<vertex_ptr_t> merged_vertices;
    for (int i = 0; i < inumv; i++) {
      auto inp_v0 = *ply._vertices[i];
      inp_v0.mNrm = N;
      auto out_v  = outsubmesh._vtxpool->mergeVertex(inp_v0);
      merged_vertices.push_back(out_v);
    }
    outsubmesh.mergePoly(merged_vertices);
  }
  xatlas::Destroy(atlas);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
