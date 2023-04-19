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

submesh* PolyGroup::submesh() const{
  bool has_polys = _polys.size()>0;
  if( has_polys ){
    auto p = *_polys.begin();
    return p->_parentSubmesh;
  }
  return nullptr;
}

std::vector<island_ptr_t> PolyGroup::splitByIsland() const{

  std::vector<island_ptr_t> islands;

  auto copy_of_polys = _polys;

  while(copy_of_polys.size()>0){

    merged_polyconst_set_t processed;

    for( auto p : copy_of_polys ){
      submesh::PolyVisitContext visit_ctx;
      visit_ctx._visitor = [&](merged_poly_const_ptr_t p) -> bool {
          processed.insert(p);
          return true;
      };
      auto par_submesh = p->_parentSubmesh;
      par_submesh->visitConnectedPolys(p,visit_ctx);
    }

    if( processed.size() ){
      auto island = std::make_shared<Island>();
      islands.push_back(island);
      for( auto it_p : processed._the_map ){
        auto p = it_p.second;
        auto itp = copy_of_polys.find(p);
        if(itp!=copy_of_polys.end()){
          OrkAssert(itp!=copy_of_polys.end());
          copy_of_polys.erase(itp);
          island->_polys.insert(p);
        }
      }
    }
  }
  return islands;
}

///////////////////////////////////////////////////////////////////////////////

std::unordered_map<uint64_t,polygroup_ptr_t> PolyGroup::splitByPlane() const {

  OrkAssert(_polys.size()>0);
  std::unordered_map<uint64_t,polygroup_ptr_t> polyset_by_plane;

  for (auto inp_poly : _polys ) {

   auto plane = inp_poly->computePlane();
   uint64_t hash = plane.hash();

    auto it = polyset_by_plane.find(hash);
    polygroup_ptr_t dest_set;
    if(it!=polyset_by_plane.end()){
      dest_set = it->second;
    }
    else{
      dest_set = std::make_shared<PolyGroup>();
      polyset_by_plane[hash] = dest_set;
    }
    dest_set->_polys.insert(inp_poly);

  }
  OrkAssert(polyset_by_plane.size()>0);

  return polyset_by_plane;
}

dvec3 PolyGroup::averageNormal() const{
  dvec3 avgnorm(0.0);
  for( auto p : _polys ){
    avgnorm += p->computePlane().n;
  }
  return avgnorm.normalized();
}

///////////////////////////////////////////////////////////////////////////////

edge_vect_t Island::boundaryEdges() const {

  auto subm = submesh();
  //////////////////////////////////////////
  // grab poly indices present in island
  //////////////////////////////////////////
  std::set<int> polyidcs_in_island;
  for( auto p : _polys ){
    polyidcs_in_island.insert(p->_submeshIndex);
  }
  //////////////////////////////////////////
  // 
  //////////////////////////////////////////

  edge_set_t loose_edges;
  for( auto p : _polys ){
    auto edges = p->edges();
    size_t num_edges = edges.size();
    OrkAssert(num_edges!=2);
    int poly_index = p->_submeshIndex;

    // find num connections within island
    for(auto e : edges ) {
      int inumcon_in_island = 0;
      for( int con : subm->connectedPolys(*e) ){
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
        int va = e->_vertexA->_poolindex;
        int vb = e->_vertexB->_poolindex;
        //printf("poly<%d> edge[%d->%d] inumcon_in_island<%d>\n", poly_index, va,vb,inumcon_in_island );
        loose_edges.insert(e);
      }
    } // for(auto e : p->_edges) {
  } // for( auto p : _polys ){

  edge_vect_t rval;
  for (auto edge_item : loose_edges._the_map) {
    auto edge = edge_item.second;
    rval.push_back(edge);
  }
  //////////////////////////////////////////
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

edge_vect_t Island::boundaryLoop() const {
  auto subm = submesh();

  //////////////////////////////////////////
  // grab poly indices present in island
  //////////////////////////////////////////
  std::set<int> polyidcs_in_island;
  for( auto p : _polys ){
    polyidcs_in_island.insert(p->_submeshIndex);
  }
  //////////////////////////////////////////
  // 
  //////////////////////////////////////////

  edge_set_t loose_edges;
  for( auto p : _polys ){
    auto edges = p->edges();
    size_t num_edges = edges.size();
    OrkAssert(num_edges!=2);
    int poly_index = p->_submeshIndex;

    // find num connections within island
    for(auto e : edges) {
      int inumcon_in_island = 0;
      for( int con : subm->connectedPolys(*e) ){
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
      int va = e->_vertexA->_poolindex;
      int vb = e->_vertexB->_poolindex;
      //printf("poly<%d> edge[%d->%d] inumcon_in_island<%d>\n", poly_index, va,vb,inumcon_in_island );
      if(inumcon_in_island==0){
        loose_edges.insert(e);
      }
    } // for(auto e : p->_edges) {
  } // for( auto p : _polys ){

  EdgeChainLinker _linker;
  _linker._name = "findboundaryedges";
  for (auto edge_item : loose_edges._the_map) {
    auto edge = edge_item.second;
    _linker.add_edge(edge);
  }
  _linker.link();
  if( _linker._edge_loops.size() ){
    //printf( "boundary edge_count<%zu> loop_count<%zu>\n", loose_edges.size(), _linker._edge_loops.size() );
    for(auto loop : _linker._edge_loops ){
      size_t num_edges = loop->_edges.size();
      for( auto e : loop->_edges ){
        int va = e->_vertexA->_poolindex;
        int vb = e->_vertexB->_poolindex;
        //printf("edge[%d->%d]\n", va,vb );
      }
    }
  }

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
