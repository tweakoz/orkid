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

void submeshWithFaceNormals(const submesh& inpsubmesh, submesh& outsubmesh) {

  inpsubmesh.visitAllPolys([&](poly_const_ptr_t p) {
    dvec3 N         = p->ComputeNormal();

    int inumv = p->GetNumSides();
    std::vector<vertex_ptr_t> merged_vertices;
    for (int i = 0; i < inumv; i++) {
      auto inp_v0 = *p->_vertices[i];
      inp_v0.mNrm = N;
      auto out_v  = outsubmesh.mergeVertex(inp_v0);
      merged_vertices.push_back(out_v);
    }
    outsubmesh.mergePoly(merged_vertices);
  });
}
void submeshWithSmoothNormals(const submesh& inpsubmesh, submesh& outsubmesh, float threshold_radians) {

  threshold_radians *= 0.5f;

  inpsubmesh.visitAllPolys([&](poly_const_ptr_t p) {

    dvec3 N         = p->ComputeNormal();

    int inumv = p->GetNumSides();
    std::vector<vertex_ptr_t> merged_vertices;
    for (int i = 0; i < inumv; i++) {
      auto inp_v0 = p->_vertices[i];
      auto polys  = inpsubmesh.polysConnectedTo(inp_v0);
      dvec3 Naccum;
      int ncount = 0;
      for (auto p_item : polys._the_map) {
        auto p      = p_item.second;
        dvec3 ON    = p->ComputeNormal();
        float angle = N.angle(ON);
        // printf( "angle<%g> threshold<%g>\n", angle, threshold_radians);
        if (angle <= threshold_radians) {
          Naccum += ON;
          ncount++;
        }
      }
      if (ncount == 0) {
        Naccum = N;
      } else {
        Naccum *= (1.0 / float(ncount));
      }
      auto copy_v0 = *inp_v0;
      copy_v0.mNrm = Naccum;
      auto out_v   = outsubmesh.mergeVertex(copy_v0);
      merged_vertices.push_back(out_v);
    }
    outsubmesh.mergePoly(merged_vertices);
  });
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
