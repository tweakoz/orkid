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

void submeshFixWindingOrder(const submesh& inpsubmesh, submesh& outsmesh, bool inside_out) {
  dvec3 C = inpsubmesh.centerOfVertices();

  std::unordered_map<int, int> vtx_map;
  inpsubmesh.visitAllVertices([&](vertex_const_ptr_t v) {
    auto temp_v  = std::make_shared<vertex>();
    temp_v->mPos = v->mPos;
    auto new_v             = outsmesh.mergeVertex(*temp_v);
    vtx_map[v->_poolindex] = new_v->_poolindex;
  });
  inpsubmesh.visitAllPolys([&](poly_const_ptr_t p) {
    std::vector<vertex_ptr_t> newverts;
    for (auto v : p->_vertices) {
      auto it = vtx_map.find(v->_poolindex);
      OrkAssert(it != vtx_map.end());
      auto newv = outsmesh.vertex(it->second);
      newverts.push_back(newv);
    }
    Polygon new_poly(newverts);
    dvec3 N1 = new_poly.computeNormal();

    dvec3 poly_c = new_poly.centerOfMass();
    dvec3 poly_dc = (poly_c-C).normalized();

    float DOT = N1.dotWith(poly_dc);
    //printf( "DOT<%g>\n", DOT);

    bool flip = (DOT<0.0) xor inside_out;
    if(flip){
      std::reverse(newverts.begin(),newverts.end());
      new_poly = Polygon(newverts);
    }
    outsmesh.mergePoly(new_poly);
  });

  outsmesh.name         = inpsubmesh.name;
  outsmesh._aaBoxDirty  = true;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
