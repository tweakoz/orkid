////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

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
#include <ork/kernel/datablock.inl>

namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////
typedef orkmap<std::string, svar64_t> AnnotationMap;
struct XgmClusterizer;
struct XgmClusterizerDiced;
struct XgmClusterizerStd;
struct edge;
struct vertex;
struct vertexpool;
struct poly;

using edge_ptr_t       = std::shared_ptr<edge>;
using edge_constptr_t  = std::shared_ptr<const edge>;
using vertex_ptr_t     = std::shared_ptr<vertex>;
using vertexpool_ptr_t = std::shared_ptr<vertexpool>;
using poly_ptr_t       = std::shared_ptr<poly>;

///////////////////////////////////////////////////////////////////////////////

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

typedef std::unordered_map<U64, int, HashU6432> HashU64IntMap;
typedef std::unordered_map<int, int, Hash3232> HashIntIntMap;

static const int kmaxpolysperedge = 4;

struct edge {
  vertex_ptr_t _vertexA;
  vertex_ptr_t _vertexB;
  int miNumConnectedPolys;
  int miConnectedPolys[kmaxpolysperedge];

  vertex_ptr_t edgeVertex(int iv) const;

  U64 GetHashKey(void) const;
  bool Matches(const edge& other) const;

  edge();
  edge(vertex_ptr_t va, vertex_ptr_t vb);

  void ConnectToPoly(int ipoly);

  int GetNumConnectedPolys(void) const;
  int GetConnectedPoly(int ip) const;
};

///////////////////////////////////////////////////////////////////////////////

struct uvmapcoord {
  fvec3 mMapBiNormal;
  fvec3 mMapTangent;
  fvec2 mMapTexCoord;

  void Lerp(const uvmapcoord& ina, const uvmapcoord& inb, float flerp);

  uvmapcoord operator+(const uvmapcoord& ina) const;
  uvmapcoord operator*(const float Scalar) const;

  uvmapcoord();
  void Clear(void);
};

///////////////////////////////////////////////////////////////////////////////

struct vertex {
  static const int kmaxinfluences = 4;
  static const int kmaxcolors     = 2;
  static const int kmaxuvs        = 2;
  static const int kmaxconpoly    = 8;

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

  vertex();
  vertex(fvec3 pos, fvec3 nrm, fvec3 bin, fvec2 uv, fvec4 col);
  void set(fvec3 pos, fvec3 nrm, fvec3 bin, fvec2 uv, fvec4 col);

  vertex Lerp(const vertex& vtx, float flerp) const;
  void Lerp(const vertex& a, const vertex& b, float flerp);

  const fvec3& Pos() const;

  void Center(const vertex** pverts, int icnt);

  U64 Hash() const;
};

///////////////////////////////////////////////////////////////////////////////

struct vertexpool {
  static const vertexpool EmptyPool;

  std::unordered_map<uint64_t, vertex_ptr_t, HashU6432> _vtxmap;
  orkvector<vertex_ptr_t> _orderedVertices;

  vertex_ptr_t newMergeVertex(const vertex& vtx);

  const vertex& GetVertex(size_t ivid) const {
    return *_orderedVertices[ivid].get();
  }
  vertex& GetVertex(size_t ivid) {
    return *_orderedVertices[ivid].get();
  }

  size_t GetNumVertices(void) const {
    return _orderedVertices.size();
  }

  vertexpool();
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

static const int kmaxsidesperpoly = 5;

class poly {
  const AnnoMap* mAnnotationSet;

public:
  const AnnoMap* GetAnnoMap() const;
  void SetAnnoMap(const AnnoMap* pmap);

  const std::string& GetAnnotation(const std::string& annoname) const;

  vertex_ptr_t _vertices[kmaxsidesperpoly];
  edge_ptr_t mEdges[kmaxsidesperpoly];
  int miNumSides;

  int GetNumSides(void) const;
  int GetVertexID(int i) const;

  poly(
      vertex_ptr_t ia = nullptr, //
      vertex_ptr_t ib = nullptr,
      vertex_ptr_t ic = nullptr,
      vertex_ptr_t id = nullptr);

  poly(const vertex_ptr_t verts[], int numSides);

  // vertex clockwise around the poly from the given one
  // int VertexCW(int vert) const;
  // vertex counter-clockwise around the poly from the given one
  // int VertexCCW(int vert) const;

  vertex ComputeCenter(const vertexpool& vpool) const;
  float ComputeEdgeLength(const vertexpool& vpool, const fmtx4& MatRange, int iedge) const;
  float ComputeArea(const vertexpool& vpool, const fmtx4& MatRange) const;
  fvec3 ComputeNormal(const vertexpool& vpool) const;

  U64 HashIndices(void) const;
};

///////////////////////////////////////////////////////////////////////////////

struct IndexTestContext {
  int iset;
  int itest;
  orkset<int> PairedIndices[2];
  orkset<int> PairedIndicesCombined;
  orkset<int> CornerIndices;
};

struct submesh;

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

  submesh(const vertexpool& vpool = vertexpool::EmptyPool);
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
    if (anno.IsA<T>())
      return anno.Get<T>();
    return anno.Make<T>();
  }

  template <typename T> const T& typedAnnotation(const std::string annokey) const {
    auto it = _annotations.find(annokey);
    if (it != _annotations.end()) {
      const auto& anno = it->second;
      return anno.Get<T>();
    }
    assert(false);
    static T rval;
    return rval;
  }

  //////////////////////////////////////////////////////////////////////////////

  const vertexpool& RefVertexPool() const {
    return _vtxpool;
  }
  void SetVertexPool(const vertexpool& vpool) {
    _vtxpool = vpool;
  }

  //////////////////////////////////////////////////////////////////////////////

  const edge& RefEdge(U64 edgekey) const;
  poly& RefPoly(int i);
  const poly& RefPoly(int i) const;
  const orkvector<poly_ptr_t>& RefPolys() const;

  //////////////////////////////////////////////////////////////////////////////

  vertex_ptr_t newMergeVertex(const vertex& vtx);
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

  //////////////////////////////////////////////////////////////////////////////

  void addQuad(fvec3 p0, fvec3 p1, fvec3 p2, fvec3 p3, fvec2 uv0, fvec2 uv1, fvec2 uv2, fvec2 uv3, fvec4 c); /// add quad helper
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

  /////////////////////////////////////////////////////////////////////////

  std::string name;
  AnnotationMap _annotations;
  float _surfaceArea;
  vertexpool _vtxpool;
  std::unordered_map<uint64_t, edge_ptr_t, HashU6432> _edgemap;
  std::unordered_map<uint64_t, poly_ptr_t, HashU6432> _polymap;
  orkvector<poly_ptr_t> _orderedPolys;
  int _polyTypeCounter[kmaxsidesperpoly];
  bool _mergeEdges;

  /////////////////////////////////////
  // these are mutable so we can get bounding boxes faster with const refs to Mesh's
  mutable AABox _aaBox;
  mutable bool _aaBoxDirty;
  /////////////////////////////////////
};

void submeshTriangulate(const submesh& inpsubmesh, submesh& outsmesh);
void submeshTrianglesToQuads(const submesh& inpsubmesh, submesh& outsmesh);
void submeshWriteObj(const submesh& inpsubmesh, const file::Path& BasePath);
// void SubDivQuads(submesh* poutsmesh) const;
// void SubDivTriangles(submesh* poutsmesh) const;
// void SubDiv(submesh* poutsmesh) const;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
