////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/orklut.hpp>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/util/logger.h>

namespace ork::meshutil {
static logchannel_ptr_t logchan_connectivity = logger()->createChannel("meshutil.connectivity", fvec3(.9, .9, 1), false);
////////////////////////////////////////////////////////////////

IConnectivity::IConnectivity(submesh* sub)
    : _submesh(sub) {
}
////////////////////////////////////////////////////////////////
IConnectivity::~IConnectivity() {
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
  return polysConnectedToEdge(*edge, ordered);
}
////////////////////////////////////////////////////////////////
poly_index_set_t DefaultConnectivity::polysConnectedToEdge(const edge& ed, bool ordered) const {
  poly_index_set_t output;
  size_t num_polys = _orderedPolys.size();
  for (size_t i = 0; i < num_polys; i++) {
    auto p = _orderedPolys[i];
    if (p->containsEdge(ed, ordered)) {
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
    auto con_polys = this->polysConnectedToEdge(e, false);
    for (auto ip : con_polys) {
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
poly_set_t DefaultConnectivity::polysConnectedToVertex(vertex_ptr_t v) const {
  poly_set_t output;
  auto it = _polys_by_vertex.find(v);
  if (it != _polys_by_vertex.end()) {
    output = it->second;
  }
  return output;
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
  for (auto p : _orderedPolys)
    visitor(p);
}
////////////////////////////////////////////////////////////////
void DefaultConnectivity::visitAllPolys(const_poly_void_visitor_t visitor) const {
  for (auto p : _orderedPolys)
    visitor(p);
}
////////////////////////////////////////////////////////////////
void DefaultConnectivity::visitAllVertices(vertex_void_visitor_t visitor) {
  for (auto v : _vtxpool->_orderedVertices)
    visitor(v);
}
////////////////////////////////////////////////////////////////
void DefaultConnectivity::visitAllVertices(const_vertex_void_visitor_t visitor) const {
  for (auto v : _vtxpool->_orderedVertices)
    visitor(v);
}
////////////////////////////////////////////////////////////////
vertex_ptr_t DefaultConnectivity::mergeVertex(const struct vertex& v) {
  return _vtxpool->mergeVertex(v);
}
////////////////////////////////////////////////////////////////
void DefaultConnectivity::removePoly(poly_ptr_t ply) {
  int ipindex = ply->_submeshIndex;
  auto it     = _orderedPolys.begin() + ipindex;
  if (it != _orderedPolys.end()) {
    OrkAssert(ply == *it);
    auto last_poly = _orderedPolys.back();
    _orderedPolys.pop_back();
    _orderedPolys[ipindex]   = last_poly;
    last_poly->_submeshIndex = ipindex;
  }
  auto it2 = _polymap.find(ply->hash());
  if (it2 != _polymap.end()) {
    OrkAssert(ply == it2->second);
    _polymap.erase(it2);
    auto itx = _halfedges_by_poly.find(ply);
    OrkAssert(itx != _halfedges_by_poly.end());
    for( auto he : itx->second ){
      OrkAssert(he);
      if(he->_twin){
        OrkAssert(he->_twin->_twin==he);
        he->_twin->_twin = nullptr;
      }
    }
    _halfedges_by_poly.erase(itx);
  }
  ply->visitVertices([&](vertex_ptr_t v) {
    _centerOfPolysAccum -= v->mPos;
    v->_numConnectedPolys--;
    _centerOfPolysCount--;
    auto it_x = _polys_by_vertex.find(v);
    if (it_x != _polys_by_vertex.end()) {
      auto& polyset = it_x->second;
      polyset.remove(ply);
    }
  });
}
////////////////////////////////////////////////////////////////
void DefaultConnectivity::removePolys(std::vector<poly_ptr_t>& polys) {
  for (auto ply : polys) {
    removePoly(ply);
  }
}
////////////////////////////////////////////////////////////////
void DefaultConnectivity::clearPolys() {
  _orderedPolys.clear();
  _polymap.clear();
  _halfedges_by_poly.clear();
  _polys_by_vertex.clear();
  visitAllVertices([](vertex_ptr_t v) { v->_numConnectedPolys = 0; });
  _centerOfPolysCount = 0;
}
////////////////////////////////////////////////////////////////
poly_ptr_t DefaultConnectivity::mergePoly(const Polygon& ply) {
  poly_ptr_t rval;
  int ipolyindex = numPolys();
  int inumv      = ply.numVertices();
  OrkAssert(inumv >= 3);
  ply.visitVertices([&](vertex_ptr_t v) {
    OrkAssert(v != nullptr);
    v->_numConnectedPolys++;
    _centerOfPolysCount++;
    _centerOfPolysAccum += v->mPos;
  });
  ///////////////////////////////
  // zero area poly removal
  ///////////////////////////////
  double area = ply.computeArea();
  if(area<0.00001){

    std::string poly_str = "[";
    ply.visitVertices([&](vertex_ptr_t v) {
      poly_str += FormatString(" %d", v->_poolindex);
    });
    poly_str += " ]";
    logchan_connectivity->log("Mesh::mergePoly() removing zero area poly %s", poly_str.c_str());
    return nullptr;
  }
  //////////////////////////////
  // dupe check
  //////////////////////////////
  uint64_t ucrc = ply.hash();
  auto itfhm    = _polymap.find(ucrc);
  ///////////////////////////////
  if (itfhm == _polymap.end()) { // no match
    ///////////////////////////////
    // merge poly
    ///////////////////////////////
    int inewpi = (int)_orderedPolys.size();
    Polygon temp_poly   = ply;
    temp_poly.SetAnnoMap(ply.GetAnnoMap());
    auto new_poly = std::make_shared<Polygon>(temp_poly);
    _orderedPolys.push_back(new_poly);
    new_poly->_submeshIndex = inewpi;
    _polymap[ucrc]          = new_poly;
    //////////////////////////////////////////////////
    // add edges
    //////////////////////////////////////////////////
    for (int iv=0; iv<inumv; iv++) {
      auto v0 = new_poly->vertex(iv);
      auto v1 = new_poly->vertex((iv+1)%inumv);
      auto e0 = _makeHalfEdge(v0,v1,new_poly);
    }
    for (int iv=0; iv<inumv; iv++) {
      auto v0 = new_poly->vertex(iv);
      _polys_by_vertex[v0].insert(new_poly);
    }
    //////////////////////////////////////////////////
    // add n sided counters
    _polyTypeCounter[inumv]++;
    //////////////////////////////////////////////////
    rval = new_poly;
  } else {
    rval = itfhm->second;
  }
  return rval;
}
////////////////////////////////////////////////////////////////
halfedge_ptr_t DefaultConnectivity::_makeHalfEdge(vertex_ptr_t a,
                                                  vertex_ptr_t b, 
                                                  poly_ptr_t p) {

  auto& this_poly_edges = _halfedges_by_poly[p];

  size_t prev_count = this_poly_edges.size();

  ////////////////////////
  // create edge
  ////////////////////////

  auto he = std::make_shared<HalfEdge>();
  he->_vertexA = a;
  he->_vertexB = b;
  he->_polygon = p;

  ////////////////////////
  // link poly's previous next edge to this edge
  ////////////////////////

  if(prev_count>0){
    OrkAssert(this_poly_edges[prev_count-1]->_vertexB==a);
    this_poly_edges[prev_count-1]->_next = he;
  }

  ////////////////////////
  // add this edge to poly's edge list                                                
  ////////////////////////

  this_poly_edges.push_back(he);

  ////////////////////////
  // link this edges next to poly's first edge
  ////////////////////////

  he->_next = this_poly_edges[0];

  ////////////////////////
  // add to connectivity DB's halfedge map
  ////////////////////////

  _halfedge_map[he->hash()] = he;

  ////////////////////////
  // link twins (if any)
  ////////////////////////

  HalfEdge check_twin;
  check_twin._vertexA = b;
  check_twin._vertexB = a;
  auto ittwin         = _halfedge_map.find(check_twin.hash());
  if (ittwin != _halfedge_map.end()) {
    he->_twin = ittwin->second;
    ittwin->second->_twin = he;
  }

  ////////////////////////

  return he;
}
///////////////////////////////////////////////////////////////////////////////

dvec3 DefaultConnectivity::centerOfPolys() const {
  /*
  std::unordered_set<vertex_ptr_t> all_mesh_verts;
  visitAllPolys([&](poly_const_ptr_t the_poly) {
    all_mesh_verts.insert(the_poly->_vertices[0]);
    all_mesh_verts.insert(the_poly->_vertices[1]);
    all_mesh_verts.insert(the_poly->_vertices[2]);
  });
  dvec3 center;
  for (auto v : all_mesh_verts) {
    center += v->mPos;
  }
  center *= 1.0 / double(all_mesh_verts.size());
  //printf( "xxx<%f %f %f> center<%f %f %f>\n", xxx.x, xxx.y, xxx.z, center.x, center.y, center.z );
  //return center;
  */
  return _centerOfPolysAccum * 1.0 / double(_centerOfPolysCount);
}
  halfedge_vect_t DefaultConnectivity::edgesForPoly(poly_ptr_t p) const{
    auto it = _halfedges_by_poly.find(p);
    if( it != _halfedges_by_poly.end() )
      return it->second;
    return halfedge_vect_t();
  }

////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
