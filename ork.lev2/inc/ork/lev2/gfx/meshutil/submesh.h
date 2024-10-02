////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include "submesh_component.h"

namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////
typedef orkmap<std::string, svar64_t> AnnotationMap;
struct XgmClusterizer;
struct XgmClusterizerDiced;
struct XgmClusterizerStd;
///////////////////////////////////////////////////////////////////////////////

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

///////////////////////////////////////////////////////////////////////////////

struct IndexTestContext {
  int iset;
  int itest;
  orkset<int> PairedIndices[2];
  orkset<int> PairedIndicesCombined;
  orkset<int> CornerIndices;
};

struct annopolylut {
  orkmap<U64, const AnnoMap*> mAnnoMap;
  virtual U64 HashItem(const submesh& tmesh, const Polygon& item) const = 0;
  const AnnoMap* Find(const submesh& tmesh, const Polygon& item) const;
};
struct annopolyposlut : public annopolylut {
  virtual U64 HashItem(const submesh& tmesh, const Polygon& item) const;
};

///////////////////////////////////////////////////////////////////////////////

struct submesh {

  submesh();
  ~submesh();

  //////////////////////////////////////////////////////////////////////////////

  void importPolyAnnotations(const annopolylut& apl);
  void exportPolyAnnotations(annopolylut& apl) const;

  void setStringAnnotation(const char* annokey, std::string annoval);
  AnnotationMap& annotations() {
    return _annotations;
  }
  const AnnotationMap& annotations() const {
    return _annotations;
  }

  void MergeAnnos(const AnnotationMap& mrgannos, bool boverwrite);

  svar64_t annotation(const char* annokey) const;

  template <typename T> T& typedAnnotation(const std::string annokey) {
    auto& anno = _annotations[annokey];
    if (anno.isA<T>())
      return anno.get<T>();
    return anno.make<T>();
  }

  template <typename T> const T& typedAnnotation(const std::string annokey) const {
    auto it = _annotations.find(annokey);
    if (it != _annotations.end()) {
      const auto& anno = it->second;
      return anno.get<T>();
    }
    static T rval;
    return rval;
  }

  //////////////////////////////////////////////////////////////////////////////

  Polygon& RefPoly(int i);
  const Polygon& RefPoly(int i) const;
  vertex_ptr_t vertex(int i) const;
  merged_poly_ptr_t poly(int i);
  merged_poly_const_ptr_t poly(int i) const;

  void dumpPolys(std::string hdr, bool showverts = false) const;

  //////////////////////////////////////////////////////////////////////////////

  void copy(submesh& dest, bool preserve_normals = true, bool preserve_colors = true, bool preserve_texcoords = true) const;

  //////////////////////////////////////////////////////////////////////////////

  vertex_ptr_t mergeVertex(const struct vertex& vtx);
  vertex_ptr_t mergeVertexConcurrent(const struct vertex& vtx);
  merged_poly_ptr_t mergePoly(const struct Polygon& ply);
  merged_poly_ptr_t mergePoly(const vertex_vect_t& ply);
  merged_poly_ptr_t mergePolyConcurrent(const struct Polygon& ply);
  merged_poly_ptr_t mergeTriangle(vertex_ptr_t va, vertex_ptr_t vb, vertex_ptr_t vc);
  merged_poly_ptr_t mergeTriangleConcurrent(vertex_ptr_t va, vertex_ptr_t vb, vertex_ptr_t vc);
  merged_poly_ptr_t mergeUnorderedTriangle(vertex_ptr_t va, vertex_ptr_t vb, vertex_ptr_t vc);
  merged_poly_ptr_t mergeQuad(vertex_ptr_t va, vertex_ptr_t vb, vertex_ptr_t vc, vertex_ptr_t vd);
  void removePoly(merged_poly_ptr_t ply);
  void removePolys(std::vector<merged_poly_ptr_t>& polys);
  void clearPolys();
  void MergeSubMesh(const submesh& oth);
  void mergePolyGroup(const PolyGroup& pset);
  bool isVertexInsideConvexHull(vertex_const_ptr_t vtx) const;

  //////////////////////////////////////////////////////////////////////////////


  template <typename T> T& mergeVar(halfedge_ptr_t he, const std::string& varname) {
    auto& varmap = _connectivityIMPL->varmapForHalfEdge(he);
    auto& var    = varmap[varname];
    if (var.isA<T>())
      return var.get<T>();
    return var.make<T>();
  }
  template <typename T> T& typedVar(halfedge_ptr_t he, const std::string& varname) {
    auto& varmap = _connectivityIMPL->varmapForHalfEdge(he);
    auto& var    = varmap[varname];
    return var.get<T>();
  }
  template <typename T> attempt_cast<T> tryVarAs(halfedge_ptr_t he, const std::string& varname) {
    auto& varmap = _connectivityIMPL->varmapForHalfEdge(he);
    return varmap.typedValueForKey<T>(varname);
  }
  inline bool hasVar(halfedge_ptr_t he, const std::string& varname) {
    auto& varmap = _connectivityIMPL->varmapForHalfEdge(he);
    return varmap.hasKey(varname);
  }
  inline varmap::VarMap& varmapForHalfEdge(halfedge_ptr_t he) const {
    return _connectivityIMPL->varmapForHalfEdge(he);
  }

