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

  ////////////////////////////////////////////////////////

  int inump = inpsubmesh.GetNumPolys();
  const auto& inp_polys = inpsubmesh._orderedPolys;

  std::unordered_map<uint64_t,PolySet> polys_by_plane;

  for (int ip = 0; ip < inump; ip++) {
    auto inp_poly = inp_polys[ip];
    auto plane = inp_poly->computePlane();

    //////////////////////////////////////////////////////////
    // quantize normals
    //  2^28 possible encodings more or less equally distributed (octahedral encoding)
    //  -> each encoding covers 4.682e-8 steradians (12.57 steradians / 2^28)
    // TODO: make an argument ?
    //////////////////////////////////////////////////////////

    fvec2 nenc = plane.n.normalOctahedronEncoded();
    double normal_quantization = 16383.0;
    uint64_t ux = uint64_t(double(nenc.x)*normal_quantization);        // 14 bits
    uint64_t uy = uint64_t(double(nenc.y)*normal_quantization);        // 14 bits  (total of 2^28 possible normals ~= )

    //////////////////////////////////////////////////////////
    // quantize plane distance
    //   (64km [-32k..+32k] range with .25 millimeter precision)
    // TODO: make an argument ?
    //////////////////////////////////////////////////////////

    double distance_quantization = 4096.0;
    uint64_t ud = uint64_t( (plane.d+32767.0)*distance_quantization ); //  16+12 bits 
    uint64_t hash = ud | (ux<<32) | (uy<<48);
    polys_by_plane[hash]._polys.insert(inp_poly);

  }

  printf( "NUMPLANES<%zu>\n", polys_by_plane.size() );

  int plane_count = 0;



  for( auto item_by_plane : polys_by_plane ){
    uint64_t plane_hash = item_by_plane.first;
    auto& planar_polyset = item_by_plane.second;


    auto islands = planar_polyset.splitByIsland();

    printf( "plane<%d:%llx> numpolys<%zu> numislands<%zu>\n", plane_count, plane_hash, planar_polyset._polys.size(), islands.size() );

    int i = 0;
    for( auto island : islands ){
      printf( "  island<%d> numpolys<%zu>\n", i, island->_polys.size() );
      i++;
    }


    plane_count++;

  }

  ////////////////////////////////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////
