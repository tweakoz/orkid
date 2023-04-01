////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/orklut.hpp>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/meshutil/submesh.h>

namespace ork::meshutil {
////////////////////////////////////////////////////////////////

IConnectivity::IConnectivity(submesh* sub) 
  : _submesh(sub) {
}
////////////////////////////////////////////////////////////////
IConnectivity::~IConnectivity(){

}
////////////////////////////////////////////////////////////////
DefaultConnectivity::DefaultConnectivity(submesh* sub) 
  : IConnectivity(sub) {
}
////////////////////////////////////////////////////////////////
poly_index_set_t DefaultConnectivity::polysConnectedToEdge(edge_ptr_t edge, bool ordered) const {
  return polysConnectedToEdge(*edge,ordered);
}
////////////////////////////////////////////////////////////////
poly_index_set_t DefaultConnectivity::polysConnectedToEdge(const edge& ed, bool ordered) const {
  poly_index_set_t output;
  size_t num_polys = _submesh->_orderedPolys.size();
  for (size_t i = 0; i < num_polys; i++) {
    auto p = _submesh->_orderedPolys[i];
    if (p->containsEdge(ed,ordered)) {
      output.insert(i);
    }
  }
  return output;
}
////////////////////////////////////////////////////////////////
poly_index_set_t DefaultConnectivity::polysConnectedToPoly(poly_ptr_t test_p) const {
  poly_index_set_t output;
  auto edges = test_p->edges();
  for (auto e : edges) {
    auto con_polys = this->polysConnectedToEdge(e,false);
    for (auto ip : con_polys ) {
      auto oth_p = _submesh->_orderedPolys[ip];
      if (oth_p != test_p) {
        output.insert(ip);
      }
    }
  }
  return output;
}
////////////////////////////////////////////////////////////////
poly_index_set_t DefaultConnectivity::polysConnectedToPoly(int ip) const {
  auto p = _submesh->_orderedPolys[ip];
  return polysConnectedToPoly(p);
}
////////////////////////////////////////////////////////////////
} //namespace ork::meshutil {
