////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/math/plane.hpp>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <deque>

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

void submeshSliceWithPlane(
    const submesh& inpsubmesh, //
    fplane3& slicing_plane,    //
    submesh& outsmeshFront,    //
    submesh& outsmesh_Back,
    submesh& outsmeshIntersects) {

  std::unordered_set<vertex_ptr_t> front_verts, back_verts, planar_verts;

  for (auto item : inpsubmesh._vtxpool->_vtxmap) {
    auto vertex          = item.second;
    const auto& pos      = vertex->mPos;
    float point_distance = slicing_plane.pointDistance(pos);
    if (point_distance > 0.0f) {
      front_verts.insert(vertex);
    } else if (point_distance < 0.0f) {
      back_verts.insert(vertex);
    } else { // on plane
      planar_verts.insert(vertex);
    }
  }

  for (auto input_poly : inpsubmesh.RefPolys()) {
    int numverts = input_poly->GetNumSides();
    //////////////////////////////////////////////
    // count sides for which the poly's vertices belong
    //////////////////////////////////////////////
    int front_count  = 0;
    int back_count   = 0;
    int planar_count = 0;
    for (auto v : input_poly->_vertices) {
      if (front_verts.find(v) != front_verts.end()) {
        front_count++;
      }
      if (back_verts.find(v) != back_verts.end()) {
        back_count++;
      }
      if (planar_verts.find(v) != planar_verts.end()) {
        planar_count++;
      }
    }
    //////////////////////////////////////////////
    auto do_poly = [input_poly](submesh& dest) {
      std::vector<vertex_ptr_t> new_verts;
      for (auto v : input_poly->_vertices) {
        OrkAssert(v);
        auto newv = dest.mergeVertex(*v);
        new_verts.push_back(newv);
      }
      switch (new_verts.size()) {
        case 3: {
          dest.mergePoly(poly(new_verts[0], new_verts[1], new_verts[2]));
          break;
        }
        case 4: {
          dest.mergePoly(poly(new_verts[0], new_verts[1], new_verts[2], new_verts[3]));
          break;
        }
        default:
          OrkAssert(false);
      }
    };
    //////////////////////////////////////////////
    if (numverts == front_count) { // all front ?
      do_poly(outsmeshFront);
    }
    //////////////////////////////////////////////
    else if (numverts == back_count) { // all back ?
      do_poly(outsmesh_Back);
    }
    //////////////////////////////////////////////
    else { // the rest
      do_poly(outsmeshIntersects);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
