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

std::vector<island_ptr_t> PolySet::splitByIsland() const{

  std::vector<island_ptr_t> islands;

  auto copy_of_polys = _polys;

  while(copy_of_polys.size()>0){

    std::unordered_set<poly_ptr_t> processed;

    for( auto p : copy_of_polys ){
      submesh::PolyVisitContext visit_ctx;
      visit_ctx._visitor = [&](poly_ptr_t p) -> bool {
          processed.insert(p);
          return true;
      };
      auto par_submesh = p->_parentSubmesh;
      par_submesh->visitConnectedPolys(p,visit_ctx);
    }

    if( processed.size() ){
      auto island = std::make_shared<Island>();
      islands.push_back(island);
      for( auto p : processed ){
        auto itp = copy_of_polys.find(p);
        OrkAssert(itp!=copy_of_polys.end());
        copy_of_polys.erase(itp);
        island->_polys.insert(p);
      }
    }
  }
  return islands;
}

///////////////////////////////////////////////////////////////////////////////

edge_vect_t Island::boundaryLoop() const {

  //////////////////////////////////////////
  // grab poly indices present in island
  //////////////////////////////////////////
  std::unordered_set<int> polyidcs_in_island;
  for( auto p : _polys ){
    polyidcs_in_island.insert(p->_submeshIndex);
  }
  //////////////////////////////////////////
  // 
  //////////////////////////////////////////

  std::unordered_set<edge_ptr_t> loose_edges;
  for( auto p : _polys ){
    size_t num_edges = p->_edges.size();
    OrkAssert(num_edges!=2);
    int poly_index = p->_submeshIndex;

    // find num connections within island
    for(auto e : p->_edges) {
      int inumcon_in_island = 0;
      for( int con : e->_connectedPolys ){
        if(con!=poly_index){
          ///////////////////////////////
          // is connected poly in island?
          ///////////////////////////////
          auto it_in_island = polyidcs_in_island.find(con);
          if(it_in_island!=polyidcs_in_island.end()){
            inumcon_in_island++;
          }
          else{
          }
        }
      }
      if(inumcon_in_island==1){
        //printf("poly<%d> edge<%p> inumcon_in_island<%d>\n", poly_index, inumcon_in_island );
        loose_edges.insert(e);
      }
    } // for(auto e : p->_edges) {
  } // for( auto p : _polys ){

  EdgeChainLinker _linker;
  _linker._name = "findboundaryedges";
  for (auto edge : loose_edges) {
    _linker.add_edge(edge);
  }
  _linker.link();
  printf( "boundary edge_count<%zu> loop_count<%zu>\n", loose_edges.size(), _linker._edge_loops.size() );

  //////////////////////////////////////////
  edge_vect_t rval;
  if( _linker._edge_loops.size() == 1 ){
    auto loop = _linker._edge_loops[0];
    for( auto e : loop->_edges ){
      rval.push_back(e);
    }
  }
  //////////////////////////////////////////
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////
