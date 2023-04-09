////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

namespace ork::meshutil {

///////////////////////////////////////////////////////////////////////////////
using vertex_void_visitor_t = std::function<void(vertex_ptr_t)>;
using const_vertex_void_visitor_t = std::function<void(vertex_const_ptr_t)>;
using edge_map_t = std::unordered_map<uint64_t, edge_ptr_t>;
using poly_index_set_t = orkset<int>;
using poly_bool_visitor_t = std::function<bool(poly_ptr_t)>;
using poly_void_visitor_t = std::function<void(poly_ptr_t)>;
using const_poly_void_visitor_t = std::function<void(poly_const_ptr_t)>;
///////////////////////////////////////////////////////////////////////////////

template <typename T>
struct unique_set {
  using ptr_t = std::shared_ptr<T>;

  inline unique_set() {
  }
  inline unique_set(ptr_t initial) {
    insert(initial);
  }
  inline bool insert(ptr_t v){
    uint64_t h = v->hash();
    auto it = _the_map.find(h);
    if(it==_the_map.end()){
      _the_map[h]=v;
      return true;
    }
    return false;
  }
  inline bool contains(ptr_t v) const {
    return _the_map.find(v->hash())!=_the_map.end();
  }
  inline size_t size() const { return _the_map.size(); }
  inline bool remove(ptr_t v)  {
    auto it = _the_map.find(v->hash());
    if (it!=_the_map.end()) {
      _the_map.erase(it);
      return true;
    }
    return false;
  }
  inline ptr_t first()  {
    auto it = _the_map.begin();
    if (it!=_the_map.end()) {
      return it->second;
    }
    return nullptr;
  }
  inline void visit(const std::function<void(ptr_t)>& visitor) const {
    for(auto it : _the_map) {
      visitor(it.second);
    }
  }

  std::unordered_map<uint64_t,ptr_t> _the_map;
};

///////////////////////////////////////////////////////////////////////////////

struct edge {
  edge();
  edge(vertex_ptr_t va, vertex_ptr_t vb);

  vertex_ptr_t edgeVertex(int iv) const;

  uint64_t hash() const;
  bool Matches(const edge& other) const;
  submesh* submesh() const;

  vertex_ptr_t _vertexA;
  vertex_ptr_t _vertexB;
};

using edge_set_t = unique_set<edge>;

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
  vertex(dvec3 pos, dvec3 nrm, fvec3 bin, fvec2 uv, fvec4 col);
  vertex(const vertex& rhs);

  void set(fvec3 pos, fvec3 nrm, fvec3 bin, fvec2 uv, fvec4 col);
  void set(dvec3 pos, dvec3 nrm, fvec3 bin, fvec2 uv, fvec4 col);

  vertex lerp(const vertex& vtx, float flerp) const;
  void lerp(const vertex& a, const vertex& b, float flerp);

  const dvec3& Pos() const; 
  
  void Center(const vertex** pverts, int icnt);
  void center(const std::vector<vertex_ptr_t>& verts);

  uint64_t hash(double quantization=3333.0) const;

  void dump(const std::string& name) const;
  void clearAllExceptPosition();


  uint32_t _poolindex = 0xffffffff;

  dvec3 mPos;  
  dvec3 mNrm;

  int miNumWeights = 0;
  int miNumColors = 0;
  int miNumUvs = 0;

  std::string mJointNames[kmaxinfluences];

  fvec4 mCol[kmaxcolors];
  uvmapcoord mUV[kmaxuvs];
  float mJointWeights[kmaxinfluences];
  submesh* _parentSubmesh = nullptr;
};

using vertex_set_t = unique_set<vertex>;

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

  void rehash();

  static const vertexpool EmptyPool;

  std::unordered_map<uint64_t, vertex_ptr_t> _vtxmap;
  orkvector<vertex_ptr_t> _orderedVertices;
};

///////////////////////////////////////////////////////////////////////////////

struct AnnoMap {
  orkmap<std::string, std::string> _annotations;
  AnnoMap* Fork() const;
  void SetAnnotation(const std::string& key, const std::string& val);
  const std::string& GetAnnotation(const std::string& annoname) const;

  AnnoMap();
  ~AnnoMap();
};

///////////////////////////////////////////////////////////////////////////////


struct Polygon {

  const AnnoMap* GetAnnoMap() const;
  void SetAnnoMap(const AnnoMap* pmap);

  const std::string& GetAnnotation(const std::string& annoname) const;

  int GetNumSides(void) const;
  int GetVertexID(int i) const;

  Polygon(
      vertex_ptr_t ia, //
      vertex_ptr_t ib,
      vertex_ptr_t ic);

  Polygon(
      vertex_ptr_t ia, //
      vertex_ptr_t ib,
      vertex_ptr_t ic,
      vertex_ptr_t id);

  Polygon(const std::vector<vertex_ptr_t>& vertices);

  vertex computeCenter() const;
  dvec3 centerOfMass() const;
  double computeEdgeLength(const dmtx4& MatRange, int iedge) const;
  double minEdgeLength(const dmtx4& MatRange = dmtx4::Identity()) const;
  double maxEdgeLength(const dmtx4& MatRange = dmtx4::Identity()) const;
  double computeArea(const dmtx4& MatRange = dmtx4::Identity()) const;
  double signedVolumeWithPoint(const dvec3& point) const;
  dvec3 computeNormal() const;
  dplane3 computePlane() const;
  
