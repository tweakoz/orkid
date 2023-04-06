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

template <typename T> struct InfiniteLine3D {

  using vec3_type = Vector3<T>;

  InfiniteLine3D() = default;

  InfiniteLine3D(vec3_type n, vec3_type p);
  T distanceToPoint(const vec3_type& point) const;
  T distanceToLine(const InfiniteLine3D& othline) const;

  vec3_type _normal; 
  vec3_type _point;
};

template <typename T> 
InfiniteLine3D<T>::InfiniteLine3D(vec3_type n, vec3_type p) : _normal(n), _point(p) {
}

template <typename T> 
T InfiniteLine3D<T>::distanceToPoint(const vec3_type& point) const {
  return _normal.dotWith(point - _point);
}
template <typename T> 
T InfiniteLine3D<T>::distanceToLine(const InfiniteLine3D& othline) const {
  auto n = _normal.crossWith(othline._normal).normalized();
  return n.dotWith(_point - othline._point);
}


void submeshConvexHullQuicker(const submesh& inpsubmesh, submesh& outsmesh) {

  using line_t = InfiniteLine3D<double> ;
  std::vector<vertex_ptr_t> vertices;
  inpsubmesh.visitAllVertices([&](vertex_const_ptr_t va) { vertices.push_back(outsmesh.mergeVertex(*va)); });

  // quicker algorithm
  auto numverts = vertices.size();

  ///////////////////////////////////////////////////
  // find 2 most distant vertices
  ///////////////////////////////////////////////////

  double fmaxdist_points = 0.0;
  line_t line;
  vertex_ptr_t v0, v1;
  for (int i = 0; i < numverts; i++) {
    auto va = vertices[i];
    for (int j = i + 1; j < numverts; j++) {
      auto vb = vertices[j];
      double fdist = (va->mPos - vb->mPos).magnitudeSquared();
      if (fdist > fmaxdist_points) {
        fmaxdist_points = fdist;
        line = line_t(va->mPos, vb->mPos);
        v0 = va;
        v1 = vb;
      }
    }
  }

  ///////////////////////////////////////////////////
  // construct plane from line and most distant vertex from line v0
  ///////////////////////////////////////////////////

  double fmaxdist_line = 0.0;
  std::multimap<double, vertex_ptr_t> line_distmap;
  for (int i = 0; i < numverts; i++) {
    auto va = vertices[i];
    double fdist = line.distanceToPoint(va->mPos);
    line_distmap.insert(std::make_pair(fdist, va));
  }
  auto v2 = line_distmap.rbegin()->second;
  line_distmap.erase(line_distmap.rbegin()->first);
  auto v3 = line_distmap.rbegin()->second;

  dplane3 plane(v0->mPos, v2->mPos, v3->mPos);

  ///////////////////////////////////////////////////
  // find 2 most distant vertices from plane 
  ///////////////////////////////////////////////////

  double fmaxdist_plane = -1e6;
  double fmindist_plane = 1e6;
  vertex_ptr_t v4, v5;
  for (int i = 0; i < numverts; i++) {
    auto va = vertices[i];
    if (va != v0 && va != v1 && va != v2) {
      double fdist = plane.pointDistance(va->mPos);
      if (fdist > fmaxdist_plane) {
        fmaxdist_plane = fdist;
        v3       = va;
      }
      if (fdist < fmindist_plane) {
        fmindist_plane = fdist;
        v4       = va;
      }
    }
  }

  ///////////////////////////////////////////////////
  // construct octahedron from
  ///////////////////////////////////////////////////


  // create diamond from 6 vertices
  struct triangle_t {
    vertex_ptr_t _v0;
    vertex_ptr_t _v1;
    vertex_ptr_t _v2;
    dplane3 _plane;
  };
  std::unordered_map<uint64_t, triangle_t> xxx;


}

// the inefficient method

 void submeshConvexHullBruteForce(const submesh& inpsubmesh, submesh& outsmesh) {
 
inpsubmesh.visitAllVertices([&](vertex_const_ptr_t va) {
  dvec3 a = va->mPos;
  inpsubmesh.visitAllVertices([&](vertex_const_ptr_t vb) {
    dvec3 b  = vb->mPos;
    dvec3 ab = b - a;
    if (va != vb) {
      inpsubmesh.visitAllVertices([&](vertex_const_ptr_t vc) {
        if (va != vc && vb != vc) {
          dvec3 c = vc->mPos;
          dvec3 ac = c - a;
          dvec3 n = ab.crossWith(ac).normalized();
          bool binside = true;
          inpsubmesh.visitAllVertices([&](vertex_const_ptr_t vd) {
            if (vd != va && vd != vb && vd != vc) {
              dvec3 d  = vd->mPos;
              dvec3 ad = d - a;
              float dp = ad.dotWith(n);
              if (dp > 0.0f) {
                binside = false;
              }
            }
          });
          if (binside) {
            std::vector<vertex_ptr_t> merged_vertices;
            merged_vertices.push_back(outsmesh.mergeVertex(*va));
            merged_vertices.push_back(outsmesh.mergeVertex(*vb));
            merged_vertices.push_back(outsmesh.mergeVertex(*vc));
            outsmesh.mergePoly(merged_vertices);
          }
        }
      });
    }
  });
});
 }

void submeshConvexHull(const submesh& inpsubmesh, submesh& outsmesh) {
    submeshConvexHullBruteForce(inpsubmesh, outsmesh);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
