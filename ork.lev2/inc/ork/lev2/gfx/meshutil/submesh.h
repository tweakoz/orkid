////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/lev2/config.h>
#include <ork/util/crc.h>
#include <ork/util/crc64.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/box.h>
#include <algorithm>
#include <ork/kernel/Array.h>
#include <ork/kernel/varmap.inl>

#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/targetinterfaces.h>
#include <unordered_map>
#include <ork/kernel/datablock.h>

namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////
typedef orkmap<std::string, svar64_t> AnnotationMap;
struct XgmClusterizer;
struct XgmClusterizerDiced;
struct XgmClusterizerStd;
///////////////////////////////////////////////////////////////////////////////

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

struct HashU6432 : public std::unary_function<U64, std::size_t> {
  std::size_t operator()(U64 v) const {
    U64 sh   = v >> 32;
    size_t h = size_t(sh);
    return h;
  }
  bool operator()(U64 s1, U64 s2) const {
    return s1 < s2;
  }
};

struct Hash3232 : public std::unary_function<int, std::size_t> {
  std::size_t operator()(int v) const {
    size_t h = size_t(v);
    return h;
  }
  bool operator()(int s1, int s2) const {
    return s1 < s2;
  }
};
///////////////////////////////////////////////////////////////////////////////

static const int kmaxpolysperedge = 4;

struct edge {
  edge();
  edge(vertex_ptr_t va, vertex_ptr_t vb);

  vertex_ptr_t edgeVertex(int iv) const;

  U64 GetHashKey(void) const;
  bool Matches(const edge& other) const;
  void ConnectToPoly(int ipoly);
  int GetNumConnectedPolys(void) const;
  int GetConnectedPoly(int ip) const;

  vertex_ptr_t _vertexA;
  vertex_ptr_t _vertexB;
  int miNumConnectedPolys;
  int miConnectedPolys[kmaxpolysperedge];
};

///////////////////////////////////////////////////////////////////////////////

struct uvmapcoord {
  uvmapcoord();
  void lerp(const uvmapcoord& ina, const uvmapcoord& inb, float flerp);
  uvmapcoord operator+(const uvmapcoord& ina) const;
  uvmapcoord operator*(const float Scalar) const;
  void Clear(void);

  fvec3 mMapBiNormal;
  fvec3 mMapTangent;
  fvec2 mMapTexCoord;
};

///////////////////////////////////////////////////////////////////////////////

struct vertex {
  static const int kmaxinfluences = 4;
  static const int kmaxcolors     = 2;
  static const int kmaxuvs        = 2;
  static const int kmaxconpoly    = 8;

  vertex();
  vertex(fvec3 pos, fvec3 nrm, fvec3 bin, fvec2 uv, fvec4 col);
  void set(fvec3 pos, fvec3 nrm, fvec3 bin, fvec2 uv, fvec4 col);

  vertex lerp(const vertex& vtx, float flerp) const;
  void lerp(const vertex& a, const vertex& b, float flerp);

  const fvec3& Pos() const;

  void Center(const vertex** pverts, int icnt);

  U64 Hash() const;

  uint32_t _poolindex = 0xffffffff;

  fvec3 mPos;
  fvec3 mNrm;

  int miNumWeights;
  int miNumColors;
  int miNumUvs;

  std::string mJointNames[kmaxinfluences];

  fvec4 mCol[kmaxcolors];
  uvmapcoord mUV[kmaxuvs];
  float mJointWeights[kmaxinfluences];
};

///////////////////////////////////////////////////////////////////////////////

struct vertexpool {

  vertexpool();
  vertex_ptr_t mergeVertex(const vertex& vtx);

  const vertex& GetVertex(size_t ivid) const {
    return *_orderedVertices[ivid].get();
  }
  vertex& GetVertex(size_t ivid) {
    return *_orderedVertices[ivid].get();
  }

  size_t GetNumVertices(void) const {
    return _orderedVertices.size();
  }

  static const vertexpool EmptyPool;

  std::unordered_map<uint64_t, vertex_ptr_t, HashU6432> _vtxmap;
  orkvector<vertex_ptr_t> _orderedVertices;
};

///////////////////////////////////////////////////////////////////////////////

struct AnnoMap {
  orkmap<std::string, std::string> _annotations;
  AnnoMap* Fork() const;
  static orkset<AnnoMap*> gAllAnnoSets;
  void SetAnnotation(const std::string& key, const std::string& val);
  const std::string& GetAnnotation(const std::string& annoname) const;

  AnnoMap();
  ~AnnoMap();
};

///////////////////////////////////////////////////////////////////////////////


struct poly {

  const AnnoMap* GetAnnoMap() const;
  void SetAnnoMap(const AnnoMap* pmap);

  const std::string& GetAnnotation(const std::string& annoname) const;

