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

void EdgeLoop::reverseOf(const EdgeLoop& src) {
  _edges = _reverse_edgelist(src._edges);
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
    // printf("Create New Chain vb<%d>\n", vb->_poolindex);
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
edge_chain_ptr_t EdgeChainLinker::findChainForVertex(vertex_ptr_t va) {
  for (auto chain : _edge_chains) {
    auto& edges    = chain->_edges;
    auto last_edge = *edges.rbegin();
    if (last_edge->_vertexB == va) {
      return chain;
    }
    auto first_edge = *edges.begin();
    if (first_edge->_vertexA == va) {
      EdgeChain reversed;
      reversed.reverseOf(*chain);
      chain->_edges = reversed._edges;
      return chain;
    }
  }
  return nullptr;
}
//////////////////////////////////////////////////////////
void EdgeChainLinker::removeChain(edge_chain_ptr_t chain_to_remove) {
  // printf( "removeChain chain<%p> numedges<%zu>\n", (void*) chain_to_remove.get(), chain_to_remove->_edges.size() );
  auto the_lambda = std::remove_if(_edge_chains.begin(), _edge_chains.end(), [chain_to_remove](edge_chain_ptr_t testchain) {
    return (testchain == chain_to_remove);
  });
  _edge_chains.erase(the_lambda, _edge_chains.end());
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

  // printf("[%s] prelink numchains<%zu>\n", _name.c_str(), _edge_chains.size());
  for (int i = 0; i < _edge_chains.size(); i++) {
    auto chain = _edge_chains[i];
    auto d = chain->dump();
    if (0)
      printf(
          "[%s] chain %d:%p | numedges<%zu> [%s]\n",
          _name.c_str(),
          i,
          chain.get(),
          chain->_edges.size(),
          d.c_str());
  }

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

    for (auto c : _edge_chains) {

      auto subj_vtx = (*c->_edges.begin())->_vertexA;
      auto c2       = findChainForVertex(subj_vtx);
      if (c2 != c) {
        left_chain  = c2;
        right_chain = c;
        break;
      }
    }

    //////////////////////////////////////////////////
    // join the left and right chain
    //////////////////////////////////////////////////

    if (left_chain and right_chain) {

      size_t presize = left_chain->_edges.size();

      left_chain->_edges.insert(
          left_chain->_edges.end(),    //
          right_chain->_edges.begin(), //
          right_chain->_edges.end());

      size_t postsize = left_chain->_edges.size();

      if (_edge_chains.size() > 1)
        removeChain(right_chain);

      if (0)
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
