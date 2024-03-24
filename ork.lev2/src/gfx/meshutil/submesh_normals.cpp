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
    dvec3 N         = p->computeNormal();
    std::vector<vertex_ptr_t> merged_vertices;
    p->visitVertices([&](vertex_const_ptr_t v) {
      auto copy_v0 = *v;
      copy_v0.mNrm = N;
      auto out_v   = outsubmesh.mergeVertex(copy_v0);
      merged_vertices.push_back(out_v);
    });
    outsubmesh.mergePoly(merged_vertices);
  });
}

///////////////////////////////////////////////////////////////////////////////

void submeshWithFaceNormalsAndBinormals(const submesh& inpsubmesh, submesh& outsubmesh) {

  inpsubmesh.visitAllPolys([&](poly_const_ptr_t p) {
    dvec3 N         = p->computeNormal()*-1;
    int numv = p->numVertices();
    std::map<vertex_ptr_t,dvec3> vertex_binormal_sums;
    std::map<vertex_const_ptr_t,vertex_ptr_t> remapped_vertices;
    for( int i=0; i<numv; i++ ) {
      auto inp_v0 = p->vertex(i);
      auto inp_v1 = p->vertex((i+1)%numv);
      auto edge = (inp_v1->mPos-inp_v0->mPos).normalized();
      auto cross_product = N.crossWith(edge).normalized();
      vertex_binormal_sums[inp_v0] += cross_product;
    }
    for( auto v : vertex_binormal_sums ) {
      auto inp_v = v.first;
      auto binormal_sum = v.second;
      auto binormal = binormal_sum.normalized();
      auto vertex_copy = *inp_v;
      vertex_copy.mNrm = N;
      vertex_copy.mUV[0].mMapBiNormal = dvec3_to_fvec3(binormal);
      vertex_copy.miNumUvs = 1;
      auto merged = outsubmesh.mergeVertex(vertex_copy);
      remapped_vertices[inp_v] = merged;
    }
    std::vector<vertex_ptr_t> merged_vertices;
    p->visitVertices([&](vertex_const_ptr_t v) {
      auto it_r = remapped_vertices.find(v);
      OrkAssert(it_r!=remapped_vertices.end());
      auto remapped = it_r->second;
      merged_vertices.push_back(remapped);
    });
    outsubmesh.mergePoly(merged_vertices);
  });
}

///////////////////////////////////////////////////////////////////////////////

void submeshWithSmoothNormals(const submesh& inpsubmesh, submesh& outsubmesh, float threshold_radians) {

  threshold_radians *= 0.5f;

  inpsubmesh.visitAllPolys([&](poly_const_ptr_t p) {

    dvec3 N         = p->computeNormal();

    std::vector<vertex_ptr_t> merged_vertices;
    p->visitVertices([&](vertex_ptr_t inp_v0) {
      auto polys  = inpsubmesh.polysConnectedToVertex(inp_v0);
      dvec3 Naccum;
      int ncount = 0;
      for (auto p_item : polys._the_map) {
        auto p      = p_item.second;
        dvec3 ON    = p->computeNormal();
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
    });
    outsubmesh.mergePoly(merged_vertices);
  });
}

///////////////////////////////////////////////////////////////////////////////

void submeshWithSmoothNormalsAndBinormals(const submesh& inpsubmesh, submesh& outsubmesh, float threshold_radians) {

  submesh with_smooth_normals;
  submeshWithSmoothNormals(inpsubmesh,with_smooth_normals,threshold_radians);

  with_smooth_normals.visitAllPolys([&](poly_const_ptr_t p) {
    //dvec3 N         = p->computeNormal()*-1;
    int numv = p->numVertices();
    std::map<vertex_ptr_t,dvec3> vertex_binormal_sums;
    std::map<vertex_const_ptr_t,vertex_ptr_t> remapped_vertices;
    for( int i=0; i<numv; i++ ) {
      auto inp_v0 = p->vertex(i);
      auto inp_v1 = p->vertex((i+1)%numv);
      auto edge = (inp_v1->mPos-inp_v0->mPos).normalized();
      auto N = inp_v1->mNrm;
      auto cross_product = N.crossWith(edge).normalized();
      vertex_binormal_sums[inp_v0] += cross_product;
    }
    for( auto v : vertex_binormal_sums ) {
      auto inp_v = v.first;
      auto binormal_sum = v.second;
      auto binormal = binormal_sum.normalized();
      auto vertex_copy = *inp_v;
      vertex_copy.mUV[0].mMapBiNormal = dvec3_to_fvec3(binormal);
      vertex_copy.miNumUvs = 1;
      auto merged = outsubmesh.mergeVertex(vertex_copy);
      remapped_vertices[inp_v] = merged;
    }
    std::vector<vertex_ptr_t> merged_vertices;
    p->visitVertices([&](vertex_const_ptr_t v) {
      auto it_r = remapped_vertices.find(v);
      OrkAssert(it_r!=remapped_vertices.end());
      auto remapped = it_r->second;
      merged_vertices.push_back(remapped);
    });
    outsubmesh.mergePoly(merged_vertices);
  });

}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
