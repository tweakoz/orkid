////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/math/plane.hpp>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/opq.h>
#include <deque>
#include <ork/math/misc_math.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// iterative method
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct ConflictItem {
  ConflictItem()
      : _mutex("citem") {
  }
  void removePoly(merged_poly_ptr_t p) {
    _polys.remove(p);
  }
  //////////////////////////////////
  void appendPoly(merged_poly_ptr_t p) {
    _mutex.Lock();
    _polys.insert(p);
    _mutex.UnLock();
  }
  //////////////////////////////////
  void appendPolys(std::vector<merged_poly_ptr_t> polys) {
    _mutex.Lock();
    for (auto p : polys) {
      _polys.insert(p);
    }
    _mutex.UnLock();
  }
  //////////////////////////////////
  vertex_ptr_t _vertex;
  merged_poly_set_t _polys;
  mutex _mutex;
};

///////////////////////////////////////////////////////////////////////////////

struct ConflictGraph {
  ConflictGraph()
      : _mutex("cgraph") {
  }
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
  void insert(vertex_ptr_t v, merged_poly_ptr_t p) {
    _mutex.Lock();
    auto& item = this->item(v);
    _mutex.UnLock();
    item.appendPoly(p);
  }
  //////////////////////////////////
  void insertMultiple(vertex_ptr_t v, std::vector<merged_poly_ptr_t>& polys) {
    _mutex.Lock();
    auto& item = this->item(v);
    _mutex.UnLock();
    item.appendPolys(polys);
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
  void removePoly(merged_poly_ptr_t p) {
    for (auto& item : _items) {
      item.second.removePoly(p);
    }
  }
  void removePolys(std::vector<merged_poly_ptr_t> polys) {
    for (auto p : polys) {
      this->removePoly(p);
    }
  }
  //////////////////////////////////
  std::map<uint64_t, ConflictItem> _items;
  mutex _mutex;
};

///////////////////////////////////////////////////////////////////////////////

void submeshConvexHullIterative(const submesh& inpsubmesh, submesh& outsmesh, int num_steps) {

  ork::Timer timer;
  timer.Start();
  constexpr bool _do_parallel = false;

  bool debug = false;

  ///////////////////////////////////////////////////
  // trivially reject
  ///////////////////////////////////////////////////

  if (inpsubmesh.numVertices() < 4) {
    return;
  }

  ///////////////////////////////////////////////////
  // collect vertices
  ///////////////////////////////////////////////////

  if (debug)
    printf("//////////////////////////////////////////////////////////////////////\n");

  vertex_set_t vertices;
  int initial_counter = 0;
  dvec3 inp_center;
  inpsubmesh.visitAllVertices([&](vertex_const_ptr_t va) {
    auto nva = outsmesh.mergeVertex(*va);
    vertices.insert(nva);
    inp_center += nva->mPos;
    if (0) // debug)
      printf("v<%d> : pos<%f %f %f>\n", initial_counter, nva->mPos.x, nva->mPos.y, nva->mPos.z);
    initial_counter++;
  });
  inp_center *= (1.0f / float(initial_counter));
  OrkAssert(vertices.size() >= 4); // ensure we still have 4 vertices after welding

  if (debug)
    printf("numverts raw<%d> merged<%d>\n", initial_counter, outsmesh.numVertices());

  ///////////////////////////////////////////////////
  // find 4 points that form a tetrahedron
  //  with the largest volume
  ///////////////////////////////////////////////////

  int a           = 0;
  int b           = 0;
  vertex_ptr_t va = nullptr;
  vertex_ptr_t vb = nullptr;

  double max_distsq = 0.0;

  // find vertex a and b that are furthest apart
  for (int ia = 0; ia < initial_counter; ia++) {
    auto tva = outsmesh.vertex(ia);
    for (int ib = 0; ib < initial_counter; ib++) {
      if (ia != ib) {
        auto tvb      = outsmesh.vertex(ib);
        double distsq = (tvb->mPos - tva->mPos).magnitudeSquared();
        if (distsq > max_distsq) {
          max_distsq = distsq;
          a          = ia;
          b          = ib;
          va         = tva;
          vb         = tvb;
        }
      }
    }
  }

  // find vertex c that forms the triangle with the largest area with a,b
  std::map<double, poly_ptr_t> polys_by_area;
  for (int ic = 0; ic < initial_counter; ic++) {
    if (ic != a and ic != b) {
      auto tvc            = outsmesh.vertex(ic);
      auto p              = std::make_shared<Polygon>(va, vb, tvc);
      double area         = p->computeArea();
      polys_by_area[area] = p;
    }
  }


  vertex_ptr_t vc = polys_by_area.rbegin()->second->vertex(2);
  int c           = vc->_poolindex;
  // find vertex d that forms the tetrahedron with the largest volume with a,b,c
  std::map<double, poly_ptr_t> polys_by_volume;
  for (int id = 0; id < initial_counter; id++) {
    if (id != a and id != b and id != c) {
      auto tvd                = outsmesh.vertex(id);
      auto p                  = std::make_shared<Polygon>(va, vb, vc);
      double volume           = abs(p->signedVolumeWithPoint(tvd->mPos));
      polys_by_volume[volume] = p;
      p->_vertices.push_back(tvd);
    }
  }
  vertex_ptr_t vd = polys_by_volume.rbegin()->second->vertex(3);
  int d           = vd->_poolindex;

  ///////////////////////////////////////////////////
  ///////////////////////////////////////////////////

  if (false)
    for (auto it : polys_by_volume) {
      auto p = it.second;
      printf(
          "poly<%p> a<%d> b<%d> c<%d> d<%d> volume<%g>\n",
          p.get(),
          p->vertexID(0),
          p->vertexID(1),
          p->vertexID(2),
          p->vertexID(3),
          it.first);
    }

  ///////////////////////////////////////////////////
  // create initial tetrahedron
  ///////////////////////////////////////////////////

  auto it_largest_volume = polys_by_volume.rbegin();
  auto p                 = it_largest_volume->second;
  auto nva               = p->vertex(0);
  auto nvb               = p->vertex(1);
  auto nvc               = p->vertex(2);
  auto nvd               = p->vertex(3);

  dvec3 tetra_center;
  tetra_center += nva->mPos;
  tetra_center += nvb->mPos;
  tetra_center += nvc->mPos;
  tetra_center += nvd->mPos;
  tetra_center *= 0.25;

  if (debug)
    printf("final point selection a<%d> b<%d> c<%d> d<%d> volume<%g>\n", a, b, c, d, it_largest_volume->first);

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

  make_tri(nva, nvb, nvc);
  make_tri(nva, nvc, nvd);
  make_tri(nva, nvd, nvb);
  make_tri(nvb, nvc, nvd);

  ///////////////////////////////////////////////////
  // build conflict graph
  ///////////////////////////////////////////////////

  ConflictGraph conflict_graph;

  if (debug)
    printf("//\n");
  auto update_conflict_graph = [&]() {
    conflict_graph.clear();
    vertices.visit([&](vertex_ptr_t v) {
      if (false) { // outsmesh.isVertexInsideConvexHull(v)) {
        if (debug)
          printf("vertex<%d> is inside convex hull\n", v->_poolindex);
      } else {
        auto vpos = v->mPos;
        outsmesh.visitAllPolys([&](merged_poly_ptr_t p) {
          if (not p->containsVertex(v)) {
            bool visible = p->signedVolumeWithPoint(vpos) >= 0.0;
            if (visible) {
              conflict_graph.insert(v, p);
              if (false) {
                printf("visible poly[");
                p->visitVertices([&](vertex_ptr_t v) { printf("%d ", v->_poolindex); });
                printf("] : visible vertex<%d>\n", v->_poolindex);
              }
            }
          }
        });
      }
    });
  };

  update_conflict_graph();

  if (debug) {
    //printf("conflict graph size<%d>\n", conflict_graph._items.size());
    for (auto& it : conflict_graph._items) {
      ConflictItem& item = it.second;
      auto vertex        = item._vertex;
      printf("vertex<%d> : ", vertex->_poolindex);
      for (auto it_p : item._polys._the_map) {
        auto p = it_p.second;
        printf("poly[");
        p->visitVertices([&](vertex_ptr_t v) { printf("%d ", v->_poolindex); });
        printf("] ");
      }
      printf("\n");
    }
  }

  ///////////////////////////////////////////////////
  // iterate on conflict graph
  ///////////////////////////////////////////////////

  edge_set_t edgeset;
  std::vector<merged_poly_ptr_t> polys_to_remove;
  EdgeChainLinker linker;
  std::unordered_set<vertex_ptr_t> all_mesh_verts;

  double dt01 = 0.0;
  double dt12 = 0.0;
  double dt23 = 0.0;
  double dt34 = 0.0;

  int total_edges_visited = 0;
  // for( int i=0; i<num_steps; i++ ) {
  while (not conflict_graph.empty()) {

    double t0 = timer.SecsSinceStart();

    ///////////////////////////////
    // get next conflict point
    ///////////////////////////////

    auto& conflict_item = conflict_graph.front();
    auto conflict_point = conflict_item._vertex;

    if (outsmesh.isVertexInsideConvexHull(conflict_point)) {
      vertices.remove(conflict_point);
      conflict_graph.remove(conflict_point);
      continue;
    }

    //if (debug)
    //  printf("try point<%d> : conflict graph size<%d>\n", conflict_point->_poolindex, conflict_graph._items.size());

    ///////////////////////////////
    // remove all conflicting polys from outsmesh
    //  and build edge set
    ///////////////////////////////

    conflict_item._polys.visit([&](merged_poly_ptr_t the_poly) {
      polys_to_remove.push_back(the_poly);
      the_poly->visitEdges([&](edge_ptr_t the_edge) {
        auto reverse_edge = std::make_shared<edge>(the_edge->_vertexB, the_edge->_vertexA);
        if (edgeset.contains(reverse_edge)) {
          edgeset.remove(the_edge);
          if (debug)
            printf("remove edge<%d,%d>\n", the_edge->_vertexA->_poolindex, the_edge->_vertexB->_poolindex);
        } else {
          edgeset.insert(the_edge);
          if (debug)
            printf("insert edge<%d,%d>\n", the_edge->_vertexA->_poolindex, the_edge->_vertexB->_poolindex);
        }
      });
    });
    outsmesh.removePolys(polys_to_remove);
    conflict_graph.removePolys(polys_to_remove);

    double t1 = timer.SecsSinceStart();

    dt01 += (t1 - t0);
    ///////////////////////////////
    // link edge loops from chains
    ///////////////////////////////

    for (auto it : edgeset._the_map) {
      auto the_edge = it.second;
      linker.add_edge(the_edge);
      if (debug)
        printf("add_edge<%d,%d>\n", the_edge->_vertexA->_poolindex, the_edge->_vertexB->_poolindex);
    }
    linker.link();
    //if (debug)
    //  printf("linker._edge_loops.size()<%d>\n", linker._edge_loops.size());
    //if (debug)
    //  printf("linker._edge_chains.size()<%d>\n", linker._edge_chains.size());
    if (linker._edge_loops.size() == 0) {
      conflict_graph.clear();
      printf("loop failed!\n");
      // OrkAssert(false);
    } else {

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
      // merge conflict point
      ///////////////////////////////

      auto new_c = outsmesh.mergeVertexConcurrent(*conflict_point);

      ///////////////////////////////
      // merge triangle fan
      ///////////////////////////////

      double t2 = timer.SecsSinceStart();
      dt12 += (t2 - t1);

      int num_edges = loop->_edges.size();
      total_edges_visited += num_edges;

      dvec3 new_center = outsmesh.centerOfPolys();

      std::vector<merged_poly_ptr_t> polys_added;

      /////////////////////////////////////////////////////
      for (int i = 0; i < num_edges; i++) {

        auto edge = loop->_edges[i];

        ////////////////////////////////////
        // verts a and b for the fan triangle
        ////////////////////////////////////

        auto new_a = outsmesh.mergeVertex(*edge->_vertexA);
        auto new_b = outsmesh.mergeVertex(*edge->_vertexB);

        ////////////////////////////////////
        // compute new center of mesh
        ////////////////////////////////////

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
        if( new_tri ){ // zero area triangles are not merged (and therefore nullptr)
          polys_added.push_back(new_tri);
        }
      }
      ///////////////////////////////////////////////////////////////////////////

      size_t poly_count = polys_added.size();
      int istart                   = 0;
      std::atomic<int> job_counter = 0;
      // printf( "num_edges<%d>\n", num_edges );
      while (poly_count > 0) {

        int icount = 8;
        if (icount > poly_count)
          icount = poly_count;

        job_counter.fetch_add(1);
        auto op = [=,                   //
                   &job_counter,        //
                   &polys_added,        //
                   &vertices,           //
                   &outsmesh,           //
                   &conflict_graph]() { //
          /////////////////////////////////////////////////////
          std::unordered_map<vertex_ptr_t, std::vector<merged_poly_ptr_t>> insert_map;
          for (int i = 0; i < icount; i++) {
            OrkAssert((istart + i) < polys_added.size());
            auto new_tri = polys_added[istart + i];
            OrkAssert(new_tri);
            ////////////////////////////////////
            // update visibility of vertices with respect to new triangle
            ////////////////////////////////////
            vertices.visit([&](vertex_ptr_t v) {
              if (not new_tri->containsVertex(v)) {
                double sv = new_tri->signedVolumeWithPoint(v->mPos);
                if (sv > 0.0) {
                  insert_map[v].push_back(new_tri);
                }
              }
            });
          } // for (auto new_tri : polys_added) {
          /////////////////////////////////////////////////////
          for (auto& item : insert_map) {
            auto v      = item.first;
            auto& polys = item.second;
            conflict_graph.insertMultiple(v, polys);
          }
          job_counter.fetch_sub(1);
        };

        ////////////////////////////////////

        if (_do_parallel) {
          opq::concurrentQueue()->enqueue(op);
        } else // immediate and serial
          op();

        ////////////////////////////////////

        poly_count -= icount;
        istart += icount;

      } // while(num_edges>0){

      ///////////////////////////////

      polys_to_remove.clear();
      edgeset.clear();
      linker.clear();

      if (_do_parallel) {
        while (job_counter.load() > 0) {
          sched_yield();
        }
      }

      ///////////////////////////////
      double t3 = timer.SecsSinceStart();
      dt23 += (t3 - t2);
      ///////////////////////////////
      // remove point and move to next conflict
      ///////////////////////////////
      vertices.remove(conflict_point);
      conflict_graph.remove(conflict_point);
    }
    ///////////////////////////////////////////////////////
  } // while (not conflict_graph.empty()) {

  // printf("dt0..1: %f, dt1..2: %f, dt2..3: %f  total_edges: %d\n", dt01, dt12, dt23, total_edges_visited);
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
