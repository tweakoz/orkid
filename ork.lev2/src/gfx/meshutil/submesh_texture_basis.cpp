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

  inpsubmesh.visitAllPolys( [&](poly_const_ptr_t input_poly) {
    dvec3 N = input_poly->ComputeNormal();

    int inumv = input_poly->GetNumSides();
    std::vector<vertex_ptr_t> merged_vertices;
    for( int i=0; i<inumv; i++ ){
        auto inp_v0 = *(input_poly->_vertices[i]);
        inp_v0.mNrm = N;
        auto out_v = outsubmesh.mergeVertex(inp_v0);
        merged_vertices.push_back(out_v);
    }
    outsubmesh.mergePoly(merged_vertices);
  });

}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

