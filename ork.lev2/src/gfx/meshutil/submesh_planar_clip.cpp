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

static constexpr bool debug = true;

static constexpr bool do_front        = true;
static constexpr bool do_back         = true;
static constexpr double PLANE_EPSILON = 0.001f;

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

const std::unordered_set<int> test_verts = {0, 1, 2, 3, 4, 9};

struct PolyVtxCount {
  int _front_count  = 0;
  int _back_count   = 0;
  int _planar_count = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct PlanarVertexCategorize {

  PlanarVertexCategorize(const submesh& inpsubmesh, const dplane3& slicing_plane) {

    int iv = 0;
    inpsubmesh.visitAllVertices([&](vertex_const_ptr_t vtx) {
      // todo: fix nonconst
      auto nonconst_vertex = std::const_pointer_cast<struct vertex>(vtx);
      nonconst_vertex->clearAllExceptPosition();
      double point_distance = slicing_plane.pointDistance(vtx->mPos);
      if (debug)
        printf("iv<%d> point_distance<%f>\n", iv, point_distance);
      if (point_distance > (-PLANE_EPSILON)) {
        _front_verts.insert(nonconst_vertex);
      } else if (point_distance < (PLANE_EPSILON)) {
        _back_verts.insert(nonconst_vertex);
      } else { // on plane
        _planar_verts.insert(nonconst_vertex);
      }
      iv++;
    });

    // printf("_front_verts<%zu>\n", _front_verts.size());
    // printf("_back_verts<%zu>\n", _back_verts.size());
    // printf("_planar_verts<%zu>\n", _planar_verts.size());
  }

  PolyVtxCount categorizePolygon(poly_const_ptr_t input_poly) const {
    PolyVtxCount counts;
    input_poly->visitVertices([&](vertex_ptr_t vtx) {
      if (_front_verts.contains(vtx)) {
        counts._front_count++;
      }
      if (_back_verts.contains(vtx)) {
        counts._back_count++;
      }
      if (_planar_verts.contains(vtx)) {
        counts._planar_count++;
      }
    });
    return counts;
  }

  vertex_set_t _front_verts;
  vertex_set_t _back_verts;
  vertex_set_t _planar_verts;
};

///////////////////////////////////////////////////////////////////////////////

vertex_set_t addWholePoly(std::string hdr, poly_const_ptr_t src_poly, submesh& dest) {
  std::vector<vertex_ptr_t> new_verts;
  vertex_set_t added;
  src_poly->visitVertices([&](vertex_ptr_t v) {
    OrkAssert(v);
    auto newv = dest.mergeVertex(*v);
    new_verts.push_back(newv);
    added.insert(newv);
  });
  dest.mergePoly(Polygon(new_verts));
  bool log_poly = true;
  src_poly->visitVertices([&](vertex_ptr_t v) {
    if (test_verts.find(v->_poolindex) == test_verts.end())
      log_poly = false;
  });
  if (debug and log_poly) {

    printf("<%s> add whole poly: [", hdr.c_str());
    src_poly->visitVertices([&](vertex_ptr_t v) {
      printf("v<%d> ", v->_poolindex);
    });
    printf("]\n");
  }
  return added;
}

///////////////////////////////////////////////////////////////////////////////

using dvec_cb_t = std::function<void(const dvec3&)>;

/*
        auto process_clipped_poly = [&](std::vector<dvec3>& clipped_poly_vertices, //
                                        submesh& outsubmesh,                        //
                                        vertex_set_t& planar_verts) {               //
          std::vector<vertex_ptr_t> merged_vertices;

          /////////////////////////////////////////
          // classify all points in clipped poly, with respect to plane
          //  put all points which live on plane into planar_verts
          /////////////////////////////////////////

          std::vector<vertex_ptr_t> potentials;

          int inumv = clipped_poly_vertices.size();
          for (int iv = 0; iv < inumv; iv++) {
            int iv0        = iv;
            vertex v0;
            v0.mPos = clipped_poly_vertices[iv0];
            auto merged_v0 = outsubmesh.mergeVertex(v0);

            merged_v0->clearAllExceptPosition();
            merged_vertices.push_back(merged_v0);
            double point_dist_to_plane = abs(slicing_plane.pointDistance(merged_v0->mPos));
            if (point_dist_to_plane < PLANE_EPSILON) {
              planar_verts.insert(merged_v0);
            }
          }

          /////////////////////////////////////////
          // if we have enough merged vertices for a polygon,
          //  then create a polygon
          /////////////////////////////////////////

          if (merged_vertices.size() >= 3) {
            auto out_bpoly = std::make_shared<Polygon>(merged_vertices);
            addWholePoly("C: ",out_bpoly, outsubmesh);
          }
          */

enum class EPlanarStatus { CROSS_F2B, CROSS_B2F, PLANAR, NONE };

struct PlanarCrossing {
  vertex_ptr_t _vtxA;
  vertex_ptr_t _vtxB;
  EPlanarStatus _status;
};

void clipImpl(
    const dplane3& slicing_plane,
    poly_const_ptr_t input_poly, //
    submesh& out_submesh,        //
    std::vector<PlanarCrossing>& planars,
    bool do_front) {             //

  bool do_back = not do_front;

  const int inuminverts = input_poly->numVertices();
  OrkAssert(inuminverts >= 3);

  printf( "CLIP INPUT POLY[");
  input_poly->visitVertices([&](vertex_ptr_t vtx) {
    auto v_m = out_submesh.mergeVertex(*vtx);
    printf( " %d", v_m->_poolindex );
  });
  printf( " ]\n");
  //if (debug)
    //printf("clip poly num verts<%d>\n", inuminverts);

  // loop around the input polygon's edges

  std::vector<vertex_ptr_t> front_vertices;

  for (int iva = 0; iva < inuminverts; iva++) {
   //if (debug)
     // printf("  iva<%d> of inuminverts<%d>\n", iva, inuminverts);
    int ivb =
        ((iva == inuminverts - 1) //
             ? 0                  //
             : iva + 1);

    auto out_vtx_a = out_submesh.mergeVertex(*input_poly->vertex(iva));
    auto out_vtx_b = out_submesh.mergeVertex(*input_poly->vertex(ivb));

    // get the side of each vert to the plane
    bool is_vertex_a_front = slicing_plane.isPointInFront(out_vtx_a->mPos);
    bool is_vertex_b_front = slicing_plane.isPointInFront(out_vtx_b->mPos);

    if (debug){
      int aindex = out_vtx_a->_poolindex;
      int bindex = out_vtx_b->_poolindex;
      printf("  is_vertex_a_front<%d:%d> is_vertex_b_front<%d:%d>\n", aindex, int(is_vertex_a_front), bindex, int(is_vertex_b_front));
    }

    if (is_vertex_a_front) {
      if (do_front) {
        printf( "emit front vtx<%d>\n", out_vtx_a->_poolindex );
        front_vertices.push_back(out_vtx_a);
      }
      // if( debug ) printf("  add a to front cnt<%zu>\n", out_front_poly.GetNumVertices());
    } else {
      if (do_back) {
        // on_out_back(vA->mPos);
      }
      // if( debug ) printf("  add a to back cnt<%zu>\n", out_back_poly.GetNumVertices());
    }

    if (is_vertex_a_front != is_vertex_b_front) { // did we cross plane ?

      bool front_to_back = (is_vertex_a_front and not is_vertex_b_front);
      bool back_to_front = (not is_vertex_a_front and is_vertex_b_front);

      //if (debug)
        //printf("  plane crossed iva<%d> ivb<%d>\n", iva, ivb);
      dvec3 vPos;
      double isectdist;
      dlineseg3 lsegab(out_vtx_a->mPos, out_vtx_b->mPos);
      bool does_intersect = slicing_plane.Intersect(lsegab, isectdist, vPos);
      dvec3 LerpedVertex;

      printf("  does_intersectAB<%d>\n", int(does_intersect));
      if(does_intersect) {
        double fDist   = (out_vtx_a->mPos - out_vtx_b->mPos).magnitude();
        double fDist2  = (out_vtx_a->mPos - vPos).magnitude();
        double fScalar = (abs(fDist) < PLANE_EPSILON) ? 0.0 : fDist2 / fDist;
        LerpedVertex.lerp(out_vtx_a->mPos, out_vtx_b->mPos, fScalar);
      }
      else{
        dlineseg3 lsegba(out_vtx_b->mPos, out_vtx_a->mPos);
        does_intersect = slicing_plane.Intersect(lsegba, isectdist, vPos);
        printf("  does_intersectBA<%d>\n", int(does_intersect));
        double fDist   = (out_vtx_b->mPos - out_vtx_a->mPos).magnitude();
        double fDist2  = (out_vtx_b->mPos - vPos).magnitude();
        double fScalar = (abs(fDist) < PLANE_EPSILON) ? 0.0 : fDist2 / fDist;
        LerpedVertex.lerp(out_vtx_b->mPos, out_vtx_a->mPos, fScalar);
      }
      if (does_intersect) {
        vertex smvert;
        smvert.mPos  = LerpedVertex;
        auto out_vtx_lerp = out_submesh.mergeVertex(smvert);
        //////////////////////
        if (do_front) {
          printf( "emit front vtx<%d>\n", out_vtx_lerp->_poolindex );
          front_vertices.push_back(out_vtx_lerp);
        }
        //////////////////////
        PlanarCrossing pc;
        if(front_to_back){
          pc._status = EPlanarStatus::CROSS_F2B;
          pc._vtxA = out_vtx_a;
          pc._vtxB = out_vtx_lerp;
          printf( "CROSS F2B %d %d\n", out_vtx_a->_poolindex, out_vtx_lerp->_poolindex );
        }
        else if(back_to_front){
          pc._status = EPlanarStatus::CROSS_B2F;
          pc._vtxA = out_vtx_b;
          pc._vtxB = out_vtx_lerp;
          printf( "CROSS B2F %d %d\n", out_vtx_b->_poolindex, out_vtx_lerp->_poolindex );
        }
        //////////////////////
        planars.push_back(pc);
        //////////////////////
      }      // isect1 ?
    }  // did we cross plane ?

  }



  if (do_front and (front_vertices.size() > 2)) {

    printf( "CLIP OUTPUT POLY[");
    for( auto vtx : front_vertices ) {
      printf( " %d", vtx->_poolindex );
    }
    printf( " ]\n");


    out_submesh.mergePoly(Polygon(front_vertices));
  }
}

///////////////////////////////////////////////////////////////////////////////

struct Clipper {

