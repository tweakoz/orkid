////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/orklut.hpp>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/util/logger.h>

template class ork::orklut<std::string, ork::meshutil::submesh_ptr_t>;

namespace ork::meshutil {
static logchannel_ptr_t logchan_submesh = logger()->createChannel("meshutil.submesh", fvec3(.9, .9, 1));

const vertexpool vertexpool::EmptyPool;

/////////////////////////////////////////////////////////////////////////
submesh::submesh()
    : _surfaceArea(0)
    , _concmutex("submesh") {

  _connectivityIMPL = std::make_shared<DefaultConnectivity>(this);
}

/////////////////////////////////////////////////////////////////////////

void submesh::inheritParams( const submesh* from ){
  auto con_from = std::dynamic_pointer_cast<const DefaultConnectivity>(from->_connectivityIMPL);
  auto con_to = std::dynamic_pointer_cast<DefaultConnectivity>(_connectivityIMPL);
  con_to->_enable_zero_area_check = con_from->_enable_zero_area_check;
}

/////////////////////////////////////////////////////////////////////////
submesh::~submesh() {
}

///////////////////////////////////////////////////////////////////////////////
uint64_t submesh::hash() const{
  boost::Crc64 crc64;
  crc64.init();

  visitAllVertices( [&](vertex_const_ptr_t vtx) {
    crc64.accumulateItem<uint64_t>(vtx->hash());
  });

  crc64.finish();
  return crc64.result();
}
///////////////////////////////////////////////////////////////////////////////
void submesh::removePoly(merged_poly_ptr_t p) {
  _connectivityIMPL->removePoly(p);
}
///////////////////////////////////////////////////////////////////////////////
void submesh::removePolys(std::vector<merged_poly_ptr_t>& polys){
  _connectivityIMPL->removePolys(polys);
}
///////////////////////////////////////////////////////////////////////////////
void submesh::clearPolys(){
  _connectivityIMPL->clearPolys();
}
///////////////////////////////////////////////////////////////////////////////
merged_poly_ptr_t submesh::mergePoly(const Polygon& ply) {
  auto p = _connectivityIMPL->mergePoly(ply);
  if (p) {
    if (p->_parentSubmesh == nullptr) {
      p->_parentSubmesh = this;
      _surfaceArea += p->computeArea(ork::dmtx4::Identity());
      _aaBoxDirty = true;
    }
  }
  return p;
}
merged_poly_ptr_t submesh::mergePolyConcurrent(const Polygon& ply) {
  _concmutex.Lock();
  auto merged = mergePoly(ply);
  _concmutex.UnLock();
  return merged;
}
///////////////////////////////////////////////////////////////////////////////
merged_poly_ptr_t submesh::mergePoly(const vertex_vect_t& vertices){
  return mergePoly(Polygon(vertices));
}
///////////////////////////////////////////////////////////////////////////////
merged_poly_ptr_t submesh::mergeQuad(vertex_ptr_t va, vertex_ptr_t vb, vertex_ptr_t vc, vertex_ptr_t vd) {
  return mergePoly(Polygon(va, vb, vc, vd));
}
///////////////////////////////////////////////////////////////////////////////
merged_poly_ptr_t submesh::mergeTriangle(vertex_ptr_t va, vertex_ptr_t vb, vertex_ptr_t vc) {
  return mergePoly(Polygon(va, vb, vc));
}
merged_poly_ptr_t submesh::mergeTriangleConcurrent(vertex_ptr_t va, vertex_ptr_t vb, vertex_ptr_t vc) {
  return mergePolyConcurrent(Polygon(va, vb, vc));
}
///////////////////////////////////////////////////////////////////////////////
merged_poly_ptr_t submesh::mergeUnorderedTriangle(vertex_ptr_t va, vertex_ptr_t vb, vertex_ptr_t vc) {

  if( numPolys() == 0 ){
    return mergeTriangle(va, vb, vc);
  }

  auto e0         = std::make_shared<edge>(va, vb);
  auto e1         = std::make_shared<edge>(vb, vc);
  auto e2         = std::make_shared<edge>(vc, va);
  auto con_polys_ab = connectedPolys(e0, false);
  auto con_polys_bc = connectedPolys(e1, false);
  auto con_polys_ca = connectedPolys(e2, false);
  int matching_order = 0;
  int not_matching_order = 0;
  for (auto ic : con_polys_ab) {
    int icon_poly = *con_polys_ab.begin();
    auto con_poly = poly(icon_poly);
    if (con_poly->edgeForVertices(vb, va)) {
      not_matching_order++;
    }
    else{
      matching_order++;
    }
  }
  for (auto ic : con_polys_bc) {
    int icon_poly = *con_polys_bc.begin();
    auto con_poly = poly(icon_poly);
    if (con_poly->edgeForVertices(vc, vb)) {
      not_matching_order++;
    }
    else{
      matching_order++;
    }
  }
  for (auto ic : con_polys_ca) {
    int icon_poly = *con_polys_ca.begin();
    auto con_poly = poly(icon_poly);
    if (con_poly->edgeForVertices(va, vc)) {
      not_matching_order++;
    }
    else{
      matching_order++;
    }
  }
  printf( "matching_order<%d>\n", matching_order );
  printf( "not_matching_order<%d>\n", not_matching_order );
  
  merged_poly_ptr_t pmerged = nullptr;

  if( matching_order < not_matching_order ){
    pmerged = mergeTriangle(va, vb, vc);
  }
  else if( matching_order > not_matching_order ){
    pmerged = mergeTriangle(va, vc, vb);
  }
  else if( matching_order == not_matching_order ) {
    pmerged = mergeTriangle(va, vb, vc);
  }
  else{
    OrkAssert(false);
  }

  return pmerged;
}
///////////////////////////////////////////////////////////////////////////////
vertex_ptr_t submesh::vertex(int i) const {
  return _connectivityIMPL->vertex(i);
}
///////////////////////////////////////////////////////////////////////////////
merged_poly_const_ptr_t submesh::poly(int i) const {
  return _connectivityIMPL->poly(i);
}
///////////////////////////////////////////////////////////////////////////////
merged_poly_ptr_t submesh::poly(int i) {
  return _connectivityIMPL->poly(i);
}
///////////////////////////////////////////////////////////////////////////////
void submesh::visitAllEdges(halfedge_void_visitor_t visitor) {
  _connectivityIMPL->visitAllEdges(visitor);
}
///////////////////////////////////////////////////////////////////////////////
void submesh::visitAllVertices(vertex_void_visitor_t visitor) {
  _connectivityIMPL->visitAllVertices(visitor);
}
///////////////////////////////////////////////////////////////////////////////
void submesh::visitAllVertices(const_vertex_void_visitor_t visitor) const {
  auto const_con = const_cast<const IConnectivity*>(_connectivityIMPL.get());
  const_con->visitAllVertices(visitor);
}
/////////////////////////////////////////////////////////////////////////////////
//void submesh::visitAllPolys(poly_void_visitor_t visitor) {
//  _connectivityIMPL->visitAllPolys(visitor);
//}
///////////////////////////////////////////////////////////////////////////////
void submesh::visitAllPolys(merged_poly_void_mutable_visitor_t visitor) {
  _connectivityIMPL->visitAllPolys(visitor);
}
void submesh::visitAllPolys(merged_poly_void_visitor_t visitor) const {
  auto const_con = const_cast<const IConnectivity*>(_connectivityIMPL.get());
  const_con->visitAllPolys(visitor);
}
///////////////////////////////////////////////////////////////////////////////
vertex_ptr_t submesh::mergeVertex(const struct vertex& vtx) {
  _aaBoxDirty            = true;
  auto merged            = _connectivityIMPL->mergeVertex(vtx);
  merged->_parentSubmesh = this;
  return merged;
}

vertex_ptr_t submesh::mergeVertexConcurrent(const struct vertex& vtx) {
  _concmutex.Lock();
  auto merged = mergeVertex(vtx);
  _concmutex.UnLock();
  return merged;
}

///////////////////////////////////////////////////////////////////////////////
Polygon& submesh::RefPoly(int i) {
  return *_connectivityIMPL->poly(i);
}
///////////////////////////////////////////////////////////////////////////////
const Polygon& submesh::RefPoly(int i) const {
  return *_connectivityIMPL->poly(i);
}
///////////////////////////////////////////////////////////////////////////////
int submesh::numVertices() const {
  return _connectivityIMPL->numVertices();
}
///////////////////////////////////////////////////////////////////////////////
int submesh::numPolys(int inumsides) const {
  int count = 0;
  if( inumsides==0 ){
    count = _connectivityIMPL->numPolys();
  }
  else{
    visitAllPolys([&](merged_poly_const_ptr_t p) {
      if(inumsides==0){
        count++;
      }
      else if (p->numVertices() == inumsides) {
        count++;
      }
    });
  }
  return count;
}
/////////////////////////////////////////////////////////////////////////
svar64_t submesh::annotation(const char* annokey) const {
  static const char* defret("");
  auto it = _annotations.find(std::string(annokey));
  if (it != _annotations.end()) {
    return (*it).second;
  }
  return defret;
}
/////////////////////////////////////////////////////////////////////////
void submesh::MergeAnnos(const AnnotationMap& mrgannos, bool boverwrite) {
  for (AnnotationMap::const_iterator it = mrgannos.begin(); it != mrgannos.end(); it++) {
    const std::string& key      = it->first;
    const auto& val             = it->second;
    AnnotationMap::iterator itf = _annotations.find(key);
    if (itf == _annotations.end()) {
      _annotations[key] = val;
    } else if (boverwrite) {
      itf->second = val;
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
void submesh::importPolyAnnotations(const annopolylut& apl) {
  visitAllPolys([&](merged_poly_ptr_t p) {
    const AnnoMap* amap = apl.Find(*this, *p);
    p->SetAnnoMap(amap);
  });
}
///////////////////////////////////////////////////////////////////////////////
void submesh::exportPolyAnnotations(annopolylut& apl) const {
  visitAllPolys([&](merged_poly_const_ptr_t p) {
    uint64_t uhash      = apl.HashItem(*this, *p);
    const AnnoMap* amap = p->GetAnnoMap();
    apl.mAnnoMap[uhash] = amap;
  });
}
///////////////////////////////////////////////////////////////////////////////
const AABox& submesh::aabox() const {
  if (_aaBoxDirty) {
    _aaBoxDirty = false;
    _aaBox.BeginGrow();
    visitAllVertices([&](vertex_const_ptr_t v) {
      _aaBox.Grow(dvec3_to_fvec3(v->mPos));
    });
    _aaBox.EndGrow();
  }
  return _aaBox;
}
/////////////////////////////////////////////////////////////////////////
void submesh::FindNSidedPolys(orkvector<int>& output, int inumsides) const {
  visitAllPolys([&](merged_poly_const_ptr_t p) {
    if (p->numVertices() == inumsides) {
      output.push_back(p->_submeshIndex);
    }
  });
}
///////////////////////////////////////////////////////////////////////////////
poly_index_set_t submesh::adjacentPolys(int ply) const {
  return _connectivityIMPL->polysConnectedToPoly(ply);
}
///////////////////////////////////////////////////////////////////////////////
poly_index_set_t submesh::connectedPolys(edge_ptr_t ed, bool ordered) const { //
  return _connectivityIMPL->polysConnectedToEdge(ed, ordered);
}
///////////////////////////////////////////////////////////////////////////////
poly_index_set_t submesh::connectedPolys(const edge& ed, bool ordered) const { //
  return _connectivityIMPL->polysConnectedToEdge(ed, ordered);
}
///////////////////////////////////////////////////////////////////////////////
dvec3 submesh::boundingMin() const{
  dvec3 min(1e9,1e9,1e9);
  visitAllVertices([&](vertex_const_ptr_t v) {
    auto pos = v->mPos;
    if(pos.x<min.x){
      min.x = pos.x;
    }
    if(pos.y<min.y){
      min.y = pos.y;
    }
    if(pos.z<min.z){
      min.z = pos.z;
    }
  });
  return min;
}
dvec3 submesh::boundingMax() const{
  dvec3 max(-1e9,-1e9,-1e9);
  visitAllVertices([&](vertex_const_ptr_t v) {
    auto pos = v->mPos;
    if(pos.x>max.x){
      max.x = pos.x;
    }
    if(pos.y>max.y){
      max.y = pos.y;
    }
    if(pos.z>max.z){
      max.z = pos.z;
    }
  });
  return max;
}
///////////////////////////////////////////////////////////////////////////////
void submesh::MergeSubMesh(const submesh& inp_mesh) {
  //float ftimeA     = float(OldSchool::GetRef().GetLoResTime());
  int inumpingroup = 0;
  inp_mesh.visitAllPolys([&](merged_poly_const_ptr_t p) {
    std::vector<vertex_ptr_t> new_vertices;
    for (int iv = 0; iv < p->numVertices(); iv++) {
      int ivi      = p->vertexID(iv);
      auto src_vtx = inp_mesh.vertex(ivi);
      new_vertices.push_back(mergeVertex(*src_vtx));
    }
    poly_ptr_t new_poly = std::make_shared<Polygon>(new_vertices);
    new_poly->SetAnnoMap(p->GetAnnoMap());
    mergePoly(*new_poly);
    inumpingroup++;
  });
  //logchan_submesh->log("inumpingroup<%d> numoutpolys<%d>", inumpingroup, numPolys());
  //float ftimeB = float(OldSchool::GetRef().GetLoResTime());
  //float ftime  = (ftimeB - ftimeA);
  //logchan_submesh->log("<<PROFILE>> <<submesh::MergeSubMesh %f seconds>>", ftime);
}
///////////////////////////////////////////////////////////////////////////////
void submesh::mergePolyGroup(const PolyGroup& pset) {
  for (auto p : pset._polys) {
    std::vector<vertex_ptr_t> merged_vertices;
    p->visitVertices([&](vertex_const_ptr_t v) {
      merged_vertices.push_back(mergeVertex(*v));
    });
    mergePoly(Polygon(merged_vertices));
  }
  _aaBoxDirty = true;
}
///////////////////////////////////////////////////////////////////////////////
polygroup_ptr_t submesh::asPolyGroup() const {
  polygroup_ptr_t rval = std::make_shared<PolyGroup>();
  visitAllPolys([&](merged_poly_const_ptr_t p) {
    // todo : fix const
    auto as_non_const = std::const_pointer_cast<MergedPolygon>(p);
    rval->_polys.insert(as_non_const);
  });
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
// addPoly helper methods
///////////////////////////////////////////////////////////////////////////////
void submesh::addQuad(dvec3 p0, dvec3 p1, dvec3 p2, dvec3 p3, dvec4 c) {
  struct vertex muvtx[4];

  dvec3 B = (p0 - p1).normalized();
  dvec3 T = (p2 - p0).normalized();
  dvec3 N = B.crossWith(T);
  fvec4 cf = dvec4_to_fvec4(c);
  muvtx[0].set(p0, N, fvec3(), fvec2(), cf);
  muvtx[1].set(p1, N, fvec3(), fvec2(), cf);
  muvtx[2].set(p2, N, fvec3(), fvec2(), cf);
  muvtx[3].set(p3, N, fvec3(), fvec2(), cf);
  auto v0 = mergeVertex(muvtx[0]);
  auto v1 = mergeVertex(muvtx[1]);
  auto v2 = mergeVertex(muvtx[2]);
  auto v3 = mergeVertex(muvtx[3]);
  mergePoly(Polygon(v0, v1, v2, v3));
}
///////////////////////////////////////////////////////////////////////////////
void submesh::addQuad(dvec3 p0, dvec3 p1, dvec3 p2, dvec3 p3, dvec2 uv0, dvec2 uv1, dvec2 uv2, dvec2 uv3, dvec4 c) {
  struct vertex muvtx[4];
  dvec3 p0p1 = (p1 - p0).normalized();
  dvec3 p0p2 = (p2 - p0).normalized();
  dvec3 nrm  = p0p1.crossWith(p0p2);
  // todo compute tangent space from uv gradients
  fvec3 bin = dvec3_to_fvec3(p0p1);
  fvec4 cf = dvec4_to_fvec4(c);
  muvtx[0].set(p0, nrm, bin, dvec2_to_fvec2(uv0), cf);
  muvtx[1].set(p1, nrm, bin, dvec2_to_fvec2(uv1), cf);
  muvtx[2].set(p2, nrm, bin, dvec2_to_fvec2(uv2), cf);
  muvtx[3].set(p3, nrm, bin, dvec2_to_fvec2(uv3), cf);

  auto v0 = mergeVertex(muvtx[0]);
  auto v1 = mergeVertex(muvtx[1]);
  auto v2 = mergeVertex(muvtx[2]);
  auto v3 = mergeVertex(muvtx[3]);
  mergePoly(Polygon(v0, v1, v2, v3));
}
///////////////////////////////////////////////////////////////////////////////
void submesh::addQuad(
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
    dvec4 c) { /// add quad helper method
  struct vertex muvtx[4];
  dvec3 p0p1 = (p1 - p0).normalized();
  fvec3 bin  = dvec3_to_fvec3(p0p1);
  fvec4 cf  = dvec4_to_fvec4(c);
  muvtx[0].set(p0, n0, bin, dvec2_to_fvec2(uv0), cf);
  muvtx[1].set(p1, n1, bin, dvec2_to_fvec2(uv1), cf);
  muvtx[2].set(p2, n2, bin, dvec2_to_fvec2(uv2), cf);
  muvtx[3].set(p3, n3, bin, dvec2_to_fvec2(uv3), cf);

  auto v0 = mergeVertex(muvtx[0]);
  auto v1 = mergeVertex(muvtx[1]);
  auto v2 = mergeVertex(muvtx[2]);
  auto v3 = mergeVertex(muvtx[3]);
  mergePoly(Polygon(v0, v1, v2, v3));
}
///////////////////////////////////////////////////////////////////////////////
void submesh::addQuad(
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
    dvec4 c) { /// add quad helper method
  struct vertex muvtx[4];

  fvec3 fb0 = dvec3_to_fvec3(b0);
  fvec3 fb1 = dvec3_to_fvec3(b1);
  fvec3 fb2 = dvec3_to_fvec3(b2);
  fvec3 fb3 = dvec3_to_fvec3(b3);
  fvec2 fuv0 = dvec2_to_fvec2(uv0);
  fvec2 fuv1 = dvec2_to_fvec2(uv1);
  fvec2 fuv2 = dvec2_to_fvec2(uv2);
  fvec2 fuv3 = dvec2_to_fvec2(uv3);
  fvec4 cf  = dvec4_to_fvec4(c);
  
  muvtx[0].set(p0, n0, fb0, fuv0, cf);
  muvtx[1].set(p1, n1, fb1, fuv1, cf);
  muvtx[2].set(p2, n2, fb2, fuv2, cf);
  muvtx[3].set(p3, n3, fb3, fuv3, cf);

  auto v0 = mergeVertex(muvtx[0]);
  auto v1 = mergeVertex(muvtx[1]);
  auto v2 = mergeVertex(muvtx[2]);
  auto v3 = mergeVertex(muvtx[3]);
  mergePoly(Polygon(v0, v1, v2, v3));
}

///////////////////////////////////////////////////////////////////////////////

bool submesh::isConvexHull() const {
  int front = 0;
  int back  = 0;
  visitAllPolys([&](merged_poly_const_ptr_t p1) {
    auto pl = p1->computePlane();
    printf( "poly [ " );
    for( int i=0; i<p1->numVertices(); i++ ){
      printf( "%d ", p1->vertexID(i) );
    }
    printf( "] ");
    printf( "plane<%f %f %f> <%f>\n", p1.get(), pl.n.x, pl.n.y, pl.n.z, pl.d );
    visitAllPolys([&](merged_poly_const_ptr_t p2) {
      if (p1 != p2) {
        p2->visitVertices([&](vertex_const_ptr_t v) {
          switch(pl.classifyPoint(v->mPos)) {
            case PointClassification::FRONT:
            case PointClassification::COPLANAR:
              front++;
              break;
            case PointClassification::BACK:{
              back++;
              double distance = pl.pointDistance(v->mPos);
              int vindex = v->_poolindex;
              auto d =FormatString( "vd<%d> distance<%.e>", vindex, 10, distance );
              printf( "%s\n", d.c_str());
              //v->dump(d);
              break;
            }
          }
        });  
      }
    });
    //bool is_convex = (front > 0) and (back == 0);
    //OrkAssert(is_convex);
  });
   printf( "front<%d> back<%d>\n", front, back );
  bool is_convex = (front > 0) and (back == 0);
  //OrkAssert(is_convex);
  return is_convex;
}

///////////////////////////////////////////////////////////////////////////////

void submesh::copy(
    submesh& dest,                   //
    bool preserve_normals,           //
    bool preserve_colors,            //
    bool preserve_texcoords) const { //

  std::unordered_map<int, int> vtx_map;

  // copy vertices

  visitAllVertices([&](vertex_const_ptr_t v) {
    auto temp_v  = std::make_shared<struct vertex>();
    temp_v->mPos = v->mPos;
    if (preserve_normals) {
      temp_v->mNrm = v->mNrm;
    }
    if (preserve_colors) {
      for (int ic = 0; ic < vertex::kmaxcolors; ic++) {
        temp_v->mCol[ic] = v->mCol[ic];
      }
      temp_v->miNumColors = v->miNumColors;
    }
    if (preserve_texcoords) {
      for (int it = 0; it < vertex::kmaxuvs; it++) {
        temp_v->mUV[it] = v->mUV[it];
      }
      temp_v->miNumUvs = v->miNumUvs;
    }
    auto new_v             = dest.mergeVertex(*temp_v);
    vtx_map[v->_poolindex] = new_v->_poolindex;
  });

  // copy polys

  visitAllPolys([&](merged_poly_const_ptr_t p) {
    std::vector<vertex_ptr_t> newverts;
    p->visitVertices([&](vertex_const_ptr_t v) {
      auto it = vtx_map.find(v->_poolindex);
      OrkAssert(it != vtx_map.end());
      int v_index = it->second;
      auto newv   = dest.vertex(v_index);
      newverts.push_back(newv);
    });
    auto newp = std::make_shared<Polygon>(newverts);
    dest.mergePoly(*newp);
  });

  // copy misc

  dest.name         = name;
  dest._annotations = _annotations;
  dest._aaBoxDirty  = true;
  dest._surfaceArea = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

dvec3 submesh::centerOfVertices() const {
  size_t num_verts = 0;
  dvec3 accum;
  visitAllVertices([&](vertex_const_ptr_t v) {
    num_verts++;
    accum += v->mPos;
  });
  return accum * 1.0 / double(num_verts);
}

///////////////////////////////////////////////////////////////////////////////

dvec3 submesh::centerOfPolys() const {
  return _connectivityIMPL->centerOfPolys();
}

///////////////////////////////////////////////////////////////////////////////

dvec3 submesh::centerOfPolysConcurrent() const {
  //_concmutex.Lock();
  auto rval = _connectivityIMPL->centerOfPolys();
  //_concmutex.UnLock();
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void submesh::dumpPolys(std::string hdr, bool showverts) const{
  int index = 0;
  if( hdr.length() )
    printf( "polys <%s>: \n", hdr.c_str() );
  visitAllPolys([&](merged_poly_const_ptr_t p) {
    printf( "  p %d [ ", index );
    p->visitVertices([&](vertex_const_ptr_t v) {
      if( showverts ){
        printf( "%d<%f %f %f> ", v->_poolindex, v->mPos.x, v->mPos.y, v->mPos.z );
      }
      else{
        printf( "%d ", v->_poolindex );
      }
    });
    printf( "]\n" );
    index++;
  });
}
  
///////////////////////////////////////////////////////////////////////////////

bool submesh::isVertexInsideConvexHull(vertex_const_ptr_t vtx) const{

  bool rval = true;

  visitAllPolys([&](merged_poly_const_ptr_t p) {
    auto pl = p->computePlane();
    pl.n = -pl.n;
    pl.d = -pl.d;
    auto pc = pl.classifyPoint(vtx->mPos);
    if (pc == PointClassification::BACK) {
      rval = false;
    }
  });

  return rval;
}

///////////////////////////////////////////////////////////////////////////////
double submesh::convexVolume() const {
  double volume = 0.0f;
  dvec3 c       = centerOfVertices();
  visitAllPolys([&](merged_poly_const_ptr_t p) {
    int numsides = p->numSides();
    OrkAssert(numsides == 3);
    const auto& v0 = p->vertexPos(0);
    const auto& v1 = p->vertexPos(1);
    const auto& v2 = p->vertexPos(2);

    double U = (v0 - v1).length();
    double V = (v1 - v2).length();
    double W = (v2 - v0).length();

    double u = (v2 - c).length();
    double v = (v0 - c).length();
    double w = (v1 - c).length();

    double usq = u * u;
    double vsq = v * v;
    double wsq = w * w;

    double sqU   = U * U;
    double sqV   = V * V;
    double sqW   = W * W;
    double termA = vsq + wsq - sqU;
    double termB = wsq + usq - sqV;
    double termC = usq + vsq - sqW;

    // sqrt(4*u*u*v*v*w*w – u*u*(v*v + w*w – U*U)^2 – v*v(w*w + u*u – V*V)^2 – w*w(u*u + v*v – W*W)^2 + (u*u + v*v – W*W) * (w*w +
    // u*u – V*V) * (v*v + w*w – U*U)) / 12

    double this_vol =
        sqrt(4.0 * usq * vsq * wsq - usq * termA * termA - vsq * termB * termB - wsq * termC * termC + (termC * termB * termA)) /
        12.0;
    volume += this_vol;
  });
  return volume;
}

///////////////////////////////////////////////////////////////////////////////

void submesh::visitConnectedPolys(merged_poly_const_ptr_t p, PolyVisitContext& visitctx) const {
  if (visitctx._visited.insert(p)) {
    bool ok = visitctx._visitor(p);
    if (ok) {
      for (auto e : p->edges()) {
        for (auto i : connectedPolys(e)) {
          auto cp = poly(i);
          if (cp != p) {
            visitConnectedPolys(cp, visitctx);
          }
        }
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

poly_set_t submesh::polysConnectedToVertex(vertex_ptr_t v) const {
  return _connectivityIMPL->polysConnectedToVertex(v);
}

///////////////////////////////////////////////////////////////////////////////

edge_map_t submesh::allEdgesByVertexHash() const {
  edge_map_t edges;
  visitAllPolys([&](poly_const_ptr_t p) {
    for (auto e : p->edges()) {
      edges[e->hash()] = e;
    }
  });
  return edges;
}

///////////////////////////////////////////////////////////////////////////////

halfedge_vect_t submesh::edgesForPoly(merged_poly_const_ptr_t p) const{
  return _connectivityIMPL->edgesForPoly(p);
}

///////////////////////////////////////////////////////////////////////////////

halfedge_ptr_t submesh::edgeForVertices(vertex_ptr_t a, vertex_ptr_t b) const{
  return _connectivityIMPL->edgeForVertices(a,b);
}

///////////////////////////////////////////////////////////////////////////////

halfedge_ptr_t submesh::mergeEdgeForVertices(vertex_ptr_t a, vertex_ptr_t b) {
  return _connectivityIMPL->mergeEdgeForVertices(a,b);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
