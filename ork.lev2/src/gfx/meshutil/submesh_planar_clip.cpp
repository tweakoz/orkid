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

static constexpr bool do_front        = true;
static constexpr bool do_back         = true;
static constexpr double PLANE_EPSILON = 0.0001f;

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

struct PolyVtxCount {
  int _front_count  = 0;
  int _back_count   = 0;
  int _planar_count = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct PlanarVertexCategorize {

  PlanarVertexCategorize(const submesh& inpsubmesh, const dplane3& slicing_plane) {

    inpsubmesh.visitAllVertices([&](vertex_const_ptr_t vtx) {
      // todo: fix nonconst
      auto nonconst_vertex = std::const_pointer_cast<struct vertex>(vtx);
      nonconst_vertex->clearAllExceptPosition();
      double point_distance = slicing_plane.pointDistance(vtx->mPos);

      if (point_distance > (-PLANE_EPSILON)) {
        _front_verts.insert(nonconst_vertex);
      } else if (point_distance < (PLANE_EPSILON)) {
        _back_verts.insert(nonconst_vertex);
      } else { // on plane
        _planar_verts.insert(nonconst_vertex);
      }
    });

    //printf("_front_verts<%zu>\n", _front_verts.size());
    //printf("_back_verts<%zu>\n", _back_verts.size());
    //printf("_planar_verts<%zu>\n", _planar_verts.size());
  }

  PolyVtxCount categorizePolygon(poly_const_ptr_t input_poly) const {
    PolyVtxCount counts;
    for (auto v : input_poly->_vertices) {
      if (_front_verts.contains(v)) {
        counts._front_count++;
      }
      if (_back_verts.contains(v)) {
        counts._back_count++;
      }
      if (_planar_verts.contains(v)) {
        counts._planar_count++;
      }
    }
    return counts;
  }

  vertex_set_t _front_verts;
  vertex_set_t _back_verts;
  vertex_set_t _planar_verts;
};

///////////////////////////////////////////////////////////////////////////////

vertex_set_t addWholePoly(poly_const_ptr_t src_poly, submesh& dest) {
  std::vector<vertex_ptr_t> new_verts;
  vertex_set_t added;
  for (auto v : src_poly->_vertices) {
    OrkAssert(v);
    auto newv = dest.mergeVertex(*v);
    new_verts.push_back(newv);
    added.insert(newv);
  }
  dest.mergePoly(Polygon(new_verts));
  return added;
}

///////////////////////////////////////////////////////////////////////////////

struct Clipper {

