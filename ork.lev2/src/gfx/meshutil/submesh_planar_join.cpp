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

void submeshJoinCoplanar(const submesh& inpsubmesh, submesh& outsmesh){
  auto as_pset = inpsubmesh.asPolyset();
  auto polys_by_plane = as_pset->splitByPlane();
  printf( "NUMPOLYS<%zu> NUMPLANES<%zu>\n", inpsubmesh.GetNumPolys(), polys_by_plane.size() );
  int plane_count = 0;
  for( auto item_by_plane : polys_by_plane ){
    uint64_t plane_hash = item_by_plane.first;
    auto planar_polyset = item_by_plane.second;
    auto islands = planar_polyset->splitByIsland();
    bool polyset_larger_than_one = (planar_polyset->_polys.size()>1);
    if(polyset_larger_than_one){
      printf( "plane<%d:%llx> numpolys<%zu> numislands<%zu>\n", plane_count, plane_hash, planar_polyset->_polys.size(), islands.size() );
    }
    int i = 0;
    for( auto island : islands ){
      if(polyset_larger_than_one){
        printf( "  island<%d> numpolys<%zu>\n", i, island->_polys.size() );
      }
      bool loop_joined = false;
      if( island->_polys.size() > 1 ){

        auto loop = island->boundaryLoop();
        int inumedges = loop.size();
        if(inumedges){
          std::vector<vertex_ptr_t> new_vertices;
          for( int ie=0; ie<inumedges; ie++ ){
            auto the_edge = loop[ie];
            auto va = the_edge->_vertexA;
            auto vb = the_edge->_vertexB;

            auto do_vtx = [&](vertex_ptr_t v){
              new_vertices.push_back(outsmesh.mergeVertex(*v));
            };

            if(ie==0){
              do_vtx(va);
            }
            else{
              do_vtx(va);
              do_vtx(vb);
            }
          }
          outsmesh.mergePoly(poly(new_vertices));
          loop_joined = true;
        }
      }
      if(not loop_joined){
        for( auto ip : island->_polys ){
          std::vector<vertex_ptr_t> new_vertices;
          for( auto iv : ip->_vertices ){
            new_vertices.push_back(outsmesh.mergeVertex(*iv));
          }
          outsmesh.mergePoly(poly(new_vertices));
        }
      }
      i++;
    }
    plane_count++;
  }
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////