  int GetNumSides(void) const;
  int GetVertexID(int i) const;

  poly(
      vertex_ptr_t ia, //
      vertex_ptr_t ib,
      vertex_ptr_t ic);

  poly(
      vertex_ptr_t ia, //
      vertex_ptr_t ib,
      vertex_ptr_t ic,
      vertex_ptr_t id);

  poly(const std::vector<vertex_ptr_t>& vertices);

  //poly(const vertex_ptr_t verts[], int numSides);

  // vertex clockwise around the poly from the given one
  // int VertexCW(int vert) const;
  // vertex counter-clockwise around the poly from the given one
  // int VertexCCW(int vert) const;

  vertex ComputeCenter() const;
  float ComputeEdgeLength(const fmtx4& MatRange, int iedge) const;
  float ComputeArea(const fmtx4& MatRange) const;
  fvec3 ComputeNormal() const;
  fplane3 computePlane() const;
  
  U64 HashIndices(void) const;

  std::vector<vertex_ptr_t> _vertices;
  std::vector<edge_ptr_t> _edges;

  const AnnoMap* mAnnotationSet;
};

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
  virtual U64 HashItem(const submesh& tmesh, const poly& item) const = 0;
  const AnnoMap* Find(const submesh& tmesh, const poly& item) const;
};
struct annopolyposlut : public annopolylut {
  virtual U64 HashItem(const submesh& tmesh, const poly& item) const;
};

///////////////////////////////////////////////////////////////////////////////

struct submesh {

  submesh(vertexpool_ptr_t vpool = nullptr);
  ~submesh();

  //////////////////////////////////////////////////////////////////////////////

  void ImportPolyAnnotations(const annopolylut& apl);
  void ExportPolyAnnotations(annopolylut& apl) const;

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
    assert(false);
    static T rval;
    return rval;
  }

  //////////////////////////////////////////////////////////////////////////////

  const edge& RefEdge(U64 edgekey) const;
  poly& RefPoly(int i);
  const poly& RefPoly(int i) const;
  const orkvector<poly_ptr_t>& RefPolys() const;

  //////////////////////////////////////////////////////////////////////////////

  vertex_ptr_t mergeVertex(const vertex& vtx);
  edge_ptr_t MergeEdge(const edge& ed, int ipolyindex = -1);
  void MergePoly(const poly& ply);
  void MergeSubMesh(const submesh& oth);

  //////////////////////////////////////////////////////////////////////////////

  int GetNumPolys(int inumsides = 0) const;
  void FindNSidedPolys(orkvector<int>& output, int inumsides) const;
  void GetConnectedPolys(const edge& ed, orkset<int>& output) const;
  void GetEdges(const poly& ply, orkvector<edge>& Edges) const;
  void GetAdjacentPolys(int ply, orkset<int>& output) const;
  edge_constptr_t edgeBetween(int a, int b) const;

  /////////////////////////////////////////////////////////////////////////

  const AABox& aabox() const; /// compute axis aligned bounding box from the current state of the vertex pool

  bool isConvexHull() const;

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
  
  /////////////////////////////////////////////////////////////////////////

  std::string name;
  AnnotationMap _annotations;
  float _surfaceArea;
  vertexpool_ptr_t _vtxpool;
  std::unordered_map<uint64_t, edge_ptr_t, HashU6432> _edgemap;
  std::unordered_map<uint64_t, poly_ptr_t, HashU6432> _polymap;
  orkvector<poly_ptr_t> _orderedPolys;
  std::unordered_map<int,int> _polyTypeCounter;
  bool _mergeEdges;

  /////////////////////////////////////
  // these are mutable so we can get bounding boxes faster with const refs to Mesh's
  mutable AABox _aaBox;
  mutable bool _aaBoxDirty;
  /////////////////////////////////////
};

void submeshTriangulate(const submesh& inpsubmesh, submesh& outsmesh);

void submeshTrianglesToQuads(const submesh& inpsubmesh, 
                             submesh& outsmesh, 
                             float area_tolerance = 100.0f, // 1:100 .. 100:1
                             bool exclude_non_coplanar = true, //
                             bool exclude_non_rectangular = false //
                             );

void submeshSliceWithPlane(const submesh& inpsubmesh, //
                           fplane3& slicing_plane, //
                           submesh& outsmeshFront, //
                           submesh& outsmeshBack,
                           submesh& outsmeshIntersects
                           );

void submeshClipWithPlane(const submesh& inpsubmesh, //
                           fplane3& slicing_plane, //
                           submesh& outsmeshFront, //
                           submesh& outsmeshBack
                           );

void submeshWriteObj(const submesh& inpsubmesh, const file::Path& BasePath);
// void SubDivQuads(submesh* poutsmesh) const;
// void SubDivTriangles(submesh* poutsmesh) const;
// void SubDiv(submesh* poutsmesh) const;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