  Clipper(
      const submesh& inpsubmesh,    //
      const dplane3& slicing_plane, //
      submesh& outsmesh_Front,      //
      submesh& outsmesh_Back) {     //

    /////////////////////////////////////////////////////////////////////
    // categorize all vertices in input mesh
    /////////////////////////////////////////////////////////////////////

    PlanarVertexCategorize categorized(inpsubmesh, slicing_plane);

    /////////////////////////////////////////////////////////////////////
    // input mesh polygon loop
    /////////////////////////////////////////////////////////////////////

    int ip = 0;
    inpsubmesh.visitAllPolys([&](poly_const_ptr_t input_poly) {
      int numverts    = input_poly->numVertices();
      auto polyvtxcnt = categorized.categorizePolygon(input_poly);
      if (debug)
        printf(
            "ip<%d> numverts<%d> front<%d> back<%d> planar<%d>\n",
            ip,
            numverts,
            polyvtxcnt._front_count,
            polyvtxcnt._back_count,
            polyvtxcnt._planar_count);
      ip++;
      //////////////////////////////////////////////
      // all of this poly's vertices in front ? -> trivially route to outsmesh_Front
      //////////////////////////////////////////////
      if (numverts == polyvtxcnt._front_count) {
        addWholePoly("A:", input_poly, outsmesh_Front);
      }
      //////////////////////////////////////////////
      // all of this poly's vertices in back ? -> trivially route to outsmesh_Back
      //////////////////////////////////////////////
      else if (numverts == polyvtxcnt._back_count) { // all back ?
        addWholePoly("B: ", input_poly, outsmesh_Back);
        // TODO when closing the mesh, construct the closing face
        // with the planar vertices and input edge connectivity info
        // every input edge should have a matching output edge (which was clipped)
        printf( "BACK POLY[");
        input_poly->visitVertices([&](vertex_ptr_t vtx) {
          auto v_m = outsmesh_Front.mergeVertex(*vtx);
          printf( " %d", v_m->_poolindex );
        });
        printf( " ]\n");
      }
      //////////////////////////////////////////////
      // the remaining are those which must be clipped against plane
      //////////////////////////////////////////////
      else {

        /////////////////////////////////////////////////
        // clip the input poly into clipped_front, clipped_back
        /////////////////////////////////////////////////

        if (do_front)
          clipImpl(
              slicing_plane,  //
              input_poly,     //
              outsmesh_Front, //
              _planars_front,
              true);

        ///////////////////////////////////////////

        // if (do_front){
        // process_clipped_poly(front_verts, outsmesh_Front, front_planar_verts);
        //}

        // if (do_back)
        // process_clipped_poly(back_verts, outsmesh_Back, back_planar_verts);

      } // clipped ?
    }); // inpsubmesh.visitAllPolys( [&](poly_const_ptr_t input_poly){
  }

