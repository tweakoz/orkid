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

  inpsubmesh.visitAllPolys([&](poly_const_ptr_t p) {
    dvec3 N = p->computeNormal();
    std::vector<vertex_ptr_t> out_polygon;
    p->visitVertices([&](vertex_ptr_t inp_v0) {
      auto polys = inpsubmesh.polysConnectedToVertex(inp_v0);
      dvec3 Naccum;
      int ncount = 0;
      for (auto p_item : polys._the_map) {
        auto p2     = p_item.second;
        dvec3 ON    = p2->computeNormal();
        float angle = N.angle(ON);
        if (angle <= threshold_radians) {
          Naccum += ON;
          ncount++;
        }
      }
      if (ncount == 0) {
        Naccum = N;
      }
      auto copy_v0 = *inp_v0;
      copy_v0.mNrm = Naccum.normalized();
      auto out_v   = outsubmesh.mergeVertex(copy_v0);
      out_polygon.push_back(out_v);
    });
    outsubmesh.mergePoly(out_polygon);
  });
}

///////////////////////////////////////////////////////////////////////////////

void submeshWithSmoothNormalsAndBinormals(const submesh& inpsubmesh, submesh& outsubmesh, float threshold_radians) {
  // First compute smooth normals
  submesh smoothed;
  submeshWithSmoothNormals(inpsubmesh, smoothed, threshold_radians);
  // Then compute binormals from the smoothed normals
  submeshWithBinormalsFromNormalsAndUvs(smoothed, outsubmesh);
}

///////////////////////////////////////////////////////////////////////////////

void submeshWithBinormalsFromNormalsAndUvs(const submesh& inpsubmesh, submesh& outsubmesh) {
  // This function computes tangent/binormal vectors from existing normals and UVs
  // using the standard tangent space computation algorithm

  int index = 0;

  using tri_t = std::vector<int>;
  std::map<int, dvec3> pos;
  std::map<int, fvec2> uva;
  std::map<int, dvec3> nrm;
  std::map<int, fvec4> col;
  std::map<int, fvec3> tanA;
  std::map<int, fvec3> tan1;
  std::map<int, fvec3> tan2;
  std::vector<tri_t> triangles;

  // Collect vertex data from input mesh (must be triangulated)
  inpsubmesh.visitAllPolys([&](poly_const_ptr_t p) {
    OrkAssert(p->numVertices() == 3);
    tri_t tri;
    p->visitVertices([&](vertex_ptr_t inp_v0) {
      pos[index] = inp_v0->mPos;
      uva[index] = inp_v0->mUV[0].mMapTexCoord;
      nrm[index] = inp_v0->mNrm;
      col[index] = inp_v0->mCol[0];
      tan1[index] = fvec3(0, 0, 0);
      tan2[index] = fvec3(0, 0, 0);
      tri.push_back(index++);
    });
    triangles.push_back(tri);
  });

  // Accumulate tangent and bitangent vectors per vertex
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

    float denom = s1 * t2 - s2 * t1;
    if (std::abs(denom) < 1e-6f) {
      // Degenerate UV triangle, skip
      continue;
    }
    float r = 1.0F / denom;
    fvec3 sdir((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r, (t2 * z1 - t1 * z2) * r);
    fvec3 tdir((s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r, (s1 * z2 - s2 * z1) * r);

    tan1[i1] += sdir;
    tan1[i2] += sdir;
    tan1[i3] += sdir;

    tan2[i1] += tdir;
    tan2[i2] += tdir;
    tan2[i3] += tdir;
  }

  // Orthogonalize and compute final tangent (stored as binormal in shader convention)
  for (int ivertex : vertices) {
    auto n1 = dvec3_to_fvec3(nrm[ivertex]);
    auto t1 = tan1[ivertex];

    // Handle zero tangent case
    if (t1.magnitudeSquared() < 1e-6f) {
      // Generate arbitrary tangent perpendicular to normal
      fvec3 up(0, 1, 0);
      if (std::abs(n1.dotWith(up)) > 0.9f) {
        up = fvec3(1, 0, 0);
      }
      tanA[ivertex] = n1.crossWith(up).normalized();
    } else {
      // Gram-Schmidt orthogonalize
      tanA[ivertex] = (t1 - n1 * n1.dotWith(t1)).normalized();

      // Calculate handedness
      float sign = ((n1.crossWith(t1)).dotWith(tan2[ivertex]) < 0.0F) ? -1.0F : 1.0F;
      tanA[ivertex] = tanA[ivertex] * sign;
    }
  }

  // Build output mesh with tangents stored in binormal slot
  for (const auto& tri : triangles) {
    int i1 = tri[0];
    int i2 = tri[1];
    int i3 = tri[2];

    vertex v1, v2, v3;

    v1.mPos = pos[i1];
    v2.mPos = pos[i2];
    v3.mPos = pos[i3];

    v1.mNrm = nrm[i1];
    v2.mNrm = nrm[i2];
    v3.mNrm = nrm[i3];

    v1.mCol[0] = col[i1];
    v2.mCol[0] = col[i2];
    v3.mCol[0] = col[i3];
    v1.miNumColors = 1;
    v2.miNumColors = 1;
    v3.miNumColors = 1;

    v1.mUV[0].mMapTexCoord = uva[i1];
    v2.mUV[0].mMapTexCoord = uva[i2];
    v3.mUV[0].mMapTexCoord = uva[i3];

    v1.mUV[0].mMapBiNormal = tanA[i1];
    v2.mUV[0].mMapBiNormal = tanA[i2];
    v3.mUV[0].mMapBiNormal = tanA[i3];

    v1.miNumUvs = 1;
    v2.miNumUvs = 1;
    v3.miNumUvs = 1;

    std::vector<vertex_ptr_t> merged_vertices;
    auto nv1 = outsubmesh.mergeVertex(v1);
    auto nv2 = outsubmesh.mergeVertex(v2);
    auto nv3 = outsubmesh.mergeVertex(v3);
    merged_vertices.push_back(nv1);
    merged_vertices.push_back(nv2);
    merged_vertices.push_back(nv3);

    outsubmesh.mergePoly(merged_vertices);
  }
}

void submeshWithVertexColorsFromNormals(const submesh& inpsubmesh, submesh& outsubmesh) {

  inpsubmesh.visitAllPolys([&](poly_const_ptr_t p) {
    dvec3 N = p->computeNormal();
    auto VN = dvec3(0.5)+N*0.5;
    VN.lerp(VN,dvec3(0.5),0.5);
    std::vector<vertex_ptr_t> out_polygon;
    p->visitVertices([&](vertex_ptr_t inp_v0) {
      auto copy_v0 = *inp_v0;
      copy_v0.mCol[0] = fvec4(VN.x, VN.y, VN.z, 1.0f);
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
