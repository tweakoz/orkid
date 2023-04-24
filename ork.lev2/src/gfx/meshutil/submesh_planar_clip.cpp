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

static constexpr double PLANE_EPSILON = 0.001f;

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_clip = logger()->createChannel("meshutil.clipper", fvec3(.9, .9, 1), true);

const std::unordered_set<int> test_verts = {1, 0, 4, 7};

bool matchTestPoly(merged_poly_const_ptr_t src_poly) {
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

using vtx_heio_t = std::map<int, halfedge_ptr_t>;

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
      const submesh& inpsubmesh,                    //
      const dplane3& plane,                         //
      submesh& smfront,                             //
      bool do_close)                                //
      : _inpsubmesh(inpsubmesh)                     //
      , _outsubmesh(smfront)                        //
      , _slicing_plane(plane)                       //
      , _do_close(do_close)                         //
      , _categorized(_inpsubmesh, _slicing_plane) { //
    process();
  }
  /////////////////////////////////////
  void process();
  bool procEdges(merged_poly_const_ptr_t input_poly);
  void postProcEdges(merged_poly_const_ptr_t input_poly);
  void clipPolygon(merged_poly_const_ptr_t input_poly);
  void closeSubMesh();
  /////////////////////////////////////
  void dumpEdgeVars(halfedge_ptr_t input_edge) const;
  void dumpPolyVars(merged_poly_const_ptr_t input_poly) const;
  void dumpVertexVars(vertex_ptr_t input_vtx) const;
  void printPoly(const std::string& HDR, merged_poly_const_ptr_t input_poly) const;
  /////////////////////////////////////
  vertex_ptr_t remappedVertex(vertex_ptr_t input_vtx) const;
  halfedge_ptr_t remappedEdge(halfedge_ptr_t input_edge) const;
  int f2bindex(merged_poly_const_ptr_t input_poly) const;
  /////////////////////////////////////

  vertex_set_t addWholePoly(
      std::string hdr,                  //
      merged_poly_const_ptr_t src_poly, //
      submesh& dest);
  /////////////////////////////////////
  const submesh& _inpsubmesh;
  submesh& _outsubmesh;
  dplane3 _slicing_plane;
  bool _do_close;
  /////////////////////////////////////
  edge_set_t _planar_edges;
  vertex_set_t _planar_verts;
  polyconst_set_t _backpolys;
  polyconst_set_t _frontpolys;
  std::map<int, int> _vertex_remap;
  PlanarVertexCategorize _categorized;
  std::vector<merged_poly_const_ptr_t> _polys_to_clip;
  std::unordered_map<merged_poly_const_ptr_t, varmap::VarMap> _inp_poly_varmap;
};

