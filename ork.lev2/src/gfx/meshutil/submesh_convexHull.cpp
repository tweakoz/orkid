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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// iterative method
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void submeshConvexHullIterative(const submesh& inpsubmesh, submesh& outsmesh) {

  ///////////////////////////////////////////////////
  // trivially reject
  ///////////////////////////////////////////////////

  if (inpsubmesh.numVertices() < 4) {
    return;
  }

  ///////////////////////////////////////////////////
  // collect vertices
  ///////////////////////////////////////////////////

  std::unordered_set<vertex_const_ptr_t> vertices;
  inpsubmesh.visitAllVertices([&](vertex_const_ptr_t va) { 
    vertices.insert(va);
    });
  OrkAssert(vertices.size()>=4); // ensure we still have 4 vertices after welding

  ///////////////////////////////////////////////////
  // create initial tetrahedron
  ///////////////////////////////////////////////////

  dvec3 center;
  auto make_vtx = [&]() -> vertex_ptr_t {
    auto va = vertices.begin();
    auto nva = outsmesh.mergeVertex(*(*va));
    center += nva->mPos;
    vertices.erase(va);
    return nva;
  };
  auto nva = make_vtx();
  auto nvb = make_vtx();
  auto nvc = make_vtx();
  auto nvd = make_vtx();
  center *= 0.25;
  
  auto do_flip = [&](vertex_ptr_t a, vertex_ptr_t b, vertex_ptr_t c) -> bool {
    return (a->mPos-center).crossWith(b->mPos-center).dotWith(c->mPos-center)>=0.0;
  };
  auto make_tri = [&](vertex_ptr_t a, vertex_ptr_t b, vertex_ptr_t c) -> poly_ptr_t {
    return do_flip(a,b,c) ? outsmesh.mergeTriangle(a,c,b) : outsmesh.mergeTriangle(a,b,c);
  };

  std::vector<poly_ptr_t> polys;
  polys.push_back(make_tri(nva,nvb,nvc));
  polys.push_back(make_tri(nva,nvc,nvd));
  polys.push_back(make_tri(nva,nvd,nvb));
  polys.push_back(make_tri(nvb,nvc,nvd));

  ///////////////////////////////////////////////////
  // build conflict graph
  ///////////////////////////////////////////////////

  using conflict_graph_t = std::map<vertex_const_ptr_t, std::unordered_set<poly_ptr_t>>;

  conflict_graph_t conflict_graph;

  auto update_conflict_graph = [&]() {
    conflict_graph.clear();
    for( auto v : vertices ){
      auto vpos = v->mPos;
      for( auto p : polys ){
        if(p->computePlane().isPointInFront(vpos)){
          conflict_graph[v].insert(p);
        }
      }
    }
  };

  update_conflict_graph();

  ///////////////////////////////////////////////////
  // iterate on conflict graph
  ///////////////////////////////////////////////////

  while( not conflict_graph.empty() ){

    ///////////////////////////////
    // get next conflict point
    ///////////////////////////////

    auto it_p =  conflict_graph.begin();
    auto conflict_point = it_p->first;
    auto& polyset = it_p->second;
    size_t num_conflicts = polyset.size();
    //printf( "conflict_point<%p> num_conflicts<%zu>\n", conflict_point.get(), num_conflicts );

    ///////////////////////////////
    // remove all conflicting polys from outsmesh
    //  and build edge set
    ///////////////////////////////

    edge_set_t edgeset;
    for( auto it : polyset ){
      auto the_poly = it;

      the_poly->visitEdges([&](edge_ptr_t the_edge) {
        auto reverse_edge = std::make_shared<edge>(the_edge->_vertexB,the_edge->_vertexA);
        if(edgeset.contains(reverse_edge))
          edgeset.remove(the_edge);
        else
          edgeset.insert(the_edge);
      });

      outsmesh.removePoly(the_poly);
    }

    ///////////////////////////////
    // link edge loops from chains
    ///////////////////////////////

    EdgeChainLinker linker;
    for( auto it : edgeset._the_map ){
      auto the_edge = it.second;
      if(0)printf( "  edge[%d->%d]\n", //
                 the_edge->_vertexA->_poolindex, //
                 the_edge->_vertexB->_poolindex);
      linker.add_edge(the_edge);
    }
    linker.link();
    OrkAssert(linker._edge_loops.size()==1);

    ///////////////////////////////
    // flip edge loop if needed
    ///////////////////////////////

    auto loop = linker._edge_loops[0];
    auto edgeA = loop->_edges[0];
    auto edgeB = loop->_edges[1];
    bool flip = do_flip(edgeA->_vertexA, //
                        edgeA->_vertexB, //
                        edgeB->_vertexB);

    if(flip){
      auto temp = std::make_shared<EdgeLoop>();
      temp->reverseOf(*loop);
      loop = temp;
    }

    ///////////////////////////////
    // merge new polys
    ///////////////////////////////

    int num_edges = loop->_edges.size();
    auto new_c = outsmesh.mergeVertex(*conflict_point);
    for( int i=0; i<num_edges; i++ ){
      auto edge = loop->_edges[i];
      if(0)printf( "  loopedge[%d->%d]\n", //
                 edge->_vertexA->_poolindex, //
                 edge->_vertexB->_poolindex);
      auto new_tri = outsmesh.mergeTriangle(edge->_vertexB, //
                                            edge->_vertexA, //
                                            new_c);
    }

    ///////////////////////////////
    // remove point and move to next conflict 
    ///////////////////////////////
    vertices.erase(conflict_point);
    update_conflict_graph();
  }

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// brute force method
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void submeshConvexHullBruteForce(const submesh& inpsubmesh, submesh& outsmesh) {

  inpsubmesh.visitAllVertices([&](vertex_const_ptr_t va) {
    dvec3 a = va->mPos;
    inpsubmesh.visitAllVertices([&](vertex_const_ptr_t vb) {
      dvec3 b  = vb->mPos;
      dvec3 ab = b - a;
      if (va != vb) {
        inpsubmesh.visitAllVertices([&](vertex_const_ptr_t vc) {
          if (va != vc && vb != vc) {
            dvec3 c      = vc->mPos;
            dvec3 ac     = c - a;
            dvec3 n      = ab.crossWith(ac).normalized();
            bool binside = true;
            inpsubmesh.visitAllVertices([&](vertex_const_ptr_t vd) {
              if (vd != va && vd != vb && vd != vc) {
                dvec3 d  = vd->mPos;
                dvec3 ad = d - a;
                float dp = ad.dotWith(n);
                if (dp > 0.0f) {
                  binside = false;
                }
              }
            });
            if (binside) {
              std::vector<vertex_ptr_t> merged_vertices;
              merged_vertices.push_back(outsmesh.mergeVertex(*va));
              merged_vertices.push_back(outsmesh.mergeVertex(*vb));
              merged_vertices.push_back(outsmesh.mergeVertex(*vc));
              outsmesh.mergePoly(merged_vertices);
            }
          }
        });
      }
    });
  });
}

///////////////////////////////////////////////////////////////////////////////

void submeshConvexHull(const submesh& inpsubmesh, submesh& outsmesh) {
  // submeshConvexHullBruteForce(inpsubmesh, outsmesh);
  submeshConvexHullIterative(inpsubmesh, outsmesh);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
