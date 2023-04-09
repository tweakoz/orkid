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
  size_t num_planes = polys_by_plane.size();
 // printf( "num_planes<%d>\n", int(num_planes) );
  for( auto item_by_plane : polys_by_plane ){
    uint64_t plane_hash = item_by_plane.first;
    auto planar_polyset = item_by_plane.second;
    auto islands = planar_polyset->splitByIsland();
    bool polyset_larger_than_one = (planar_polyset->_polys.size()>1);
    int i = 0;
    size_t num_islands = islands.size();
    dvec3 plane_n = planar_polyset->averageNormal();
   // printf( "plane<0x%016llx : %g %g %g> num_islands<%d>\n", plane_hash, plane_n.x, plane_n.y, plane_n.z, int(num_islands) );
    for( auto island : islands ){
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

          Polygon new_poly(new_vertices);
          dvec3 poly_n = new_poly.computeNormal();
          float DOT = new_poly.computeNormal().dotWith(plane_n);
          //printf( "poly_n<%g %g %g> DOT<%g>\n", poly_n.x, poly_n.y, poly_n.z, DOT);
          if(DOT>0){
           std::reverse(std::begin(new_vertices), std::end(new_vertices));
            new_poly = Polygon(new_vertices);
          }
          outsmesh.mergePoly(new_poly);
          loop_joined = true;
        }
      }
      else if( island->_polys.size() == 1 ){
          auto p = *island->_polys.begin();
          std::vector<vertex_ptr_t> new_vertices;
          for( auto iv : p->_vertices ){
            new_vertices.push_back(outsmesh.mergeVertex(*iv));
          }
          Polygon new_poly(new_vertices);
          dvec3 poly_n = new_poly.computeNormal();
          float DOT = new_poly.computeNormal().dotWith(plane_n);
          //printf( "poly_n<%g %g %g>\n", poly_n.x, poly_n.y, poly_n.z);
          //printf( "poly_n<%g %g %g> DOT<%g>\n", poly_n.x, poly_n.y, poly_n.z, DOT);
          if(DOT>0){
           std::reverse(std::begin(new_vertices), std::end(new_vertices));
            new_poly = Polygon(new_vertices);
          }
          outsmesh.mergePoly(new_poly);
      }
      else if( island->_polys.size() == 0 ){
        OrkAssert(false);
      }
      if(not loop_joined){
        for( auto ip : island->_polys ){
          std::vector<vertex_ptr_t> new_vertices;
          for( auto iv : ip->_vertices ){
            new_vertices.push_back(outsmesh.mergeVertex(*iv));
          }
          outsmesh.mergePoly(Polygon(new_vertices));
        }
      }
      i++;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////
