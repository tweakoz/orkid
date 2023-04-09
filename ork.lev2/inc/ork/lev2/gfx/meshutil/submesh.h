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
  poly_ptr_t poly(int i) const;

  //////////////////////////////////////////////////////////////////////////////

  void copy( submesh& dest,
             bool preserve_normals=true,
             bool preserve_colors=true,
             bool preserve_texcoords=true ) const;

  //////////////////////////////////////////////////////////////////////////////

  vertex_ptr_t mergeVertex(const struct vertex& vtx);
  vertex_ptr_t mergeVertexConcurrent(const struct vertex& vtx);
  edge_ptr_t mergeEdge(const edge& ed);
  poly_ptr_t mergePoly(const struct Polygon& ply);
  poly_ptr_t mergePolyConcurrent(const struct Polygon& ply);
  poly_ptr_t mergeTriangle(vertex_ptr_t va, vertex_ptr_t vb, vertex_ptr_t vc);
  poly_ptr_t mergeTriangleConcurrent(vertex_ptr_t va, vertex_ptr_t vb, vertex_ptr_t vc);
  poly_ptr_t mergeUnorderedTriangle(vertex_ptr_t va, vertex_ptr_t vb, vertex_ptr_t vc);
  poly_ptr_t mergeQuad(vertex_ptr_t va, vertex_ptr_t vb, vertex_ptr_t vc, vertex_ptr_t vd);
  void removePoly(poly_ptr_t ply);
  void clearPolys();
  void MergeSubMesh(const submesh& oth);
  void mergePolySet(const PolySet& pset);

  //////////////////////////////////////////////////////////////////////////////

  int numVertices() const;

  void visitAllVertices(vertex_void_visitor_t visitor);
  void visitAllVertices(const_vertex_void_visitor_t visitor) const;

  int numPolys(int inumsides = 0) const;
  void FindNSidedPolys(orkvector<int>& output, int inumsides) const;
  poly_index_set_t connectedPolys(edge_ptr_t edge, bool ordered = true) const;
  poly_index_set_t connectedPolys(const edge& edge, bool ordered = true) const;
  poly_index_set_t adjacentPolys(int ply) const;
  edge_ptr_t edgeBetweenPolys(int a, int b) const;
  poly_set_t polysConnectedTo(vertex_ptr_t v) const;

  uint64_t hash() const;

  struct PolyVisitContext{
    poly_set_t _visited;
    poly_bool_visitor_t _visitor;
    bool _indirect = false;
  };

  void visitAllPolys(poly_void_visitor_t visitor);
  void visitAllPolys(const_poly_void_visitor_t visitor) const;
  void visitConnectedPolys(poly_ptr_t p,PolyVisitContext& context) const;

  edge_map_t allEdgesByVertexHash() const;

  /////////////////////////////////////////////////////////////////////////

  const AABox& aabox() const; /// compute axis aligned bounding box from the current state of the vertex pool

  bool isConvexHull() const;
  dvec3 centerOfVertices() const;
  dvec3 centerOfPolys() const;
  dvec3 centerOfPolysConcurrent() const;
  double convexVolume() const;

  //////////////////////////////////////////////////////////////////////////////

  void addQuad(
      fvec3 p0, //
      fvec3 p1,
      fvec3 p2,
      fvec3 p3,
      fvec4 c = fvec4(1, 1, 1, 1)); /// add quad helper
                                    /// method

  void addQuad(
      fvec3 p0, //
      fvec3 p1,
      fvec3 p2,
      fvec3 p3,
      fvec2 uv0,
      fvec2 uv1,
      fvec2 uv2,
      fvec2 uv3,
      fvec4 c); /// add quad helper
                /// method

  void addQuad(
      fvec3 p0,
      fvec3 p1,
      fvec3 p2,
      fvec3 p3,
      fvec3 n0,
      fvec3 n1,
      fvec3 n2,
      fvec3 n3,
      fvec2 uv0,
      fvec2 uv1,
      fvec2 uv2,
      fvec2 uv3,
      fvec4 c); /// add quad helper method

  void addQuad(
      fvec3 p0,
      fvec3 p1,
      fvec3 p2,
      fvec3 p3,
      fvec3 n0,
      fvec3 n1,
      fvec3 n2,
      fvec3 n3,
      fvec3 b0,
      fvec3 b1,
      fvec3 b2,
      fvec3 b3,
      fvec2 uv0,
      fvec2 uv1,
      fvec2 uv2,
      fvec2 uv3,
      fvec4 c); /// add quad helper method

  /////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_IGL)
  iglmesh_ptr_t toIglMesh(int numsides) const;
  void igl_test();
#endif
  
  polyset_ptr_t asPolyset() const;
  
  /////////////////////////////////////////////////////////////////////////

  std::string name;
  AnnotationMap _annotations;
  float _surfaceArea;

  /////////////////////////////////////
  // these are mutable so we can get bounding boxes faster with const refs to Mesh's
  mutable AABox _aaBox;
  mutable bool _aaBoxDirty;
  connectivity_impl_ptr_t _connectivityIMPL;
  mutable mutex _concmutex;

  /////////////////////////////////////
};

void submeshTriangulate(const submesh& inpsubmesh, submesh& outsmesh);

void submeshTrianglesToQuads(const submesh& inpsubmesh, 
                             submesh& outsmesh, 
                             double area_tolerance = 100.0, // 1:100 .. 100:1
                             bool exclude_non_coplanar = true, //
                             bool exclude_non_rectangular = false //
                             );

void submeshSliceWithPlane(const submesh& inpsubmesh, //
                           dplane3& slicing_plane, //
                           submesh& outsmeshFront, //
                           submesh& outsmeshBack,
                           submesh& outsmeshIntersects
                           );

void submeshClipWithPlane(const submesh& inpsubmesh, //
                           dplane3& slicing_plane, //
                           bool close_mesh,
                           bool flip_orientation,
                           submesh& outsmeshFront, //
                           submesh& outsmeshBack
                           );

void submeshWithTextureBasis(const submesh& inpsubmesh, submesh& outsmesh);
void submeshWithTextureUnwrap(const submesh& inpsubmesh, submesh& outsmesh);

void submeshWithFaceNormals(const submesh& inpsubmesh, submesh& outsmesh);
void submeshWithSmoothNormals(const submesh& inpsubmesh, submesh& outsmesh, float threshold_radians);
void submeshJoinCoplanar(const submesh& inpsubmesh, submesh& outsmesh);
void submeshBarycentricUV(const submesh& inpsubmesh, submesh& outsmesh);
submesh_ptr_t submeshFromFrustum(const Frustum& frustum, bool projective_rect_uv);

std::vector<submesh_ptr_t> submeshBulletConvexDecomposition(const submesh& inpsubmesh);
void submeshConvexHull(const submesh& inpsubmesh, submesh& outsmesh, int steps = 0);

void submeshFixWindingOrder(const submesh& inpsubmesh, submesh& outsmesh, bool inside_out);

void submeshWriteObj(const submesh& inpsubmesh, const file::Path& BasePath);
// void SubDivQuads(submesh* poutsmesh) const;
// void SubDivTriangles(submesh* poutsmesh) const;
// void SubDiv(submesh* poutsmesh) const;

std::vector<polyset_ptr_t> splitByIsland(polyset_ptr_t inpset);

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
