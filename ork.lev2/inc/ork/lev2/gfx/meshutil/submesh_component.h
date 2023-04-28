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
//
using poly_index_set_t = orkset<int>;
//
using poly_bool_visitor_t = std::function<bool(poly_ptr_t)>;
using poly_void_visitor_t = std::function<void(poly_ptr_t)>;
using const_poly_void_visitor_t = std::function<void(poly_const_ptr_t)>;
//
using merged_poly_bool_visitor_t = std::function<bool(merged_poly_const_ptr_t)>;
using merged_poly_void_visitor_t = std::function<void(merged_poly_const_ptr_t)>;
using merged_poly_void_mutable_visitor_t = std::function<void(merged_poly_ptr_t)>;
using merged_poly_bool_mutable_visitor_t = std::function<bool(merged_poly_ptr_t)>;
//
using halfedge_void_visitor_t = std::function<void(halfedge_ptr_t)>;

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
  inline void clear(){
    _the_map.clear();
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
  varmap::VarMap _varmap;
};

using edge_set_t = unique_set<edge>;

struct HalfEdge {

  HalfEdge();
  static uint64_t hashStatic(vertex_const_ptr_t a, vertex_const_ptr_t b);

  uint64_t hash() const;
  submesh* submesh() const;

  // conditions for a valid manifold mesh
  //  _twin != nullptr
  //  _twin != this
  //  _twin->_twin == this


  vertex_ptr_t _vertexA;
  vertex_ptr_t _vertexB;
  halfedge_ptr_t _next;
  halfedge_ptr_t _twin; 
  poly_ptr_t _polygon;
};

using halfedge_set_t = unique_set<HalfEdge>;

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
  int _numConnectedPolys = 0;
};

using vertex_set_t = unique_set<vertex>;
using vertexconst_set_t = unique_set<const vertex>;

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
  dvec3 computeNormal() const; // using CCW rules
  dplane3 computePlane() const; // front of plane is facing poly normal using CCW rules
  void reverse();
  
  bool containsVertex(vertex_ptr_t v) const;
  bool containsVertex(vertex_const_ptr_t v) const;
  bool containsVertices(vertex_set_t v) const;
  bool containsEdge(const edge& e, bool ordered = true) const;
  bool containsEdge(edge_ptr_t e, bool ordered = true) const;
  void visitEdges(const std::function<void(edge_ptr_t)>& visitor) const;
  edge_ptr_t edgeForVertices(vertex_ptr_t vA, vertex_ptr_t vB) const;
  void visitVertices(const std::function<void(vertex_ptr_t)>& visitor) const;
  size_t numVertices() const;
  size_t numSides() const;
  int vertexID(int i) const;
  vertex_ptr_t vertex(int i) const;
  dvec3 vertexPos(int i) const;
  dvec3 vertexNormal(int i) const;
  //void addVertex(vertex_ptr_t v);

  uint64_t hash() const;
  edge_vect_t edges() const;

  const AnnoMap* mAnnotationSet;

  std::vector<vertex_ptr_t> _vertices;
};

struct MergedPolygon : public Polygon{
  MergedPolygon(
      vertex_ptr_t ia, //
      vertex_ptr_t ib,
      vertex_ptr_t ic);

  MergedPolygon(
      vertex_ptr_t ia, //
      vertex_ptr_t ib,
      vertex_ptr_t ic,
      vertex_ptr_t id);

  MergedPolygon(const std::vector<vertex_ptr_t>& vertices);

  int _submeshIndex = -1;
  submesh* _parentSubmesh = nullptr;
};

using poly_set_t = unique_set<Polygon>;
using polyconst_set_t = unique_set<const Polygon>;

using merged_poly_set_t = unique_set<MergedPolygon>;
using merged_polyconst_set_t = unique_set<const MergedPolygon>;

///////////////////////////////////////////////////////////////////////////////

struct PolyGroup {
  std::vector<island_ptr_t> splitByIsland() const;
  std::unordered_map<uint64_t,polygroup_ptr_t> splitByPlane() const;
  std::unordered_set<merged_poly_const_ptr_t> _polys;
  dvec3 averageNormal() const;
  submesh* submesh() const;
};

///////////////////////////////////////////////////////////////////////////////

struct Island : public PolyGroup {
  edge_vect_t boundaryLoop() const;
  edge_vect_t boundaryEdges() const;
};

///////////////////////////////////////////////////////////////////////////////

struct EdgeChain {

  std::string dump() const;
  void reverseOf(const EdgeChain& src);
  dvec3 center() const;
  dvec3 centroid() const;
  bool isPlanar() const;
  dvec3 avgNormalOfFaces() const; // CCW rules
  dvec3 avgNormalOfEdges() const; // CCW rules
  double planarDeviation() const;
  bool containsVertexID(int ivtx) const;
  bool containsVertexID(std::unordered_set<int>& verts) const;
  void visit(const std::function<void(edge_ptr_t)>& visitor) const;
  edge_vect_t _edges;
  std::vector<vertex_ptr_t> orderedVertices() const;
  std::unordered_set<vertex_ptr_t> _vertices;
};

///////////////////////////////////////////////////////////////////////////////

struct EdgeLoop : public EdgeChain {
};

///////////////////////////////////////////////////////////////////////////////

