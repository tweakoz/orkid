////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/meshutil/submesh_clip.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan_clip = logger()->createChannel("meshutil.clipper", fvec3(.9, .9, 1), true);

SubMeshClipper::SubMeshClipper(
    const submesh& inpsubmesh,      //
    submesh& smfront,               //
    bool debug)                     //
    : _inpsubmesh(inpsubmesh)       //
    , _outsubmesh(smfront)          //
    , _debug(debug) {               //
}
///////////////////////////////////////////////////////////////////////////////

bool SubMeshClipper::matchTestPoly(merged_poly_const_ptr_t src_poly) const {
  bool match_poly = true;
  src_poly->visitVertices([&](vertex_ptr_t v) {
    if (_test_verts.find(v->_poolindex) == _test_verts.end())
      match_poly = false;
  });
  return match_poly;
}

///////////////////////////////////////////////////////////////////////////////

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
      if (match_poly and _debug)
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
  double d                                         = _clipprimitive->pointDistance(merged->mPos);
  _outsubmesh.mergeVar<double>(merged, "distance") = d;

  switch (cat) {
    case ESurfaceStatus::FRONT:
      _outsubmesh.mergeVar<std::string>(merged, "plside") = "FRONT";
      break;
    case ESurfaceStatus::BACK:
      _outsubmesh.mergeVar<std::string>(merged, "plside") = "BACK";
      break;
    case ESurfaceStatus::PLANAR:
      _outsubmesh.mergeVar<std::string>(merged, "plside") = "PLANAR";
      _surface_verts_pending_close.insert(merged);
      break;
    default:
      break;
  }

  return merged;
}

///////////////////////////////////////////////////////////////////////////////