  //////////////////////////////////////////////////////////////////////////////

  template <typename T> T& mergeVar(vertex_const_ptr_t v, const std::string& varname) {
    auto& varmap = _connectivityIMPL->varmapForVertex(v);
    auto& var    = varmap[varname];
    if (var.isA<T>())
      return var.get<T>();
    return var.make<T>();
  }
  template <typename T> T& typedVar(vertex_const_ptr_t v, const std::string& varname) {
    auto& varmap = _connectivityIMPL->varmapForVertex(v);
    auto& var    = varmap[varname];
    return var.get<T>();
  }
  template <typename T> attempt_cast<T> tryVarAs(vertex_const_ptr_t v, const std::string& varname) {
    auto& varmap = _connectivityIMPL->varmapForVertex(v);
    return varmap.typedValueForKey<T>(varname);
  }
  inline bool hasVar(vertex_const_ptr_t v, const std::string& varname) {
    auto& varmap = _connectivityIMPL->varmapForVertex(v);
    return varmap.hasKey(varname);
  }
  inline varmap::VarMap& varmapForVertex(vertex_const_ptr_t v) const {
    return _connectivityIMPL->varmapForVertex(v);
  }

  //////////////////////////////////////////////////////////////////////////////

  template <typename T> T& mergeVar(merged_poly_const_ptr_t p, const std::string& varname) {
    auto& varmap = _connectivityIMPL->varmapForPolygon(p);
    auto& var    = varmap[varname];
    if (var.isA<T>())
      return var.get<T>();
    return var.make<T>();
  }
  template <typename T> T& typedVar(merged_poly_const_ptr_t p, const std::string& varname) {
    auto& varmap = _connectivityIMPL->varmapForPolygon(p);
    auto& var    = varmap[varname];
    return var.get<T>();
  }
  template <typename T> attempt_cast<T> tryVarAs(merged_poly_const_ptr_t p, const std::string& varname) {
    auto& varmap = _connectivityIMPL->varmapForPolygon(p);
    return varmap.typedValueForKey<T>(varname);
  }
  inline bool hasVar(merged_poly_const_ptr_t p, const std::string& varname) {
    auto& varmap = _connectivityIMPL->varmapForPolygon(p);
    return varmap.hasKey(varname);
  }
  inline varmap::VarMap& varmapForPolygon(merged_poly_const_ptr_t p) const {
    return _connectivityIMPL->varmapForPolygon(p);
  }

  //////////////////////////////////////////////////////////////////////////////

  int numVertices() const;

  void visitAllVertices(vertex_void_visitor_t visitor);
  void visitAllVertices(const_vertex_void_visitor_t visitor) const;
  void visitAllEdges(halfedge_void_visitor_t visitor);

  int numPolys(int inumsides = 0) const;
  void FindNSidedPolys(orkvector<int>& output, int inumsides) const;
  poly_index_set_t connectedPolys(edge_ptr_t edge, bool ordered = true) const;
  poly_index_set_t connectedPolys(const edge& edge, bool ordered = true) const;
  poly_index_set_t adjacentPolys(int ply) const;
  poly_set_t polysConnectedToVertex(vertex_ptr_t v) const;

  halfedge_vect_t edgesForPoly(merged_poly_const_ptr_t p) const;
  halfedge_ptr_t edgeForVertices(vertex_ptr_t a, vertex_ptr_t b) const;
  halfedge_ptr_t mergeEdgeForVertices(vertex_ptr_t a, vertex_ptr_t b);

  uint64_t hash() const;

  struct PolyVisitContext {
    merged_polyconst_set_t _visited;
    merged_poly_bool_visitor_t _visitor;
    bool _indirect = false;
  };

  void visitAllPolys(merged_poly_void_mutable_visitor_t visitor);
  void visitAllPolys(merged_poly_void_visitor_t visitor) const;
  void visitConnectedPolys(merged_poly_const_ptr_t p, PolyVisitContext& context) const;

  edge_map_t allEdgesByVertexHash() const;

  /////////////////////////////////////////////////////////////////////////

  const AABox& aabox() const; /// compute axis aligned bounding box from the current state of the vertex pool

  bool isConvexHull() const;
  dvec3 centerOfVertices() const;
  dvec3 centerOfPolys() const;
  dvec3 centerOfPolysConcurrent() const;
  double convexVolume() const;
  dvec3 boundingMin() const;
  dvec3 boundingMax() const;

  //////////////////////////////////////////////////////////////////////////////

