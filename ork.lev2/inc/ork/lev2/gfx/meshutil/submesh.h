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

class edge {
  int miVertexA;
  int miVertexB;
  int miNumConnectedPolys;
  int miConnectedPolys[kmaxpolysperedge];

public:
  int GetVertexID(int iv) const {
    int id = -1;

    switch (iv) {
      case 0:
        id = miVertexA;
        break;
      case 1:
        id = miVertexB;
        break;
      default:
        OrkAssert(false);
        break;
    }

    return id;
  }

  U64 GetHashKey(void) const;
  bool Matches(const edge& other) const;

  edge()
      : miVertexA(-1)
      , miVertexB(-1)
      , miNumConnectedPolys(0) {
    for (int i = 0; i < kmaxpolysperedge; i++)
      miConnectedPolys[i] = -1;
  }

  edge(int iva, int ivb)
      : miNumConnectedPolys(0)
      , miVertexA(iva)
      , miVertexB(ivb) {
    for (int i = 0; i < kmaxpolysperedge; i++)
      miConnectedPolys[i] = -1;
  }

  void ConnectToPoly(int ipoly);

  int GetNumConnectedPolys(void) const {
    return miNumConnectedPolys;
  }

  int GetConnectedPoly(int ip) const {
    OrkAssert(ip < miNumConnectedPolys);
    return miConnectedPolys[ip];
  }
};

///////////////////////////////////////////////////////////////////////////////

struct uvmapcoord {
  fvec3 mMapBiNormal;
  fvec3 mMapTangent;
  fvec2 mMapTexCoord;

  void Lerp(const uvmapcoord& ina, const uvmapcoord& inb, float flerp);

  uvmapcoord operator+(const uvmapcoord& ina) const;
  uvmapcoord operator*(const float Scalar) const;

  uvmapcoord() {
  }

  void Clear(void) {
    mMapBiNormal = fvec3();
    mMapTangent  = fvec3();
    mMapTexCoord = fvec2();
  }
};

///////////////////////////////////////////////////////////////////////////////

struct vertex {
  static const int kmaxinfluences = 4;
  static const int kmaxcolors     = 2;
  static const int kmaxuvs        = 2;
  static const int kmaxconpoly    = 8;

  fvec3 mPos;
  fvec3 mNrm;

  int miNumWeights;
  int miNumColors;
  int miNumUvs;

  std::string mJointNames[kmaxinfluences];

  fvec4 mCol[kmaxcolors];
  uvmapcoord mUV[kmaxuvs];
  float mJointWeights[kmaxinfluences];

  vertex()
      : miNumWeights(0)
      , miNumColors(0)
      , miNumUvs(0) {
    for (int i = 0; i < kmaxcolors; i++) {
      mCol[i] = fvec4::White();
    }
    for (int i = 0; i < kmaxinfluences; i++) {
      mJointNames[i]   = "";
      mJointWeights[i] = float(0.0f);
    }
  }

  vertex(fvec3 pos, fvec3 nrm, fvec3 bin, fvec2 uv, fvec4 col)
      : vertex() {
    set(pos, nrm, bin, uv, col);
  }

  void set(fvec3 pos, fvec3 nrm, fvec3 bin, fvec2 uv, fvec4 col) {
    mPos                = pos;
    mNrm                = nrm;
    mUV[0].mMapTexCoord = uv;
    mUV[0].mMapBiNormal = bin;
    mCol[0]             = col;
    miNumColors         = 1;
    miNumUvs            = 1;
  }

  vertex Lerp(const vertex& vtx, float flerp) const;
  void Lerp(const vertex& a, const vertex& b, float flerp);

  const fvec3& Pos() const {
    return mPos;
  }

  void Center(const vertex** pverts, int icnt);

  U64 Hash() const;
};

///////////////////////////////////////////////////////////////////////////////

struct vertexpool {
  static const vertexpool EmptyPool;

  HashU64IntMap VertexPoolMap;
  orkvector<vertex> VertexPool;

  int MergeVertex(const vertex& vtx, int idx = -1);

  const vertex& GetVertex(int ivid) const {
    OrkAssert(orkvector<vertex>::size_type(ivid) < VertexPool.size());
    return VertexPool[ivid];
  }
  vertex& GetVertex(int ivid) {
    OrkAssert(orkvector<vertex>::size_type(ivid) < VertexPool.size());
    return VertexPool[ivid];
  }

  size_t GetNumVertices(void) const {
    return VertexPool.size();
  }

  vertexpool();
};

///////////////////////////////////////////////////////////////////////////////

struct AnnoMap {
  orkmap<std::string, std::string> mAnnotations;
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
  static const U64 Inv = 0xffffffffffffffffL;
  const AnnoMap* GetAnnoMap() const {
    return mAnnotationSet;
  }
  void SetAnnoMap(const AnnoMap* pmap) {
    mAnnotationSet = pmap;
  }

  const std::string& GetAnnotation(const std::string& annoname) const;

  int miVertices[kmaxsidesperpoly];
  U64 mEdges[kmaxsidesperpoly];
  int miNumSides;