struct EdgeChainLinker {
  edge_chain_ptr_t add_edge(edge_ptr_t e);
  bool loops_possible() const;
  edge_chain_ptr_t findChainToLink(edge_chain_ptr_t ch);
  void removeChain(edge_chain_ptr_t chain_to_remove);
  void closeChains();
  void link();
  void clear();

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
  virtual poly_index_set_t polysConnectedToPoly(merged_poly_ptr_t p) const = 0;
  virtual poly_index_set_t polysConnectedToPoly(int ip) const = 0;
  virtual poly_set_t polysConnectedToVertex(vertex_ptr_t v) const = 0;
  virtual halfedge_vect_t edgesForPoly(merged_poly_const_ptr_t p) const = 0;
  virtual halfedge_ptr_t edgeForVertices(vertex_ptr_t a, vertex_ptr_t b) const = 0;
  virtual halfedge_ptr_t mergeEdgeForVertices(vertex_ptr_t a, vertex_ptr_t b) = 0;

  virtual vertex_ptr_t mergeVertex(const struct vertex& v) = 0;
  virtual merged_poly_ptr_t mergePoly(const Polygon& p) = 0;
  virtual vertex_ptr_t vertex(int id) const = 0;
  virtual merged_poly_ptr_t poly(int id) = 0;
  virtual merged_poly_const_ptr_t poly(int id) const = 0;
  virtual size_t numPolys() const = 0;
  virtual size_t numVertices() const = 0;

  virtual void visitAllEdges(halfedge_void_visitor_t visitor) = 0;
  virtual void visitAllPolys(merged_poly_void_mutable_visitor_t visitor) = 0;
  virtual void visitAllPolys(merged_poly_void_visitor_t visitor) const = 0;
  virtual void visitAllVertices(vertex_void_visitor_t visitor) = 0;
  virtual void visitAllVertices(const_vertex_void_visitor_t visitor) const = 0;
  virtual void removePoly(merged_poly_ptr_t) = 0;
  virtual void removePolys(std::vector<merged_poly_ptr_t>& polys) = 0;
  virtual void clearPolys() =0;
  virtual dvec3 centerOfPolys() const = 0;
  virtual varmap::VarMap& varmapForHalfEdge(halfedge_ptr_t he) = 0;
  virtual varmap::VarMap& varmapForVertex(vertex_const_ptr_t v) = 0;
  virtual varmap::VarMap& varmapForPolygon(merged_poly_const_ptr_t p) = 0;

  submesh* _submesh = nullptr;

};

using connectivity_impl_ptr_t = std::shared_ptr<IConnectivity>;

struct DefaultConnectivity : public IConnectivity{
  DefaultConnectivity(submesh* sub);
  poly_index_set_t polysConnectedToEdge(edge_ptr_t edge, bool ordered = true) const final;
  poly_index_set_t polysConnectedToEdge(const edge& edge, bool ordered = true) const final;
  poly_index_set_t polysConnectedToPoly(merged_poly_ptr_t p) const final;
  poly_index_set_t polysConnectedToPoly(int ip) const final;
  poly_set_t polysConnectedToVertex(vertex_ptr_t v) const;
  halfedge_vect_t edgesForPoly(merged_poly_const_ptr_t p) const;
  halfedge_ptr_t edgeForVertices(vertex_ptr_t a, vertex_ptr_t b) const final;
  halfedge_ptr_t mergeEdgeForVertices(vertex_ptr_t a, vertex_ptr_t b) final;

  vertex_ptr_t mergeVertex(const struct vertex& v) final;
  merged_poly_ptr_t mergePoly(const Polygon& p) final;
  vertex_ptr_t vertex(int id) const final;
  merged_poly_ptr_t poly(int id) final;
  merged_poly_const_ptr_t poly(int id) const final;
  size_t numPolys() const final;
  size_t numVertices() const final;
  void removePoly(merged_poly_ptr_t) final;
  void removePolys(std::vector<merged_poly_ptr_t>& polys) final;
  void clearPolys() final;

  void visitAllEdges(halfedge_void_visitor_t visitor) final;
  void visitAllPolys(merged_poly_void_mutable_visitor_t visitor) final;
  void visitAllPolys(merged_poly_void_visitor_t visitor) const final;
  void visitAllVertices(vertex_void_visitor_t visitor) final;
  void visitAllVertices(const_vertex_void_visitor_t visitor) const final;
  dvec3 centerOfPolys() const final;

  halfedge_ptr_t _makeHalfEdge(vertex_ptr_t a,
                               vertex_ptr_t b, 
                               merged_poly_ptr_t p);

  varmap::VarMap& varmapForHalfEdge(halfedge_ptr_t he);
  varmap::VarMap& varmapForVertex(vertex_const_ptr_t v);
  varmap::VarMap& varmapForPolygon(merged_poly_const_ptr_t p);

  vertexpool_ptr_t _vtxpool;
  std::unordered_map<uint64_t, merged_poly_ptr_t> _polymap;
  std::unordered_map<vertex_ptr_t, poly_set_t> _polys_by_vertex;
  orkvector<merged_poly_ptr_t> _orderedPolys;
  std::unordered_map<int,int> _polyTypeCounter;
  std::unordered_map<uint64_t,varmap::VarMap> _halfedge_varmap;
  std::unordered_map<uint64_t,varmap::VarMap> _vertex_varmap;
  std::unordered_map<merged_poly_const_ptr_t,varmap::VarMap> _poly_varmap;

  dvec3 _centerOfPolysAccum;
  int _centerOfPolysCount = 0;

  std::unordered_map<merged_poly_const_ptr_t, halfedge_vect_t> _halfedges_by_poly;
  std::unordered_map<uint64_t, halfedge_ptr_t> _halfedge_map;

};

///////////////////////////////////////////////////////////////////////////////
} 