  void inheritParams( const submesh* from );

  //////////////////////////////////////////////////////////////////////////////

  void addQuad(
      dvec3 p0, //
      dvec3 p1,
      dvec3 p2,
      dvec3 p3,
      dvec4 c = dvec4(1, 1, 1, 1)); /// add quad helper
                                    /// method

  void addQuad(
      dvec3 p0, //
      dvec3 p1,
      dvec3 p2,
      dvec3 p3,
      dvec2 uv0,
      dvec2 uv1,
      dvec2 uv2,
      dvec2 uv3,
      dvec4 c); /// add quad helper
                /// method

  void addQuad(
      dvec3 p0,
      dvec3 p1,
      dvec3 p2,
      dvec3 p3,
      dvec3 n0,
      dvec3 n1,
      dvec3 n2,
      dvec3 n3,
      dvec2 uv0,
      dvec2 uv1,
      dvec2 uv2,
      dvec2 uv3,
      dvec4 c); /// add quad helper method

  void addQuad(
      dvec3 p0,
      dvec3 p1,
      dvec3 p2,
      dvec3 p3,
      dvec3 n0,
      dvec3 n1,
      dvec3 n2,
      dvec3 n3,
      dvec3 b0,
      dvec3 b1,
      dvec3 b2,
      dvec3 b3,
      dvec2 uv0,
      dvec2 uv1,
      dvec2 uv2,
      dvec2 uv3,
      dvec4 c); /// add quad helper method

  /////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_IGL)
  iglmesh_ptr_t toIglMesh(int numsides) const;
  void igl_test();
#endif

  polygroup_ptr_t asPolyGroup() const;

  /////////////////////////////////////////////////////////////////////////

  std::string name;
  AnnotationMap _annotations;
  float _surfaceArea;

  /////////////////////////////////////
  // these are mutable so we can get bounding boxes faster with const refs to Mesh's
  mutable AABox _aaBox;
  mutable bool _aaBoxDirty;

  connectivity_impl_ptr_t _connectivityIMPL;

private:
  mutable mutex _concmutex;

  /////////////////////////////////////
};

void submeshTriangulate(const submesh& inpsubmesh, submesh& outsmesh);

void submeshTrianglesToQuads(
    const submesh& inpsubmesh,
    submesh& outsmesh,
    double area_tolerance        = 100.0, // 1:100 .. 100:1
    bool exclude_non_coplanar    = true,  //
    bool exclude_non_rectangular = false  //
);

void submeshSliceWithPlane(
    const submesh& inpsubmesh, //
    dplane3& slicing_plane,    //
    submesh& outsmeshFront,    //
    submesh& outsmeshBack,
    submesh& outsmeshIntersects);

void submeshClipWithPlane(
    const submesh& inpsubmesh, //
    dplane3& slicing_plane,    //
    bool close_mesh,
    bool flip_orientation,
    submesh& outsmeshFront, //
    bool debug = false);

void submeshPrune(const submesh& inpsubmesh, submesh& outsmesh);
void submeshWithTextureBasis(const submesh& inpsubmesh, submesh& outsmesh);
void submeshWithTextureUnwrap(const submesh& inpsubmesh, submesh& outsmesh);

void submeshWithVertexColorsFromNormals(const submesh& inpsubmesh, submesh& outsmesh);
void submeshWithFaceNormals(const submesh& inpsubmesh, submesh& outsmesh);
void submeshWithFaceNormalsAndBinormals(const submesh& inpsubmesh, submesh& outsubmesh);
void submeshWithSmoothNormalsAndBinormals(const submesh& inpsubmesh, submesh& outsubmesh, float threshold_radians);
void submeshWithSmoothNormals(const submesh& inpsubmesh, submesh& outsmesh, float threshold_radians);
void submeshJoinCoplanar(const submesh& inpsubmesh, submesh& outsmesh);
void submeshBarycentricUV(const submesh& inpsubmesh, submesh& outsmesh);
submesh_ptr_t submeshFromFrustum(const dfrustum& frustum, bool projective_rect_uv);

std::vector<submesh_ptr_t> submeshBulletConvexDecomposition(const submesh& inpsubmesh);
void submeshConvexHull(const submesh& inpsubmesh, submesh& outsmesh, int steps = 0);

void submeshFixWindingOrder(const submesh& inpsubmesh, submesh& outsmesh, bool inside_out);

void submeshWriteObj(const submesh& inpsubmesh, const file::Path& BasePath);
std::string submeshConvexCheckWindingOrder(const submesh& inpsubmesh);

void submesh_xatlas(const submesh& inpsubmesh, submesh& outsubmesh);

// void SubDivQuads(submesh* poutsmesh) const;
// void SubDivTriangles(submesh* poutsmesh) const;
// void SubDiv(submesh* poutsmesh) const;

std::vector<polygroup_ptr_t> splitByIsland(polygroup_ptr_t inpset);

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