  bool containsVertex(vertex_ptr_t v) const;
  bool containsVertex(vertex_const_ptr_t v) const;
  bool containsEdge(const edge& e, bool ordered = true) const;
  bool containsEdge(edge_ptr_t e, bool ordered = true) const;
  void visitEdges(const std::function<void(edge_ptr_t)>& visitor) const;
  edge_ptr_t edgeForVertices(vertex_ptr_t vA, vertex_ptr_t vB) const;

  uint64_t hash() const;
  edge_vect_t edges() const;
  std::vector<vertex_ptr_t> _vertices;
  int _submeshIndex = -1;
  submesh* _parentSubmesh = nullptr;
  varmap::VarMap _varmap;

  const AnnoMap* mAnnotationSet;
};

using poly_set_t = unique_set<Polygon>;

///////////////////////////////////////////////////////////////////////////////

struct PolySet {
  std::vector<island_ptr_t> splitByIsland() const;
  std::unordered_map<uint64_t,polyset_ptr_t> splitByPlane() const;
  std::unordered_set<poly_ptr_t> _polys;
  dvec3 averageNormal() const;
  submesh* submesh() const;
};

///////////////////////////////////////////////////////////////////////////////

struct Island : public PolySet {
  edge_vect_t boundaryLoop() const;
  edge_vect_t boundaryEdges() const;
};

///////////////////////////////////////////////////////////////////////////////

struct EdgeChain {

  std::string dump() const;
  void reverseOf(const EdgeChain& src);

  edge_vect_t _edges;
  std::unordered_set<vertex_ptr_t> _vertices;
};

///////////////////////////////////////////////////////////////////////////////

struct EdgeLoop {
  edge_vect_t _edges;
  void reverseOf(const EdgeLoop& src);
};

///////////////////////////////////////////////////////////////////////////////

struct EdgeChainLinker {
  edge_chain_ptr_t add_edge(edge_ptr_t e);
  bool loops_possible() const;
  edge_chain_ptr_t findChainForVertex(vertex_ptr_t va);
  void removeChain(edge_chain_ptr_t chain_to_remove);
  void closeChains();
  void link();

  std::vector<edge_chain_ptr_t> _edge_chains;
  std::vector<edge_loop_ptr_t> _edge_loops;
  std::unordered_map<vertex_ptr_t, int> _vtxrefcounts;
  std::string _name;
};

///////////////////////////////////////////////////////////////////////////////

struct IConnectivity{

  IConnectivity(submesh* sub);
  virtual ~IConnectivity();

  virtual poly_index_set_t polysConnectedToEdge(edge_ptr_t edge, bool ordered = true) const = 0;
  virtual poly_index_set_t polysConnectedToEdge(const edge& edge, bool ordered = true) const = 0;
  virtual poly_index_set_t polysConnectedToPoly(poly_ptr_t p) const = 0;
  virtual poly_index_set_t polysConnectedToPoly(int ip) const = 0;
  virtual poly_index_set_t polysConnectedToVertex(vertex_ptr_t v) const = 0;
  virtual vertex_ptr_t mergeVertex(const struct vertex& v) = 0;
  virtual poly_ptr_t mergePoly(const Polygon& p) = 0;
  virtual edge_ptr_t mergeEdge(const edge& ed) = 0;
  virtual vertex_ptr_t vertex(int id) const = 0;
  virtual poly_ptr_t poly(int id) const = 0;
  virtual size_t numPolys() const = 0;
  virtual size_t numVertices() const = 0;
  virtual void visitAllPolys(poly_void_visitor_t visitor) = 0;
  virtual void visitAllPolys(const_poly_void_visitor_t visitor) const = 0;
  virtual void visitAllVertices(vertex_void_visitor_t visitor) = 0;
  virtual void visitAllVertices(const_vertex_void_visitor_t visitor) const = 0;
  virtual edge_ptr_t edgeBetweenPolys(int aind, int bind) const = 0;
  virtual void removePoly(poly_ptr_t) = 0;
  submesh* _submesh = nullptr;

};

using connectivity_impl_ptr_t = std::shared_ptr<IConnectivity>;

struct DefaultConnectivity : public IConnectivity{
  DefaultConnectivity(submesh* sub);
  poly_index_set_t polysConnectedToEdge(edge_ptr_t edge, bool ordered = true) const final;
  poly_index_set_t polysConnectedToEdge(const edge& edge, bool ordered = true) const final;
  poly_index_set_t polysConnectedToPoly(poly_ptr_t p) const final;
  poly_index_set_t polysConnectedToPoly(int ip) const final;
  poly_index_set_t polysConnectedToVertex(vertex_ptr_t v) const final;
  edge_ptr_t edgeBetweenPolys(int aind, int bind) const final;

  vertex_ptr_t mergeVertex(const struct vertex& v) final;
  poly_ptr_t mergePoly(const Polygon& p) final;
  edge_ptr_t mergeEdge(const edge& ed) final;
  vertex_ptr_t vertex(int id) const final;
  poly_ptr_t poly(int id) const final;
  size_t numPolys() const final;
  size_t numVertices() const final;
  void removePoly(poly_ptr_t) final;


  void visitAllPolys(poly_void_visitor_t visitor) final;
  void visitAllPolys(const_poly_void_visitor_t visitor) const final;
  void visitAllVertices(vertex_void_visitor_t visitor) final;
  void visitAllVertices(const_vertex_void_visitor_t visitor) const final;

  vertexpool_ptr_t _vtxpool;
  std::unordered_map<uint64_t, poly_ptr_t> _polymap;
  orkvector<poly_ptr_t> _orderedPolys;
  std::unordered_map<int,int> _polyTypeCounter;

};

///////////////////////////////////////////////////////////////////////////////
} 
