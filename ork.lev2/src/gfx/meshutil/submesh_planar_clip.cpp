////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/math/plane.hpp>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <ork/util/logger.h>
#include <deque>

static constexpr bool debug = false;

static constexpr bool do_front        = true;
static constexpr bool do_back         = true;
static constexpr double PLANE_EPSILON = 0.001f;

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_clip = logger()->createChannel("meshutil.clipper", fvec3(.9, .9, 1), true);

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
      logchan_clip->log("iv<%d> point_distance<%f>\n", iv, point_distance);
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

vertex_set_t addWholePoly(
    std::string hdr,           //
    poly_const_ptr_t src_poly, //
    submesh& dest) {           //

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
  if (log_poly) {

    logchan_clip->log_continue("<%s> add whole poly: [", hdr.c_str());
    src_poly->visitVertices([&](vertex_ptr_t v) { logchan_clip->log_continue("v<%d> ", v->_poolindex); });
    logchan_clip->log_continue("]\n");
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

///////////////////////////////////////////////////////////////////////////////

enum class EPlanarStatus { CROSS_F2B, CROSS_B2F, PLANAR, NONE };

struct PlanarCrossing {
  vertex_ptr_t _vtxA;
  vertex_ptr_t _vtxB;
  EPlanarStatus _status;
};

struct EdgeChangeRecord {
  vertex_ptr_t _orig_vtxA;
  vertex_ptr_t _orig_vtxB;
  vertex_ptr_t _new_vtxA;
  vertex_ptr_t _new_vtxB;
};
using edge_change_record_ptr_t = std::shared_ptr<EdgeChangeRecord>;

///////////////////////////////////////////////////////////////////////////////

void ClipPolygon(
    const dplane3& slicing_plane,
    poly_const_ptr_t input_poly, //
    submesh& out_submesh,        //
    std::vector<PlanarCrossing>& crossings,
    bool do_front) { //

  bool do_back = not do_front;

  const int inuminverts = input_poly->numVertices();
  OrkAssert(inuminverts >= 3);

  logchan_clip->log_continue("CLIP INPUT POLY[");
  vertex_vect_t input_poly_verts;
  input_poly->visitVertices([&](vertex_ptr_t vtx) {
    auto v_m = out_submesh.mergeVertex(*vtx);
    input_poly_verts.push_back(v_m);
    logchan_clip->log_continue(" %d", v_m->_poolindex);
  });
  logchan_clip->log_continue(" ]\n");
  // if (debug)
  // printf("clip poly num verts<%d>\n", inuminverts);

  // loop around the input polygon's edges

  std::vector<vertex_ptr_t> front_vertices;

  for (int iva = 0; iva < inuminverts; iva++) {
    // if (debug)
    //  printf("  iva<%d> of inuminverts<%d>\n", iva, inuminverts);
    int ivb =
        ((iva == inuminverts - 1) //
             ? 0                  //
             : iva + 1);

    auto out_vtx_a = out_submesh.mergeVertex(*input_poly->vertex(iva));
    auto out_vtx_b = out_submesh.mergeVertex(*input_poly->vertex(ivb));

    auto he_ab = out_submesh.mergeEdgeForVertices(out_vtx_a, out_vtx_b);
    auto he_ba = out_submesh.mergeEdgeForVertices(out_vtx_b, out_vtx_a);

    // get the side of each vert to the plane
    bool is_vertex_a_front = slicing_plane.isPointInFront(out_vtx_a->mPos);
    bool is_vertex_b_front = slicing_plane.isPointInFront(out_vtx_b->mPos);

    {
      int aindex = out_vtx_a->_poolindex;
      int bindex = out_vtx_b->_poolindex;
      logchan_clip->log(
          "  is_vertex_a_front<%d:%d> is_vertex_b_front<%d:%d>", aindex, int(is_vertex_a_front), bindex, int(is_vertex_b_front));
    }

    if (is_vertex_a_front) {
      if (do_front) {
        logchan_clip->log("emit front vtx<%d>", out_vtx_a->_poolindex);
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

      // if (debug)
      // printf("  plane crossed iva<%d> ivb<%d>\n", iva, ivb);
      dvec3 vPos;
      double isectdist;
      dlineseg3 lsegab(out_vtx_a->mPos, out_vtx_b->mPos);
      bool does_intersect = slicing_plane.Intersect(lsegab, isectdist, vPos);
      dvec3 LerpedVertex;

      logchan_clip->log("  does_intersectAB<%d>\n", int(does_intersect));
      if (does_intersect) {
        double fDist   = (out_vtx_a->mPos - out_vtx_b->mPos).magnitude();
        double fDist2  = (out_vtx_a->mPos - vPos).magnitude();
        double fScalar = (abs(fDist) < PLANE_EPSILON) ? 0.0 : fDist2 / fDist;
        LerpedVertex.lerp(out_vtx_a->mPos, out_vtx_b->mPos, fScalar);
      } else {
        dlineseg3 lsegba(out_vtx_b->mPos, out_vtx_a->mPos);
        does_intersect = slicing_plane.Intersect(lsegba, isectdist, vPos);
        logchan_clip->log("  does_intersectBA<%d>\n", int(does_intersect));
        double fDist   = (out_vtx_b->mPos - out_vtx_a->mPos).magnitude();
        double fDist2  = (out_vtx_b->mPos - vPos).magnitude();
        double fScalar = (abs(fDist) < PLANE_EPSILON) ? 0.0 : fDist2 / fDist;
        LerpedVertex.lerp(out_vtx_b->mPos, out_vtx_a->mPos, fScalar);
      }
      if (does_intersect) {
        vertex smvert;
        smvert.mPos       = LerpedVertex;
        auto out_vtx_lerp = out_submesh.mergeVertex(smvert);

        //////////////////////
        if (out_vtx_a->_varmap.hasKey("clipped_vertex")) {
          // auto pre_existing = out_vtx_a->_varmap.typedValueForKey<vertex_ptr_t>("clipped_vertex").value();
          // printf( "pre_existing clipped vtxa<%d> : clipped vtx<%d>\n", pre_existing->_poolindex, out_vtx_lerp->_poolindex );
          // OrkAssert( pre_existing == out_vtx_lerp );
        } else {
          // out_vtx_a->_varmap.mergedValueForKey<vertex_ptr_t>("clipped_vertex") = out_vtx_lerp;
          // printf( "marked vtxa<%d> as clipped vtx<%d>\n", out_vtx_a->_poolindex, out_vtx_lerp->_poolindex );
        }
        if (out_vtx_b->_varmap.hasKey("clipped_vertex")) {
          // auto pre_existing = out_vtx_b->_varmap.typedValueForKey<vertex_ptr_t>("clipped_vertex").value();
          // printf( "pre_existing clipped vtxb<%d> : clipped vtx<%d>\n", pre_existing->_poolindex, out_vtx_lerp->_poolindex );
          // OrkAssert( pre_existing == out_vtx_lerp );
        } else {
          // out_vtx_b->_varmap.mergedValueForKey<vertex_ptr_t>("clipped_vertex") = out_vtx_lerp;
          // printf( "marked vtxb<%d> as clipped vtx<%d>\n", out_vtx_b->_poolindex, out_vtx_lerp->_poolindex );
        }
        //////////////////////
        if (do_front) {
          logchan_clip->log("emit front vtx<%d>\n", out_vtx_lerp->_poolindex);
          front_vertices.push_back(out_vtx_lerp);
        }
        //////////////////////
        PlanarCrossing pc;
        if (front_to_back) {
          pc._status = EPlanarStatus::CROSS_F2B;
          pc._vtxA   = out_vtx_a;
          pc._vtxB   = out_vtx_lerp;
          logchan_clip->log("CROSS F2B %d->%d: %d\n", out_vtx_a->_poolindex, out_vtx_b->_poolindex, out_vtx_lerp->_poolindex);
          he_ab->_varmap.mergedValueForKey<PlanarCrossing>("crossing") = pc;
          auto clipped_edge                                            = out_submesh.mergeEdgeForVertices(out_vtx_a, out_vtx_lerp);
          he_ab->_varmap.mergedValueForKey<halfedge_ptr_t>("clipped_edge") = clipped_edge;
        } else if (back_to_front) {
          pc._status = EPlanarStatus::CROSS_B2F;
          pc._vtxA   = out_vtx_b;
          pc._vtxB   = out_vtx_lerp;
          logchan_clip->log("CROSS B2F %d->%d : %d\n", out_vtx_b->_poolindex, out_vtx_a->_poolindex, out_vtx_lerp->_poolindex);
          he_ba->_varmap.mergedValueForKey<PlanarCrossing>("crossing") = pc;
          auto clipped_edge                                            = out_submesh.mergeEdgeForVertices(out_vtx_b, out_vtx_lerp);
          he_ba->_varmap.mergedValueForKey<halfedge_ptr_t>("clipped_edge") = clipped_edge;
        }
        //////////////////////
        crossings.push_back(pc);
        //////////////////////
      }                                                             // isect1 ?
    }                                                               // did we cross plane ?
    else if ((not is_vertex_a_front) and (not is_vertex_b_front)) { // all back ?
      he_ab->_varmap.mergedValueForKey<bool>("back_edge") = true;
    }

  } // for (int iva = 0; iva < inuminverts; iva++) {

  if (do_front and (front_vertices.size() > 2)) {

    logchan_clip->log_continue("CLIP OUTPUT POLY[");
    for (auto vtx : front_vertices) {
      logchan_clip->log_continue(" %d", vtx->_poolindex);
    }
    logchan_clip->log_continue(" ]\n");
    out_submesh.mergePoly(Polygon(front_vertices));

    int inumverts = input_poly_verts.size();
    if(front_vertices.size()==inumverts){

    for( int iva=0; iva<inumverts; iva++ ) {
      int ivb = (iva+1)%inumverts;
      auto inp_vtx_a = input_poly_verts[iva];
      auto inp_vtx_b = input_poly_verts[ivb];
      auto frn_vtx_a = front_vertices[iva];
      auto frn_vtx_b = front_vertices[ivb];

      if( (inp_vtx_a != frn_vtx_a) or (inp_vtx_b != frn_vtx_b)) {
        auto he_inp = out_submesh.mergeEdgeForVertices(inp_vtx_a,inp_vtx_b);
        auto he_out = out_submesh.mergeEdgeForVertices(frn_vtx_a,frn_vtx_b);
        he_inp->_varmap.mergedValueForKey<halfedge_ptr_t>("clipped_edge") = he_out;

        he_inp = out_submesh.mergeEdgeForVertices(inp_vtx_b,inp_vtx_a);
        he_out = out_submesh.mergeEdgeForVertices(frn_vtx_b,frn_vtx_a);
        he_inp->_varmap.mergedValueForKey<halfedge_ptr_t>("clipped_edge") = he_out;
      }
    }
    }



    
  }
}

///////////////////////////////////////////////////////////////////////////////

struct SubMeshClipper {

  SubMeshClipper(
      const submesh& inpsubmesh,    //
      const dplane3& slicing_plane, //
      submesh& outsmesh_front,      //
      submesh& outsmesh_back) {     //

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
        logchan_clip->log_continue(
            "ip<%d> numverts<%d> front<%d> back<%d> planar<%d>\n",
            ip,
            numverts,
            polyvtxcnt._front_count,
            polyvtxcnt._back_count,
            polyvtxcnt._planar_count);
      ip++;

      //////////////////////////////////////////////
      // all of this poly's vertices in front ? -> trivially route to outsmesh_front
      //////////////////////////////////////////////

      if (numverts == polyvtxcnt._front_count) {
        addWholePoly("A:", input_poly, outsmesh_front);
        _frontpolys.insert(input_poly);
      }

      //////////////////////////////////////////////
      // all of this poly's vertices in back ? -> trivially route to outsmesh_Back
      //////////////////////////////////////////////

      else if (numverts == polyvtxcnt._back_count) { // all back ?
        addWholePoly("B: ", input_poly, outsmesh_back);
        // TODO when closing the mesh, construct the closing face
        // with the planar vertices and input edge connectivity info
        // every input edge should have a matching output edge (which was clipped)
        logchan_clip->log_continue("BACK POLY[");
        std::vector<vertex_ptr_t> back_vertices;
        input_poly->visitVertices([&](vertex_ptr_t vtx) {
          auto v_m                                            = outsmesh_front.mergeVertex(*vtx);
          v_m->_varmap.mergedValueForKey<bool>("back_vertex") = true;
          back_vertices.push_back(v_m);
          logchan_clip->log_continue(" %d", v_m->_poolindex);
        });
        logchan_clip->log_continue(" ]\n");
        auto back_poly = std::make_shared<Polygon>(back_vertices);
        _backpolys.insert(back_poly);
        int numv = back_poly->numVertices();
        for( int iv=0; iv<numv; iv++ ) {
          auto vtx_a = back_poly->vertex(iv);
          auto vtx_b = back_poly->vertex((iv+1)%numv);
          auto he_ab = outsmesh_front.mergeEdgeForVertices(vtx_a,vtx_b);
          he_ab->_varmap.mergedValueForKey<bool>("back_edge") = true;
        }
      }

      //////////////////////////////////////////////
      // the remaining are those which must be clipped against plane
      //////////////////////////////////////////////

      else {

        /////////////////////////////////////////////////
        // clip the input poly into clipped_front, clipped_back
        /////////////////////////////////////////////////

        if (do_front)
          ClipPolygon(
              slicing_plane,  //
              input_poly,     //
              outsmesh_front, //
              _planars_front,
              true);

        if (do_back)
          ClipPolygon(
              slicing_plane, //
              input_poly,    //
              outsmesh_back, //
              _planars_back,
              false);

      } // clipped ?
    }); // inpsubmesh.visitAllPolys( [&](poly_const_ptr_t input_poly){
  }

  edge_vect_t back_planar_edges, front_planar_edges;
  vertex_set_t front_planar_verts;
  vertex_set_t back_planar_verts;
  std::vector<PlanarCrossing> _planars_front;
  std::vector<PlanarCrossing> _planars_back;
  polyconst_set_t _backpolys;
  polyconst_set_t _frontpolys;
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
    submesh& outsmesh_front, //
    submesh& outsmesh_back) {

  if (debug) {
    logchan_clip->log_continue("///////////\n");
    inpsubmesh.dumpPolys("inpsubmesh");
  }

  // close_mesh = false;
  SubMeshClipper sm_clipper(inpsubmesh, slicing_plane, outsmesh_front, outsmesh_back);

  if (debug) {
    outsmesh_front.dumpPolys("clipped_front");
    if (sm_clipper.front_planar_verts._the_map.size() > 0) {
      logchan_clip->log_continue("fpv [");
      for (auto v_item : sm_clipper.front_planar_verts._the_map) {
        auto v = v_item.second;
        logchan_clip->log_continue(" %d", v->_poolindex);
      }
      logchan_clip->log_continue("]\n");
    }
    if (sm_clipper.back_planar_verts._the_map.size() > 0) {
      logchan_clip->log_continue("bpv [");
      for (auto v_item : sm_clipper.back_planar_verts._the_map) {
        auto v = v_item.second;
        logchan_clip->log_continue(" %d", v->_poolindex);
      }
      logchan_clip->log_continue("]\n");
    }
  }

  ///////////////////////////////////////////////////////////
  // close mesh
  ///////////////////////////////////////////////////////////

  auto do_close = [&](submesh& outsubmesh, //
                      vertex_set_t& planar_verts,
                      bool front) { //
    if (debug) {
      outsubmesh.visitAllVertices([&](vertex_ptr_t v) { //
        double point_distance = slicing_plane.pointDistance(v->mPos);
        logchan_clip->log_continue(
            "outv%d : %f %f %f point_distance<%f>\n", v->_poolindex, v->mPos.x, v->mPos.y, v->mPos.z, point_distance);
      });
      outsubmesh.dumpPolys("preclose");
    }

    if (front) {

      sm_clipper._backpolys.visit([&](poly_const_ptr_t back_poly) {
        int num_clipped = 0;
        int num_v       = back_poly->numVertices();

        logchan_clip->log_continue("BACKPOLYVISIT INPUT POLY[");

        for( int iva=0; iva<num_v; iva++ ) {
          int ivb = (iva+1)%num_v;
          auto inp_vtx_a = back_poly->vertex(iva);
          auto inp_vtx_b = back_poly->vertex(ivb);
          auto he = outsubmesh.mergeEdgeForVertices(inp_vtx_a, inp_vtx_b);
          bool has_clipped = inp_vtx_a->_varmap.hasKey("clipped_vertex");
          if (has_clipped) {
            num_clipped++;
            logchan_clip->log_continue(" <%d>", inp_vtx_a->_poolindex);
          } else {
            bool is_back = inp_vtx_a->_varmap.hasKey("back_vertex");
            if (is_back) {
              logchan_clip->log_continue(" (%d)", inp_vtx_a->_poolindex);

            } else {
              logchan_clip->log_continue(" %d", inp_vtx_a->_poolindex);
            }
          }
          if(he->_varmap.hasKey("back_edge")) {
            logchan_clip->log_continue("!");
          }
          if(he->_varmap.hasKey("clipped_edge")) {
            logchan_clip->log_continue("$");
          }
        };
        logchan_clip->log_continue(" ] num_v<%d> num_clipped<%d>\n", num_v, num_clipped);

        if (num_clipped > 1 and num_clipped == num_v) {
          // all vertices are clipped, so this poly is now a planar poly
          // and shall be added to the front mesh (with orientation flipped)
          vertex_vect_t front_verts;
          back_poly->visitVertices([&](vertex_ptr_t vtx) {
            // auto clipped_vtx = vtx->_varmap.typedValueForKey<vertex_ptr_t>("clipped_vertex");
            // front_verts.push_back(clipped_vtx.value());
          });
          // auto front_poly = outsubmesh.mergePoly(front_verts);
          // OrkAssert(false);
        }
      });
    }

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
        logchan_clip->log_continue("all e[%d %d]\n", e->_vertexA->_poolindex, e->_vertexB->_poolindex);
      }

    edge_set_t planar_edges;

    int index = 0;
    planar_verts.visit([&](vertex_ptr_t v) {
      if (debug)
        logchan_clip->log_continue("planar v%d : %f %f %f\n", v->_poolindex, v->mPos.x, v->mPos.y, v->mPos.z);
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

    if (debug) {
      for (auto e_item : planar_edges._the_map) {
        auto e = e_item.second;
        logchan_clip->log_continue("planar e %d %d\n", e->_vertexA->_poolindex, e->_vertexB->_poolindex);
      }
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
        if (debug) {
          logchan_clip->log_continue("loop [");
          for (auto e : loop->_edges) {
            logchan_clip->log_continue(" <%d %d>", e->_vertexA->_poolindex, e->_vertexB->_poolindex);
          }
          logchan_clip->log_continue("]\n");
        }
        do_chain(loop, outsubmesh);
      }

      for (auto chain : _linker._edge_chains) {
        if (debug) {
          logchan_clip->log_continue("chain [");
          for (auto e : chain->_edges) {
            logchan_clip->log_continue(" <%d %d>", e->_vertexA->_poolindex, e->_vertexB->_poolindex);
          }
          logchan_clip->log_continue("]\n");
        }
        // do_chain(chain, outsubmesh);
      }
    } // if (planar_edges.size()) {

  }; // auto do_close = [&](submesh& outsubmesh, //

  if (do_front and close_mesh) { //
    do_close(
        outsmesh_front,                //
        sm_clipper.front_planar_verts, //
        true);
  }
  if (do_back and close_mesh) { //
    do_close(
        outsmesh_back,                //
        sm_clipper.back_planar_verts, //
        false);
  }

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