  int GetNumSides(void) const {
    return miNumSides;
  }

  int GetVertexID(int i) const {
    OrkAssert(i < miNumSides);
    return miVertices[i];
  }

  poly()
      : miNumSides(0)
      , mAnnotationSet(0) {
    for (int i = 0; i < kmaxsidesperpoly; i++) {
      miVertices[i] = -1;
      mEdges[i]     = Inv;
    }
  }

  poly(int ia, int ib, int ic)
      : miNumSides(3)
      , mAnnotationSet(0) {
    miVertices[0] = ia;
    miVertices[1] = ib;
    miVertices[2] = ic;
    mEdges[0]     = Inv;
    mEdges[1]     = Inv;
    mEdges[2]     = Inv;
    for (int i = 3; i < kmaxsidesperpoly; i++) {
      miVertices[i] = -1;
      mEdges[i]     = Inv;
    }
  }

  poly(int ia, int ib, int ic, int id)
      : miNumSides(4)
      , mAnnotationSet(0) {
    miVertices[0] = ia;
    miVertices[1] = ib;
    miVertices[2] = ic;
    miVertices[3] = id;
    mEdges[0]     = Inv;
    mEdges[1]     = Inv;
    mEdges[2]     = Inv;
    mEdges[3]     = Inv;
    for (int i = 4; i < kmaxsidesperpoly; i++) {
      miVertices[i] = -1;
      mEdges[i]     = Inv;
    }
  }

  poly(const int verts[], int numSides)
      : miNumSides(numSides)
      , mAnnotationSet(0) {
    for (int i = 0; i < numSides; i++) {
      miVertices[i] = verts[i];
      mEdges[i]     = Inv;
    }
    for (int i = numSides; i < kmaxsidesperpoly; i++) {
      miVertices[i] = -1;
      mEdges[i]     = Inv;
    }
  }

  // vertex clockwise around the poly from the given one
  int VertexCW(int vert) const;
  // vertex counter-clockwise around the poly from the given one
  int VertexCCW(int vert) const;

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
    return mAnnotations;
  }
  const AnnotationMap& annotations() const {
    return mAnnotations;
  }

  void MergeAnnos(const AnnotationMap& mrgannos, bool boverwrite);

  svar64_t annotation(const char* annokey) const;

  template <typename T> T& typedAnnotation(const std::string annokey) {
    auto& anno = mAnnotations[annokey];
    if (anno.IsA<T>())
      return anno.Get<T>();
    return anno.Make<T>();
  }

  template <typename T> const T& typedAnnotation(const std::string annokey) const {
    auto it = mAnnotations.find(annokey);
    if (it != mAnnotations.end()) {
      const auto& anno = it->second;
      return anno.Get<T>();
    }
    assert(false);
    static T rval;
    return rval;
  }

  //////////////////////////////////////////////////////////////////////////////

  const vertexpool& RefVertexPool() const {
    return mvpool;
  }
  void SetVertexPool(const vertexpool& vpool) {
    mvpool = vpool;
  }

  //////////////////////////////////////////////////////////////////////////////

  const edge& RefEdge(U64 edgekey) const;
  poly& RefPoly(int i);
  const poly& RefPoly(int i) const;
  const orkvector<poly>& RefPolys() const;

  //////////////////////////////////////////////////////////////////////////////

  int MergeVertex(const vertex& vtx, int idx = -1);
  U64 MergeEdge(const edge& ed, int ipolyindex = -1);
  void MergePoly(const poly& ply);
  void MergeSubMesh(const submesh& oth);

  //////////////////////////////////////////////////////////////////////////////

  int GetNumPolys(int inumsides = 0) const;
  void FindNSidedPolys(orkvector<int>& output, int inumsides) const;
  void GetConnectedPolys(const edge& ed, orkset<int>& output) const;
  void GetEdges(const poly& ply, orkvector<edge>& Edges) const;
  void GetAdjacentPolys(int ply, orkset<int>& output) const;
  const U64 GetEdgeBetween(int a, int b) const;

  /////////////////////////////////////////////////////////////////////////

  const AABox& aabox() const; /// compute axis aligned bounding box from the current state of the vertex pool

  //////////////////////////////////////////////////////////////////////////////

  void
  addQuad(fvec3 p0, fvec3 p1, fvec3 p2, fvec3 p3, fvec2 uv0, fvec2 uv1, fvec2 uv2, fvec2 uv3, fvec4 c); /// add quad helper method

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
  AnnotationMap mAnnotations;
  float mfSurfaceArea;
  vertexpool mvpool;
  HashU64IntMap mpolyhashmap;
  orkvector<edge> mEdges;
  HashU64IntMap mEdgeMap;
  orkvector<poly> mMergedPolys;
  int mPolyTypeCounter[kmaxsidesperpoly];
  bool mbMergeEdges;

  /////////////////////////////////////
  // these are mutable so we can get bounding boxes faster with const refs to Mesh's
  mutable AABox mAABox;
  mutable bool mAABoxDirty;
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