  Clipper(const submesh& inpsubmesh, // 
          const dplane3& slicing_plane, //
          submesh& outsmesh_Front, //
          submesh& outsmesh_Back ) { //

    /////////////////////////////////////////////////////////////////////
    // categorize all vertices in input mesh
    /////////////////////////////////////////////////////////////////////

    PlanarVertexCategorize categorized(inpsubmesh, slicing_plane);

    /////////////////////////////////////////////////////////////////////
    // input mesh polygon loop
    /////////////////////////////////////////////////////////////////////

    inpsubmesh.visitAllPolys([&](poly_const_ptr_t input_poly) {
      int numverts    = input_poly->GetNumSides();
      auto polyvtxcnt = categorized.categorizePolygon(input_poly);
      //////////////////////////////////////////////
      // all of this poly's vertices in front ? -> trivially route to outsmesh_Front
      //////////////////////////////////////////////
      if (numverts == polyvtxcnt._front_count) {
        addWholePoly(input_poly, outsmesh_Front);
      }
      //////////////////////////////////////////////
      // all of this poly's vertices in back ? -> trivially route to outsmesh_Back
      //////////////////////////////////////////////
      else if (numverts == polyvtxcnt._back_count) { // all back ?
        addWholePoly(input_poly, outsmesh_Back);
      }
      //////////////////////////////////////////////
      // the remaining are those which must be clipped against plane
      //////////////////////////////////////////////
      else {

        mupoly_clip_adapter clip_input;
        mupoly_clip_adapter clipped_front;
        mupoly_clip_adapter clipped_back;

        /////////////////////////////////////////////////
        // fill in mupoly_clip_adapter clip_input
        /////////////////////////////////////////////////

        int inumv = input_poly->GetNumSides();
        for (int iv = 0; iv < inumv; iv++) {
          auto v = inpsubmesh.vertex(input_poly->GetVertexID(iv));
          clip_input.AddVertex(*v);
        }

        /////////////////////////////////////////////////
        // clip the input poly into clipped_front, clipped_back
        /////////////////////////////////////////////////

        bool ok = slicing_plane.ClipPoly(clip_input, clipped_front, clipped_back);

        ///////////////////////////////////////////

        auto process_clipped_poly = [&](std::vector<vertex>& clipped_poly_vertices,     //
                                        submesh& outsubmesh,                            //
                                        std::deque<vertex_ptr_t>& planar_verts_deque) { //
          std::vector<vertex_ptr_t> merged_vertices;

          /////////////////////////////////////////
          // classify all points in clipped poly, with respect to plane
          //  put all points which live on plane into planar_verts_deque
          /////////////////////////////////////////

          for (auto& v : clipped_poly_vertices) {

            auto merged_v = outsubmesh.mergeVertex(v);
            merged_v->clearAllExceptPosition();
            merged_vertices.push_back(merged_v);
            double point_dist_to_plane = abs(slicing_plane.pointDistance(merged_v->mPos));
            if (point_dist_to_plane < PLANE_EPSILON) {
              planar_verts_deque.push_back(merged_v);
            } else {
            }
          }

          /////////////////////////////////////////
          // if we have enough merged vertices for a polygon,
          //  then create a polygon
          /////////////////////////////////////////

          if (merged_vertices.size() >= 3) {
            auto out_bpoly = std::make_shared<Polygon>(merged_vertices);
            addWholePoly(out_bpoly, outsubmesh);
          }

        };

        ///////////////////////////////////////////

        if (do_front)
          process_clipped_poly(clipped_front.mVerts, outsmesh_Front, front_planar_verts_deque);

        if (do_back)
          process_clipped_poly(clipped_back.mVerts, outsmesh_Back, back_planar_verts_deque);
      } // clipped ?
    }); // inpsubmesh.visitAllPolys( [&](poly_const_ptr_t input_poly){
  }

