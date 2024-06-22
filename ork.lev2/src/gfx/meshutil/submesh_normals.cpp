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
    dvec3 N = p->computeNormal();
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
    dvec3 N  = p->computeNormal() * -1;
    int numv = p->numVertices();
    std::map<vertex_ptr_t, dvec3> vertex_binormal_sums;
    std::map<vertex_const_ptr_t, vertex_ptr_t> remapped_vertices;
    for (int i = 0; i < numv; i++) {
      auto inp_v0        = p->vertex(i);
      auto inp_v1        = p->vertex((i + 1) % numv);
      auto edge          = (inp_v1->mPos - inp_v0->mPos).normalized();
      auto cross_product = N.crossWith(edge).normalized();
      vertex_binormal_sums[inp_v0] += cross_product;
    }
    for (auto v : vertex_binormal_sums) {
      auto inp_v                      = v.first;
      auto binormal_sum               = v.second;
      auto binormal                   = binormal_sum.normalized();
      auto vertex_copy                = *inp_v;
      vertex_copy.mNrm                = N;
      vertex_copy.mUV[0].mMapBiNormal = dvec3_to_fvec3(binormal);
      vertex_copy.miNumUvs            = 1;
      auto merged                     = outsubmesh.mergeVertex(vertex_copy);
      remapped_vertices[inp_v]        = merged;
    }
    std::vector<vertex_ptr_t> merged_vertices;
    p->visitVertices([&](vertex_const_ptr_t v) {
      auto it_r = remapped_vertices.find(v);
      OrkAssert(it_r != remapped_vertices.end());
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
    dvec3 N = p->computeNormal();
    printf( "N<%g %g %g>\n", N.x, N.y, N.z );
    std::vector<vertex_ptr_t> out_polygon;
    p->visitVertices([&](vertex_ptr_t inp_v0) {
      auto polys = inpsubmesh.polysConnectedToVertex(inp_v0);
      dvec3 Naccum;
      int ncount = 0;
      for (auto p_item : polys._the_map) {
        auto p2      = p_item.second;
        dvec3 ON    = p2->computeNormal();
        float angle = N.angle(ON);
        // printf( "angle<%g> threshold<%g>\n", angle, threshold_radians);
        // if (angle <= threshold_radians) {
        Naccum += ON;
        ncount++;
        //}
      }
      if (ncount == 0) {
        Naccum = N;
      }
      auto copy_v0 = *inp_v0;
      auto NN = Naccum.normalized();
      printf( "NN<%g %g %g>\n", NN.x, NN.y, NN.z );
      copy_v0.mNrm = NN;
      auto out_v   = outsubmesh.mergeVertex(copy_v0);
      out_polygon.push_back(out_v);
    });
    outsubmesh.mergePoly(out_polygon);
  });
}

///////////////////////////////////////////////////////////////////////////////

