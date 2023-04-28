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

static constexpr double PLANE_EPSILON = 0.001f;

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_clip = logger()->createChannel("meshutil.clipper", fvec3(.9, .9, 1), true);

const std::unordered_set<int> test_verts = {0, 1, 2, 3};

bool matchTestPoly(merged_poly_const_ptr_t src_poly) {
  bool match_poly = true;
  src_poly->visitVertices([&](vertex_ptr_t v) {
    if (test_verts.find(v->_poolindex) == test_verts.end())
      match_poly = false;
  });
  return match_poly;
}

///////////////////////////////////////////////////////////////////////////////

struct PolyVtxCount {
  int _front_count  = 0;
  int _back_count   = 0;
  int _planar_count = 0;
};

///////////////////////////////////////////////////////////////////////////////

using vtx_heio_t = std::multimap<int, halfedge_ptr_t>;

enum class EPlanarStatus { CROSS_F2B, CROSS_B2F, PLANAR, FRONT, BACK, NONE };

struct PlanarStatus {
  vertex_ptr_t _vertexA;
  vertex_ptr_t _vertexB;
  EPlanarStatus _status;
};

struct EdgeChangeRecord {
  vertex_ptr_t _orig_vertexA;
  vertex_ptr_t _orig_vertexB;
  vertex_ptr_t _new_vertexA;
  vertex_ptr_t _new_vertexB;
};
using edge_change_record_ptr_t = std::shared_ptr<EdgeChangeRecord>;

///////////////////////////////////////////////////////////////////////////////

struct SubMeshClipper {
  /////////////////////////////////////
  SubMeshClipper(
      const submesh& inpsubmesh, //
      const dplane3& plane,      //
      submesh& smfront,          //
      bool do_close,             //
      bool debug)                //
      : _inpsubmesh(inpsubmesh)  //
      , _outsubmesh(smfront)     //
      , _slicing_plane(plane)    //
      , _do_close(do_close)      //
      , _debug(debug) {          //
    process();
  }
  /////////////////////////////////////
  void process();
  void procEdges(merged_poly_const_ptr_t input_poly);
  void postProcEdges(merged_poly_const_ptr_t input_poly);
  void clipPolygon(merged_poly_const_ptr_t input_poly);
  void closeSubMesh();
  /////////////////////////////////////
  void dumpEdgeVars(halfedge_ptr_t input_edge) const;
  void dumpPolyVars(merged_poly_const_ptr_t input_poly) const;
  void dumpVertexVars(vertex_const_ptr_t input_vtx) const;
  void printPoly(const std::string& HDR, merged_poly_const_ptr_t input_poly) const;
  /////////////////////////////////////
  vertex_ptr_t remappedVertex(vertex_ptr_t input_vtx, halfedge_ptr_t con_edge) const;
  halfedge_ptr_t remappedEdge(halfedge_ptr_t input_edge) const;
  int f2bindex(merged_poly_const_ptr_t input_poly) const;
  /////////////////////////////////////
  vertex_set_t addWholePoly(
      std::string hdr,                  //
      merged_poly_const_ptr_t src_poly, //
      submesh& dest);
  /////////////////////////////////////
  vertex_ptr_t mergeVertex(const vertex& input_vertex);
  EPlanarStatus categorizeVertex(const vertex& input_vertex) const;
  PolyVtxCount categorizePolygon(merged_poly_const_ptr_t input_poly) const;

  /////////////////////////////////////
  const submesh& _inpsubmesh;
  submesh& _outsubmesh;
  dplane3 _slicing_plane;
  bool _do_close;
  bool _debug;
  /////////////////////////////////////
  vertex_set_t _planar_verts_pending_close;
  polyconst_set_t _backpolys;
  polyconst_set_t _frontpolys;
  std::map<int, int> _vertex_remap;
  std::vector<merged_poly_const_ptr_t> _polys_to_clip;
  std::unordered_map<merged_poly_const_ptr_t, varmap::VarMap> _inp_poly_varmap;
};

void SubMeshClipper::printPoly(const std::string& HDR, merged_poly_const_ptr_t input_poly) const {
  if (_debug) {
    logchan_clip->log_begin("%s<%d>[", HDR.c_str(), input_poly->_submeshIndex);
    input_poly->visitVertices([&](vertex_ptr_t vtx) { logchan_clip->log_continue(" %d", vtx->_poolindex); });
    logchan_clip->log_continue(" ]\n");
  }
}

///////////////////////////////////////////////////////////////////////////////

int SubMeshClipper::f2bindex(merged_poly_const_ptr_t input_poly) const {
  int _f2b_index  = 0;
  bool match_poly = matchTestPoly(input_poly);
  auto it         = _inp_poly_varmap.find(input_poly);
  if (it != _inp_poly_varmap.end()) {
    if (auto try_f2b_index = it->second.typedValueForKey<int>("f2b_index")) {
      _f2b_index = try_f2b_index.value();
      if (_f2b_index < 0)
        _f2b_index = 0;
      if (match_poly)
        logchan_clip->log("_f2b_index<%d>", _f2b_index);
    }
  }
  return _f2b_index;
}

