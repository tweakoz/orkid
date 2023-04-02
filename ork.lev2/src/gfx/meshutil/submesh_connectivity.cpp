////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/orklut.hpp>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include<ork/util/logger.h>

namespace ork::meshutil {
static logchannel_ptr_t logchan_connectivity = logger()->createChannel("meshutil.connectivity",fvec3(.9,.9,1));
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
  _vtxpool = std::make_shared<vertexpool>();
  for (int i = 0; i < 8; i++)
    _polyTypeCounter[i] = 0;
}
////////////////////////////////////////////////////////////////
poly_index_set_t DefaultConnectivity::polysConnectedToEdge(edge_ptr_t edge, bool ordered) const {
  return polysConnectedToEdge(*edge,ordered);
}
////////////////////////////////////////////////////////////////
poly_index_set_t DefaultConnectivity::polysConnectedToEdge(const edge& ed, bool ordered) const {
  poly_index_set_t output;
  size_t num_polys = _orderedPolys.size();
  for (size_t i = 0; i < num_polys; i++) {
    auto p = _orderedPolys[i];
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
      auto oth_p = _orderedPolys[ip];
      if (oth_p != test_p) {
        output.insert(ip);
      }
    }
  }
  return output;
}
////////////////////////////////////////////////////////////////
poly_index_set_t DefaultConnectivity::polysConnectedToPoly(int ip) const {
  auto p = _orderedPolys[ip];
  return polysConnectedToPoly(p);
}
////////////////////////////////////////////////////////////////
poly_index_set_t DefaultConnectivity::polysConnectedToVertex(vertex_ptr_t v) const {
  return poly_index_set_t();
}
////////////////////////////////////////////////////////////////
vertex_ptr_t DefaultConnectivity::vertex(int id) const {
  return _vtxpool->_orderedVertices[id];
}
////////////////////////////////////////////////////////////////
poly_ptr_t DefaultConnectivity::poly(int id) const {
  return _orderedPolys[id];
}
////////////////////////////////////////////////////////////////
size_t DefaultConnectivity::numPolys() const {
  return _orderedPolys.size();
}
////////////////////////////////////////////////////////////////
size_t DefaultConnectivity::numVertices() const {
  return _vtxpool->_orderedVertices.size();
}
////////////////////////////////////////////////////////////////
void DefaultConnectivity::visitAllPolys(poly_void_visitor_t visitor) {
  for( auto p : _orderedPolys )
    visitor(p);
}
////////////////////////////////////////////////////////////////
void DefaultConnectivity::visitAllPolys(const_poly_void_visitor_t visitor) const {
  for( auto p : _orderedPolys )
    visitor(p);
}
////////////////////////////////////////////////////////////////
void DefaultConnectivity::visitAllVertices(vertex_void_visitor_t visitor) {
  for( auto v : _vtxpool->_orderedVertices )
    visitor(v);
}
////////////////////////////////////////////////////////////////
void DefaultConnectivity::visitAllVertices(const_vertex_void_visitor_t visitor) const {
  for( auto v : _vtxpool->_orderedVertices )
    visitor(v);
}
///////////////////////////////////////////////////////////////////////////////
edge_ptr_t DefaultConnectivity::edgeBetweenPolys(int aind, int bind) const {
  auto pa = poly(aind);
  auto pb = poly(bind);
  edge_ptr_t rval;
  std::vector<vertex_ptr_t> verts_in_both;
  for( auto v_in_b : pb->_vertices ){
    if( pa->containsVertex(v_in_b) ){
      verts_in_both.push_back(v_in_b);
    }
  }
  switch( verts_in_both.size() ){
    case 2: {
      auto v0 = verts_in_both[0];
      auto v1 = verts_in_both[1];

      auto ep0 = pa->edgeForVertices(v0,v1);
      if( nullptr == ep0 ){
        ep0 = pa->edgeForVertices(v0,v1);
      }
      OrkAssert(ep0);

      auto ep1 = pb->edgeForVertices(ep0->_vertexB, ep0->_vertexA);
      OrkAssert(ep1);

      rval = ep0;

    }
    case 0:
    case 1: 
      break;
    default: // we do not support more than 2 verts in common yet
      OrkAssert(false);
      break;
  }

  return rval;
}
////////////////////////////////////////////////////////////////
vertex_ptr_t DefaultConnectivity::mergeVertex(const struct vertex& v) {
  return _vtxpool->mergeVertex(v);
}
////////////////////////////////////////////////////////////////
poly_ptr_t DefaultConnectivity::mergePoly(const Polygon& ply) {
  poly_ptr_t rval;
  int ipolyindex = numPolys();
  Polygon nply      = ply;
  int inumv      = ply.GetNumSides();
  OrkAssert(inumv >= 3 );
  for( auto v : ply._vertices ){
    OrkAssert(v!=nullptr);
  }
  ///////////////////////////////
  // zero area poly removal
  switch (inumv) {
    case 3: {
      if ((ply._vertices[0]->_poolindex == ply._vertices[1]->_poolindex) ||
          (ply._vertices[1]->_poolindex == ply._vertices[2]->_poolindex) ||
          (ply._vertices[2]->_poolindex == ply._vertices[0]->_poolindex)) {
        if(0)logchan_connectivity->log(
            "Mesh::mergePoly() removing zero area tri<%d %d %d>",
            ply._vertices[0]->_poolindex,
            ply._vertices[1]->_poolindex,
            ply._vertices[2]->_poolindex);


        return nullptr;
      }
      break;
    }
    case 4: {
      if ((ply._vertices[0]->_poolindex == ply._vertices[1]->_poolindex) ||
          (ply._vertices[0]->_poolindex == ply._vertices[2]->_poolindex) ||
          (ply._vertices[0]->_poolindex == ply._vertices[3]->_poolindex) ||
          (ply._vertices[1]->_poolindex == ply._vertices[2]->_poolindex) ||
          (ply._vertices[1]->_poolindex == ply._vertices[3]->_poolindex) ||
          (ply._vertices[2]->_poolindex == ply._vertices[3]->_poolindex)) {
        if(0)logchan_connectivity->log(
            "Mesh::mergePoly() removing zero area quad<%d %d %d %d>",
            ply._vertices[0]->_poolindex,
            ply._vertices[1]->_poolindex,
            ply._vertices[2]->_poolindex,
            ply._vertices[3]->_poolindex);

        return nullptr;
      }
      break;
    }
    default:
      break;
      // TODO n-sided polys
  }
  //////////////////////////////
  // dupe check
  //////////////////////////////
  uint64_t ucrc   = ply.hash();
  auto itfhm = _polymap.find(ucrc);
  ///////////////////////////////
  if (itfhm == _polymap.end()) { // no match

    int inewpi = (int)_orderedPolys.size();
    //////////////////////////////////////////////////
    // connect to vertices
    for (int i = 0; i < inumv; i++) {
      auto vtx = ply._vertices[i];
      // vtx->ConnectToPoly(inewpi);
    }
    nply.SetAnnoMap(ply.GetAnnoMap());
    auto new_poly = std::make_shared<Polygon>(nply);
    _orderedPolys.push_back(new_poly);
    new_poly->_submeshIndex = inewpi;
    _polymap[ucrc] = new_poly;
    //////////////////////////////////////////////////
    // add n sided counters
    _polyTypeCounter[inumv]++;
    //////////////////////////////////////////////////
    rval = new_poly;
  }
  else{
    rval = itfhm->second;
  }
  return rval;
}
////////////////////////////////////////////////////////////////
edge_ptr_t DefaultConnectivity::mergeEdge(const edge& e) {
  auto v0 = e._vertexA;
  auto v1 = e._vertexB;
  auto mv0 = mergeVertex(*v0);
  auto mv1 = mergeVertex(*v1);
  return std::make_shared<edge>(mv0,mv1);
}
////////////////////////////////////////////////////////////////
} //namespace ork::meshutil {