ESurfaceStatus SubMeshClipper::categorizeVertex(const vertex& input_vertex) const {
  ESurfaceStatus rval   = ESurfaceStatus::NONE;
  double point_distance = _clipprimitive->pointDistance(input_vertex.mPos);
  if (abs(point_distance) < SURFACE_EPSILON) {
    rval = ESurfaceStatus::PLANAR;
  } else if (point_distance >= 0.0) {
    rval = ESurfaceStatus::FRONT;
  } else {
    rval = ESurfaceStatus::BACK;
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

PolyVtxCount SubMeshClipper::categorizePolygon(merged_poly_const_ptr_t input_poly) const {
  PolyVtxCount counts;
  input_poly->visitVertices([&](vertex_ptr_t vtx) {
    ESurfaceStatus cat = categorizeVertex(*vtx);
    switch (cat) {
      case ESurfaceStatus::FRONT:
        counts._front_count++;
        break;
      case ESurfaceStatus::BACK:
        counts._back_count++;
        break;
      case ESurfaceStatus::PLANAR:
        counts._surface_count++;
        break;
      default:
        OrkAssert(false);
        break;
    }
  });
  return counts;
}

///////////////////////////////////////////////////////////////////////////////

void SubMeshClipper::clipWithPrimitive(clipprimitive_ptr_t clipprimitive) {

  _clipprimitive = clipprimitive;

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
          polyvtxcnt._surface_count);
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

  _clipprimitive = nullptr; // release clip primitive
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
    bool is_vertex_a_front = _clipprimitive->isPointInFront(out_vtx_a->mPos);
    bool is_vertex_b_front = _clipprimitive->isPointInFront(out_vtx_b->mPos);
    if (do_log)
      logchan_clip->log(
          "   iva<%d> edge<%d->%d> front<%d,%d>",
          iva,
          out_vtx_a->_poolindex,
          out_vtx_b->_poolindex,
          int(is_vertex_a_front),
          int(is_vertex_b_front));
    auto& plstat    = _outsubmesh.mergeVar<SurfaceStatus>(he_ab, "plstatus");
    plstat._status  = ESurfaceStatus::NONE;
    plstat._vertexA = out_vtx_a;
    plstat._vertexB = out_vtx_b;
    if (is_vertex_a_front and is_vertex_b_front) {
      plstat._status  = ESurfaceStatus::FRONT;
      plstat._vertexA = out_vtx_a;
      plstat._vertexB = out_vtx_b;
      if (do_log)
        logchan_clip->log("    emit front he_ab<%p>", (void*)he_ab.get());
    } else if ((not is_vertex_a_front) and (not is_vertex_b_front)) {
      plstat._status  = ESurfaceStatus::BACK;
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
      bool does_intersect = _clipprimitive->doesIntersect(lsegab, isectdist, vPos);
      dvec3 LerpedVertex;
      if (do_log)
        logchan_clip->log("    does_intersectAB<%d>", int(does_intersect));
      if (does_intersect) {
        double fDist   = (out_vtx_a->mPos - out_vtx_b->mPos).magnitude();
        double fDist2  = (out_vtx_a->mPos - vPos).magnitude();
        double fScalar = (abs(fDist) < SURFACE_EPSILON) ? 0.0 : fDist2 / fDist;
        LerpedVertex.lerp(out_vtx_a->mPos, out_vtx_b->mPos, fScalar);
      } else {
        dray3 lsegba(out_vtx_b->mPos + n_ab * 1000.0, -n_ab);
        does_intersect = _clipprimitive->doesIntersect(lsegba, isectdist, vPos);
        if (do_log)
          logchan_clip->log("    does_intersectBA<%d>", int(does_intersect));
        double fDist   = (out_vtx_b->mPos - out_vtx_a->mPos).magnitude();
        double fDist2  = (out_vtx_b->mPos - vPos).magnitude();
        double fScalar = (abs(fDist) < SURFACE_EPSILON) ? 0.0 : fDist2 / fDist;
        LerpedVertex.lerp(out_vtx_b->mPos, out_vtx_a->mPos, fScalar);
      }
      if (does_intersect) {
        vertex smvert;
        smvert.mPos       = LerpedVertex;
        auto out_vtx_lerp = mergeVertex(smvert);
        //////////////////////
        if (front_to_back) {
          plstat._status  = ESurfaceStatus::CROSS_F2B;
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
          plstat._status  = ESurfaceStatus::CROSS_B2F;
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
        // OrkAssert(false); // crossed the plane, but non intersecting ?
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
    if (_debug) {
      logchan_clip->log("clip poly num verts<%d>", inuminverts);
      for (auto mergedv : out_verts) {
        dumpVertexVars(mergedv);
      }
    }
  }

  auto polycat = categorizePolygon(input_poly);

  //////////////////////////////////////////////////////////
  // handle all-front polygons
  //////////////////////////////////////////////////////////

  if (polycat._back_count == 0 and polycat._surface_count == 0) {
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

    auto& plstat = _outsubmesh.typedVar<SurfaceStatus>(he_ab, "plstatus");
    switch (plstat._status) {
      case ESurfaceStatus::FRONT:
        if (match_poly) {
          if (_debug)
            logchan_clip->log_continue("  front\n");
        }
        frontmesh_vertices.push_back(out_vtx_a);
        frontmesh_vertices.push_back(out_vtx_b);
        break;
      case ESurfaceStatus::BACK:
        // OrkAssert(false);
        if (match_poly and _debug)
          logchan_clip->log_continue("  back\n");
        break;
      case ESurfaceStatus::CROSS_F2B: {
        if (match_poly and _debug) {
          logchan_clip->log_continue("  front2back\n");
        }
        OrkAssert(_F2B_EDGE == nullptr);
        frontmesh_vertices.push_back(out_vtx_a);
        frontmesh_vertices.push_back(remappedVertex(out_vtx_b, he_ab));
        _F2B_EDGE = he_ab;
        break;
      }
      case ESurfaceStatus::CROSS_B2F: {
        if (match_poly and _debug) {
          logchan_clip->log_continue("  back2front\n");
        }
        OrkAssert(_F2B_EDGE != nullptr);
        frontmesh_vertices.push_back(remappedVertex(out_vtx_a, he_ab));
        frontmesh_vertices.push_back(out_vtx_b);
        _F2B_EDGE = nullptr;
        break;
      }
      case ESurfaceStatus::NONE: {
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

    if (_debug) {
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

void planar_clip_init() {
  auto ps_type = TypeId::of<SurfaceStatus>();
  varmap::VarMap::registerStringEncoder(ps_type, [](const varmap::VarMap::value_type& val) {
    auto& ps = val.template get<SurfaceStatus>();
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

} // namespace ork::meshutil