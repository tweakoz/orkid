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

void submeshWithTextureBasis(const submesh& inpsubmesh, submesh& outsubmesh){
  int inump = inpsubmesh.GetNumPolys();

  for (int ip = 0; ip < inump; ip++) {
    const poly& ply = *inpsubmesh._orderedPolys[ip];
    fvec3 N = ply.ComputeNormal();

    int inumv = ply.GetNumSides();
    std::vector<vertex_ptr_t> merged_vertices;
    for( int i=0; i<inumv; i++ ){
        auto inp_v0 = *ply._vertices[i];
        inp_v0.mNrm = N;
        auto out_v = outsubmesh._vtxpool->mergeVertex(inp_v0);
        merged_vertices.push_back(out_v);
    }
    outsubmesh.mergePoly(merged_vertices);
  }

}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