  edge_vect_t back_planar_edges, front_planar_edges;
  vertex_set_t front_planar_verts;
  vertex_set_t back_planar_verts;
  std::vector<PlanarCrossing> _planars_front;
};

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void _submeshClipWithPlaneConcave(
    const submesh& inpsubmesh, //
    dplane3& slicing_plane,    //
    bool close_mesh,
    bool flip_orientation,
    submesh& outsmesh_Front, //
    submesh& outsmesh_Back) {

  if (debug)
    printf("///////////\n");

  if (debug)
    inpsubmesh.dumpPolys("inpsubmesh");

  // close_mesh = false;
  Clipper clipper(inpsubmesh, slicing_plane, outsmesh_Front, outsmesh_Back);

  if (debug)
    outsmesh_Front.dumpPolys("clipped_front");

  if (debug)
    if (clipper.front_planar_verts._the_map.size() > 0) {
      printf("fpv [");
      for (auto v_item : clipper.front_planar_verts._the_map) {
        auto v = v_item.second;
        printf(" %d", v->_poolindex);
      }
      printf("]\n");
    }
  if (debug)
    if (clipper.back_planar_verts._the_map.size() > 0) {
      printf("bpv [");
      for (auto v_item : clipper.back_planar_verts._the_map) {
        auto v = v_item.second;
        printf(" %d", v->_poolindex);
      }
      printf("]\n");
    }

  ///////////////////////////////////////////////////////////
  // close mesh
  ///////////////////////////////////////////////////////////

  auto do_close = [&](submesh& outsubmesh,              //
                      vertex_set_t& planar_verts,
                      bool front) {                     //
    if (debug)
      outsubmesh.visitAllVertices([&](vertex_ptr_t v) { //
        double point_distance = slicing_plane.pointDistance(v->mPos);
        printf("outv%d : %f %f %f point_distance<%f>\n", v->_poolindex, v->mPos.x, v->mPos.y, v->mPos.z, point_distance);
      });

    if (debug)
      outsubmesh.dumpPolys("preclose");

    /////////////////////////////////////////
    //  take note of edges which lie on the
    //  slicing plane
    /////////////////////////////////////////

    edge_set_t all_edges;
    outsubmesh.visitAllPolys([&](poly_ptr_t poly) { //
      poly->visitEdges([&](edge_ptr_t e) {          //
        all_edges.insert(e);
      });
    });

    if (debug)
      for (auto e_item : all_edges._the_map) {
        auto e = e_item.second;
        printf("all e[%d %d]\n", e->_vertexA->_poolindex, e->_vertexB->_poolindex);
      }

    edge_set_t planar_edges;

    int index = 0;
    planar_verts.visit([&](vertex_ptr_t v) {
      if (debug)
        printf("planar v%d : %f %f %f\n", v->_poolindex, v->mPos.x, v->mPos.y, v->mPos.z);
      planar_verts.visit([&](vertex_ptr_t v2) {
        if (v == v2)
          return;
        auto e        = std::make_shared<edge>(v, v2);
        bool has_edge = false;
        for (auto ie : all_edges._the_map) {
          auto e = ie.second;
          if (e->_vertexA == v && e->_vertexB == v2) {
            has_edge = true;
            break;
          }
        }
        if (has_edge) {
          auto e2 = std::make_shared<edge>(v2, v);
          planar_edges.insert(e2);
        }
      });
      index++;
    });

    if (debug)
      for (auto e_item : planar_edges._the_map) {
        auto e = e_item.second;
        printf("planar e %d %d\n", e->_vertexA->_poolindex, e->_vertexB->_poolindex);
      }

    /////////////////////////////////////////
    // we have some edges on the cutting plane
    /////////////////////////////////////////

    if (planar_edges.size()) {

      // link edge chains into edge loops

      EdgeChainLinker _linker;
      _linker._name = outsubmesh.name;
      for (auto edge_item : planar_edges._the_map) {
        auto edge = edge_item.second;
        _linker.add_edge(edge);
      }
      _linker.link();

      // create a new polygon for each edge loop

      dvec3 centroid = outsubmesh.centerOfVertices();

      auto do_chain = [&](edge_chain_ptr_t chain, //
                          submesh& outsubmesh) {  //
        auto v0  = chain->_edges[0]->_vertexA;
        auto v1  = chain->_edges[0]->_vertexB;
        auto v2  = chain->_edges[1]->_vertexB;
        auto d01 = (v1->mPos - v0->mPos).normalized();
        auto d12 = (v2->mPos - v1->mPos).normalized();
        auto dc  = d01.normalized().crossWith(d12.normalized());

        auto ordered = chain->orderedVertices();
        Polygon p1(ordered);
        auto vn0 = (ordered[0]->mPos - centroid).normalized();
        if (p1.computeNormal().dotWith(vn0) < 0) {
          p1.reverse();
        }
        outsubmesh.mergePoly(p1);

      };

      for (auto loop : _linker._edge_loops) {
        if (debug)
          printf("loop [");
        if (debug)
          for (auto e : loop->_edges) {
            printf(" <%d %d>", e->_vertexA->_poolindex, e->_vertexB->_poolindex);
          }
        if (debug)
          printf("]\n");
        do_chain(loop, outsubmesh);
      }

      for (auto chain : _linker._edge_chains) {
        if (debug)
          printf("chain [");
        if (debug)
          for (auto e : chain->_edges) {
            printf(" <%d %d>", e->_vertexA->_poolindex, e->_vertexB->_poolindex);
          }
        if (debug)
          printf("]\n");
        // do_chain(chain, outsubmesh);
      }
    } // if (planar_edges.size()) {

  };

  if (do_front and close_mesh) {
    do_close(outsmesh_Front, clipper.front_planar_verts, true);
  }
  // if (do_back and close_mesh)
  // do_close(outsmesh_Back, clipper.back_planar_verts, false);

  ///////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void submeshClipWithPlane(
    const submesh& inpsubmesh, //
    dplane3& slicing_plane,    //
    bool close_mesh,
    bool flip_orientation,
    submesh& outsmesh_Front, //
    submesh& outsmesh_Back) {

  _submeshClipWithPlaneConcave(
      inpsubmesh,       //
      slicing_plane,    //
      close_mesh,       //
      flip_orientation, //
      outsmesh_Front,   //
      outsmesh_Back);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
