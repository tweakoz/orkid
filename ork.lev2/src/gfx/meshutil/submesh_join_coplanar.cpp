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

  using polyvect_t = std::vector<poly_ptr_t>;

  ////////////////////////////////////////////////////////

  int inump = inpsubmesh.GetNumPolys();
  const auto& inp_polys = inpsubmesh._orderedPolys;

  std::unordered_map<uint64_t,polyvect_t> polys_by_plane;

  for (int ip = 0; ip < inump; ip++) {
    auto inp_poly = inp_polys[ip];
    auto plane = inp_poly->computePlane();
    fvec2 nenc = plane.n.normalOctahedronEncoded();
    double quantization = 16384.0;
    uint64_t ux = uint64_t(double(nenc.x)*quantization);    // 12 bits
    uint64_t uy = uint64_t(double(nenc.y)*quantization);    // 12 bits
    uint64_t ud = uint64_t( (quantization*1024.0)+plane.d*quantization ); // ~ 24 bits
    uint64_t hash = ud | (ux<<32) | (uy<<48);
    polys_by_plane[hash].push_back(inp_poly);
    //outsmesh.mergePoly(poly(out_vertices));
  }

  printf( "NUMPLANES<%zu>\n", polys_by_plane.size() );

  int plane_count = 0;
  for( auto item : polys_by_plane ){
    uint64_t hash = item.first;
    printf( "plane<%d:%zx> numpolys<%zu>\n", plane_count, hash, item.second.size() );
    plane_count++;
  }

  ////////////////////////////////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////
