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

std::vector<edge_ptr_t> Island::boundaryLoop() const {

  std::vector<edge_ptr_t> rval;


  return rval;
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////
