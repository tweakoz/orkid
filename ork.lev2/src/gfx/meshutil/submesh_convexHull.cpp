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

void submeshConvexHullIterative(const submesh& inpsubmesh, submesh& outsmesh, int num_steps) {

  ///////////////////////////////////////////////////
  // trivially reject
  ///////////////////////////////////////////////////

  if (inpsubmesh.numVertices() < 4) {
    return;
  }

  ///////////////////////////////////////////////////
  // collect vertices
  ///////////////////////////////////////////////////

  //std::unordered_set<vertex_ptr_t> vertices;
  vertex_set_t vertices;
  int initial_counter = 0;
  dvec3 inp_center;
  inpsubmesh.visitAllVertices([&](vertex_const_ptr_t va) { 
    printf( "inp-vtx[%d] <%g %g %g>\n", initial_counter, va->mPos.x, va->mPos.y, va->mPos.z);
    auto nva = outsmesh.mergeVertex(*va);
    vertices.insert(nva);
    inp_center += nva->mPos;
    initial_counter++;
    });
  inp_center *= (1.0f/float(initial_counter));
  OrkAssert(vertices.size()>=4); // ensure we still have 4 vertices after welding

  ///////////////////////////////////////////////////
  // create initial tetrahedron
  ///////////////////////////////////////////////////

  dvec3 tetra_center;
  auto make_vtx = [&]() -> vertex_ptr_t {
    auto nva = vertices.first();
    tetra_center += nva->mPos;
    vertices.remove(nva);
    printf( "  tetra-vtx[%d] <%g %g %g>\n", nva->_poolindex, nva->mPos.x, nva->mPos.y, nva->mPos.z);
    return nva;
  };
  auto nva = make_vtx();
  auto nvb = make_vtx();
  auto nvc = make_vtx();
  auto nvd = make_vtx();
  tetra_center *= 0.25;
  
  //
  // make sure tetrahedron is oriented correctly
  //

  auto do_flip = [&](dvec3 center, vertex_ptr_t a, vertex_ptr_t b, vertex_ptr_t c) -> bool {
    auto d_ab = (b->mPos-a->mPos).normalized();
    auto d_bc = (c->mPos-b->mPos).normalized();
    auto d_n = d_ab.crossWith(d_bc).normalized();
    auto d_c = (b->mPos-center).normalized();
    return d_n.dotWith(d_c)<0.0;
  };
  auto make_tri = [&](vertex_ptr_t a, vertex_ptr_t b, vertex_ptr_t c) -> poly_ptr_t {
    return do_flip(tetra_center,a,b,c) ? outsmesh.mergeTriangle(a,c,b) : outsmesh.mergeTriangle(a,b,c);
  };

  std::vector<poly_ptr_t> polys;
  polys.push_back(make_tri(nva,nvb,nvc));
  polys.push_back(make_tri(nva,nvc,nvd));
  polys.push_back(make_tri(nva,nvd,nvb));
  polys.push_back(make_tri(nvb,nvc,nvd));

  ///////////////////////////////////////////////////
  // build conflict graph
  ///////////////////////////////////////////////////

  struct ConflictItem{
    void removePoly(poly_ptr_t p){
      _polys.remove(p);
    }
    vertex_ptr_t _vertex;
    poly_set_t _polys;
  };
  struct ConflictGraph{
    void clear(){ _items.clear(); }
    ConflictItem& item(vertex_ptr_t v){
      auto it = _items.find(v->hash());
      if( it==_items.end() ){
        auto& item = _items[v->hash()];
        item._vertex = v;
        return item;
      }
      return it->second;
    }
    void insert(vertex_ptr_t v, poly_ptr_t p){
      auto& item = this->item(v);
      item._polys.insert(p);
    }
    bool empty() const { return _items.empty(); }
    ConflictItem& front() { return _items.begin()->second; }
    void removeFront(){
      auto it = _items.begin();
      _items.erase(it);
    }
    std::map<uint64_t, ConflictItem> _items;
  };

  ConflictGraph conflict_graph;

  auto update_conflict_graph = [&]() {
    conflict_graph.clear();
    vertices.visit([&](vertex_ptr_t v) {
      auto vpos = v->mPos;
      for( auto p : polys ){
        if( not p->containsVertex(v) ){
          double sv = p->signedVolumeWithPoint(vpos);
          if(sv<0.0){
            conflict_graph.insert(v,p);
          }
        }
      }
    });
  };

  update_conflict_graph();

  ///////////////////////////////////////////////////
  // iterate on conflict graph
  ///////////////////////////////////////////////////

  edge_set_t edgeset;
  for( int i=0; i<num_steps; i++) {
  //while( not conflict_graph.empty() ){

    ///////////////////////////////
    // get next conflict point
    ///////////////////////////////

    auto& conflict_item =  conflict_graph.front();
    auto conflict_point = conflict_item._vertex;
    size_t num_conflicts = conflict_item._polys.size();
    printf("//////////////////////////////\n");
    printf("//////////////////////////////\n");
    printf( "conflict_point [%D] <%g %g %g> num_conflicts<%zu>\n", 
             conflict_point->_poolindex,
             conflict_point->mPos.x,
             conflict_point->mPos.y, 
             conflict_point->mPos.z, 
             num_conflicts );
    printf("//////////////////////////////\n");
    printf("//////////////////////////////\n");

    ///////////////////////////////
    // remove all conflicting polys from outsmesh
    //  and build edge set
    ///////////////////////////////

    printf("//////////////////////////////\n");
    printf( "// visit polys (conflict resolution)\n");
    printf("//////////////////////////////\n");
    conflict_item._polys.visit([&](poly_ptr_t the_poly) {
      printf( "visit poly<%d %d %d>\n", the_poly->_vertices[0]->_poolindex, the_poly->_vertices[1]->_poolindex, the_poly->_vertices[2]->_poolindex);
      the_poly->visitEdges([&](edge_ptr_t the_edge) {
        auto reverse_edge = std::make_shared<edge>(the_edge->_vertexB,the_edge->_vertexA);
        if(edgeset.contains(reverse_edge)){
          printf( " remove edge<%d %d>\n", the_edge->_vertexA->_poolindex, the_edge->_vertexB->_poolindex);
          edgeset.remove(the_edge);
        }
        else if(edgeset.contains(the_edge)){
          OrkAssert(false);
        }
        else{
          printf( " insert edge<%d %d>\n", the_edge->_vertexA->_poolindex, the_edge->_vertexB->_poolindex);
          edgeset.insert(the_edge);
        }
      });

      outsmesh.removePoly(the_poly);
    });
    conflict_item._polys._the_map.clear();

    ///////////////////////////////
    // link edge loops from chains
    ///////////////////////////////
    printf("//////////////////////////////\n");
    printf( "// link edges\n");
    printf("//////////////////////////////\n");

    EdgeChainLinker linker;
    for( auto it : edgeset._the_map ){
      auto the_edge = it.second;
      if(1)printf( "   edge[%d->%d]\n", //
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
    bool flip = do_flip(inp_center, //
                        edgeA->_vertexA, //
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
      if(1)printf( "  loopedge[%d->%d]\n", //
                 edge->_vertexA->_poolindex, //
                 edge->_vertexB->_poolindex);
      auto new_a = outsmesh.mergeVertex(*edge->_vertexA);
      auto new_b = outsmesh.mergeVertex(*edge->_vertexB);
      auto new_tri = outsmesh.mergeTriangle(new_a, //
                                            new_b, //
                                            new_c);
      polys.push_back(new_tri);
    }

    ///////////////////////////////
    // remove point and move to next conflict 
    ///////////////////////////////
    vertices.remove(conflict_point);
    conflict_graph.removeFront();
    //update_conflict_graph();
  }

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// brute force method
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void submeshConvexHullBruteForce(const submesh& inpsubmesh, submesh& outsmesh, int steps) {

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

void submeshConvexHull(const submesh& inpsubmesh, submesh& outsmesh, int steps) {
  // submeshConvexHullBruteForce(inpsubmesh, outsmesh);
  submeshConvexHullIterative(inpsubmesh, outsmesh, steps);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