void SubMeshClipper::printPoly(const std::string& HDR, merged_poly_const_ptr_t input_poly) const {
  logchan_clip->log_begin("%s<%d>[", HDR.c_str(), input_poly->_submeshIndex);
  input_poly->visitVertices([&](vertex_ptr_t vtx) { logchan_clip->log_continue(" %d", vtx->_poolindex); });
  logchan_clip->log_continue(" ]\n");
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
void SubMeshClipper::dumpVertexVars(vertex_ptr_t input_vtx) const {
  auto& varmap = _outsubmesh.varmapForVertex(input_vtx);
  if (varmap._themap.size()) {
    printf("vtx[%d] vars:\n", input_vtx->_poolindex);
    for (auto item : varmap._themap) {
      auto key     = item.first;
      auto val_str = varmap.encodeAsString(key);
      printf("  k: %s v: %s\n", key.c_str(), val_str.c_str());
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

vertex_ptr_t SubMeshClipper::remappedVertex(vertex_ptr_t input_vtx) const {
  vertex_ptr_t rval = input_vtx;
  bool done = false;
  while(not done){
    auto prev = rval;
    auto it           = _vertex_remap.find(rval->_poolindex);
    if (it != _vertex_remap.end()) {
      rval = _outsubmesh.vertex(it->second);
    }
    auto try_clipped = _outsubmesh.tryVarAs<vertex_ptr_t>(rval, "clipped_vertex");
    if (try_clipped) {
      rval = try_clipped.value();
    }
    done = true; //(rval == prev);
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

halfedge_ptr_t SubMeshClipper::remappedEdge(halfedge_ptr_t input_edge) const{
  return input_edge;
}

///////////////////////////////////////////////////////////////////////////////

void SubMeshClipper::process() {

  _inpsubmesh.visitAllVertices([&](vertex_const_ptr_t input_vertex) { auto merged = _outsubmesh.mergeVertex(*input_vertex); });

  /////////////////////////////////////////////////////////////////////
  // categorize all vertices in input mesh
  /////////////////////////////////////////////////////////////////////

  _categorized._back_verts.visit([&](vertex_const_ptr_t vtx) {
    auto m                                       = _outsubmesh.mergeVertex(*vtx);
    _outsubmesh.mergeVar<bool>(m, "back_vertex") = true;
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
      logchan_clip->log_begin("BACK POLY[");
      std::vector<vertex_ptr_t> back_vertices;
      input_poly->visitVertices([&](vertex_ptr_t vtx) {
        auto v_m                                       = _outsubmesh.mergeVertex(*vtx);
        _outsubmesh.mergeVar<bool>(v_m, "back_vertex") = true;
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

  bool done = false;
  int pe_pass = 0;
  while (not done) {
    logchan_clip->log("## PROC EDGES PASS<%d> #########################", pe_pass );
    _inpsubmesh.visitAllPolys([&](merged_poly_const_ptr_t input_poly) { //
      done = this->procEdges(input_poly);
      pe_pass++;
    });
  }
  ////////////////////////////////////////////////////////////////////

  _inpsubmesh.visitAllPolys([&](merged_poly_const_ptr_t input_poly) { //
    this->postProcEdges(input_poly);
  });

  ////////////////////////////////////////////////////////////////////

  if (true) { // vardump

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

bool SubMeshClipper::procEdges(merged_poly_const_ptr_t input_poly) { //
  bool done             = true;
  auto& out_submesh     = _outsubmesh;
  const int inuminverts = input_poly->numVertices();
  OrkAssert(inuminverts >= 3);
  static halfedge_ptr_t _F2B_EDGE = nullptr;
  int _f2b_count                  = 0;
  int _f2b_index                  = -1;
  int _b2f_count                  = 0;
  bool do_log                     = true; // matchTestPoly(input_poly);
  printPoly("PROCEDGES POLY", input_poly);

  if (do_log)
    logchan_clip->log("  procEdges inppoly<%d> numv<%d>", input_poly->_submeshIndex, inuminverts);

  for (int iva = 0; iva < inuminverts; iva++) {

    int ivb        = (iva + 1) % inuminverts;
    auto out_vtx_a = out_submesh.mergeVertex(*input_poly->vertex(iva));
    auto out_vtx_b = out_submesh.mergeVertex(*input_poly->vertex(ivb));
    //out_vtx_a      = remappedVertex(out_vtx_a);
    //out_vtx_b      = remappedVertex(out_vtx_b);
    auto he_ab     = out_submesh.mergeEdgeForVertices(out_vtx_a, out_vtx_b);
    auto he_ba     = out_submesh.mergeEdgeForVertices(out_vtx_b, out_vtx_a);
    // get the side of each vert to the plane
    bool is_vertex_a_front = _slicing_plane.isPointInFront(out_vtx_a->mPos);
    bool is_vertex_b_front = _slicing_plane.isPointInFront(out_vtx_b->mPos);
    logchan_clip->log(
        "   iva<%d> edge<%d->%d> front<%d,%d>",
        iva,
        out_vtx_a->_poolindex,
        out_vtx_b->_poolindex,
        int(is_vertex_a_front),
        int(is_vertex_b_front));
    auto& plstat    = out_submesh.mergeVar<PlanarStatus>(he_ab, "plstatus");
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
      logchan_clip->log("    does_intersectAB<%d>", int(does_intersect));
      if (does_intersect) {
        double fDist   = (out_vtx_a->mPos - out_vtx_b->mPos).magnitude();
        double fDist2  = (out_vtx_a->mPos - vPos).magnitude();
        double fScalar = (abs(fDist) < PLANE_EPSILON) ? 0.0 : fDist2 / fDist;
        LerpedVertex.lerp(out_vtx_a->mPos, out_vtx_b->mPos, fScalar);
      } else {
        dray3 lsegba(out_vtx_b->mPos + n_ab * 1000.0, -n_ab);
        does_intersect = _slicing_plane.Intersect(lsegba, isectdist, vPos);
        logchan_clip->log("    does_intersectBA<%d>", int(does_intersect));
        double fDist   = (out_vtx_b->mPos - out_vtx_a->mPos).magnitude();
        double fDist2  = (out_vtx_b->mPos - vPos).magnitude();
        double fScalar = (abs(fDist) < PLANE_EPSILON) ? 0.0 : fDist2 / fDist;
        LerpedVertex.lerp(out_vtx_b->mPos, out_vtx_a->mPos, fScalar);
      }
      if (does_intersect) {
        done = false;
        vertex smvert;
        smvert.mPos       = LerpedVertex;
        auto out_vtx_lerp = out_submesh.mergeVertex(smvert);
        //////////////////////
        if (front_to_back) {
          plstat._status  = EPlanarStatus::CROSS_F2B;
          plstat._vertexA = out_vtx_a;
          plstat._vertexB = out_vtx_lerp;
          if (do_log)
            logchan_clip->log(
                "    emit CROSS F2B %d->%d: %d he_ab<%p>",
                out_vtx_a->_poolindex,
                out_vtx_b->_poolindex,
                out_vtx_lerp->_poolindex,
                (void*)he_ab.get());
          auto clipped_edge                                           = out_submesh.mergeEdgeForVertices(out_vtx_a, out_vtx_lerp);
          out_submesh.mergeVar<halfedge_ptr_t>(he_ab, "clipped_edge") = clipped_edge;
          out_submesh.mergeVar<vertex_ptr_t>(out_vtx_b, "clipped_vertex") = out_vtx_lerp;
          auto& clipped_plstat    = out_submesh.mergeVar<PlanarStatus>(clipped_edge, "plstatus");
          clipped_plstat._status  = EPlanarStatus::FRONT;
          clipped_plstat._vertexA = out_vtx_a;
          clipped_plstat._vertexB = out_vtx_lerp;

          auto& heIO                  = out_submesh.mergeVar<vtx_heio_t>(out_vtx_b, "heIO");
          heIO[out_vtx_a->_poolindex] = clipped_edge;
          _F2B_EDGE                   = clipped_edge;
          _f2b_count++;
          _f2b_index = iva;

        } else if (back_to_front) {
          plstat._status  = EPlanarStatus::CROSS_B2F;
          plstat._vertexA = out_vtx_lerp;
          plstat._vertexB = out_vtx_b;
          if (do_log)
            logchan_clip->log(
                "    emit CROSS B2F %d->%d : %d he_ab<%p>",
                out_vtx_b->_poolindex,
                out_vtx_a->_poolindex,
                out_vtx_lerp->_poolindex,
                (void*)he_ab.get());
          auto clipped_edge       = out_submesh.mergeEdgeForVertices(out_vtx_lerp, out_vtx_b);
          auto& clipped_plstat    = out_submesh.mergeVar<PlanarStatus>(clipped_edge, "plstatus");
          clipped_plstat._status  = EPlanarStatus::FRONT;
          clipped_plstat._vertexA = out_vtx_lerp;
          clipped_plstat._vertexB = out_vtx_b;

          out_submesh.mergeVar<halfedge_ptr_t>(he_ab, "clipped_edge")     = clipped_edge;
          out_submesh.mergeVar<vertex_ptr_t>(out_vtx_a, "clipped_vertex") = out_vtx_lerp;
          auto& heIO                                                      = out_submesh.mergeVar<vtx_heio_t>(out_vtx_a, "heIO");
          heIO[out_vtx_b->_poolindex]                                     = clipped_edge;
          _F2B_EDGE                                                       = nullptr;
          _b2f_count++;

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
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void SubMeshClipper::postProcEdges(merged_poly_const_ptr_t input_poly) { //

  const int inuminverts = input_poly->numVertices();
  OrkAssert(inuminverts >= 3);
  int _f2b_index           = f2bindex(input_poly);
  halfedge_ptr_t _F2B_EDGE = nullptr;
  for (int i = 0; i < inuminverts; i++) {
    int iva        = (i + _f2b_index) % inuminverts;
    int ivb        = (iva + 1) % inuminverts;
    auto out_vtx_a = _outsubmesh.mergeVertex(*input_poly->vertex(iva));
    auto out_vtx_b = _outsubmesh.mergeVertex(*input_poly->vertex(ivb));
    //out_vtx_a      = remappedVertex(out_vtx_a);
    //out_vtx_b      = remappedVertex(out_vtx_b);
    auto he_ab     = _outsubmesh.mergeEdgeForVertices(out_vtx_a, out_vtx_b);
    auto he_ba     = _outsubmesh.mergeEdgeForVertices(out_vtx_b, out_vtx_a);

    auto& plstat = _outsubmesh.typedVar<PlanarStatus>(he_ab, "plstatus");
    switch (plstat._status) {
      case EPlanarStatus::FRONT:
        break;
      case EPlanarStatus::BACK:
        break;
      case EPlanarStatus::CROSS_F2B: {
        auto clipped = _outsubmesh.typedVar<halfedge_ptr_t>(he_ab, "clipped_edge");
        _F2B_EDGE    = clipped;
        break;
      }
      case EPlanarStatus::CROSS_B2F: {
        OrkAssert(_F2B_EDGE != nullptr);
        // OrkAssert(_F2B_EDGE->_vertexB == he_ab->_vertexA);
        auto clipped = _outsubmesh.typedVar<halfedge_ptr_t>(he_ab, "clipped_edge");
        auto new_he  = _outsubmesh.mergeEdgeForVertices(_F2B_EDGE->_vertexB, clipped->_vertexA);
        _outsubmesh.mergeVar<halfedge_ptr_t>(out_vtx_a, "split_edge") = new_he;

        _F2B_EDGE = nullptr;
        break;
      }
      case EPlanarStatus::NONE: {
        break;
      }
      default:
        OrkAssert(false);
        break;
    }
  } // for (int iva = 0; iva < inuminverts; iva++) {
}

///////////////////////////////////////////////////////////////////////////////

void SubMeshClipper::clipPolygon(merged_poly_const_ptr_t input_poly) { //

  const int inuminverts = input_poly->numVertices();
  OrkAssert(inuminverts >= 3);

  bool match_poly = matchTestPoly(input_poly);
  if (match_poly)
    printPoly("CLIP INPUT POLY", input_poly);
  vertex_vect_t input_poly_verts;
  input_poly->visitVertices([&](vertex_ptr_t vtx) {
    auto v_m = _outsubmesh.mergeVertex(*vtx);
    input_poly_verts.push_back(v_m);
  });
  if (match_poly)
    logchan_clip->log("clip poly num verts<%d>", inuminverts);
  // loop around the input polygon's edges

  std::vector<halfedge_ptr_t> frontmesh_edges;
  // std::vector<vertex_ptr_t> back_vertices;

  int _f2b_index = f2bindex(input_poly);

  halfedge_ptr_t _F2B_EDGE = nullptr;
  for (int i = 0; i < inuminverts; i++) {
    int iva = (i + _f2b_index) % inuminverts;
    int ivb = (iva + 1) % inuminverts;

    auto out_vtx_a = _outsubmesh.mergeVertex(*input_poly->vertex(iva));
    auto out_vtx_b = _outsubmesh.mergeVertex(*input_poly->vertex(ivb));
    //out_vtx_a      = remappedVertex(out_vtx_a);
    //out_vtx_b      = remappedVertex(out_vtx_b);
    auto he_ab     = _outsubmesh.mergeEdgeForVertices(out_vtx_a, out_vtx_b);
    auto he_ba     = _outsubmesh.mergeEdgeForVertices(out_vtx_b, out_vtx_a);

    if (match_poly)
      logchan_clip->log_continue("  i<%d> iva<%d> of inuminverts<%d> he_ab<%p>\n", i, iva, inuminverts, (void*)he_ab.get());

    auto& plstat = _outsubmesh.typedVar<PlanarStatus>(he_ab, "plstatus");
    switch (plstat._status) {
      case EPlanarStatus::FRONT:
        if (match_poly)
          logchan_clip->log("  front");
        frontmesh_edges.push_back(he_ab);
        break;
      case EPlanarStatus::BACK:
        // OrkAssert(false);
        if (match_poly)
          logchan_clip->log("  back");

        if (auto try_clip_ab = _outsubmesh.tryVarAs<halfedge_ptr_t>(he_ab, "clipped_edge")) {
          auto clipped_ab = try_clip_ab.value();
          if (match_poly)
            logchan_clip->log("  back - got clipab <%d->%d>\n", clipped_ab->_vertexA->_poolindex, clipped_ab->_vertexB->_poolindex);
          OrkAssert(false);
        } else if (auto try_clip_ba = _outsubmesh.tryVarAs<halfedge_ptr_t>(he_ba, "clipped_edge")) {
          auto clipped_ba = try_clip_ba.value();
          if (match_poly)
            logchan_clip->log("  back - got clipba <%d->%d>\n", clipped_ba->_vertexA->_poolindex, clipped_ba->_vertexB->_poolindex);
          OrkAssert(false);
        } else {
          auto va            = he_ab->_vertexA;
          auto vb            = he_ab->_vertexB;
          auto inserted      = std::make_shared<HalfEdge>();
          inserted->_vertexA = va;
          inserted->_vertexB = vb;
          frontmesh_edges.push_back(inserted);
        }
        break;
      case EPlanarStatus::CROSS_F2B: {
        if (match_poly)
          logchan_clip->log("  front2back");
        auto clipped = _outsubmesh.typedVar<halfedge_ptr_t>(he_ab, "clipped_edge");
        frontmesh_edges.push_back(clipped);
        OrkAssert(clipped != nullptr);
        _F2B_EDGE = clipped;
        break;
      }
      case EPlanarStatus::CROSS_B2F: {
        if (match_poly)
          logchan_clip->log("  back2front");

        OrkAssert(_F2B_EDGE != nullptr);
        auto clipped = _outsubmesh.typedVar<halfedge_ptr_t>(he_ab, "clipped_edge");

        auto inserted      = std::make_shared<HalfEdge>();
        inserted->_vertexA = _F2B_EDGE->_vertexB;
        inserted->_vertexB = clipped->_vertexA;
        frontmesh_edges.push_back(inserted);
        frontmesh_edges.push_back(clipped);
        _F2B_EDGE = nullptr;
        break;
      }
      case EPlanarStatus::NONE: {
        if (match_poly)
          logchan_clip->log("  none");
        break;
      }
      default:
        OrkAssert(false);
        break;
    }
  } // for (int iva = 0; iva < inuminverts; iva++) {

  if ((frontmesh_edges.size() >= 3)) {
    vertex_vect_t frontmesh_vertices;
    int inumfrontedges = frontmesh_edges.size();

    // std::vector<halfedge_ptr_t> frontmesh_edges_remapped;

    for (int ife = 0; ife < inumfrontedges; ife++) {
      auto he  = frontmesh_edges[ife];
      auto vtx = he->_vertexA;

      auto rmA = remappedVertex(he->_vertexA);
      auto rmB = remappedVertex(he->_vertexB);
      if (match_poly)
        logchan_clip->log(
            "  EMIT EDGE<%p o(%d->%d) rm(%d->%d)>", //
            (void*)he.get(),                        //
            he->_vertexA->_poolindex,               //
            he->_vertexB->_poolindex,               //
            rmA->_poolindex,                        //
            rmB->_poolindex                         //
        );                                          //

      frontmesh_vertices.push_back(rmA);
      frontmesh_vertices.push_back(rmB);

      // auto remapped = _outsubmesh.mergeEdgeForVertices(rmA, rmB);
      // frontmesh_edges_remapped.push_back(remapped);
    }

    //////////////////////////////////////////////////////////////////////////////////////

    vertex_vect_t frontmesh_vertices_nonrepeat;
    vertex_ptr_t prev = nullptr;
    for (int iv = 0; iv < frontmesh_vertices.size(); iv++) {
      auto v0 = frontmesh_vertices[iv];
      if (v0 != prev) {
        frontmesh_vertices_nonrepeat.push_back(v0);
      }
      prev = v0;
    }

    for (auto item : frontmesh_vertices_nonrepeat) {
      if (match_poly)
        logchan_clip->log("  frontmesh_vertices_nonrepeat<%d>", item->_poolindex);
    }

    //////////////////////////////////////////////////////////////////////////////////////

    _outsubmesh.mergePoly(frontmesh_vertices_nonrepeat);

    //////////////////////////////////////////////////////////////////////////////////////

    // out_submesh.mergePoly(frontmesh_vertices);
  }
}

///////////////////////////////////////////////////////////////////////////////

void SubMeshClipper::closeSubMesh() {
  return;

  if (debug) {
    _outsubmesh.visitAllVertices([&](vertex_ptr_t v) { //
      double point_distance = _slicing_plane.pointDistance(v->mPos);
      logchan_clip->log("outv%d : %f %f %f point_distance<%f>", v->_poolindex, v->mPos.x, v->mPos.y, v->mPos.z, point_distance);
    });
    _outsubmesh.dumpPolys("preclose");
  }

  _backpolys.visit([&](poly_const_ptr_t back_poly) {
    int num_clipped = 0;
    int num_v       = back_poly->numVertices();

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
        logchan_clip->log_continue(" <%d>", inp_vtx_a->_poolindex);
      } else {
        bool is_back = _outsubmesh.tryVarAs<bool>(inp_vtx_a, "back_vertex").value();
        if (is_back) {
          logchan_clip->log_continue(" (%d)", inp_vtx_a->_poolindex);

        } else {
          logchan_clip->log_continue(" %d", inp_vtx_a->_poolindex);
        }
      }
      if (he_plane_status._status == EPlanarStatus::BACK) {
        logchan_clip->log_continue("!");
      }
      if (auto try_clipped_edge = _outsubmesh.tryVarAs<halfedge_ptr_t>(he, "clipped_edge")) {
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

  if (debug)
    for (auto e_item : all_edges._the_map) {
      auto e = e_item.second;
      logchan_clip->log("all e[%d %d]", e->_vertexA->_poolindex, e->_vertexB->_poolindex);
    }

  int index = 0;
  _planar_verts.visit([&](vertex_ptr_t v) {
    if (debug)
      logchan_clip->log("planar v%d : %f %f %f", v->_poolindex, v->mPos.x, v->mPos.y, v->mPos.z);
    _planar_verts.visit([&](vertex_ptr_t v2) {
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
        _planar_edges.insert(e2);
      }
    });
    index++;
  });

  if (debug) {
    for (auto e_item : _planar_edges._the_map) {
      auto e = e_item.second;
      logchan_clip->log_continue("planar e %d %d\n", e->_vertexA->_poolindex, e->_vertexB->_poolindex);
    }
  }

  /////////////////////////////////////////
  // we have some edges on the cutting plane
  /////////////////////////////////////////

  if (_planar_edges.size()) {

    // link edge chains into edge loops

    EdgeChainLinker _linker;
    _linker._name = _outsubmesh.name;
    for (auto edge_item : _planar_edges._the_map) {
      auto edge = edge_item.second;
      _linker.add_edge(edge);
    }
    _linker.link();

    // create a new polygon for each edge loop

    dvec3 centroid = _outsubmesh.centerOfVertices();

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
      _outsubmesh.mergePoly(p1);

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
  } // if (_planar_edges.size()) {
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

  SubMeshClipper sm_clipper_front(inpsubmesh, slicing_plane, outsmesh_front, close_mesh);
  // SubMeshClipper sm_clipper_back(inpsubmesh, slicing_plane, outsmesh_back, close_mesh, false);

  if (debug) {
    outsmesh_front.dumpPolys("clipped_front");
    if (sm_clipper_front._planar_verts._the_map.size() > 0) {
      logchan_clip->log_continue("fpv [");
      for (auto v_item : sm_clipper_front._planar_verts._the_map) {
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
