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
  vec3_type closestPointOnLine(const vec3_type& point) const;

  vec3_type _normal;
  vec3_type _point;
};

template <typename T>
InfiniteLine3D<T>::InfiniteLine3D(vec3_type n, vec3_type p)
    : _normal(n)
    , _point(p) {
}

template <typename T> //
T InfiniteLine3D<T>::distanceToPoint(const vec3_type& point) const {
  return _normal.dotWith(point - _point);
}
template <typename T> //
typename InfiniteLine3D<T>::vec3_type InfiniteLine3D<T>::closestPointOnLine(const vec3_type& point) const {
  auto n = _normal.crossWith(point - _point).normalized();
  return n.crossWith(_normal) + _point;
}

template <typename T> T InfiniteLine3D<T>::distanceToLine(const InfiniteLine3D& othline) const {
  auto n = _normal.crossWith(othline._normal).normalized();
  return n.dotWith(_point - othline._point);
}

// quicker algorithm

void submeshConvexHullQuicker(const submesh& inpsubmesh, submesh& outsmesh) {

  using line_t = InfiniteLine3D<double>;
  std::unordered_set<vertex_const_ptr_t> vertices;
  inpsubmesh.visitAllVertices([&](vertex_const_ptr_t va) { vertices.insert(va); });

  auto numverts = vertices.size();

  switch (numverts) {
    case 0:
    case 1:
    case 2:
    case 3:
      outsmesh = submesh();
      return;
    case 4: // tetrahedron
      outsmesh = inpsubmesh;
      return;
    default: // general case
      break;
  }

  ///////////////////////////////////////////////////
  // find 2 most distant vertices
  ///////////////////////////////////////////////////

  double fmaxdist_points = 0.0;
  line_t line;
  vertex_const_ptr_t v0, v1;
  for (auto va : vertices) {
    for (auto vb : vertices) {
      if(va!=vb){
      double fdist = (va->mPos - vb->mPos).magnitudeSquared();
      if (fdist > fmaxdist_points) {
        fmaxdist_points = fdist;
        line            = line_t(va->mPos, vb->mPos);
        v0              = va;
        v1              = vb;
      }
    }
  }
  }
  vertices.erase(v0);
  vertices.erase(v1);

  ///////////////////////////////////////////////////
  // find most distant vertex from line (v_fml)
  ///////////////////////////////////////////////////

  double fmaxdist_line = 0.0;
  std::map<double, vertex_const_ptr_t> line_distmap;
  for (auto va : vertices) {
    double fdist = line.distanceToPoint(va->mPos);
    line_distmap.insert(std::make_pair(fdist, va));
  }
  auto v_fml = line_distmap.rbegin()->second;
  vertices.erase(v_fml);

  ///////////////////////////////////////////////////
  // this gives us the info to create a basis (and a plane)
  ///////////////////////////////////////////////////

  dvec3 line_dir    = line._normal;
  dvec3 line_point  = line.closestPointOnLine(v_fml->mPos);
  dvec3 basis_dir   = (v_fml->mPos - line_point).normalized();
  dvec3 basis_cross = line_dir.crossWith(basis_dir).normalized();

  dplane3 plane(line_point, v_fml->mPos, v_fml->mPos + basis_cross);

  ///////////////////////////////////////////////////
  // sort vertices using angle from line 
  //  utilizing basis - with basis_dir as angle 0
  ///////////////////////////////////////////////////

  std::map<double, vertex_const_ptr_t> angle_distmap;
  for (auto va : vertices) {
    auto va_c = plane.closestPointOnPlane(va->mPos);
    dvec3 va_dir          = (va_c - line_point).normalized();
    double fangle         = basis_dir.orientedAngle(va_dir,line_dir);
    angle_distmap[fangle] = va;
    float fdist           = plane.pointDistance(va->mPos);
    printf("dist<%f> angle<%f> pos<%f %f %f>\n", fdist, fangle, va->mPos.x, va->mPos.y, va->mPos.z);
  }

  OrkAssert(false);
  ///////////////////////////////////////////////////
  // sort vertices with distance from plane
  ///////////////////////////////////////////////////

  std::map<double, vertex_const_ptr_t> plane_distmap;
  for (auto va : vertices) {
    double fdist         = plane.pointDistance(va->mPos);
    plane_distmap[fdist] = va;
  }

  ///////////////////////////////////////////////////
  // construct octahedron from 6 points
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
            dvec3 c      = vc->mPos;
            dvec3 ac     = c - a;
            dvec3 n      = ab.crossWith(ac).normalized();
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
  // submeshConvexHullBruteForce(inpsubmesh, outsmesh);
  submeshConvexHullQuicker(inpsubmesh, outsmesh);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
