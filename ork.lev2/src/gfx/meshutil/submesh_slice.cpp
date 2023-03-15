////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/math/plane.hpp>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

void submeshSliceWithPlane(
    const submesh& inpsubmesh, //
    fplane3& slicing_plane,    //
    submesh& outsmeshFront,    //
    submesh& outsmeshBack,
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
      do_poly(outsmeshBack);
    }
    //////////////////////////////////////////////
    else { // the rest
      do_poly(outsmeshIntersects);
    }
  }
}

void submeshClipWithPlane(
    const submesh& inpsubmesh, //
    fplane3& slicing_plane,    //
    bool close_mesh,
    submesh& outsmeshFront, //
    submesh& outsmeshBack) {

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

  std::unordered_set<vertex_ptr_t> added_vertices;

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
    auto add_whole_poly = [&added_vertices](poly_ptr_t src_poly, submesh& dest) {
      std::vector<vertex_ptr_t> new_verts;
      for (auto v : src_poly->_vertices) {
        OrkAssert(v);
        auto newv = dest.mergeVertex(*v);
        new_verts.push_back(newv);
        added_vertices.insert(newv);
      }
      dest.mergePoly(poly(new_verts));
    };
    //////////////////////////////////////////////
    if (numverts == front_count) { // all front ?
      add_whole_poly(input_poly, outsmeshFront);
    }
    //////////////////////////////////////////////
    else if (numverts == back_count) { // all back ?
      add_whole_poly(input_poly, outsmeshBack);
    }
    //////////////////////////////////////////////
    else { // those to clip

      mupoly_clip_adapter clip_input;
      mupoly_clip_adapter clipped_front;
      mupoly_clip_adapter clipped_back;
      int inumv = input_poly->GetNumSides();
      for (int iv = 0; iv < inumv; iv++) {
        auto v = inpsubmesh._vtxpool->GetVertex(input_poly->GetVertexID(iv));
        clip_input.AddVertex(v);
      }

      bool ok = slicing_plane.ClipPoly(clip_input, clipped_front, clipped_back);

      std::vector<vertex_ptr_t> front_verts;
      for (auto& v : clipped_front.mVerts) {
        auto newv = std::make_shared<vertex>(v);
        front_verts.push_back(newv);
      }
      if (front_verts.size() >= 3) {
        auto out_fpoly = std::make_shared<poly>(front_verts);
        add_whole_poly(out_fpoly, outsmeshFront);
      }

      std::vector<vertex_ptr_t> back_verts;
      for (auto& v : clipped_back.mVerts) {
        auto newv = std::make_shared<vertex>(v);
        back_verts.push_back(newv);
      }
      if (back_verts.size() >= 3) {
        auto out_bpoly = std::make_shared<poly>(back_verts);
        add_whole_poly(out_bpoly, outsmeshBack);
      }
    }
  }

  ///////////////////////////////////////////////////////////
  // close mesh
  ///////////////////////////////////////////////////////////

  if (close_mesh and added_vertices.size()) {

    //////////////////////////////////////////
    // identify edge-loop/edge-chain islands
    //////////////////////////////////////////

    struct EdgeChain {
      std::vector<edge_ptr_t> _edges;
      std::unordered_set<vertex_ptr_t> _vertices;
    };
    using edge_chain_ptr_t = std::shared_ptr<EdgeChain>;

    struct EdgeLoop {
      std::vector<edge_ptr_t> _edges;
    };

    bool keep_going = true;

    auto unattached_vertices = added_vertices;

    std::vector<edge_ptr_t> loose_edges;
    std::vector<edge_chain_ptr_t> edge_chains;
    std::vector<EdgeLoop> edge_loops;

    int i = 0;
    for (auto edge_item : outsmeshBack._edgemap) {
      auto edge = edge_item.second;
      auto va   = edge->_vertexA;
      if (added_vertices.find(va) != added_vertices.end()) {
        loose_edges.push_back(edge);
        printf("added edge<%d: %p>\n", i, (void*)edge.get());
        i++;
      }
    }

    while (loose_edges.size()) {

      //////////////////////////////////////
      // grab a loose edge
      //////////////////////////////////////

      auto e = *loose_edges.rbegin();
      loose_edges.pop_back();
      auto va = e->_vertexA;
      auto vb = e->_vertexB;
      printf("grab va<%d> vb<%d>\n", va->_poolindex, vb->_poolindex);

      ////////////////////////////////////////////
      // check if an accepting chain already exists
      ////////////////////////////////////////////

      edge_chain_ptr_t dest_chain;

      for (auto c : edge_chains) {
        auto last_edge = *c->_edges.rbegin();
        if (last_edge->_vertexB == va) {
          dest_chain = c;
        }
      }

      ////////////////////////////////////////////
      // previous dest chain found !
      ////////////////////////////////////////////

      if (dest_chain) {
        printf("Added to Chain\n");
        // find position
        dest_chain->_edges.push_back(e);
        dest_chain->_vertices.insert(vb);
      }

      ////////////////////////////////////////////
      // no dest chain found, create a new one
      ////////////////////////////////////////////

      else {
        printf("Create New Chain vb<%d>\n", vb->_poolindex);
        dest_chain = std::make_shared<EdgeChain>();
        dest_chain->_edges.push_back(e);
        dest_chain->_vertices.insert(vb);
        edge_chains.push_back(dest_chain);
      }

      ////////////////////////////////////////////
    }

    printf("numchains<%zu>\n", edge_chains.size());

    //////////////////////////////////////////////////////////////////////////

    bool progress_made = true;

    while (progress_made) {

      ////////////////////////////////////////////

      int _joinChainL = -1;
      int _joinChainR = -1;

      for( size_t i=0; i<edge_chains.size(); i++ ){
        auto chainL = edge_chains[i];

        auto last_edge = *(chainL->_edges.rbegin());

        for( size_t j=0; j<edge_chains.size(); j++ ){

          auto chainR = edge_chains[j];

          printf( "i<%d> j<%d> chainL<%p> chainR<%p>\n", i, j, (void*) chainL.get(), (void*) chainR.get() );

          if(chainL != chainR){
            auto first_edge = *(chainR->_edges.begin());

            if (last_edge->_vertexB == first_edge->_vertexA) {

              _joinChainL = i;
              _joinChainR = j;
              goto here;

            }
          }
        } // for( size_t j=0; i<edge_chains.size(); j++ ){
      } // for( size_t i=0; i<edge_chains.size(); i++ ){

      here:

      ////////////////////////////////////////////

      progress_made = (_joinChainL>=0) and (_joinChainR>=0);
      if( progress_made ){
        auto chainL = edge_chains[_joinChainL];
        auto chainR = edge_chains[_joinChainR];
        printf("JOIN [L: %d, R: %d]\n", _joinChainL, _joinChainR);

        chainL->_edges.insert( chainL->_edges.end(), //
                               chainR->_edges.begin(), //
                               chainR->_edges.end());

        auto it_rem = edge_chains.begin() + _joinChainR;
        OrkAssert(it_rem!=edge_chains.end());
        edge_chains.erase(it_rem);
        printf("JOIN2 [L: %d, R: %d]\n", _joinChainL, _joinChainR);
      } // if( progress_made ){

    } // while (progress_made) {

  }   // if (close_mesh and added_vertices.size()) {

  ///////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
