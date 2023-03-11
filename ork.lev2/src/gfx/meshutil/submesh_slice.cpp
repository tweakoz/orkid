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

void submeshSliceWithPlane(const submesh& inpsubmesh, //
                           fplane3& slicing_plane, //
                           submesh& outsmeshFront, //
                           submesh& outsmeshBack,
                           submesh& outsmeshIntersects
                           ){


  std::unordered_set<vertex_ptr_t> front_verts, back_verts, planar_verts;

  for( auto item : inpsubmesh._vtxpool._vtxmap ){
    auto vertex = item.second;
    const auto& pos = vertex->mPos;
    float point_distance = slicing_plane.pointDistance(pos);
    if( point_distance > 0.0f ){
      front_verts.insert(vertex);
    }
    else if( point_distance < 0.0f ){
      back_verts.insert(vertex);
    }
    else{ // on plane
      planar_verts.insert(vertex);
    }
  }

  for( auto input_poly : inpsubmesh.RefPolys() ){
    int numverts = input_poly->GetNumSides();
    //////////////////////////////////////////////
    // count sides for which the poly's vertices belong
    //////////////////////////////////////////////
    int front_count = 0;
    int back_count = 0;
    int planar_count = 0;
    for( auto v : input_poly->_vertices ){
      if( front_verts.find(v) != front_verts.end() ){
        front_count++;
      }
      if( back_verts.find(v) != back_verts.end() ){
        back_count++;
      }
      if( planar_verts.find(v) != planar_verts.end() ){
        planar_count++;
      }
    }
    //////////////////////////////////////////////
    auto do_poly = [input_poly](submesh& dest){
      std::vector<vertex_ptr_t> new_verts;
      for( auto v : input_poly->_vertices ){
        OrkAssert(v);
        auto newv = dest.mergeVertex(*v);
        new_verts.push_back(newv);
      }
      switch(new_verts.size()){
        case 3:{
          dest.MergePoly(poly(new_verts[0],new_verts[1],new_verts[2]));
          break;
        }
        case 4:{
          dest.MergePoly(poly(new_verts[0],new_verts[1],new_verts[2],new_verts[3]));
          break;
        }
        default:
          OrkAssert(false);
      }
    };
    //////////////////////////////////////////////
    if( numverts == front_count ){ // all front ?
      do_poly(outsmeshFront);
    }
    //////////////////////////////////////////////
    else if( numverts == back_count ){ // all back ?
      do_poly(outsmeshBack);
    }
    //////////////////////////////////////////////
    else{ // the rest
      do_poly(outsmeshIntersects);
    }
  }


}

void submeshClipWithPlane(const submesh& inpsubmesh, //
                           fplane3& slicing_plane, //
                           submesh& outsmeshFront, //
                           submesh& outsmeshBack
                           ){

  std::unordered_set<vertex_ptr_t> front_verts, back_verts, planar_verts;

  for( auto item : inpsubmesh._vtxpool._vtxmap ){
    auto vertex = item.second;
    const auto& pos = vertex->mPos;
    float point_distance = slicing_plane.pointDistance(pos);
    if( point_distance > 0.0f ){
      front_verts.insert(vertex);
    }
    else if( point_distance < 0.0f ){
      back_verts.insert(vertex);
    }
    else{ // on plane
      planar_verts.insert(vertex);
    }
  }

  for( auto input_poly : inpsubmesh.RefPolys() ){
    int numverts = input_poly->GetNumSides();
    //////////////////////////////////////////////
    // count sides for which the poly's vertices belong
    //////////////////////////////////////////////
    int front_count = 0;
    int back_count = 0;
    int planar_count = 0;
    for( auto v : input_poly->_vertices ){
      if( front_verts.find(v) != front_verts.end() ){
        front_count++;
      }
      if( back_verts.find(v) != back_verts.end() ){
        back_count++;
      }
      if( planar_verts.find(v) != planar_verts.end() ){
        planar_count++;
      }
    }
    //////////////////////////////////////////////
    auto add_whole_poly = [](poly_ptr_t src_poly, submesh& dest){
      std::vector<vertex_ptr_t> new_verts;
      for( auto v : src_poly->_vertices ){
        OrkAssert(v);
        auto newv = dest.mergeVertex(*v);
        new_verts.push_back(newv);
      }
      dest.MergePoly(poly(new_verts));
    };
    //////////////////////////////////////////////
    if( numverts == front_count ){ // all front ?
      add_whole_poly(input_poly,outsmeshFront);
    }
    //////////////////////////////////////////////
    else if( numverts == back_count ){ // all back ?
      add_whole_poly(input_poly,outsmeshBack);
    }
    //////////////////////////////////////////////
    else{ // those to clip

      mupoly_clip_adapter clip_input;
      mupoly_clip_adapter clipped_front;
      mupoly_clip_adapter clipped_back;
      int inumv = input_poly->GetNumSides();
      for (int iv = 0; iv < inumv; iv++) {
        auto v = inpsubmesh._vtxpool.GetVertex(input_poly->GetVertexID(iv));
        clip_input.AddVertex(v);
      }

      bool ok = slicing_plane.ClipPoly(clip_input, clipped_front,clipped_back);

      std::vector<vertex_ptr_t> front_verts;
      for( auto& v : clipped_front.mVerts ){
        auto newv = std::make_shared<vertex>(v);
        front_verts.push_back(newv);
      }
      if(front_verts.size()>=3){
        auto out_fpoly = std::make_shared<poly>(front_verts);
        add_whole_poly(out_fpoly,outsmeshFront);
      }

      std::vector<vertex_ptr_t> back_verts;
      for( auto& v : clipped_back.mVerts ){
        auto newv = std::make_shared<vertex>(v);
        back_verts.push_back(newv);
      }
      if(back_verts.size()>=3){
        auto out_bpoly = std::make_shared<poly>(back_verts);
        add_whole_poly(out_bpoly,outsmeshBack);
      }
    }
  }

                           }


///////////////////////////////////////////////////////////////////////////////
} //namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////
