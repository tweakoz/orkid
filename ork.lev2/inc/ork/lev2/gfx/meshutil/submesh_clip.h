////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <ork/util/logger.h>
#include <deque>

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

struct PolyVtxCount {
  int _front_count  = 0;
  int _back_count   = 0;
  int _surface_count = 0;
};

using vtx_heio_t = std::multimap<int, halfedge_ptr_t>;

enum class ESurfaceStatus { CROSS_F2B, CROSS_B2F, PLANAR, FRONT, BACK, NONE };

struct SurfaceStatus {
  vertex_ptr_t _vertexA;
  vertex_ptr_t _vertexB;
  ESurfaceStatus _status;
};

///////////////////////////////////////////////////////////////////////////////

struct SubMeshClipper;

struct ClipPrimitiveBase {
  ClipPrimitiveBase() {}
  virtual ~ClipPrimitiveBase() {}
  virtual void close() = 0;
  virtual double pointDistance(const dvec3& point) const = 0;
  virtual bool isPointInFront(const dvec3& point) const = 0;
  virtual bool doesIntersect(const dray3& ray, double& distance, dvec3& isect_point ) const = 0;

};

using clipprimitive_ptr_t = std::shared_ptr<ClipPrimitiveBase>;

///////////////////////////////////////////////////////////////////////////////

struct SubMeshClipper {

  static constexpr double SURFACE_EPSILON = 0.001f;

  /////////////////////////////////////
  SubMeshClipper(
      const submesh& inpsubmesh, //
      submesh& smfront,          //
      bool debug);
  /////////////////////////////////////
  void clipWithPrimitive(clipprimitive_ptr_t clipprimitive);
  void procEdges(merged_poly_const_ptr_t input_poly);
  void postProcEdges(merged_poly_const_ptr_t input_poly);
  void clipPolygon(merged_poly_const_ptr_t input_poly);
  void closeSubMesh();
  /////////////////////////////////////
  bool matchTestPoly(merged_poly_const_ptr_t src_poly) const;
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
  ESurfaceStatus categorizeVertex(const vertex& input_vertex) const;
  PolyVtxCount categorizePolygon(merged_poly_const_ptr_t input_poly) const;
  /////////////////////////////////////
  const submesh& _inpsubmesh;
  submesh& _outsubmesh;
  bool _debug;
  clipprimitive_ptr_t _clipprimitive;
  /////////////////////////////////////
  vertex_set_t _surface_verts_pending_close;
  polyconst_set_t _backpolys;
  polyconst_set_t _frontpolys;
  std::map<int, int> _vertex_remap;
  std::vector<merged_poly_const_ptr_t> _polys_to_clip;
  std::unordered_map<merged_poly_const_ptr_t, varmap::VarMap> _inp_poly_varmap;
  std::unordered_set<int> _test_verts;
};

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////
