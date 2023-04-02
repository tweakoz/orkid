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

void submeshBarycentricUV(const submesh& inpsubmesh, submesh& outsmesh){

  submesh triangulated;
  submeshTriangulate(inpsubmesh,triangulated);

  ////////////////////////////////////////////////////////

  triangulated.visitAllPolys([&](poly_ptr_t inp_poly) {
    int inumpv = inp_poly->GetNumSides();
    std::vector<vertex_ptr_t> out_vertices;
    for( int iv=0; iv<inumpv; iv++ ){
      auto inpv = inp_poly->_vertices[iv];
      vertex temp_out(*inpv);
      switch(iv){
        case 0:
          temp_out.mUV[0].mMapBiNormal = fvec3(1,0,0);
          break;
        case 1:
          temp_out.mUV[0].mMapBiNormal = fvec3(0,1,0);
          break;
        case 2:
          temp_out.mUV[0].mMapBiNormal = fvec3(0,0,1);
          break;
        default:
          OrkAssert(false);
          break;
      }
      temp_out.miNumUvs = 1;
      auto outv = outsmesh.mergeVertex(temp_out);
      out_vertices.push_back(outv);
    }
    outsmesh.mergePoly(Polygon(out_vertices));
  });

  ////////////////////////////////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////
