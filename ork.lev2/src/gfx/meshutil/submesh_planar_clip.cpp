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

static constexpr bool debug = true;

static constexpr bool do_front        = true;
static constexpr bool do_back         = false;
static constexpr double PLANE_EPSILON = 0.001f;

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_clip = logger()->createChannel("meshutil.clipper", fvec3(.9, .9, 1), true);

const std::unordered_set<int> test_verts = {3,11,4,0};

bool matchTestPoly(merged_poly_const_ptr_t src_poly){
  bool match_poly = true;
  src_poly->visitVertices([&](vertex_ptr_t v) {
    if (test_verts.find(v->_poolindex) == test_verts.end())
      match_poly = false;
  });
  return match_poly;
}

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
      logchan_clip->log("iv<%d> point_distance<%f>", iv, point_distance);
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

  PolyVtxCount categorizePolygon(merged_poly_const_ptr_t input_poly) const {
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

  vertexconst_set_t _front_verts;
  vertexconst_set_t _back_verts;
  vertexconst_set_t _planar_verts;
};

///////////////////////////////////////////////////////////////////////////////

enum class EPlanarStatus { CROSS_F2B, CROSS_B2F, PLANAR, FRONT, BACK, NONE };

struct PlanarStatus {
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

struct SubMeshClipper {
  /////////////////////////////////////
  SubMeshClipper(
      const submesh& inpsubmesh,                    //
      const dplane3& plane,                         //
      submesh& smfront,                             //
      submesh& smback,                              //
      bool do_close)                                //
      : _inpsubmesh(inpsubmesh)                     //
      , _slicing_plane(plane)                       //
      , _outsubmesh_front(smfront)                  //
      , _outsubmesh_back(smback)                    //
      , _do_close(do_close)                         //
      , _categorized(_inpsubmesh, _slicing_plane) { //
    process();
  }
  /////////////////////////////////////
  void process();
  void procEdges(merged_poly_const_ptr_t input_poly, bool do_front);
  void clipPolygon(merged_poly_const_ptr_t input_poly, bool do_front);
  void closeSubMesh(bool do_front);
  vertex_set_t addWholePoly(
      std::string hdr,                  //
      merged_poly_const_ptr_t src_poly, //
      submesh& dest);
  /////////////////////////////////////
  const submesh& _inpsubmesh;
  dplane3 _slicing_plane;
  submesh& _outsubmesh_front;
  submesh& _outsubmesh_back;
  bool _do_close;
  /////////////////////////////////////
  edge_vect_t back_planar_edges, front_planar_edges;
  vertex_set_t _planar_verts_front;
  vertex_set_t _planar_verts_back;
  polyconst_set_t _backpolys;
  polyconst_set_t _frontpolys;
  std::map<int, int> _vertex_remap;
  PlanarVertexCategorize _categorized;
  std::vector<merged_poly_const_ptr_t> _polys_to_clip;
  std::unordered_map<merged_poly_const_ptr_t, varmap::VarMap> _inp_poly_varmap;
};

///////////////////////////////////////////////////////////////////////////////

void SubMeshClipper::process() {

  _inpsubmesh.visitAllVertices([&](vertex_const_ptr_t input_vertex) {
    if(do_front){
      auto merged_front = _outsubmesh_front.mergeVertex(*input_vertex);
      //logchan_clip->log("INPVTX<%d> -> MERGED.subm-front<%d>", input_vertex->_poolindex, merged_front->_poolindex);
    }
    if(do_back){
      auto merged_back = _outsubmesh_back.mergeVertex(*input_vertex);
      //logchan_clip->log("INPVTX<%d> -> MERGED.subm-back<%d>", input_vertex->_poolindex, merged_back->_poolindex);
    }

  });

  /////////////////////////////////////////////////////////////////////
  // categorize all vertices in input mesh
  /////////////////////////////////////////////////////////////////////

  _categorized._back_verts.visit([&](vertex_const_ptr_t vtx) {
    auto m                                             = _outsubmesh_front.mergeVertex(*vtx);
    _outsubmesh_front.mergeVar<bool>(m, "back_vertex") = true;
  });

  /////////////////////////////////////////////////////////////////////
  // input mesh polygon loop
  /////////////////////////////////////////////////////////////////////

  int ip = 0;
  _inpsubmesh.visitAllPolys([&](merged_poly_const_ptr_t input_poly) {

    bool match_poly = matchTestPoly(input_poly);

    int numverts    = input_poly->numVertices();
    auto polyvtxcnt = _categorized.categorizePolygon(input_poly);
    if (debug)
      logchan_clip->log(
          "ip<%d> numverts<%d> front<%d> back<%d> planar<%d>",
          ip,
          numverts,
          polyvtxcnt._front_count,
          polyvtxcnt._back_count,
          polyvtxcnt._planar_count);
    ip++;

    //////////////////////////////////////////////
    // all of this poly's vertices in front ? -> trivially route to _outsubmesh_front
    //////////////////////////////////////////////

    if (numverts == polyvtxcnt._front_count) {
      addWholePoly("A:", input_poly, _outsubmesh_front);
      _frontpolys.insert(input_poly);
    }

    //////////////////////////////////////////////
    // all of this poly's vertices in back ? -> trivially route to _outsubmesh_back
    //////////////////////////////////////////////

    else if (numverts == polyvtxcnt._back_count) { // all back ?
      addWholePoly("B: ", input_poly, _outsubmesh_back);
      // TODO when closing the mesh, construct the closing face
      // with the planar vertices and input edge connectivity info
      // every input edge should have a matching output edge (which was clipped)
    logchan_clip->log_begin("BACK POLY[");
      std::vector<vertex_ptr_t> back_vertices;
      input_poly->visitVertices([&](vertex_ptr_t vtx) {
        auto v_m                                             = _outsubmesh_front.mergeVertex(*vtx);
        _outsubmesh_front.mergeVar<bool>(v_m, "back_vertex") = true;
        back_vertices.push_back(v_m);
        logchan_clip->log_continue(" %d", v_m->_poolindex);
      });
      logchan_clip->log_continue(" ]\n");
      auto back_poly = std::make_shared<Polygon>(back_vertices);
      _backpolys.insert(back_poly);
      _polys_to_clip.push_back(input_poly);

    }

    //////////////////////////////////////////////
    // the remaining are those which must be clipped against plane
    //////////////////////////////////////////////

    else {

      /////////////////////////////////////////////////
      // clip the input poly into clipped_edgeclipped_front, clipped_back
      /////////////////////////////////////////////////

      _polys_to_clip.push_back(input_poly);

    } // clipped ?
  }); // _inpsubmesh.visitAllPolys( [&](poly_const_ptr_t input_poly){

  ////////////////////////////////////////////////////////////////////
  // characterize all edges
  ////////////////////////////////////////////////////////////////////

  _inpsubmesh.visitAllPolys([&](merged_poly_const_ptr_t input_poly) {
    if (do_front)
      this->procEdges(input_poly, true);

    if (do_back)
      this->procEdges(input_poly, false);
  });

  ////////////////////////////////////////////////////////////////////
  // now that all input polys have been categorized, and the database
  //  constructed, we can clip
  ////////////////////////////////////////////////////////////////////

  for (auto input_poly : _polys_to_clip) {
    if (do_front)
      this->clipPolygon(input_poly, true);

    if (do_back)
      this->clipPolygon(input_poly, false);
  }

  ////////////////////////////////////////////////////////////////////
  // close output submeshes
  ////////////////////////////////////////////////////////////////////

  if (do_front and _do_close) { //
    closeSubMesh(true);
  }
  if (do_back and _do_close) { //
    closeSubMesh(false);
  }
}

///////////////////////////////////////////////////////////////////////////////

void SubMeshClipper::procEdges(merged_poly_const_ptr_t input_poly, bool do_front) { //
  bool do_back          = not do_front;
  auto& out_submesh     = do_front ? _outsubmesh_front : _outsubmesh_back;
  const int inuminverts = input_poly->numVertices();
  OrkAssert(inuminverts >= 3);
  static halfedge_ptr_t _F2B_EDGE = nullptr;
  int _f2b_count                  = 0;
  int _f2b_index                  = -1;
  int _b2f_count                  = 0;
  bool do_log = matchTestPoly(input_poly);

  if(do_log)logchan_clip->log("  procEdges inppoly<%d> numv<%d>", input_poly->_submeshIndex, inuminverts);
  for (int iva = 0; iva < inuminverts; iva++) {

    int ivb        = (iva + 1) % inuminverts;
    auto out_vtx_a = out_submesh.mergeVertex(*input_poly->vertex(iva));
    auto out_vtx_b = out_submesh.mergeVertex(*input_poly->vertex(ivb));
    auto he_ab     = out_submesh.mergeEdgeForVertices(out_vtx_a, out_vtx_b);
    auto he_ba     = out_submesh.mergeEdgeForVertices(out_vtx_b, out_vtx_a);
    // get the side of each vert to the plane
    bool is_vertex_a_front                                = _slicing_plane.isPointInFront(out_vtx_a->mPos);
    bool is_vertex_b_front                                = _slicing_plane.isPointInFront(out_vtx_b->mPos);
    auto& plstat = out_submesh.mergeVar<PlanarStatus>(he_ab, "plstatus");
    plstat._status                                        = EPlanarStatus::NONE;
    plstat._vtxA                                          = out_vtx_a;
    plstat._vtxB                                          = out_vtx_b;
    if (is_vertex_a_front and is_vertex_b_front) {
      plstat._status                                        = EPlanarStatus::FRONT;
      plstat._vtxA                                          = out_vtx_a;
      plstat._vtxB                                          = out_vtx_b;
      if(do_log)logchan_clip->log("  iva<%d> emit front he_ab<%p>", iva, (void*)he_ab.get());
    } else if ((not is_vertex_a_front) and (not is_vertex_b_front)) {
      plstat._status                                        = EPlanarStatus::BACK;
      plstat._vtxA                                          = out_vtx_a;
      plstat._vtxB                                          = out_vtx_b;
      if(do_log)logchan_clip->log("  iva<%d> emit back he_ab<%p>", iva, (void*)he_ab.get());
    } else { // did we cross plane ?
      OrkAssert(is_vertex_a_front != is_vertex_b_front);
      bool front_to_back = (is_vertex_a_front and not is_vertex_b_front);
      bool back_to_front = (not is_vertex_a_front and is_vertex_b_front);
      dvec3 vPos;
      double isectdist;
      auto n_ab = (out_vtx_b->mPos - out_vtx_a->mPos).normalized();
      dray3 lsegab(out_vtx_a->mPos - n_ab * 1000.0, n_ab);
      bool does_intersect = _slicing_plane.Intersect(lsegab, isectdist, vPos);
      dvec3 LerpedVertex;
      // logchan_clip->log("  does_intersectAB<%d>", int(does_intersect));
      if (does_intersect) {
        double fDist   = (out_vtx_a->mPos - out_vtx_b->mPos).magnitude();
        double fDist2  = (out_vtx_a->mPos - vPos).magnitude();
        double fScalar = (abs(fDist) < PLANE_EPSILON) ? 0.0 : fDist2 / fDist;
        LerpedVertex.lerp(out_vtx_a->mPos, out_vtx_b->mPos, fScalar);
      } else {
        dray3 lsegba(out_vtx_b->mPos + n_ab * 1000.0, -n_ab);
        does_intersect = _slicing_plane.Intersect(lsegba, isectdist, vPos);
        // logchan_clip->log("  does_intersectBA<%d>", int(does_intersect));
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
        if (front_to_back) {
          plstat._status = EPlanarStatus::CROSS_F2B;
          plstat._vtxA   = out_vtx_a;
          plstat._vtxB   = out_vtx_lerp;
          if(do_log)logchan_clip->log(
              "  iva<%d> emit CROSS F2B %d->%d: %d he_ab<%p>",
              iva,
              out_vtx_a->_poolindex,
              out_vtx_b->_poolindex,
              out_vtx_lerp->_poolindex,
              (void*)he_ab.get());
          auto clipped_edge                                           = out_submesh.mergeEdgeForVertices(out_vtx_a, out_vtx_lerp);
          out_submesh.mergeVar<halfedge_ptr_t>(he_ab, "clipped_edge") = clipped_edge;
          out_submesh.mergeVar<vertex_ptr_t>(out_vtx_b, "clipped_vertex") = out_vtx_lerp;
          _F2B_EDGE                                                       = clipped_edge;
          _f2b_count++;
          _f2b_index = iva;

        } else if (back_to_front) {
          plstat._status = EPlanarStatus::CROSS_B2F;
          plstat._vtxA   = out_vtx_lerp;
          plstat._vtxB   = out_vtx_b;
          if(do_log)logchan_clip->log(
              "  iva<%d> emit CROSS B2F %d->%d : %d he_ab<%p>",
              iva,
              out_vtx_b->_poolindex,
              out_vtx_a->_poolindex,
              out_vtx_lerp->_poolindex,
              (void*)he_ab.get());
          auto clipped_edge                                           = out_submesh.mergeEdgeForVertices(out_vtx_lerp,out_vtx_b);
          out_submesh.mergeVar<halfedge_ptr_t>(he_ab, "clipped_edge") = clipped_edge;
          out_submesh.mergeVar<vertex_ptr_t>(out_vtx_a, "clipped_vertex") = out_vtx_lerp;
          _F2B_EDGE                                                       = nullptr;
          _b2f_count++;

        } else {
          OrkAssert(false);
        }
      }                   // isect1 ?
      else {
        OrkAssert(false); // crossed the plane, but non intersecting ?
      }
    }                     // did we cross plane ?
  }                       // for (int iva = 0; iva < inuminverts; iva++) {
  OrkAssert(_b2f_count == _f2b_count);
  if (_F2B_EDGE != nullptr) {
    _inp_poly_varmap[input_poly].makeValueForKey<int>("f2b_index") = _f2b_index;
  }
}

///////////////////////////////////////////////////////////////////////////////

void SubMeshClipper::clipPolygon(merged_poly_const_ptr_t input_poly, bool do_front) { //

  bool do_back = not do_front;

  auto& out_submesh = do_front ? _outsubmesh_front : _outsubmesh_back;

  const int inuminverts = input_poly->numVertices();
  OrkAssert(inuminverts >= 3);

  bool match_poly = matchTestPoly(input_poly);
  if(match_poly)logchan_clip->log_begin("CLIP INPUT POLY<%d>[", input_poly->_submeshIndex);
  vertex_vect_t input_poly_verts;
  input_poly->visitVertices([&](vertex_ptr_t vtx) {
    auto v_m = out_submesh.mergeVertex(*vtx);
    input_poly_verts.push_back(v_m);
    if(match_poly)logchan_clip->log_continue(" %d", v_m->_poolindex);
  });
  if(match_poly)logchan_clip->log_continue(" ]\n");
  if (match_poly)
    logchan_clip->log("clip poly num verts<%d>", inuminverts);
  // loop around the input polygon's edges

  std::vector<halfedge_ptr_t> frontmesh_edges;
  // std::vector<vertex_ptr_t> back_vertices;

  int _f2b_index = 0;
  if (auto try_f2b_index = _inp_poly_varmap[input_poly].typedValueForKey<int>("f2b_index")) {
    _f2b_index = try_f2b_index.value();
    if(_f2b_index<0)_f2b_index=0;
    if (match_poly)
      logchan_clip->log("_f2b_index<%d>", _f2b_index);
  }

  halfedge_ptr_t _F2B_EDGE = nullptr;
  for (int i = 0; i < inuminverts; i++) {
    int iva = (i + _f2b_index) % inuminverts;
    int ivb = (iva + 1) % inuminverts;

    auto out_vtx_a = out_submesh.mergeVertex(*input_poly->vertex(iva));
    auto out_vtx_b = out_submesh.mergeVertex(*input_poly->vertex(ivb));
    auto he_ab     = out_submesh.mergeEdgeForVertices(out_vtx_a, out_vtx_b);
    auto he_ba     = out_submesh.mergeEdgeForVertices(out_vtx_b, out_vtx_a);

    if (match_poly)
      logchan_clip->log_continue("  i<%d> iva<%d> of inuminverts<%d> he_ab<%p>\n", i, iva, inuminverts, (void*)he_ab.get());

    if (do_front) {
      auto& plstat = out_submesh.typedVar<PlanarStatus>(he_ab, "plstatus");
      switch (plstat._status) {
        case EPlanarStatus::FRONT:
          if(match_poly)logchan_clip->log("  front");
          frontmesh_edges.push_back(he_ab);
          break;
        case EPlanarStatus::BACK:
          // OrkAssert(false);
          if(match_poly)logchan_clip->log("  back");

          if( auto try_clip_ab = out_submesh.tryVarAs<halfedge_ptr_t>(he_ab, "clipped_edge") ){
            auto clipped_ab = try_clip_ab.value();
            if(match_poly)logchan_clip->log("  back - got clipab <%d->%d>\n", clipped_ab->_vertexA->_poolindex, clipped_ab->_vertexB->_poolindex);
          }
          else if( auto try_clip_ba = out_submesh.tryVarAs<halfedge_ptr_t>(he_ba, "clipped_edge") ){
            auto clipped_ba = try_clip_ba.value();
            if(match_poly)logchan_clip->log("  back - got clipba <%d->%d>\n", clipped_ba->_vertexA->_poolindex, clipped_ba->_vertexB->_poolindex);
          }

          break;
        case EPlanarStatus::CROSS_F2B: {
          if(match_poly)logchan_clip->log("  front2back");
          auto clipped = out_submesh.typedVar<halfedge_ptr_t>(he_ab, "clipped_edge");
          frontmesh_edges.push_back(clipped);
          OrkAssert(clipped != nullptr);
          _F2B_EDGE = clipped;
          break;
        }
        case EPlanarStatus::CROSS_B2F: {
          if(match_poly)logchan_clip->log("  back2front");

          OrkAssert(_F2B_EDGE != nullptr);
          auto clipped = out_submesh.typedVar<halfedge_ptr_t>(he_ab, "clipped_edge");

          auto inserted      = std::make_shared<HalfEdge>();
          inserted->_vertexA = _F2B_EDGE->_vertexB;
          inserted->_vertexB = clipped->_vertexA;
          frontmesh_edges.push_back(inserted);
          frontmesh_edges.push_back(clipped);
          _F2B_EDGE = nullptr;
          break;
        }
        case EPlanarStatus::NONE: {
          if(match_poly)logchan_clip->log("  none");
          break;
        }
        default:
          OrkAssert(false);
          break;
      }
    } // if do_front
  }   // for (int iva = 0; iva < inuminverts; iva++) {

  if (do_front and (frontmesh_edges.size() >= 3)) {
    vertex_vect_t frontmesh_vertices;
    int inumfrontedges = frontmesh_edges.size();
    for (int ife = 0; ife < inumfrontedges; ife++) {
      auto he  = frontmesh_edges[ife];
      if(match_poly)logchan_clip->log("  EMIT EDGE<%p %d->%d>", (void*) he.get(), he->_vertexA->_poolindex, he->_vertexB->_poolindex );
      auto vtx = he->_vertexA;

      frontmesh_vertices.push_back(vtx);
      bool last_edge = (ife == inumfrontedges - 1);
      if (last_edge) {
        auto first_vtx = frontmesh_vertices.front();
        OrkAssert(he->_vertexB == first_vtx);
      }
    }
    out_submesh.mergePoly(frontmesh_vertices);
  }
}

///////////////////////////////////////////////////////////////////////////////

void SubMeshClipper::closeSubMesh(bool do_front) {
  return;
  auto& out_submesh  = do_front ? _outsubmesh_front : _outsubmesh_back;
  auto& planar_verts = do_front ? _planar_verts_front : _planar_verts_back;

  if (debug) {
    out_submesh.visitAllVertices([&](vertex_ptr_t v) { //
      double point_distance = _slicing_plane.pointDistance(v->mPos);
      logchan_clip->log(
          "outv%d : %f %f %f point_distance<%f>", v->_poolindex, v->mPos.x, v->mPos.y, v->mPos.z, point_distance);
    });
    out_submesh.dumpPolys("preclose");
  }

  if (do_front) {

    _backpolys.visit([&](poly_const_ptr_t back_poly) {
      int num_clipped = 0;
      int num_v       = back_poly->numVertices();

      logchan_clip->log_begin("BACKPOLYVISIT INPUT POLY[");

      for (int iva = 0; iva < num_v; iva++) {
        int ivb                     = (iva + 1) % num_v;
        auto inp_vtx_a              = back_poly->vertex(iva);
        auto inp_vtx_b              = back_poly->vertex(ivb);
        auto he                     = out_submesh.mergeEdgeForVertices(inp_vtx_a, inp_vtx_b);
        const auto& he_plane_status = out_submesh.typedVar<PlanarStatus>(he, "plstatus");
        bool has_clipped            = out_submesh.hasVar(inp_vtx_a, "clipped_vertex");
        if (has_clipped) {
          num_clipped++;
          logchan_clip->log_continue(" <%d>", inp_vtx_a->_poolindex);
        } else {
          bool is_back = out_submesh.tryVarAs<bool>(inp_vtx_a, "back_vertex").value();
          if (is_back) {
            logchan_clip->log_continue(" (%d)", inp_vtx_a->_poolindex);

          } else {
            logchan_clip->log_continue(" %d", inp_vtx_a->_poolindex);
          }
        }
        if (he_plane_status._status == EPlanarStatus::BACK) {
          logchan_clip->log_continue("!");
        }
        if (auto try_clipped_edge = out_submesh.tryVarAs<halfedge_ptr_t>(he, "clipped_edge")) {
          auto clipped_edge = try_clipped_edge.value();
          logchan_clip->log_continue("$");
          _vertex_remap[he->_vertexA->_poolindex] = clipped_edge->_vertexA->_poolindex;
          _vertex_remap[he->_vertexB->_poolindex] = clipped_edge->_vertexB->_poolindex;
        }
      }

      logchan_clip->log_continue(" ] num_v<%d> num_clipped<%d>\n", num_v, num_clipped);

      if (_vertex_remap.size()) {
        logchan_clip->log_continue("_vertex_remap [");
        for (auto item : _vertex_remap) {
          logchan_clip->log_continue(" %d->%d", item.first, item.second);
        }
        logchan_clip->log_continue(" ]\n");
        std::vector<vertex_ptr_t> remapped_verts;
        back_poly->visitVertices([&](vertex_ptr_t vtx) {
          auto it = _vertex_remap.find(vtx->_poolindex);
          if (it != _vertex_remap.end()) {
            auto remapped_vtx = out_submesh.vertex(it->second);
            remapped_verts.push_back(remapped_vtx);
          } else {
            remapped_verts.push_back(vtx);
          }
        });
        auto front_poly = out_submesh.mergePoly(remapped_verts);
      }
      if (num_clipped > 1 and num_clipped == num_v) {
        // all vertices are clipped, so this poly is now a planar poly
        // and shall be added to the front mesh (with orientation flipped)
        vertex_vect_t front_verts;
        back_poly->visitVertices([&](vertex_ptr_t vtx) {

        });
        // auto front_poly = out_submesh.mergePoly(front_verts);
        // OrkAssert(false);
      }
    });
  }

  /////////////////////////////////////////
  //  take note of edges which lie on the
  //  slicing plane
  /////////////////////////////////////////

  edge_set_t all_edges;
  out_submesh.visitAllPolys([&](poly_ptr_t poly) { //
    poly->visitEdges([&](edge_ptr_t e) {           //
      all_edges.insert(e);
    });
  });

  if (debug)
    for (auto e_item : all_edges._the_map) {
      auto e = e_item.second;
      logchan_clip->log("all e[%d %d]", e->_vertexA->_poolindex, e->_vertexB->_poolindex);
    }

  edge_set_t planar_edges;

  int index = 0;
  planar_verts.visit([&](vertex_ptr_t v) {
    if (debug)
      logchan_clip->log("planar v%d : %f %f %f", v->_poolindex, v->mPos.x, v->mPos.y, v->mPos.z);
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
    _linker._name = out_submesh.name;
    for (auto edge_item : planar_edges._the_map) {
      auto edge = edge_item.second;
      _linker.add_edge(edge);
    }
    _linker.link();

    // create a new polygon for each edge loop

    dvec3 centroid = out_submesh.centerOfVertices();

    auto do_chain = [&](edge_chain_ptr_t chain) { //
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
      out_submesh.mergePoly(p1);

    };

    for (auto loop : _linker._edge_loops) {
      if (debug) {
        logchan_clip->log_continue("loop [");
        for (auto e : loop->_edges) {
          logchan_clip->log_continue(" <%d %d>", e->_vertexA->_poolindex, e->_vertexB->_poolindex);
        }
        logchan_clip->log_continue("]\n");
      }
      do_chain(loop);
    }

    for (auto chain : _linker._edge_chains) {
      if (debug) {
        logchan_clip->log_continue("chain [");
        for (auto e : chain->_edges) {
          logchan_clip->log_continue(" <%d %d>", e->_vertexA->_poolindex, e->_vertexB->_poolindex);
        }
        logchan_clip->log_continue("]\n");
      }
      // do_chain(chain);
    }
  } // if (planar_edges.size()) {
}

///////////////////////////////////////////////////////////////////////////////

vertex_set_t SubMeshClipper::addWholePoly(
    std::string hdr,                  //
    merged_poly_const_ptr_t src_poly, //
    submesh& dest) {                  //

  std::vector<vertex_ptr_t> new_verts;
  vertex_set_t added;
  src_poly->visitVertices([&](vertex_ptr_t v) {
    OrkAssert(v);
    auto newv = dest.mergeVertex(*v);
    new_verts.push_back(newv);
    added.insert(newv);
  });
  dest.mergePoly(Polygon(new_verts));
  if (matchTestPoly(src_poly)) {

    logchan_clip->log_continue("<%s> add whole poly: [", hdr.c_str());
    src_poly->visitVertices([&](vertex_ptr_t v) { logchan_clip->log_continue("v<%d> ", v->_poolindex); });
    logchan_clip->log_continue("]\n");
  }
  return added;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void submeshClipWithPlane(
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

  SubMeshClipper sm_clipper(inpsubmesh, slicing_plane, outsmesh_front, outsmesh_back, close_mesh);

  if (debug) {
    outsmesh_front.dumpPolys("clipped_front");
    if (sm_clipper._planar_verts_front._the_map.size() > 0) {
      logchan_clip->log_continue("fpv [");
      for (auto v_item : sm_clipper._planar_verts_front._the_map) {
        auto v = v_item.second;
        logchan_clip->log_continue(" %d", v->_poolindex);
      }
      logchan_clip->log_continue("]\n");
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