///////////////////////////////////////////////////////////////////////////////

void SubMeshClipper::dumpEdgeVars(halfedge_ptr_t input_edge) const {
  auto& varmap = _outsubmesh.varmapForHalfEdge(input_edge);
  if (varmap._themap.size()) {
    printf("edge[%d->%d] vars:\n", input_edge->_vertexA->_poolindex, input_edge->_vertexB->_poolindex);
    for (auto item : varmap._themap) {
      auto key     = item.first;
      auto val_str = varmap.encodeAsString(key);
      printf("  k: %s v: %s\n", key.c_str(), val_str.c_str());
    }
  }
}
void SubMeshClipper::dumpPolyVars(merged_poly_const_ptr_t input_poly) const {
  auto& varmap = _outsubmesh.varmapForPolygon(input_poly);
  printf("poly[%d] vars:\n", input_poly->_submeshIndex);
  for (auto item : varmap._themap) {
    auto key     = item.first;
    auto val_str = varmap.encodeAsString(key);
    printf("  k: %s v: %s\n", key.c_str(), val_str.c_str());
  }
}
void SubMeshClipper::dumpVertexVars(vertex_const_ptr_t input_vtx) const {
  auto& varmap = _outsubmesh.varmapForVertex(input_vtx);
  printf("vtx[%d] vars: ", input_vtx->_poolindex);
  if (varmap._themap.size()) {
    printf("[\n");
    for (auto item : varmap._themap) {
      auto key = item.first;
      printf("key<%s>\n", key.c_str());
      auto val_str = varmap.encodeAsString(key);
      printf("  k: %s v: %s\n", key.c_str(), val_str.c_str());
    }
    printf("]\n");
  } else {
    printf("[] \n");
  }
}

///////////////////////////////////////////////////////////////////////////////

