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

void submeshSliceWithPlane(
    const submesh& inpsubmesh, //
    dplane3& slicing_plane,    //
    submesh& outsmeshFront,    //
    submesh& outsmesh_Back,
    submesh& outsmeshIntersects) {

  std::unordered_set<vertex_ptr_t> front_verts, back_verts, planar_verts;
  inpsubmesh.visitAllVertices([&](vertex_const_ptr_t vtx) {
    auto non_const = std::const_pointer_cast<vertex>(vtx);
    const auto& pos      = non_const->mPos;
    double point_distance = slicing_plane.pointDistance(pos);
    if (point_distance > 0.0) {
      front_verts.insert(non_const);
    } else if (point_distance < 0.0) {
      back_verts.insert(non_const);
    } else { // on plane
      planar_verts.insert(non_const);
    }
  });

  inpsubmesh.visitAllPolys( [&](poly_const_ptr_t input_poly) {
    int numverts = input_poly->numSides();
    //////////////////////////////////////////////
    // count sides for which the poly's vertices belong
    //////////////////////////////////////////////
    int front_count  = 0;
    int back_count   = 0;
    int planar_count = 0;
    input_poly->visitVertices([&](vertex_ptr_t vtx) {
      if (front_verts.find(vtx) != front_verts.end()) {
        front_count++;
      }
      if (back_verts.find(vtx) != back_verts.end()) {
        back_count++;
      }
      if (planar_verts.find(vtx) != planar_verts.end()) {
        planar_count++;
      }
    });
    //////////////////////////////////////////////
    auto do_poly = [input_poly](submesh& dest) {
      std::vector<vertex_ptr_t> new_verts;
      input_poly->visitVertices([&](vertex_ptr_t vtx) {
        auto newv = dest.mergeVertex(*vtx);
        new_verts.push_back(newv);
      });
      switch (new_verts.size()) {
        case 3: {
          dest.mergePoly(Polygon(new_verts[0], new_verts[1], new_verts[2]));
          break;
        }
        case 4: {
          dest.mergePoly(Polygon(new_verts[0], new_verts[1], new_verts[2], new_verts[3]));
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
  });
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