  edge_vect_t back_planar_edges, front_planar_edges;
  std::deque<vertex_ptr_t> front_planar_verts_deque;
  std::deque<vertex_ptr_t> back_planar_verts_deque;
};

///////////////////////////////////////////////////////////////////////////////

void _submeshClipWithPlaneConvex(
    const submesh& inpsubmesh, //
    dplane3& slicing_plane,    //
    bool close_mesh,
    bool flip_orientation,
    submesh& outsmesh_Front, //
    submesh& outsmesh_Back) {

  Clipper clipper(inpsubmesh, slicing_plane, outsmesh_Front, outsmesh_Back);

  ///////////////////////////////////////////////////////////
  // close mesh
  ///////////////////////////////////////////////////////////

  auto do_close = [&](submesh& outsubmesh, //
                      std::deque<vertex_ptr_t>& planar_verts_deque,
                      bool front) { //

    if( planar_verts_deque.size() < 3){
      return;
    }
    
    /////////////////////////////////////////
    // merge planar verts and compute center
    /////////////////////////////////////////

    std::vector<vertex_ptr_t> merged_vertices;
    for (auto v: planar_verts_deque ) {
      auto vm = outsubmesh.mergeVertex(*v);
      merged_vertices.push_back(vm);
    }
    vertex temp_loop_center;
    temp_loop_center.center(merged_vertices);
    auto center_vertex = outsubmesh.mergeVertex(temp_loop_center);

    /////////////////////////////////////////
    // sort merged vertices by angle around center (relative to first vertex)
    /////////////////////////////////////////


    auto v0 = merged_vertices[0];
    auto d0 = (v0->mPos - center_vertex->mPos).normalized();

    std::map<double,vertex_ptr_t> vertices_by_angle;
    vertices_by_angle[0] = v0;

    for( int iv=1; iv<merged_vertices.size(); iv++ ) {
      auto vn = merged_vertices[iv];
      auto dn = (vn->mPos - center_vertex->mPos).normalized();
      auto cross = dn.crossWith(d0);
      double angle = dn.orientedAngle(d0, cross);
      vertices_by_angle[-angle] = vn;
    }

    std::vector<vertex_ptr_t> sorted_vertices;
    for( auto it=vertices_by_angle.begin(); it!=vertices_by_angle.end(); it++ ){
      auto v = it->second;
      sorted_vertices.push_back(v);
    }

    outsubmesh.mergePoly(Polygon(sorted_vertices));

  };

  if (do_front and close_mesh)
    do_close(outsmesh_Front, clipper.front_planar_verts_deque, true);
  if (do_back and close_mesh)
    do_close(outsmesh_Back, clipper.back_planar_verts_deque, false);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void _submeshClipWithPlaneConcave(
    const submesh& inpsubmesh, //
    dplane3& slicing_plane,    //
    bool close_mesh,
    bool flip_orientation,
    submesh& outsmesh_Front, //
    submesh& outsmesh_Back) {

  Clipper clipper(inpsubmesh, slicing_plane, outsmesh_Front, outsmesh_Back);

  ///////////////////////////////////////////////////////////
  // close mesh
  ///////////////////////////////////////////////////////////

  auto do_close = [&](submesh& outsubmesh, //
                      std::deque<vertex_ptr_t>& planar_verts_deque,
                      bool front) { //
    /////////////////////////////////////////
    //  take note of edges which lie on the
    //  slicing plane
    /////////////////////////////////////////

    edge_vect_t planar_edges;

    while (planar_verts_deque.size() >= 2) {
      auto v0 = planar_verts_deque[0];
      auto v1 = planar_verts_deque[1];
      auto e  = std::make_shared<edge>(v0, v1);
      planar_edges.push_back(e);
      planar_verts_deque.pop_front();
      planar_verts_deque.pop_front();
    }

    /////////////////////////////////////////
    // we have some edges on the cutting plane
    /////////////////////////////////////////

    if (planar_edges.size()) {

      // link edge chains into edge loops

      EdgeChainLinker _linker;
      _linker._name = outsubmesh.name;
      for (auto edge : planar_edges) {
        _linker.add_edge(edge);
      }
      _linker.link();

      // create a new triangle fan for each edge loop

      for (auto loop : _linker._edge_loops) {
          
          // sort vertices by angle around center (relative to first vertex)
  
          auto v0 = loop->_edges[0]->_vertexA;
          auto d0 = (v0->mPos - loop->_center).normalized();
  
          std::map<double,vertex_ptr_t> vertices_by_angle;
          vertices_by_angle[0] = v0;
  
          for( int iv=1; iv<loop->_edges.size(); iv++ ) {
            auto vn = loop->_edges[iv]->_vertexA;
            auto dn = (vn->mPos - loop->_center).normalized();
            auto cross = dn.crossWith(d0);
            double angle = dn.orientedAngle(d0, cross);
            vertices_by_angle[-angle] = vn;
          }
  
          std::vector<vertex_ptr_t> sorted_vertices;
          for( auto it=vertices_by_angle.begin(); it!=vertices_by_angle.end(); it++ ){
            auto v = it->second;
            sorted_vertices.push_back(v);
          }

          outsubmesh.mergePoly(Polygon(sorted_vertices));


      }

    } // if (planar_edges.size()) {

  };

  if (do_front and close_mesh)
    do_close(outsmesh_Front, clipper.front_planar_verts_deque, true);
  if (do_back and close_mesh)
    do_close(outsmesh_Back, clipper.back_planar_verts_deque, false);

  ///////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void submeshClipWithPlane(
    const submesh& inpsubmesh, //
    dplane3& slicing_plane,    //
    bool close_mesh,
    bool flip_orientation,
    submesh& outsmesh_Front, //
    submesh& outsmesh_Back) {

  bool is_convex = inpsubmesh.isConvexHull();
  is_convex      = false;

  if (is_convex) {
    _submeshClipWithPlaneConvex(
        inpsubmesh,       //
        slicing_plane,    //
        close_mesh,       //
        flip_orientation, //
        outsmesh_Front,   //
        outsmesh_Back);
  } else {
    _submeshClipWithPlaneConcave(
        inpsubmesh,       //
        slicing_plane,    //
        close_mesh,       //
        flip_orientation, //
        outsmesh_Front,   //
        outsmesh_Back);
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