vertex_ptr_t SubMeshClipper::remappedVertex(vertex_ptr_t input_vtx, halfedge_ptr_t con_edge) const {
  vertex_ptr_t rval = input_vtx;
  auto try_clipped  = _outsubmesh.tryVarAs<vertex_ptr_t>(rval, "clipped_vertex");
  if (try_clipped) {
    rval = try_clipped.value();
  }
  if (auto as_heio_map = _outsubmesh.tryVarAs<vtx_heio_t>(input_vtx, "heio")) {
    auto& heio_map = as_heio_map.value();
    // printf( "have heio_map for vtx<%d> and conedge<%d->%d>\n", input_vtx->_poolindex, con_edge->_vertexA->_poolindex,
    // con_edge->_vertexB->_poolindex );
    bool found = false;
    for (auto item : heio_map) {
      auto item_edge = item.second;
      if (item_edge->_vertexA == con_edge->_vertexA) {
        OrkAssert(con_edge->_vertexB == input_vtx);
        rval = item_edge->_vertexB;
        // printf( "remapped <%d> to <%d>\n", input_vtx->_poolindex, rval->_poolindex );
        found = true;
      } else if (item_edge->_vertexA == con_edge->_vertexB) {
        OrkAssert(con_edge->_vertexA == input_vtx);
        rval = item_edge->_vertexB;
        // printf( "remapped <%d> to <%d>\n", input_vtx->_poolindex, rval->_poolindex );
        found = true;
      }
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

halfedge_ptr_t SubMeshClipper::remappedEdge(halfedge_ptr_t input_edge) const {
  return input_edge;
}

///////////////////////////////////////////////////////////////////////////////

vertex_ptr_t SubMeshClipper::mergeVertex(const vertex& input_vertex) {
  auto cat                                         = categorizeVertex(input_vertex);
  auto merged                                      = _outsubmesh.mergeVertex(input_vertex);
  double d                                         = _slicing_plane.pointDistance(merged->mPos);
  _outsubmesh.mergeVar<double>(merged, "distance") = d;

  switch (cat) {
    case EPlanarStatus::FRONT:
      _outsubmesh.mergeVar<std::string>(merged, "plside") = "FRONT";
      break;
    case EPlanarStatus::BACK:
      _outsubmesh.mergeVar<std::string>(merged, "plside") = "BACK";
      break;
    case EPlanarStatus::PLANAR:
      _outsubmesh.mergeVar<std::string>(merged, "plside") = "PLANAR";
      _planar_verts_pending_close.insert(merged);
      break;
    default:
      break;
  }

  return merged;
}

///////////////////////////////////////////////////////////////////////////////

EPlanarStatus SubMeshClipper::categorizeVertex(const vertex& input_vertex) const {
  EPlanarStatus rval    = EPlanarStatus::NONE;
  double point_distance = _slicing_plane.pointDistance(input_vertex.mPos);
  if (abs(point_distance) < PLANE_EPSILON) {
    rval = EPlanarStatus::PLANAR;
  } else if (point_distance >= 0.0) {
    rval = EPlanarStatus::FRONT;
  } else {
    rval = EPlanarStatus::BACK;
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

PolyVtxCount SubMeshClipper::categorizePolygon(merged_poly_const_ptr_t input_poly) const {
  PolyVtxCount counts;
  input_poly->visitVertices([&](vertex_ptr_t vtx) {
    EPlanarStatus cat = categorizeVertex(*vtx);
    switch (cat) {
      case EPlanarStatus::FRONT:
        counts._front_count++;
        break;
      case EPlanarStatus::BACK:
        counts._back_count++;
        break;
      case EPlanarStatus::PLANAR:
        counts._planar_count++;
        break;
      default:
        OrkAssert(false);
        break;
    }
  });
  return counts;
}

///////////////////////////////////////////////////////////////////////////////

void SubMeshClipper::process() {

  /////////////////////////////////////////////////////////////////////
  // merge all input vertices (for ease of debugging due to ID matching)
  /////////////////////////////////////////////////////////////////////

  _inpsubmesh.visitAllVertices([&](vertex_const_ptr_t input_vertex) { //
    mergeVertex(*input_vertex);
  });

  /////////////////////////////////////////////////////////////////////
  // input mesh polygon loop
  /////////////////////////////////////////////////////////////////////

  int ip = 0;
  _inpsubmesh.visitAllPolys([&](merged_poly_const_ptr_t input_poly) {
    bool match_poly = matchTestPoly(input_poly);

    int numverts    = input_poly->numVertices();
    auto polyvtxcnt = categorizePolygon(input_poly);
    if (_debug)
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
      addWholePoly("A:", input_poly, _outsubmesh);
      _frontpolys.insert(input_poly);
    }

    //////////////////////////////////////////////
    // all of this poly's vertices in back ? -> trivially route to _outsubmesh_back
    //////////////////////////////////////////////

    else if (numverts == polyvtxcnt._back_count) { // all back ?
      // addWholePoly("B: ", input_poly, _outsubmesh_back);
      //  TODO when closing the mesh, construct the closing face
      //  with the planar vertices and input edge connectivity info
      //  every input edge should have a matching output edge (which was clipped)
      if (_debug)
        logchan_clip->log_begin("BACK POLY[");
      std::vector<vertex_ptr_t> back_vertices;
      input_poly->visitVertices([&](vertex_ptr_t vtx) {
        auto v_m                                       = _outsubmesh.mergeVertex(*vtx);
        _outsubmesh.mergeVar<bool>(v_m, "back_vertex") = true;
        back_vertices.push_back(v_m);
        if (_debug)
          logchan_clip->log_continue(" %d", v_m->_poolindex);
      });
      if (_debug)
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
  // categorize all edges
  ////////////////////////////////////////////////////////////////////

  if (_debug)
    logchan_clip->log("## PROC EDGES #########################");
  _inpsubmesh.visitAllPolys([&](merged_poly_const_ptr_t input_poly) { //
    this->procEdges(input_poly);
  });

  ////////////////////////////////////////////////////////////////////

  _inpsubmesh.visitAllPolys([&](merged_poly_const_ptr_t input_poly) { //
    this->postProcEdges(input_poly);
  });

  ////////////////////////////////////////////////////////////////////

  if (_debug) { // vardump

    logchan_clip->log("## EDGE VAR MAP #########################");
    _outsubmesh.visitAllEdges([&](halfedge_ptr_t e) { dumpEdgeVars(e); });
    logchan_clip->log("## VTX VAR MAP #########################");
    _outsubmesh.visitAllVertices([&](vertex_ptr_t vtx) { dumpVertexVars(vtx); });
    logchan_clip->log("## VTX REMAP TABLE #########################");

    for (auto item : _vertex_remap) {
      logchan_clip->log("vtx<%d> -> vtx<%d>", item.first, item.second);
    }

    logchan_clip->log("###################################");
  }

  ////////////////////////////////////////////////////////////////////
  // now that all input polys have been categorized, and the database
  //  constructed, we can clip
  ////////////////////////////////////////////////////////////////////

  for (auto input_poly : _polys_to_clip) {
    this->clipPolygon(input_poly);
  }

  ////////////////////////////////////////////////////////////////////
  // close output submeshes
  ////////////////////////////////////////////////////////////////////

  if (_do_close) { //
    closeSubMesh();
  }
}

///////////////////////////////////////////////////////////////////////////////

void SubMeshClipper::procEdges(merged_poly_const_ptr_t input_poly) { //
  const int inuminverts = input_poly->numVertices();
  OrkAssert(inuminverts >= 3);
  static halfedge_ptr_t _F2B_EDGE = nullptr;
  int _f2b_count                  = 0;
  int _f2b_index                  = -1;
  int _b2f_count                  = 0;
  bool do_log                     = _debug; // matchTestPoly(input_poly);
  printPoly("PROCEDGES POLY", input_poly);

  if (do_log)
    logchan_clip->log("  procEdges inppoly<%d> numv<%d>", input_poly->_submeshIndex, inuminverts);

  for (int iva = 0; iva < inuminverts; iva++) {

    int ivb        = (iva + 1) % inuminverts;
    auto out_vtx_a = _outsubmesh.mergeVertex(*input_poly->vertex(iva));
    auto out_vtx_b = _outsubmesh.mergeVertex(*input_poly->vertex(ivb));
    auto he_ab     = _outsubmesh.mergeEdgeForVertices(out_vtx_a, out_vtx_b);
    // get the side of each vert to the plane
    bool is_vertex_a_front = _slicing_plane.isPointInFront(out_vtx_a->mPos);
    bool is_vertex_b_front = _slicing_plane.isPointInFront(out_vtx_b->mPos);
    if (do_log)
      logchan_clip->log(
          "   iva<%d> edge<%d->%d> front<%d,%d>",
          iva,
          out_vtx_a->_poolindex,
          out_vtx_b->_poolindex,
          int(is_vertex_a_front),
          int(is_vertex_b_front));
    auto& plstat    = _outsubmesh.mergeVar<PlanarStatus>(he_ab, "plstatus");
    plstat._status  = EPlanarStatus::NONE;
    plstat._vertexA = out_vtx_a;
    plstat._vertexB = out_vtx_b;
    if (is_vertex_a_front and is_vertex_b_front) {
      plstat._status  = EPlanarStatus::FRONT;
      plstat._vertexA = out_vtx_a;
      plstat._vertexB = out_vtx_b;
      if (do_log)
        logchan_clip->log("    emit front he_ab<%p>", (void*)he_ab.get());
    } else if ((not is_vertex_a_front) and (not is_vertex_b_front)) {
      plstat._status  = EPlanarStatus::BACK;
      plstat._vertexA = out_vtx_a;
      plstat._vertexB = out_vtx_b;
      if (do_log)
        logchan_clip->log("    emit back he_ab<%p>", (void*)he_ab.get());
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
      if (do_log)
        logchan_clip->log("    does_intersectAB<%d>", int(does_intersect));
      if (does_intersect) {
        double fDist   = (out_vtx_a->mPos - out_vtx_b->mPos).magnitude();
        double fDist2  = (out_vtx_a->mPos - vPos).magnitude();
        double fScalar = (abs(fDist) < PLANE_EPSILON) ? 0.0 : fDist2 / fDist;
        LerpedVertex.lerp(out_vtx_a->mPos, out_vtx_b->mPos, fScalar);
      } else {
        dray3 lsegba(out_vtx_b->mPos + n_ab * 1000.0, -n_ab);
        does_intersect = _slicing_plane.Intersect(lsegba, isectdist, vPos);
        if (do_log)
          logchan_clip->log("    does_intersectBA<%d>", int(does_intersect));
        double fDist   = (out_vtx_b->mPos - out_vtx_a->mPos).magnitude();
        double fDist2  = (out_vtx_b->mPos - vPos).magnitude();
        double fScalar = (abs(fDist) < PLANE_EPSILON) ? 0.0 : fDist2 / fDist;
        LerpedVertex.lerp(out_vtx_b->mPos, out_vtx_a->mPos, fScalar);
      }
      if (does_intersect) {
        vertex smvert;
        smvert.mPos       = LerpedVertex;
        auto out_vtx_lerp = mergeVertex(smvert);
        //////////////////////
        if (front_to_back) {
          plstat._status  = EPlanarStatus::CROSS_F2B;
          plstat._vertexA = out_vtx_a;
          plstat._vertexB = out_vtx_lerp;
          if (do_log) {
            logchan_clip->log(
                "    emit CROSS F2B %d->%d: %d he_ab<%p>",
                out_vtx_a->_poolindex,
                out_vtx_b->_poolindex,
                out_vtx_lerp->_poolindex,
                (void*)he_ab.get());
          }
          _outsubmesh.mergeVar<vertex_ptr_t>(out_vtx_b, "clipped_vertex") = out_vtx_lerp;
          halfedge_ptr_t he_xx = _outsubmesh.mergeEdgeForVertices(out_vtx_a, out_vtx_lerp);
          auto hepair          = std::make_pair(out_vtx_b->_poolindex, he_xx);
          _outsubmesh.mergeVar<vtx_heio_t>(out_vtx_b, "heio").insert(hepair);

          _F2B_EDGE = he_ab;
          _f2b_count++;
          _f2b_index = iva;

        } else if (back_to_front) {
          plstat._status  = EPlanarStatus::CROSS_B2F;
          plstat._vertexA = out_vtx_lerp;
          plstat._vertexB = out_vtx_b;
          if (do_log) {
            logchan_clip->log(
                "    emit CROSS B2F %d->%d : %d he_ab<%p>",
                out_vtx_b->_poolindex,
                out_vtx_a->_poolindex,
                out_vtx_lerp->_poolindex,
                (void*)he_ab.get());
          }
          _outsubmesh.mergeVar<vertex_ptr_t>(out_vtx_a, "clipped_vertex") = out_vtx_lerp;
          halfedge_ptr_t he_xx = _outsubmesh.mergeEdgeForVertices(out_vtx_lerp, out_vtx_b);
          auto hepair          = std::make_pair(out_vtx_a->_poolindex, he_xx);
          _outsubmesh.mergeVar<vtx_heio_t>(out_vtx_b, "heio").insert(hepair);
          _b2f_count++;
          _F2B_EDGE = nullptr;
        } else {
          OrkAssert(false);
        }
      } // isect1 ?
      else {
        OrkAssert(false); // crossed the plane, but non intersecting ?
      }
    } // did we cross plane ?
  }   // for (int iva = 0; iva < inuminverts; iva++) {
  OrkAssert(_b2f_count == _f2b_count);
  if (_F2B_EDGE != nullptr) {
    _inp_poly_varmap[input_poly].makeValueForKey<int>("f2b_index") = _f2b_index;
  }
}

///////////////////////////////////////////////////////////////////////////////

void SubMeshClipper::postProcEdges(merged_poly_const_ptr_t input_poly) { //
}

///////////////////////////////////////////////////////////////////////////////

void SubMeshClipper::clipPolygon(merged_poly_const_ptr_t input_poly) { //

  const int inuminverts = input_poly->numVertices();
  OrkAssert(inuminverts >= 3);

  bool match_poly = matchTestPoly(input_poly);
  if (match_poly) {
    printPoly("CLIP INPUT POLY", input_poly);
    std::vector<vertex_const_ptr_t> out_verts;
    input_poly->visitVertices([&](vertex_const_ptr_t vtx) {
      auto mergedv = _outsubmesh.vertex(vtx->_poolindex);
      out_verts.push_back(mergedv);
    });
    if (_debug)
      logchan_clip->log("clip poly num verts<%d>", inuminverts);
    for (auto mergedv : out_verts) {
      dumpVertexVars(mergedv);
    }
  }

  auto polycat = categorizePolygon(input_poly);

  //////////////////////////////////////////////////////////
  // handle all-front polygons
  //////////////////////////////////////////////////////////

  if (polycat._back_count == 0 and polycat._planar_count == 0) {
    if (polycat._front_count >= 3) {
      _outsubmesh.mergePoly(*input_poly);
      return;
    }
  }

  //////////////////////////////////////////////////////////
  // handle intersecting polygons
  //////////////////////////////////////////////////////////

  int _f2b_index = f2bindex(input_poly);

  vertex_vect_t frontmesh_vertices;

  // loop around the input polygon's edges
  halfedge_ptr_t _F2B_EDGE = nullptr;
  for (int i = 0; i < inuminverts; i++) {
    int iva = (i + _f2b_index) % inuminverts;
    int ivb = (iva + 1) % inuminverts;

    auto out_vtx_a = _outsubmesh.mergeVertex(*input_poly->vertex(iva));
    auto out_vtx_b = _outsubmesh.mergeVertex(*input_poly->vertex(ivb));
    auto he_ab     = _outsubmesh.mergeEdgeForVertices(out_vtx_a, out_vtx_b);

    if (match_poly and _debug) {
      int aindex = out_vtx_a->_poolindex;
      int bindex = out_vtx_b->_poolindex;
      if (_debug)
        logchan_clip->log_continue("  i<%d> iva<%d> ivb<%d> : ", i, aindex, bindex);
    }

    auto& plstat = _outsubmesh.typedVar<PlanarStatus>(he_ab, "plstatus");
    switch (plstat._status) {
      case EPlanarStatus::FRONT:
        if (match_poly) {
          if (_debug)
            logchan_clip->log_continue("  front\n");
        }
        frontmesh_vertices.push_back(out_vtx_a);
        frontmesh_vertices.push_back(out_vtx_b);
        break;
      case EPlanarStatus::BACK:
        // OrkAssert(false);
        if (match_poly and _debug)
          logchan_clip->log_continue("  back\n");
        break;
      case EPlanarStatus::CROSS_F2B: {
        if (match_poly and _debug) {
          logchan_clip->log_continue("  front2back\n");
        }
        OrkAssert(_F2B_EDGE == nullptr);
        frontmesh_vertices.push_back(out_vtx_a);
        frontmesh_vertices.push_back(remappedVertex(out_vtx_b, he_ab));
        _F2B_EDGE = he_ab;
        break;
      }
      case EPlanarStatus::CROSS_B2F: {
        if (match_poly and _debug) {
          logchan_clip->log_continue("  back2front\n");
        }
        OrkAssert(_F2B_EDGE != nullptr);
        frontmesh_vertices.push_back(remappedVertex(out_vtx_a, he_ab));
        frontmesh_vertices.push_back(out_vtx_b);
        _F2B_EDGE = nullptr;
        break;
      }
      case EPlanarStatus::NONE: {
        if (match_poly and _debug)
          logchan_clip->log("  none");
        break;
      }
      default:
        OrkAssert(false);
        break;
    }
  } // for (int iva = 0; iva < inuminverts; iva++) {

  //////////////////////////////////////////////////////////////////////////////////////

  if (_debug)
    logchan_clip->log("  frontmesh_vertices.size<%d>", frontmesh_vertices.size());
  vertex_vect_t frontmesh_vertices_nonrepeat;
  vertex_ptr_t prev = nullptr;
  for (int iv = 0; iv < frontmesh_vertices.size(); iv++) {
    auto v0 = frontmesh_vertices[iv];
    if (v0 != prev) {
      frontmesh_vertices_nonrepeat.push_back(v0);
    }
    prev = v0;
  }

  if (_debug) {
    logchan_clip->log("  frontmesh_vertices_nonrepeat.size<%d>", frontmesh_vertices_nonrepeat.size());
    for (auto item : frontmesh_vertices_nonrepeat) {
      if (match_poly)
        logchan_clip->log("  frontmesh_vertices_nonrepeat<%d>", item->_poolindex);
    }
  }

  //////////////////////////////////////////////////////////////////////////////////////
  if (frontmesh_vertices_nonrepeat.size() >= 3) {
    _outsubmesh.mergePoly(frontmesh_vertices_nonrepeat);
  }
}

///////////////////////////////////////////////////////////////////////////////

void SubMeshClipper::closeSubMesh() {
  // return;                                              //
  if (_debug) {
    _outsubmesh.visitAllVertices([&](vertex_ptr_t v) { //
      double point_distance = _slicing_plane.pointDistance(v->mPos);
      logchan_clip->log("outv%d : %f %f %f point_distance<%f>", v->_poolindex, v->mPos.x, v->mPos.y, v->mPos.z, point_distance);
    });
    _outsubmesh.dumpPolys("preclose");
  }

  _backpolys.visit([&](poly_const_ptr_t back_poly) {
    int num_clipped = 0;
    int num_v       = back_poly->numVertices();

    if (_debug)
      logchan_clip->log_begin("BACKPOLYVISIT INPUT POLY[");

    for (int iva = 0; iva < num_v; iva++) {
      int ivb                     = (iva + 1) % num_v;
      auto inp_vtx_a              = back_poly->vertex(iva);
      auto inp_vtx_b              = back_poly->vertex(ivb);
      auto he                     = _outsubmesh.mergeEdgeForVertices(inp_vtx_a, inp_vtx_b);
      const auto& he_plane_status = _outsubmesh.typedVar<PlanarStatus>(he, "plstatus");
      bool has_clipped            = _outsubmesh.hasVar(inp_vtx_a, "clipped_vertex");
      if (has_clipped) {
        num_clipped++;
        if (_debug)
          logchan_clip->log_continue(" <%d>", inp_vtx_a->_poolindex);
      } else {
        bool is_back = _outsubmesh.tryVarAs<bool>(inp_vtx_a, "back_vertex").value();
        if (is_back) {
          if (_debug)
            logchan_clip->log_continue(" (%d)", inp_vtx_a->_poolindex);

        } else {
          if (_debug)
            logchan_clip->log_continue(" %d", inp_vtx_a->_poolindex);
        }
      }
      if (he_plane_status._status == EPlanarStatus::BACK) {
        if (_debug)
          logchan_clip->log_continue("!");
      }
      if (auto try_clipped_edge = _outsubmesh.tryVarAs<halfedge_ptr_t>(he, "clipped_edge")) {
        auto clipped_edge = try_clipped_edge.value();
        if (_debug)
          logchan_clip->log_continue("$");
        _vertex_remap[he->_vertexA->_poolindex] = clipped_edge->_vertexA->_poolindex;
        _vertex_remap[he->_vertexB->_poolindex] = clipped_edge->_vertexB->_poolindex;
      }
    }

    if (_debug)
      logchan_clip->log_continue(" ] num_v<%d> num_clipped<%d>\n", num_v, num_clipped);

    if (_vertex_remap.size()) {
      if (_debug)
        logchan_clip->log_continue("_vertex_remap [");
      for (auto item : _vertex_remap) {
        if (_debug)
          logchan_clip->log_continue(" %d->%d", item.first, item.second);
      }
      if (_debug)
        logchan_clip->log_continue(" ]\n");
      std::vector<vertex_ptr_t> remapped_verts;
      back_poly->visitVertices([&](vertex_ptr_t vtx) {
        auto it = _vertex_remap.find(vtx->_poolindex);
        if (it != _vertex_remap.end()) {
          auto remapped_vtx = _outsubmesh.vertex(it->second);
          remapped_verts.push_back(remapped_vtx);
        } else {
          remapped_verts.push_back(vtx);
        }
      });
      auto front_poly = _outsubmesh.mergePoly(remapped_verts);
    }
    if (num_clipped > 1 and num_clipped == num_v) {
      // all vertices are clipped, so this poly is now a planar poly
      // and shall be added to the front mesh (with orientation flipped)
      vertex_vect_t front_verts;
      back_poly->visitVertices([&](vertex_ptr_t vtx) {

      });
      // auto front_poly = _outsubmesh.mergePoly(front_verts);
      // OrkAssert(false);
    }
  });

  /////////////////////////////////////////
  //  take note of edges which lie on the
  //  slicing plane
  /////////////////////////////////////////

  edge_set_t all_edges;
  _outsubmesh.visitAllPolys([&](poly_ptr_t poly) { //
    poly->visitEdges([&](edge_ptr_t e) {           //
      all_edges.insert(e);
    });
  });

  if (_debug)
    for (auto e_item : all_edges._the_map) {
      auto e = e_item.second;
      logchan_clip->log("all e[%d %d]", e->_vertexA->_poolindex, e->_vertexB->_poolindex);
    }

  int index = 0;
  edge_set_t planar_edges;
  _planar_verts_pending_close.visit([&](vertex_ptr_t v) {
    if (_debug)
      logchan_clip->log("planar v%d : %f %f %f", v->_poolindex, v->mPos.x, v->mPos.y, v->mPos.z);
    _planar_verts_pending_close.visit([&](vertex_ptr_t v2) {
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

  if (_debug) {
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
    _linker._name = _outsubmesh.name;
    for (auto edge_item : planar_edges._the_map) {
      auto edge = edge_item.second;
      _linker.add_edge(edge);
    }
    _linker.link();

    // create a new polygon for each edge loop

    dvec3 submesh_centroid = _outsubmesh.centerOfVertices();

    auto do_chain = [&](edge_chain_ptr_t chain) { //

      if( chain->_edges.size() < 3 )
        return;
        
      auto ordered_x = chain->orderedVertices();
      vertex_vect_t ordered;
      for( auto vtx : ordered_x ) {
        ordered.push_back(_outsubmesh.mergeVertex(*vtx));
      }

      bool flip_polygon = false;

      double planar_deviation = chain->planarDeviation();

      //printf( "chain<%p> planarDeviation<%f>\n", chain.get(), planar_deviation );

      if (false) { // winding order from adjacent polys
        // compute correct winding order via the connectivity of
        //  adjacent polygons

        int forward  = 0;
        int backward = 0;

        for (auto e : chain->_edges) {
          auto poly_set = _outsubmesh.connectedPolys(e, false);
          printf("edge<%d->%d> poly_set<%d>\n", e->_vertexA->_poolindex, e->_vertexB->_poolindex, int(poly_set.size()));
          size_t num_connected_polys = poly_set.size();
          switch (num_connected_polys) {
            case 0: {
              // no connected polys ?
              // this edge is on the boundary of the mesh
              // so we can't do anything with it
              printf("HAVE BOUNDARY EDGE\n");
              break;
            }
            case 1: {
              int ipoly    = *poly_set.begin();
              auto polygon = _outsubmesh.poly(ipoly);
              // find edge in polygon
              polygon->visitEdges([&](edge_ptr_t e_poly) {
                if (e_poly->_vertexA == e->_vertexA and e_poly->_vertexB == e->_vertexB) {
                  // edge is in same direction as polygon
                  forward++;
                } else if (e_poly->_vertexA == e->_vertexB and e_poly->_vertexB == e->_vertexA) {
                  // edge is in opposite direction as polygon
                  backward++;
                }
              });
              break;
            }
            default: {
              printf("HAVE OVERBOOKED EDGE count<%d>!\n", int(num_connected_polys));
              for (auto p : poly_set) {
                printf("poly<%d>\n", p);
              }
              break;
            }
          }
          // auto P = e->_polygon;

        } // for (auto e : chain->_edges) {

        bool all_forward  = (forward == chain->_edges.size());
        bool all_backward = (backward == chain->_edges.size());
        printf("forward %d backward %d\n", forward, backward);
        OrkAssert(all_forward or all_backward);

        if (all_forward) {
          printf("all forward\n");
          // flip polygon
          flip_polygon = true;
        } else if (all_backward) {
          printf("all backward\n");
        }
        if( flip_polygon ){
          std::reverse(ordered.begin(), ordered.end());
        }
        auto P = Polygon(ordered);
        _outsubmesh.mergePoly(P);

      } // if (false) { // winding order from adjacent polys
      else{
        // compute correct winding order via the centroid of the polygon
        //  and the centroid of the vertices of the polygon

        printf( "planar_deviation<%g>\n", planar_deviation );

        if( planar_deviation < 0.0001 ){

          dvec3 poly_centroid = chain->centroid();
          dvec3 poly_normal   = chain->avgNormalOfFaces();

          dvec3 poly_to_centroid = (poly_centroid - submesh_centroid).normalized();

          double dot = poly_to_centroid.dotWith(poly_normal);

          if(0)printf( "submesh center<%g %g %g>\n", submesh_centroid.x, submesh_centroid.y, submesh_centroid.z );
          if(0)printf( "poly center<%g %g %g>\n", poly_centroid.x, poly_centroid.y, poly_centroid.z );
          if(0)printf( "poly normal<%g %g %g>\n", poly_normal.x, poly_normal.y, poly_normal.z );
          if(0)printf( "poly to center<%g %g %g>\n", poly_to_centroid.x, poly_to_centroid.y, poly_to_centroid.z );
          if(0)printf( "DOT<%g>\n", dot );
          
          if (dot < 0.0) {
            flip_polygon = true;
          }

          if( flip_polygon ){
            std::reverse(ordered.begin(), ordered.end());
          }
          auto P = Polygon(ordered);
          _outsubmesh.mergePoly(P);
        }
        else{ // triangle fan with correct winding order

          vertex center_vertex;
          center_vertex.mPos = chain->centroid();;
          auto center_merged = _outsubmesh.mergeVertex(center_vertex);
          if(0)printf( "center_merged poolindex<%d>\n", center_merged->_poolindex );
          // triangle fan
          for( size_t i=0; i<ordered.size(); i++ ){
            auto va = ordered[i];
            auto vb = ordered[(i+1)%ordered.size()];
            auto vc = ordered[(i+2)%ordered.size()];
            auto dba = (va->mPos - vb->mPos);
            auto dbc = (vc->mPos - vb->mPos);
            auto dbp = (center_merged->mPos - vb->mPos);
            auto cross_0 = dba.crossWith(dbp);
            auto cross_1 = dbp.crossWith(dbc);
            auto dot = cross_0.dotWith(cross_1);

            bool is_ccw = (dot > 0.0);
            bool is_cw = (dot < 0.0);
            bool is_coplanar = (fabs(dot) < 0.00001);

            if( is_coplanar ){
              if(0)printf( "tri<%d %d %d> is_coplanar<%d>\n", //
                      center_merged->_poolindex, //
                      va->_poolindex, //
                      vb->_poolindex, int(is_coplanar) );
            }
            else if( 1 ){
              bool flip = is_cw;
              if( flip ){
                std::swap(va,vb);
              }
              if(0)printf( "tri<%d %d %d> is_ccw<%d>\n", //
                      center_merged->_poolindex, va->_poolindex, vb->_poolindex, int(is_ccw) );

              auto P = Polygon(center_merged,va,vb);
              _outsubmesh.mergePoly(P);

            }

          }        
        }

      }



    }; // auto do_chain = [&](edge_chain_ptr_t chain) { //

    for (auto loop : _linker._edge_loops) {
      if (_debug) {
        logchan_clip->log_continue("loop [");
        for (auto e : loop->_edges) {
          logchan_clip->log_continue(" <%d %d>", e->_vertexA->_poolindex, e->_vertexB->_poolindex);
        }
        logchan_clip->log_continue("]\n");
      }
      do_chain(loop);
    }

    for (auto chain : _linker._edge_chains) {
      if (_debug) {
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

    if (_debug){
      logchan_clip->log_continue("<%s> add whole poly: [", hdr.c_str());
      src_poly->visitVertices([&](vertex_ptr_t v) { //
        logchan_clip->log_continue("v<%d> ", v->_poolindex);
      });
      logchan_clip->log_continue("]\n");
    }
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
    bool debug) {

  if (debug) {
    logchan_clip->log_continue("///////////\n");
    inpsubmesh.dumpPolys("inpsubmesh");
  }

  SubMeshClipper sm_clipper_front(inpsubmesh, slicing_plane, outsmesh_front, close_mesh, debug);

  if (debug) {
    outsmesh_front.dumpPolys("clipped_front");
    if (sm_clipper_front._planar_verts_pending_close._the_map.size() > 0) {
      logchan_clip->log_continue("fpv [");
      for (auto v_item : sm_clipper_front._planar_verts_pending_close._the_map) {
        auto v = v_item.second;
        logchan_clip->log_continue(" %d", v->_poolindex);
      }
      logchan_clip->log_continue("]\n");
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void planar_clip_init() {
  auto ps_type = TypeId::of<PlanarStatus>();
  varmap::VarMap::registerStringEncoder(ps_type, [](const varmap::VarMap::value_type& val) {
    auto& ps = val.template get<PlanarStatus>();
    return CreateFormattedString("plstat[%d : %d->%d]", int(ps._status), ps._vertexA->_poolindex, ps._vertexB->_poolindex);
  });

  auto heio_type = TypeId::of<vtx_heio_t>();
  varmap::VarMap::registerStringEncoder(heio_type, [](const varmap::VarMap::value_type& val) {
    auto& heio       = val.template get<vtx_heio_t>();
    std::string rval = "HEIO[ ";
    for (auto item : heio) {
      auto he = item.second;
      rval += CreateFormattedString("(%d:(%d->%d))", item.first, he->_vertexA->_poolindex, he->_vertexB->_poolindex);
    }
    rval += "]";
    return rval;
  });
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
