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

static constexpr bool debug         = false;

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

HalfEdge::HalfEdge(){

}

uint64_t HalfEdge::hashStatic(vertex_const_ptr_t a, vertex_const_ptr_t b) {
  return uint64_t(a->_poolindex) | (uint64_t(b->_poolindex) << 32);
}
uint64_t HalfEdge::hash() const {
  return hashStatic(_vertexA,_vertexB);
}
submesh* HalfEdge::submesh() const {
  return _vertexA->_parentSubmesh;
}

edge::edge() {
}

////////////////////////////////////////////////////////////////

edge::edge(vertex_ptr_t va, vertex_ptr_t vb)
    : _vertexA(va)
    , _vertexB(vb) {
}

submesh* edge::submesh() const {
  return _vertexA->_parentSubmesh;
}

////////////////////////////////////////////////////////////////

vertex_ptr_t edge::edgeVertex(int iv) const {
  switch (iv) {
    case 0:
      return _vertexA;
      break;
    case 1:
      return _vertexB;
      break;
    default:
      OrkAssert(false);
      break;
  }

  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

uint64_t edge::hash(void) const {
  uint64_t uv = (_vertexA->_poolindex < _vertexB->_poolindex) //
               ? uint64_t(_vertexA->_poolindex) | (uint64_t(_vertexB->_poolindex) << 32)
               : uint64_t(_vertexB->_poolindex) | (uint64_t(_vertexA->_poolindex) << 32);
  return uv;
}

///////////////////////////////////////////////////////////////////////////////

bool edge::Matches(const edge& other) const {
  return ((_vertexA == other._vertexA) && (_vertexB == other._vertexB)) ||
         ((_vertexB == other._vertexA) && (_vertexA == other._vertexB)); // should we care about order here ?
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
static std::vector<edge_ptr_t> _reverse_edgelist(const std::vector<edge_ptr_t>& inp) {
  std::vector<edge_ptr_t> rval;
  for (auto it_e = inp.rbegin(); //
       it_e != inp.rend();       //
       it_e++) {                 //

    auto ne      = std::make_shared<edge>();
    auto e       = *it_e;
    ne->_vertexA = e->_vertexB;
    ne->_vertexB = e->_vertexA;
    rval.push_back(ne);
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void EdgeChain::reverseOf(const EdgeChain& src) {
  _edges = _reverse_edgelist(src._edges);
}

dvec3 EdgeChain::center() const{
  dvec3 rval;
  for(auto e : _edges) {
    rval += e->_vertexA->mPos;
  }
  return rval * 1.0/double(_edges.size());
}

///////////////////////////////////////////////////////////////////////////////

dvec3 EdgeChain::centroid() const{
  // compute centroid via weighting area of triangles
  dvec3 vc = center();
  struct tri{
    dvec3 a,b,c;
  };
  std::multimap<double,tri> tri_by_area;
  double total_area = 0.0;
  for(auto e : _edges) {
    auto va = e->_vertexA->mPos;
    auto vb = e->_vertexB->mPos;
    auto tri_area_abc = dvec3::areaOfTriangle(va,vb,vc);
    total_area += tri_area_abc;
    tri_by_area.insert(std::make_pair(tri_area_abc,tri{va,vb,vc}));
  }
  // add weighted centroid of each triangle
  dvec3 rval;
  for( auto it : tri_by_area ) {
    auto tri_area = it.first;
    auto tri = it.second;
    auto tri_center = (tri.a+tri.b+tri.c)*1.0/3.0;
    rval += tri_center * tri_area;
  }
  rval *= 1.0/total_area;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void EdgeChain::visit(const std::function<void(edge_ptr_t)>& visitor) const {
  for (auto e : _edges) {
    visitor(e);
  }
}

///////////////////////////////////////////////////////////////////////////////

bool EdgeChain::isPlanar() const{
  bool rval = true;
  OrkAssert(_edges.size()>2);
  auto e0 = _edges[0];
  auto e1 = _edges[1];
  auto va = e0->_vertexA;
  auto vb = e0->_vertexB;
  auto vc = e1->_vertexB;
  auto va_pos = va->mPos;
  auto vb_pos = vb->mPos;
  auto vc_pos = vc->mPos;
  Plane plane(va_pos,vb_pos,vc_pos);
  visit([&](edge_ptr_t e){
    auto va = e->_vertexA->mPos;
    auto vb = e->_vertexB->mPos;
    double da = plane.pointDistance(va);
    double db = plane.pointDistance(vb);
    if( fabs(da)>0.0001 or fabs(db)>0.0001 )
      rval = false;
  });
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

dvec3 EdgeChain::avgNormalOfFaces() const{
  dvec3 rval;
  auto c = center();
  for(auto e : _edges) {
    auto a = e->_vertexA->mPos;
    auto b = e->_vertexB->mPos;
    auto dba = (b-a).normalized();
    auto dca = (c-b).normalized();
    rval += dba.crossWith(dca);
  }
  return rval.normalized();
}

///////////////////////////////////////////////////////////////////////////////

dvec3 EdgeChain::avgNormalOfEdges() const{
  dvec3 rval;
  dvec3 nf = avgNormalOfFaces();
  for(auto e : _edges) {
    auto a = e->_vertexA->mPos;
    auto b = e->_vertexB->mPos;
    auto dba = (b-a).normalized();
    auto edge_nrm = dba.crossWith(nf);
    rval += edge_nrm;
  }
  rval = rval.normalized();
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

std::string EdgeChain::dump() const {
  std::string rval;
  vertex_set_t visited_verts;
  for (auto e : _edges) {
    auto va   = e->_vertexA;
    auto vb   = e->_vertexB;
    if( not visited_verts.contains(va) ) {
      visited_verts.insert(va);
      rval += FormatString("%d ", va->_poolindex);
    }
    if( not visited_verts.contains(vb) ) {
      visited_verts.insert(vb);
      rval += FormatString("%d ", vb->_poolindex);
    }
  }
  return rval;
}  

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// ChainLinker - links edge chains into edge loops (if possible)
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

edge_chain_ptr_t EdgeChainLinker::add_edge(edge_ptr_t e) {
  auto va = e->_vertexA;
  auto vb = e->_vertexB;
  _vtxrefcounts[va]++;
  _vtxrefcounts[vb]++;

  // printf("[%s] EDGE va<%d> vb<%d>\n", _name.c_str(), va->_poolindex, vb->_poolindex);

  // printf( "[%s] VA<%g %g %g> <%g %g %g>\n", _name.c_str(), va->mPos.x, va->mPos.y, va->mPos.z, va->mNrm.x, va->mNrm.y,
  // va->mNrm.z ); printf( "[%s] VB<%g %g %g> <%g %g %g>\n", _name.c_str(), vb->mPos.x, vb->mPos.y, vb->mPos.z, vb->mNrm.x,
  // vb->mNrm.y, vb->mNrm.z );

  //////////////////////////////////
  edge_chain_ptr_t dest_chain;
  for (auto c : _edge_chains) {
    auto last_edge = *c->_edges.rbegin();
    if (last_edge->_vertexB == va) {
      dest_chain = c;
    } else if (last_edge->_vertexB == vb) {
      std::swap(va, vb);
      std::swap(e->_vertexA, e->_vertexB);
      dest_chain = c;
    }
  }
  //////////////////////////////////
  // previous dest chain found !
  //////////////////////////////////
  if (dest_chain) {
    // printf("Added to Chain\n");
    //  find position
    dest_chain->_edges.push_back(e);
    dest_chain->_vertices.insert(vb);
  }
  //////////////////////////////////
  // no dest chain found, create a new one
  //////////////////////////////////
  else {
    if(debug)printf("Create New Chain vb<%d>\n", vb->_poolindex);
    dest_chain = std::make_shared<EdgeChain>();
    dest_chain->_edges.push_back(e);
    dest_chain->_vertices.insert(vb);
    _edge_chains.push_back(dest_chain);
  }
  return dest_chain;
}
//////////////////////////////////////////////////////////
bool EdgeChainLinker::loops_possible() const {
  for (auto vrcitem : _vtxrefcounts) {
    vertex_ptr_t vtx = vrcitem.first;
    int count        = vrcitem.second;
    if (count != 2) {
      return false;
    }
  }
  return true;
}
//////////////////////////////////////////////////////////
edge_chain_ptr_t EdgeChainLinker::findChainToLink(edge_chain_ptr_t lch) {
  auto va = (*lch->_edges.begin())->_vertexA;
  auto vb = (*lch->_edges.rbegin())->_vertexB;
  for (auto chain : _edge_chains) {
    if( lch != chain){
      auto& edges    = chain->_edges;
      auto last_edge = *edges.rbegin();
      if (last_edge->_vertexB == va) {
        return chain;
      }
      else if (last_edge->_vertexB == vb) {
        EdgeChain reversed;
        reversed.reverseOf(*chain);
        chain->_edges = reversed._edges;
        return chain;
      }
      auto first_edge = *edges.begin();
      if (first_edge->_vertexA == va) {
        EdgeChain reversed;
        reversed.reverseOf(*chain);
        chain->_edges = reversed._edges;
        return chain;
      }
      else if (first_edge->_vertexA == vb) {
        return chain;
      }
    }
  }
  return nullptr;
}
//////////////////////////////////////////////////////////
bool EdgeChain::containsVertexID(int ivtx) const{
  for(auto e : _edges) {
    if( e->_vertexA->_poolindex == ivtx )
      return true;
    if( e->_vertexB->_poolindex == ivtx )
      return true;
  }
  return false;
}
//////////////////////////////////////////////////////////
bool EdgeChain::containsVertexID(std::unordered_set<int>& verts) const{
  for(auto e : _edges) {
    int iva = e->_vertexA->_poolindex;
    int ivb = e->_vertexB->_poolindex;
    if( verts.find(iva)!=verts.end() )
      return true;
    if( verts.find(ivb)!=verts.end() )
      return true;
  }
  return false;
}
//////////////////////////////////////////////////////////
std::vector<vertex_ptr_t> EdgeChain::orderedVertices() const{
  std::vector<vertex_ptr_t> rval;
  for(auto e : _edges) {
    rval.push_back(e->_vertexA);
  }
  rval.push_back(_edges.back()->_vertexB);
  return rval;
}
//////////////////////////////////////////////////////////
void EdgeChainLinker::removeChain(edge_chain_ptr_t chain_to_remove) {
  if(debug)printf( "removeChain chain<%p> numedges<%zu>\n", (void*) chain_to_remove.get(), chain_to_remove->_edges.size() );
  auto it = std::find(_edge_chains.begin(), _edge_chains.end(), chain_to_remove);
  if( it != _edge_chains.end() ){
    _edge_chains.erase(it);
  }
}
//////////////////////////////////////////////////////////
void EdgeChainLinker::closeChains() {
  std::unordered_set<edge_chain_ptr_t> closed;
  //////////////////////////////////
  for (auto chain : _edge_chains) {
    auto first_edge = *chain->_edges.begin();
    auto last_edge  = *chain->_edges.rbegin();
    if (first_edge->_vertexA == last_edge->_vertexB) {
      closed.insert(chain);
    }
  }
  //////////////////////////////////
  for (auto chain : closed) {
    removeChain(chain);
    auto loop    = std::make_shared<EdgeLoop>();
    loop->_edges = chain->_edges;
    _edge_loops.push_back(loop);
  }
  //////////////////////////////////
  closed.clear();
  for (auto chain : _edge_chains) {
    OrkAssert(chain);
    auto first_edge = *chain->_edges.begin();
    auto last_edge  = *chain->_edges.rbegin();
    auto new_edge = std::make_shared<edge>();
    new_edge->_vertexA = last_edge->_vertexB;
    new_edge->_vertexB = first_edge->_vertexA;
    chain->_edges.push_back(new_edge);
    auto loop    = std::make_shared<EdgeLoop>();
    loop->_edges = chain->_edges;
    _edge_loops.push_back(loop);
  }
  //////////////////////////////////
  for (auto chain : closed) {
    removeChain(chain);
  }
  //////////////////////////////////
  if (_edge_loops.size() == 0) {
    // printf("[%s] EDGELOOP DEBUG\n", _name.c_str());
    for (auto chain : _edge_chains) {
      auto d = chain->dump();
      // printf("[%s]   CHAIN numedges<%zu> [%s]\n", _name.c_str(), chain->_edges.size(), d.c_str());
    }
  }
}

//////////////////////////////////////////////////////////
void EdgeChainLinker::link() {

  // OrkAssert( loops_possible() );

  /////////////////////////////////////////////////////

  if(debug)printf("[%s] prelink numchains<%zu>\n", _name.c_str(), _edge_chains.size());

  //////////////////////////////////////////////////////////////////////////
  // link chains
  //////////////////////////////////////////////////////////////////////////

  bool keep_joining = true;

  while (keep_joining) {

    //////////////////////////////////////////////////
    // find a left and right chain to join
    //////////////////////////////////////////////////

    edge_chain_ptr_t left_chain;
    edge_chain_ptr_t right_chain;

    int ic  = 0;
    if(debug)for (auto c : _edge_chains) {
      printf( "chain<%d: %p [%s]>\n", ic, (void*) c.get(), c->dump().c_str() );
      ic ++;
    }

    for (auto c1 : _edge_chains) {

      auto c2       = findChainToLink(c1);
      if(debug)printf( "chain1<%p> chain2<%p>\n", (void*) c1.get(), (void*) c2.get() );
      if ((c2 != nullptr) and (c2 != c1)) {
        int c1end = c1->_edges.rbegin()->get()->_vertexB->_poolindex;
        int c1beg = c1->_edges.begin()->get()->_vertexA->_poolindex;
        int c2end = c2->_edges.rbegin()->get()->_vertexB->_poolindex;
        int c2beg = c2->_edges.begin()->get()->_vertexA->_poolindex;
        if( c1end == c2beg ){
          left_chain  = c1;
          right_chain = c2;
        }
        else if( c1beg == c2end ){
          left_chain  = c2;
          right_chain = c1;
        }
        else{
          OrkAssert(false);
        }
        break;
      }
    }

    //////////////////////////////////////////////////
    // join the left and right chain
    //////////////////////////////////////////////////

    if( left_chain ){
      if(debug)printf( "lchain<%s>\n", left_chain->dump().c_str() );
    }
    else{
      if(debug)printf( "lchain<null>\n" );
    }
    if( right_chain ){
      if(debug)printf( "rchain<%s>\n", right_chain->dump().c_str() );
    }
    else{
      if(debug)printf( "rchain<null>\n" );
    }

    if (left_chain and right_chain) {

      size_t presize = left_chain->_edges.size();

      left_chain->_edges.insert(
          left_chain->_edges.end(),    //
          right_chain->_edges.begin(), //
          right_chain->_edges.end());

      size_t postsize = left_chain->_edges.size();

      if (_edge_chains.size() > 1)
        removeChain(right_chain);

      if (debug)
        printf(
            "[%s] lchain<%p> rchain<%p> presize<%zu> postsize<%zu> chcount<%zu>\n",
            _name.c_str(),
            (void*)left_chain.get(),
            (void*)right_chain.get(),
            presize,
            postsize,
            _edge_chains.size());

      keep_joining = _edge_chains.size() > 1;
    }

    //////////////////////////////////////////////////
    // no joinable chains...
    //////////////////////////////////////////////////

    else {
      keep_joining = false;
    }

    //////////////////////////////////////////////////

  } // while (keep_joining) {

  closeChains();

  // printf("[%s] postlink numchains<%zu>\n", _name.c_str(), _edge_chains.size());
  // printf("[%s] postlink numloops<%zu>\n", _name.c_str(), _edge_loops.size());
}

void EdgeChainLinker::clear(){
  _edge_chains.clear();
  _edge_loops.clear();
  _vtxrefcounts.clear();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
