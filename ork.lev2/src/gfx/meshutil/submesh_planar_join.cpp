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

void submeshJoinCoplanar(const submesh& inpsubmesh, submesh& outsmesh){
  auto as_pset = inpsubmesh.asPolyset();
  auto polys_by_plane = as_pset->splitByPlane();
  printf( "NUMPLANES<%zu>\n", polys_by_plane.size() );
  int plane_count = 0;
  for( auto item_by_plane : polys_by_plane ){
    uint64_t plane_hash = item_by_plane.first;
    auto planar_polyset = item_by_plane.second;
    auto islands = planar_polyset->splitByIsland();
    printf( "plane<%d:%llx> numpolys<%zu> numislands<%zu>\n", plane_count, plane_hash, planar_polyset->_polys.size(), islands.size() );
    int i = 0;
    for( auto island : islands ){
      printf( "  island<%d> numpolys<%zu>\n", i, island->_polys.size() );
      i++;
    }
    plane_count++;
  }
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////