void submeshWithSmoothNormalsAndBinormals(const submesh& inpsubmesh, submesh& outsubmesh, float threshold_radians) {


  submesh smoothed;
  submeshWithSmoothNormals(inpsubmesh, outsubmesh, threshold_radians);
  threshold_radians *= 0.5f;
  return;
  
  int index = 0;

  using tri_t = std::vector<int>;
  std::map<int, dvec3> pos;
  std::map<int, fvec2> uva;
  std::map<int, dvec3> nrm;
  std::map<int, fvec3> tanA;
  std::map<int, fvec3> tan1;
  std::map<int, fvec3> tan2;
  std::vector<tri_t> triangles;

  smoothed.visitAllPolys([&](poly_const_ptr_t p) {
    OrkAssert(p->numVertices() == 3);
    tri_t tri;
    p->visitVertices([&](vertex_ptr_t inp_v0) {
      pos[index] = inp_v0->mPos;
      uva[index] = inp_v0->mUV[0].mMapTexCoord;
      nrm[index] = inp_v0->mNrm;
      tri.push_back(index++);
    });
    triangles.push_back(tri);
  });

  for (const auto& tri : triangles) {
    int i1   = tri[0];
    int i2   = tri[1];
    int i3   = tri[2];
    tan1[i1] = fvec3(0, 0, 0);
    tan2[i2] = fvec3(0, 0, 0);
  }

  std::set<int> vertices;
  for (const auto& tri : triangles) {
    int i1 = tri[0];
    int i2 = tri[1];
    int i3 = tri[2];
    vertices.insert(i1);
    vertices.insert(i2);
    vertices.insert(i3);

    const auto& v1 = pos[i1];
    const auto& v2 = pos[i2];
    const auto& v3 = pos[i3];

    const auto& w1 = uva[i1];
    const auto& w2 = uva[i2];
    const auto& w3 = uva[i3];

    float x1 = v2.x - v1.x;
    float x2 = v3.x - v1.x;
    float y1 = v2.y - v1.y;
    float y2 = v3.y - v1.y;
    float z1 = v2.z - v1.z;
    float z2 = v3.z - v1.z;

    float s1 = w2.x - w1.x;
    float s2 = w3.x - w1.x;
    float t1 = w2.y - w1.y;
    float t2 = w3.y - w1.y;

    float r = 1.0F / (s1 * t2 - s2 * t1);
    fvec3 sdir((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r, (t2 * z1 - t1 * z2) * r);
    fvec3 tdir((s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r, (s1 * z2 - s2 * z1) * r);

    tan1[i1] += sdir;
    tan1[i2] += sdir;
    tan1[i3] += sdir;

    tan2[i1] += tdir;
    tan2[i2] += tdir;
    tan2[i3] += tdir;
  }

  for (int ivertex : vertices) {

    auto n1 = dvec3_to_fvec3(nrm[ivertex]);
    auto t1 = tan1[ivertex];

    // Gram-Schmidt orthogonalize
    tanA[ivertex] = (t1 - n1 * n1.dotWith(t1)).normalized();

    // Calculate handedness
    float sign = ((n1.crossWith(t1)).dotWith(tan2[ivertex]) < 0.0F) ? -1.0F : 1.0F;

    tanA[ivertex] = tanA[ivertex] * sign;
  }
  /////////

  for (const auto& tri : triangles) {
    int i1 = tri[0];
    int i2 = tri[1];
    int i3 = tri[2];

    const auto& pos1 = pos[i1];
    const auto& pos2 = pos[i2];
    const auto& pos3 = pos[i3];

    const auto& nrm1 = nrm[i1];
    const auto& nrm2 = nrm[i2];
    const auto& nrm3 = nrm[i3];

    const auto& tanA1 = tanA[i1];
    const auto& tanA2 = tanA[i2];
    const auto& tanA3 = tanA[i3];

    const auto& uv1 = uva[i1];
    const auto& uv2 = uva[i2];
    const auto& uv3 = uva[i3];

    vertex v1, v2, v3;

    v1.mPos = pos1;
    v2.mPos = pos2;
    v3.mPos = pos3;

    v1.mNrm = nrm1;
    v2.mNrm = nrm2;
    v3.mNrm = nrm3;

    v1.mUV[0].mMapTexCoord = uv1;
    v2.mUV[0].mMapTexCoord = uv2;
    v3.mUV[0].mMapTexCoord = uv3;

    v1.mUV[0].mMapBiNormal = tanA1;
    v2.mUV[0].mMapBiNormal = tanA2;
    v3.mUV[0].mMapBiNormal = tanA3;

    std::vector<vertex_ptr_t> merged_vertices;
    auto nv1 = outsubmesh.mergeVertex(v1);
    auto nv2 = outsubmesh.mergeVertex(v2);
    auto nv3 = outsubmesh.mergeVertex(v3);
    merged_vertices.push_back(nv1);
    merged_vertices.push_back(nv2);
    merged_vertices.push_back(nv3);

    outsubmesh.mergePoly(merged_vertices);
  }

  // outsubmesh = smoothed;
}

void submeshWithVertexColorsFromNormals(const submesh& inpsubmesh, submesh& outsubmesh) {

  inpsubmesh.visitAllPolys([&](poly_const_ptr_t p) {
    dvec3 N = p->computeNormal();
    std::vector<vertex_ptr_t> out_polygon;
    p->visitVertices([&](vertex_ptr_t inp_v0) {
      auto copy_v0 = *inp_v0;
      N = dvec3(0.5)+N*0.5;
      copy_v0.mCol[0] = fvec4(N.x, N.y, N.z, 1.0f);
      copy_v0.miNumColors = 1;
      auto out_v   = outsubmesh.mergeVertex(copy_v0);
      out_polygon.push_back(out_v);
    });
    outsubmesh.mergePoly(out_polygon);
  });


}


///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
