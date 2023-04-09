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

struct ConflictItem {
  void removePoly(poly_ptr_t p) {
    _polys.remove(p);
  }
  //////////////////////////////////
  vertex_ptr_t _vertex;
  poly_set_t _polys;
};

///////////////////////////////////////////////////////////////////////////////

struct ConflictGraph {
  //////////////////////////////////
  void clear() {
    _items.clear();
  }
  //////////////////////////////////
  ConflictItem& item(vertex_ptr_t v) {
    auto it = _items.find(v->_poolindex);
    if (it == _items.end()) {
      auto& item   = _items[v->_poolindex];
      item._vertex = v;
      return item;
    }
    return it->second;
  }
  //////////////////////////////////
  void insert(vertex_ptr_t v, poly_ptr_t p) {
    auto& item = this->item(v);
    item._polys.insert(p);
  }
  //////////////////////////////////
  bool empty() const {
    return _items.empty();
  }
  //////////////////////////////////
  ConflictItem& front() {
    return _items.begin()->second;
  }
  //////////////////////////////////
  void remove(vertex_ptr_t v) {
    auto it = _items.find(v->_poolindex);
    _items.erase(it);
  }
  //////////////////////////////////
  void removePoly(poly_ptr_t p) {
    for (auto& item : _items) {
      item.second.removePoly(p);
    }
  }
  //////////////////////////////////
  std::map<uint64_t, ConflictItem> _items;
};

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

  vertex_set_t vertices;
  int initial_counter = 0;
  dvec3 inp_center;
  inpsubmesh.visitAllVertices([&](vertex_const_ptr_t va) {
    auto nva = outsmesh.mergeVertex(*va);
    vertices.insert(nva);
    inp_center += nva->mPos;
    initial_counter++;
  });
  inp_center *= (1.0f / float(initial_counter));
  OrkAssert(vertices.size() >= 4); // ensure we still have 4 vertices after welding

  ///////////////////////////////////////////////////
  // create initial tetrahedron
  ///////////////////////////////////////////////////

  dvec3 tetra_center;
  auto make_vtx = [&](int index) -> vertex_ptr_t {
    vertex_ptr_t rval;
    for (auto iv : vertices._the_map) {
      auto v = iv.second;
      if (v->_poolindex == index) {
        rval = v;
        tetra_center += v->mPos;
      }
    }
    vertices.remove(rval);
    return rval;
  };
  auto nva = make_vtx(0);
  auto nvb = make_vtx(1);
  auto nvc = make_vtx(2);
  auto nvd = make_vtx(3);
  tetra_center *= 0.25;

  //
  // make sure tetrahedron is oriented correctly
  //

  auto do_flip = [&](dvec3 center, vertex_ptr_t a, vertex_ptr_t b, vertex_ptr_t c) -> bool {
    auto d_ab = (b->mPos - a->mPos).normalized();
    auto d_bc = (c->mPos - b->mPos).normalized();
    auto d_n  = d_ab.crossWith(d_bc).normalized();
    auto d_c  = (b->mPos - center).normalized();
    return d_n.dotWith(d_c) < 0.0;
  };
  auto make_tri = [&](vertex_ptr_t a, vertex_ptr_t b, vertex_ptr_t c) -> poly_ptr_t {
    bool flip       = do_flip(tetra_center, a, b, c);
    poly_ptr_t rval = nullptr;
    if (flip) {
      rval = outsmesh.mergeTriangle(a, c, b);
    } else {
      rval = outsmesh.mergeTriangle(a, b, c);
    }
    return rval;
  };

  // std::vector<poly_ptr_t> polys;
  make_tri(nva, nvb, nvc);
  make_tri(nva, nvc, nvd);
  make_tri(nva, nvd, nvb);
  make_tri(nvb, nvc, nvd);

  ///////////////////////////////////////////////////
  // build conflict graph
  ///////////////////////////////////////////////////

  ConflictGraph conflict_graph;

  auto update_conflict_graph = [&]() {
    conflict_graph.clear();
    vertices.visit([&](vertex_ptr_t v) {
      auto vpos = v->mPos;
      outsmesh.visitAllPolys([&](poly_ptr_t p) {
        if (not p->containsVertex(v)) {
          double sv = p->signedVolumeWithPoint(vpos);
          if (sv > 0.0) {
            conflict_graph.insert(v, p);
          }
        }
      });
    });
  };

  update_conflict_graph();

  /////////////////////////////////////////////////

  auto addPolyToConflictGraph = [&](poly_ptr_t p) {
    vertices.visit([&](vertex_ptr_t v) {
      auto vpos = v->mPos;
      if (not p->containsVertex(v)) {
        double sv = p->signedVolumeWithPoint(vpos);
        if (sv > 0.0) {
          conflict_graph.insert(v, p);
        }
      }
    });
  };

  ///////////////////////////////////////////////////
  // iterate on conflict graph
  ///////////////////////////////////////////////////

  edge_set_t edgeset;
  std::vector<poly_ptr_t> polys_to_remove;
  EdgeChainLinker linker;

  while (not conflict_graph.empty()) {

    polys_to_remove.clear();
    edgeset.clear();
    linker.clear();

    ///////////////////////////////
    // get next conflict point
    ///////////////////////////////

    auto& conflict_item  = conflict_graph.front();
    auto conflict_point  = conflict_item._vertex;

    ///////////////////////////////
    // remove all conflicting polys from outsmesh
    //  and build edge set
    ///////////////////////////////

    conflict_item._polys.visit([&](poly_ptr_t the_poly) {
      the_poly->visitEdges([&](edge_ptr_t the_edge) {
        auto reverse_edge = std::make_shared<edge>(the_edge->_vertexB, the_edge->_vertexA);
        if (edgeset.contains(reverse_edge)) {
          edgeset.remove(the_edge);
        } else if (edgeset.contains(the_edge)) {
          OrkAssert(false);
        } else {
          edgeset.insert(the_edge);
        }
      });
      polys_to_remove.push_back(the_poly);
    });
    for (auto the_poly : polys_to_remove) {
      outsmesh.removePoly(the_poly);
      conflict_graph.removePoly(the_poly);
    }

    ///////////////////////////////
    // link edge loops from chains
    ///////////////////////////////

    for (auto it : edgeset._the_map) {
      auto the_edge = it.second;
      linker.add_edge(the_edge);
    }
    linker.link();
    if (linker._edge_loops.size() == 0) {
      conflict_graph.clear();
      break;
    }
    ///////////////////////////////
    // flip edge loop if needed
    ///////////////////////////////

    auto loop  = linker._edge_loops[0];
    auto edgeA = loop->_edges[0];
    auto edgeB = loop->_edges[1];
    bool flip  = do_flip(
        inp_center,      //
        edgeA->_vertexA, //
        edgeA->_vertexB, //
        edgeB->_vertexB);

    if (flip) {
      auto temp = std::make_shared<EdgeLoop>();
      temp->reverseOf(*loop);
      loop = temp;
    }

    ///////////////////////////////
    // merge new polys
    ///////////////////////////////

    int num_edges = loop->_edges.size();
    auto new_c    = outsmesh.mergeVertex(*conflict_point);
    std::unordered_set<vertex_ptr_t> verts;
    for (int i = 0; i < num_edges; i++) {
      auto edge  = loop->_edges[i];
      auto new_a = outsmesh.mergeVertex(*edge->_vertexA);
      auto new_b = outsmesh.mergeVertex(*edge->_vertexB);
      verts.clear();
      outsmesh.visitAllPolys([&](poly_ptr_t the_poly) {
        verts.insert(the_poly->_vertices[0]);
        verts.insert(the_poly->_vertices[1]);
        verts.insert(the_poly->_vertices[2]);
      });
      dvec3 new_center;
      for (auto v : verts) {
        new_center += v->mPos;
      }
      new_center *= 1.0 / double(verts.size());

      bool flip = do_flip(
          new_center, //
          new_a,      //
          new_b,      //
          new_c);

      if (flip) {
        std::swap(new_a, new_b);
      }
      auto new_tri = outsmesh.mergeTriangle(
          new_a, //
          new_b, //
          new_c);
      addPolyToConflictGraph(new_tri);
    }

    ///////////////////////////////
    // remove point and move to next conflict
    ///////////////////////////////
    vertices.remove(conflict_point);
    conflict_graph.remove(conflict_point);
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
