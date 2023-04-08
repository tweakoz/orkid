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
    : _surfaceArea(0) {

  _connectivityIMPL = std::make_shared<DefaultConnectivity>(this);
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
void submesh::removePoly(poly_ptr_t p) {
  _connectivityIMPL->removePoly(p);
}
///////////////////////////////////////////////////////////////////////////////
poly_ptr_t submesh::mergePoly(const Polygon& ply) {
  auto p = _connectivityIMPL->mergePoly(ply);
  if (p) {
    if (p->_parentSubmesh == nullptr) {
      p->_parentSubmesh = this;
      _surfaceArea += p->ComputeArea(ork::dmtx4::Identity());
      _aaBoxDirty = true;
    }
  }
  return p;
}
///////////////////////////////////////////////////////////////////////////////
poly_ptr_t submesh::mergeQuad(vertex_ptr_t va, vertex_ptr_t vb, vertex_ptr_t vc, vertex_ptr_t vd) {
  return mergePoly(Polygon(va, vb, vc, vd));
}
///////////////////////////////////////////////////////////////////////////////
poly_ptr_t submesh::mergeTriangle(vertex_ptr_t va, vertex_ptr_t vb, vertex_ptr_t vc) {
  return mergePoly(Polygon(va, vb, vc));
}
///////////////////////////////////////////////////////////////////////////////
poly_ptr_t submesh::mergeUnorderedTriangle(vertex_ptr_t va, vertex_ptr_t vb, vertex_ptr_t vc) {

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
  
  poly_ptr_t ptemp = nullptr;

  if( matching_order < not_matching_order ){
    ptemp = mergeTriangle(va, vb, vc);
  }
  else if( matching_order > not_matching_order ){
    ptemp = mergeTriangle(va, vc, vb);
  }
  else if( matching_order == not_matching_order ) {
    ptemp = mergeTriangle(va, vb, vc);
  }
  else{
    OrkAssert(false);
  }

  auto p = _connectivityIMPL->mergePoly(*ptemp);
  if (p) {
    if (p->_parentSubmesh == nullptr) {
      p->_parentSubmesh = this;
      _surfaceArea += p->ComputeArea(ork::dmtx4::Identity());
      _aaBoxDirty = true;
    }
  }
  return p;
}
///////////////////////////////////////////////////////////////////////////////
edge_ptr_t submesh::mergeEdge(const edge& ed) {
  return _connectivityIMPL->mergeEdge(ed);
}
///////////////////////////////////////////////////////////////////////////////
vertex_ptr_t submesh::vertex(int i) const {
  return _connectivityIMPL->vertex(i);
}
///////////////////////////////////////////////////////////////////////////////
poly_ptr_t submesh::poly(int i) const {
  return _connectivityIMPL->poly(i);
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
///////////////////////////////////////////////////////////////////////////////
void submesh::visitAllPolys(poly_void_visitor_t visitor) {
  _connectivityIMPL->visitAllPolys(visitor);
}
///////////////////////////////////////////////////////////////////////////////
void submesh::visitAllPolys(const_poly_void_visitor_t visitor) const {
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
  visitAllPolys([&](poly_const_ptr_t p) {
    if(inumsides==0){
      count++;
    }
    else if (p->GetNumSides() == inumsides) {
      count++;
    }
  });
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
  visitAllPolys([&](poly_ptr_t p) {
    const AnnoMap* amap = apl.Find(*this, *p);
    p->SetAnnoMap(amap);
  });
}
///////////////////////////////////////////////////////////////////////////////
void submesh::exportPolyAnnotations(annopolylut& apl) const {
  visitAllPolys([&](poly_const_ptr_t p) {
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
  visitAllPolys([&](poly_const_ptr_t p) {
    if (p->GetNumSides() == inumsides) {
      output.push_back(p->_submeshIndex);
    }
  });
}
///////////////////////////////////////////////////////////////////////////////
poly_index_set_t submesh::adjacentPolys(int ply) const {
  return _connectivityIMPL->polysConnectedToPoly(ply);
}
///////////////////////////////////////////////////////////////////////////////
edge_ptr_t submesh::edgeBetweenPolys(int aind, int bind) const {
  return _connectivityIMPL->edgeBetweenPolys(aind, bind);
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
void submesh::MergeSubMesh(const submesh& inp_mesh) {
  //float ftimeA     = float(OldSchool::GetRef().GetLoResTime());
  int inumpingroup = 0;
  inp_mesh.visitAllPolys([&](poly_const_ptr_t p) {
    std::vector<vertex_ptr_t> new_vertices;
    for (int iv = 0; iv < p->GetNumSides(); iv++) {
      int ivi      = p->GetVertexID(iv);
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
void submesh::mergePolySet(const PolySet& pset) {
  for (auto p : pset._polys) {
    std::vector<vertex_ptr_t> merged_vertices;
    for (auto v : p->_vertices) {
      auto newv = mergeVertex(*v);
      merged_vertices.push_back(newv);
    }
    mergePoly(Polygon(merged_vertices));
  }
  _aaBoxDirty = true;
}
///////////////////////////////////////////////////////////////////////////////
polyset_ptr_t submesh::asPolyset() const {
  polyset_ptr_t rval = std::make_shared<PolySet>();
  visitAllPolys([&](poly_const_ptr_t p) {
    // todo : fix const
    auto as_non_const = std::const_pointer_cast<Polygon>(p);
    rval->_polys.insert(as_non_const);
  });
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
// addPoly helper methods
///////////////////////////////////////////////////////////////////////////////
void submesh::addQuad(fvec3 p0, fvec3 p1, fvec3 p2, fvec3 p3, fvec4 c) {
  struct vertex muvtx[4];

  fvec3 B = (p0 - p1).normalized();
  fvec3 T = (p2 - p0).normalized();
  fvec3 N = B.crossWith(T);

  muvtx[0].set(p0, N, fvec3(), fvec2(), c);
  muvtx[1].set(p1, N, fvec3(), fvec2(), c);
  muvtx[2].set(p2, N, fvec3(), fvec2(), c);
  muvtx[3].set(p3, N, fvec3(), fvec2(), c);
  auto v0 = mergeVertex(muvtx[0]);
  auto v1 = mergeVertex(muvtx[1]);
  auto v2 = mergeVertex(muvtx[2]);
  auto v3 = mergeVertex(muvtx[3]);
  mergePoly(Polygon(v0, v1, v2, v3));
}
///////////////////////////////////////////////////////////////////////////////
void submesh::addQuad(fvec3 p0, fvec3 p1, fvec3 p2, fvec3 p3, fvec2 uv0, fvec2 uv1, fvec2 uv2, fvec2 uv3, fvec4 c) {
  struct vertex muvtx[4];
  fvec3 p0p1 = (p1 - p0).normalized();
  fvec3 p0p2 = (p2 - p0).normalized();
  fvec3 nrm  = p0p1.crossWith(p0p2);
  // todo compute tangent space from uv gradients
  fvec3 bin = p0p1;
  muvtx[0].set(p0, nrm, bin, uv0, c);
  muvtx[1].set(p1, nrm, bin, uv1, c);
  muvtx[2].set(p2, nrm, bin, uv2, c);
  muvtx[3].set(p3, nrm, bin, uv3, c);

  auto v0 = mergeVertex(muvtx[0]);
  auto v1 = mergeVertex(muvtx[1]);
  auto v2 = mergeVertex(muvtx[2]);
  auto v3 = mergeVertex(muvtx[3]);
  mergePoly(Polygon(v0, v1, v2, v3));
}
///////////////////////////////////////////////////////////////////////////////
void submesh::addQuad(
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
    fvec4 c) { /// add quad helper method
  struct vertex muvtx[4];
  fvec3 p0p1 = (p1 - p0).normalized();
  fvec3 bin  = p0p1;
  muvtx[0].set(p0, n0, bin, uv0, c);
  muvtx[1].set(p1, n1, bin, uv1, c);
  muvtx[2].set(p2, n2, bin, uv2, c);
  muvtx[3].set(p3, n3, bin, uv3, c);

  auto v0 = mergeVertex(muvtx[0]);
  auto v1 = mergeVertex(muvtx[1]);
  auto v2 = mergeVertex(muvtx[2]);
  auto v3 = mergeVertex(muvtx[3]);
  mergePoly(Polygon(v0, v1, v2, v3));
}
///////////////////////////////////////////////////////////////////////////////
void submesh::addQuad(
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
    fvec4 c) { /// add quad helper method
  struct vertex muvtx[4];
  muvtx[0].set(p0, n0, b0, uv0, c);
  muvtx[1].set(p1, n1, b1, uv1, c);
  muvtx[2].set(p2, n2, b2, uv2, c);
  muvtx[3].set(p3, n3, b3, uv3, c);

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
  visitAllPolys([&](poly_const_ptr_t p1) {
    auto pl = p1->computePlane();
    //printf( "plane<%f %f %f> <%f>\n", p1.get(), pl.n.x, pl.n.y, pl.n.z, pl.d );
    visitAllPolys([&](poly_const_ptr_t p2) {
      if (p1 != p2) {
        for (auto v : p2->_vertices) {
          switch(pl.classifyPoint(v->mPos)) {
            case PointClassification::FRONT:
            case PointClassification::COPLANAR:
              front++;
              break;
            case PointClassification::BACK:{
              back++;
              //double distance = pl.pointDistance(v->mPos);
              //auto d =FormatString( "vd:distance<%.e>", 10, distance );
              //v->dump(d);
              break;
            }
          }
        }
      }
    });
    //bool is_convex = (front > 0) and (back == 0);
    //OrkAssert(is_convex);
  });
   //printf( "front<%d> back<%d>\n", front, back );
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

  visitAllPolys([&](poly_const_ptr_t p) {
    std::vector<vertex_ptr_t> newverts;
    for (auto v : p->_vertices) {
      auto it = vtx_map.find(v->_poolindex);
      OrkAssert(it != vtx_map.end());
      int v_index = it->second;
      auto newv   = dest.vertex(v_index);
      newverts.push_back(newv);
    }
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

dvec3 submesh::center() const {
  size_t num_verts = 0;
  dvec3 accum;
  visitAllVertices([&](vertex_const_ptr_t v) {
    num_verts++;
    accum += v->mPos;
  });
  return accum * 1.0 / double(num_verts);
}

///////////////////////////////////////////////////////////////////////////////
double submesh::convexVolume() const {
  double volume = 0.0f;
  dvec3 c       = center();
  visitAllPolys([&](poly_const_ptr_t p) {
    int numsides = p->_vertices.size();
    OrkAssert(numsides == 3);
    const auto& v0 = p->_vertices[0]->mPos;
    const auto& v1 = p->_vertices[1]->mPos;
    const auto& v2 = p->_vertices[2]->mPos;

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

void submesh::visitConnectedPolys(poly_ptr_t p, PolyVisitContext& visitctx) const {
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

poly_set_t submesh::polysConnectedTo(vertex_ptr_t v) const {
  poly_set_t connected;
  visitAllPolys([&](poly_const_ptr_t p) {
    for (auto pv : p->_vertices) {
      if (pv == v) {
        // todo: remove const_cast
        auto non_const = std::const_pointer_cast<Polygon>(p);
        connected.insert(non_const);
      }
    }
  });
  return connected;
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
} // namespace ork::meshutil
