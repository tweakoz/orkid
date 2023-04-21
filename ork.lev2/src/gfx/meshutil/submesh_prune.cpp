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

void submeshPrune(const submesh& inpsubmesh, submesh& outsubmesh) {
  std::unordered_map<vertex_const_ptr_t,vertex_ptr_t> used_vertices;
  inpsubmesh.visitAllPolys([&](poly_const_ptr_t p) {
    p->visitVertices([&](vertex_const_ptr_t v) {
        auto it = used_vertices.find(v);
        if(it==used_vertices.end()){
            auto merged = outsubmesh.mergeVertex(*v);
            used_vertices[v] = merged;
        }
    });
  }); // visit all polys       
  inpsubmesh.visitAllPolys([&](poly_const_ptr_t p) {
    std::vector<vertex_ptr_t> merged_vertices;
    p->visitVertices([&](vertex_const_ptr_t v) {
        auto it = used_vertices.find(v);
        OrkAssert(it!=used_vertices.end());
        merged_vertices.push_back(it->second);
    });
    outsubmesh.mergePoly(merged_vertices);
  }); // visit all polys       
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
