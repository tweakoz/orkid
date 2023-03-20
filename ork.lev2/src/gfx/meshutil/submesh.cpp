////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/orklut.hpp>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <Eigen/Dense>
#include <Eigen/Sparse>
#include<ork/util/logger.h>

template class ork::orklut<std::string, ork::meshutil::submesh_ptr_t>;

namespace ork::meshutil {
static logchannel_ptr_t logchan_submesh = logger()->createChannel("meshutil.submesh",fvec3(.9,.9,1));

const vertexpool vertexpool::EmptyPool;

/////////////////////////////////////////////////////////////////////////
submesh::submesh(vertexpool_ptr_t vpool)
    : _surfaceArea(0)
    , _mergeEdges(true) {

    if(nullptr==vpool){
      vpool = std::make_shared<vertexpool>();
    }

    _vtxpool = vpool;

  for (int i = 0; i < 8; i++)
    _polyTypeCounter[i] = 0;
}
/////////////////////////////////////////////////////////////////////////
// eigen to submesh converter for interfacing
//  with various python/numpy packages
/////////////////////////////////////////////////////////////////////////
submesh_ptr_t submeshFromEigen(
    const Eigen::MatrixXd& verts, //
    const Eigen::MatrixXi& faces,
    const Eigen::MatrixXd& uvs,
    const Eigen::MatrixXd& colors,
    const Eigen::MatrixXd& normals,
    const Eigen::MatrixXd& binormals,
    const Eigen::MatrixXd& tangents) {
  auto rval           = std::make_shared<submesh>();
  size_t numVerts     = verts.rows();
  size_t numFaces     = faces.rows();
  size_t sidesPerFace = faces.cols();
  size_t numUvs       = uvs.rows();
  size_t numColors    = colors.rows();
  size_t numNormals   = normals.rows();
  size_t numBinormals = binormals.rows();
  size_t numTangents  = tangents.rows();
  /////////////////////////////////////////////
  OrkAssert(verts.cols() == 3);                                          // make sure we have vec3's
  auto generateVertex = [&](int faceindex, int facevtxindex) -> vertex { //
    vertex outv;
    const Eigen::MatrixXi& face = faces.row(faceindex);
    int per_vert_index          = face(facevtxindex);
    /////////////////////////////////////////////
    // position
    /////////////////////////////////////////////
    auto inp_pos = verts.row(per_vert_index);
    outv.mPos    = fvec3(inp_pos(0), inp_pos(1), inp_pos(2));
    /////////////////////////////////////////////
    // normal
    /////////////////////////////////////////////
    auto donormal = [&](int index) {
      OrkAssert(normals.cols() == 3);
      auto inp  = normals.row(index);
      outv.mNrm = fvec3(inp(0), inp(1), inp(2));
    };
    if (numNormals == numVerts) // per vertex
      donormal(per_vert_index);
    else if (numNormals == numFaces) // per face
      donormal(faceindex);
    else if (numNormals == 0) {
    } // no normals
    else
      OrkAssert(false);
    /////////////////////////////////////////////
    // binormal
    /////////////////////////////////////////////
    auto dobinormal = [&](int index) {
      OrkAssert(binormals.cols() == 3);
      auto inp                 = binormals.row(index);
      outv.mUV[0].mMapBiNormal = fvec3(inp(0), inp(1), inp(2));
    };
    if (numBinormals == numVerts) // per vertex
      dobinormal(per_vert_index);
    else if (numBinormals == numFaces) // per face
      dobinormal(faceindex);
    else if (numBinormals == 0) {
    } // no binormals
    else
      OrkAssert(false);
    /////////////////////////////////////////////
    // tangent
    /////////////////////////////////////////////
    auto dotangent = [&](int index) {
      OrkAssert(tangents.cols() == 3);
      auto inp                = tangents.row(index);
      outv.mUV[0].mMapTangent = fvec3(inp(0), inp(1), inp(2));
    };
    if (numTangents == numVerts) // per vertex
      dotangent(per_vert_index);
    else if (numTangents == numFaces) // per face
      dotangent(faceindex);
    else if (numTangents == 0) {
    } // no tangents
    else
      OrkAssert(false);
    /////////////////////////////////////////////
    // texturecoord
    /////////////////////////////////////////////
    auto dotexcoord = [&](int index) {
      OrkAssert(uvs.cols() == 2);
      auto inp                 = uvs.row(index);
      outv.mUV[0].mMapTexCoord = fvec2(inp(0), inp(1));
    };
    if (numUvs == numVerts) // per vertex
      dotexcoord(per_vert_index);
    else if (numUvs == numFaces) // per face
      dotexcoord(faceindex);
    else if (numUvs == 0) {
    } // no texcoords
    else
      OrkAssert(false);
    /////////////////////////////////////////////
    // color
    /////////////////////////////////////////////
    auto docolor = [&](int index) {
      auto inp = colors.row(index);
      switch (colors.cols()) {
        case 1: // luminance
          outv.mCol[0] = fvec4(inp(0), inp(0), inp(0), 1);
          break;
        case 3: // rgb
          outv.mCol[0] = fvec4(inp(0), inp(1), inp(2), 1);
          break;
        case 4: // rgba
          outv.mCol[0] = fvec4(inp(0), inp(1), inp(2), inp(3));
          break;
        default:
          OrkAssert(false);
          break;
      }
    };
    if (numColors == numVerts)
      docolor(per_vert_index);
    else if (numColors == numFaces)
      docolor(faceindex);
    else if (numColors == 1)
      docolor(0);
    else if (numColors == 0)
      outv.mCol[0] = fvec4(1, 1, 1, 1);
    else
      OrkAssert(false);
    return outv;
  }; // auto generateVertex = [&](int faceindex, int facevtxindex) -> vertex { //
  /////////////////////////////////////////////
  for (int f = 0; f < numFaces; f++) {
    switch (sidesPerFace) {
      case 3: {
        auto o0 = rval->mergeVertex(generateVertex(f, 0));
        auto o1 = rval->mergeVertex(generateVertex(f, 1));
        auto o2 = rval->mergeVertex(generateVertex(f, 2));
        rval->mergePoly(poly(o0, o1, o2));
        break;
      }
      case 4: {
        auto o0 = rval->mergeVertex(generateVertex(f, 0));
        auto o1 = rval->mergeVertex(generateVertex(f, 1));
        auto o2 = rval->mergeVertex(generateVertex(f, 2));
        auto o3 = rval->mergeVertex(generateVertex(f, 3));
        rval->mergePoly(poly(o0, o1, o2, o3));
        break;
      }
      default:
        OrkAssert(false);
        break;
    }
  }
  return rval;
}
/////////////////////////////////////////////////////////////////////////
submesh::~submesh() {
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
void submesh::ImportPolyAnnotations(const annopolylut& apl) {
  int inumpolys = (int)_orderedPolys.size();
  for (int ip = 0; ip < inumpolys; ip++) {
    auto ply            = _orderedPolys[ip];
    const AnnoMap* amap = apl.Find(*this, *ply);
    if (amap) {
      ply->SetAnnoMap(amap);
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
void submesh::ExportPolyAnnotations(annopolylut& apl) const {
  int inumpolys = (int)_orderedPolys.size();
  for (int ip = 0; ip < inumpolys; ip++) {
    auto ply            = _orderedPolys[ip];
    U64 uhash           = apl.HashItem(*this, *ply);
    const AnnoMap* amap = ply->GetAnnoMap();
    apl.mAnnoMap[uhash] = amap;
  }
}
///////////////////////////////////////////////////////////////////////////////
const AABox& submesh::aabox() const {
  if (_aaBoxDirty) {
    _aaBox.BeginGrow();
    int inumvtx = (int)_vtxpool->GetNumVertices();
    for (int i = 0; i < inumvtx; i++) {
      const vertex& v = _vtxpool->GetVertex(i);
      _aaBox.Grow(v.mPos);
    }
    _aaBox.EndGrow();
    _aaBoxDirty = false;
  }
  return _aaBox;
}
///////////////////////////////////////////////////////////////////////////////
const edge& submesh::RefEdge(U64 edgekey) const {
  auto it = _edgemap.find(edgekey);
  OrkAssert(it != _edgemap.end());
  return *it->second;
}
///////////////////////////////////////////////////////////////////////////////
vertex_ptr_t submesh::mergeVertex(const vertex& vtx) {
  _aaBoxDirty = true;
  return _vtxpool->mergeVertex(vtx);
}
///////////////////////////////////////////////////////////////////////////////
poly& submesh::RefPoly(int i) {
  OrkAssert(orkvector<int>::size_type(i) < _orderedPolys.size());
  return *_orderedPolys[i];
}
///////////////////////////////////////////////////////////////////////////////
const poly& submesh::RefPoly(int i) const {
  OrkAssert(orkvector<int>::size_type(i) < _orderedPolys.size());
  return *_orderedPolys[i];
}
///////////////////////////////////////////////////////////////////////////////
const orkvector<poly_ptr_t>& submesh::RefPolys() const {
  return _orderedPolys;
}
/////////////////////////////////////////////////////////////////////////
void submesh::FindNSidedPolys(orkvector<int>& output, int inumsides) const {
  int inump = (int)_orderedPolys.size();
  for (int i = 0; i < inump; i++) {
    const poly& ply = RefPoly(i);
    if (ply.GetNumSides() == inumsides) {
      output.push_back(i);
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
int submesh::GetNumPolys(int inumsides) const {
  int iret = 0;
  if (0 == inumsides) {
    iret = (int)_orderedPolys.size();
  } else {
    auto it = _polyTypeCounter.find(inumsides);
    OrkAssert(it!=_polyTypeCounter.end());
    iret = it->second;
  }
  return iret;
}
///////////////////////////////////////////////////////////////////////////////
void submesh::GetEdges(const poly& ply, orkvector<edge>& Edges) const {
  int icnt  = 0;
  int icntf = 0;
  for (int is = 0; is < ply.GetNumSides(); is++) {
    U64 ue  = ply._edges[is]->GetHashKey();
    auto it = _edgemap.find(ue);
    if (it != _edgemap.end()) {
      Edges.push_back(*it->second);
      icntf++;
    }
    icnt++;
  }
}
///////////////////////////////////////////////////////////////////////////////
void submesh::GetAdjacentPolys(int ply, orkset<int>& output) const {
  orkvector<edge> edges;
  GetEdges(RefPoly(ply), edges);
  for (orkvector<edge>::const_iterator edgeIter = edges.begin(); edgeIter != edges.end(); edgeIter++) {
    orkset<int> connectedPolys;
    GetConnectedPolys(*edgeIter, connectedPolys);
    for (orkset<int>::const_iterator it2 = connectedPolys.begin(); it2 != connectedPolys.end(); it2++) {
      int ic = *it2;
      if (ic != ply) {
        output.insert(connectedPolys.begin(), connectedPolys.end());
      }
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
edge_constptr_t submesh::edgeBetween(int aind, int bind) const {
  const poly& a = RefPoly(aind);
  const poly& b = RefPoly(bind);
  for (int eaind = 0; eaind < a.GetNumSides(); eaind++)
    for (int ebind = 0; ebind < b.GetNumSides(); ebind++)
      if (a._edges[eaind] == b._edges[ebind])
        return std::const_pointer_cast<const edge>(a._edges[eaind]);
  return nullptr;
}
///////////////////////////////////////////////////////////////////////////////
void submesh::GetConnectedPolys(const edge& ed, orkset<int>& output) const {
  U64 keyA    = ed.GetHashKey();
  auto itfind = _edgemap.find(keyA);
  if (itfind != _edgemap.end()) {
    auto edfound = itfind->second;
    int inump    = edfound->GetNumConnectedPolys();
    for (int ip = 0; ip < inump; ip++) {
      int ipi = edfound->GetConnectedPoly(ip);
      output.insert(ipi);
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
void submesh::MergeSubMesh(const submesh& inp_mesh) {
  float ftimeA     = float(OldSchool::GetRef().GetLoResTime());
  int inumpingroup = inp_mesh.GetNumPolys();
  for (int i = 0; i < inumpingroup; i++) {
    const poly& input_poly = inp_mesh.RefPoly(i);
    std::vector<vertex_ptr_t> new_vertices;
    for (int iv = 0; iv < input_poly.GetNumSides(); iv++) {
      int ivi               = input_poly.GetVertexID(iv);
      const vertex& src_vtx = inp_mesh._vtxpool->GetVertex(ivi);
      new_vertices.push_back(mergeVertex(src_vtx));
    }
    poly_ptr_t new_poly = std::make_shared<poly>(new_vertices);
    new_poly->SetAnnoMap(input_poly.GetAnnoMap());
    mergePoly(*new_poly);
  }
  logchan_submesh->log("inumpingroup<%d> numoutpolys<%d>", inumpingroup, GetNumPolys() );
  float ftimeB = float(OldSchool::GetRef().GetLoResTime());
  float ftime  = (ftimeB - ftimeA);
  logchan_submesh->log("<<PROFILE>> <<submesh::MergeSubMesh %f seconds>>", ftime);
}
///////////////////////////////////////////////////////////////////////////////
poly_ptr_t submesh::mergeTriangle(vertex_ptr_t va, vertex_ptr_t vb, vertex_ptr_t vc){
  return mergePoly(poly(va,vb,vc));
}
poly_ptr_t submesh::mergeQuad(vertex_ptr_t va, vertex_ptr_t vb, vertex_ptr_t vc, vertex_ptr_t vd){
  return mergePoly(poly(va,vb,vc,vd));
}
///////////////////////////////////////////////////////////////////////////////
poly_ptr_t submesh::mergePoly(const poly& ply) {
  poly_ptr_t rval;
  int ipolyindex = GetNumPolys();
  poly nply      = ply;
  int inumv      = ply.GetNumSides();
  OrkAssert(inumv >= 3 );
  for( auto v : ply._vertices ){
    OrkAssert(v!=nullptr);
  }
  ///////////////////////////////
  // zero area poly removal
  switch (inumv) {
    case 3: {
      if ((ply._vertices[0]->_poolindex == ply._vertices[1]->_poolindex) ||
          (ply._vertices[1]->_poolindex == ply._vertices[2]->_poolindex) ||
          (ply._vertices[2]->_poolindex == ply._vertices[0]->_poolindex)) {
        if(0)logchan_submesh->log(
            "Mesh::mergePoly() removing zero area tri<%d %d %d>",
            ply._vertices[0]->_poolindex,
            ply._vertices[1]->_poolindex,
            ply._vertices[2]->_poolindex);


        return nullptr;
      }
      break;
    }
    case 4: {
      if ((ply._vertices[0]->_poolindex == ply._vertices[1]->_poolindex) ||
          (ply._vertices[0]->_poolindex == ply._vertices[2]->_poolindex) ||
          (ply._vertices[0]->_poolindex == ply._vertices[3]->_poolindex) ||
          (ply._vertices[1]->_poolindex == ply._vertices[2]->_poolindex) ||
          (ply._vertices[1]->_poolindex == ply._vertices[3]->_poolindex) ||
          (ply._vertices[2]->_poolindex == ply._vertices[3]->_poolindex)) {
        if(0)logchan_submesh->log(
            "Mesh::mergePoly() removing zero area quad<%d %d %d %d>",
            ply._vertices[0]->_poolindex,
            ply._vertices[1]->_poolindex,
            ply._vertices[2]->_poolindex,
            ply._vertices[3]->_poolindex);

        return nullptr;
      }
      break;
    }
    default:
      break;
      // TODO n-sided polys
  }
  //////////////////////////////
  // dupe check
  //////////////////////////////
  U64 ucrc   = ply.HashIndices();
  auto itfhm = _polymap.find(ucrc);
  ///////////////////////////////
  if (itfhm == _polymap.end()) { // no match

    int inewpi = (int)_orderedPolys.size();
    //////////////////////////////////////////////////
    // connect to vertices
    for (int i = 0; i < inumv; i++) {
      auto vtx = ply._vertices[i];
      // vtx->ConnectToPoly(inewpi);
    }
    //////////////////////////////////////////////////
    // add edges
    if (_mergeEdges) {
      for (int i = 0; i < inumv; i++) {
        int i0  = (i);
        int i1  = (i + 1) % inumv;
        int iv0 = ply.GetVertexID(i0);
        int iv1 = ply.GetVertexID(i1);
        auto v0 = _vtxpool->_orderedVertices[iv0];
        auto v1 = _vtxpool->_orderedVertices[iv1];

        edge Edge(v0, v1);
        nply._edges.push_back(MergeEdge(Edge, ipolyindex));
      }
    }
    nply.SetAnnoMap(ply.GetAnnoMap());
    auto new_poly = std::make_shared<poly>(nply);
    _orderedPolys.push_back(new_poly);
    _polymap[ucrc] = new_poly;
    //////////////////////////////////////////////////
    // add n sided counters
    _polyTypeCounter[inumv]++;
    //////////////////////////////////////////////////
    float farea = ply.ComputeArea(ork::fmtx4::Identity());
    _surfaceArea += farea;
    rval = new_poly;
    _aaBoxDirty = true;
  }
  else{
    rval = itfhm->second;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
edge_ptr_t submesh::MergeEdge(const edge& ed, int ipolyindex) {
  U64 crcA    = ed.GetHashKey();
  auto itfind = _edgemap.find(crcA);

  edge_ptr_t rval;

  if (_edgemap.end() != itfind) {
    rval     = itfind->second;
    U64 crcB = rval->GetHashKey();
    OrkAssert(ed.Matches(*rval));
  } else {
    rval           = std::make_shared<edge>(ed);
    _edgemap[crcA] = rval;
  }
  if (ipolyindex >= 0) {
    rval->ConnectToPoly(ipolyindex);
  }

  _aaBoxDirty = true;
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
// addPoly helper methods
///////////////////////////////////////////////////////////////////////////////
void submesh::addQuad(fvec3 p0, fvec3 p1, fvec3 p2, fvec3 p3, fvec4 c) {
  vertex muvtx[4];

  fvec3 B = (p0-p1).normalized();
  fvec3 T = (p2-p0).normalized();
  fvec3 N = B.crossWith(T);

  muvtx[0].set(p0, N, fvec3(), fvec2(), c);
  muvtx[1].set(p1, N, fvec3(), fvec2(), c);
  muvtx[2].set(p2, N, fvec3(), fvec2(), c);
  muvtx[3].set(p3, N, fvec3(), fvec2(), c);
  auto v0 = mergeVertex(muvtx[0]);
  auto v1 = mergeVertex(muvtx[1]);
  auto v2 = mergeVertex(muvtx[2]);
  auto v3 = mergeVertex(muvtx[3]);
  mergePoly(poly(v0, v1, v2, v3));
}
void submesh::addQuad(fvec3 p0, fvec3 p1, fvec3 p2, fvec3 p3, fvec2 uv0, fvec2 uv1, fvec2 uv2, fvec2 uv3, fvec4 c) {
  vertex muvtx[4];
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
  mergePoly(poly(v0, v1, v2, v3));
}
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
  vertex muvtx[4];
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
  mergePoly(poly(v0, v1, v2, v3));
}
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
  vertex muvtx[4];
  muvtx[0].set(p0, n0, b0, uv0, c);
  muvtx[1].set(p1, n1, b1, uv1, c);
  muvtx[2].set(p2, n2, b2, uv2, c);
  muvtx[3].set(p3, n3, b3, uv3, c);

  auto v0 = mergeVertex(muvtx[0]);
  auto v1 = mergeVertex(muvtx[1]);
  auto v2 = mergeVertex(muvtx[2]);
  auto v3 = mergeVertex(muvtx[3]);
  mergePoly(poly(v0, v1, v2, v3));
}

///////////////////////////////////////////////////////////////////////////////

bool submesh::isConvexHull() const {

  int front = 0;
  int back = 0;
  for( auto p : _orderedPolys ){
    auto pl = p->computePlane();
    for( auto p2 : _orderedPolys ){
      if(p!=p2){
        for( auto v : p2->_vertices){
          bool is_front = pl.IsPointInFront(v->mPos);
          if(is_front){
            front++;
          }
          else{
            back++;
          }
        }
      }
    }
  }
  //printf( "front<%d> back<%d>\n", front, back );
  return (front>0) and (back==0);
}

///////////////////////////////////////////////////////////////////////////////

void submesh::copy(  submesh& dest, //
                     bool preserve_normals, //
                     bool preserve_colors, //
                     bool preserve_texcoords ) const{ //

  std::unordered_map<int,int> vtx_map;

  for( auto v : _vtxpool->_orderedVertices ){

    auto temp_v = std::make_shared<vertex>();
    temp_v->mPos = v->mPos;
    if(preserve_normals){
      temp_v->mNrm = v->mNrm;
    }
    if(preserve_colors){
      for( int ic=0; ic<vertex::kmaxcolors; ic++ ){
        temp_v->mCol[ic] = v->mCol[ic];
      }
      temp_v->miNumColors = v->miNumColors;
    }
    if(preserve_texcoords){
      for( int it=0; it<vertex::kmaxuvs; it++ ){
        temp_v->mUV[it] = v->mUV[it];
      }
      temp_v->miNumUvs = v->miNumUvs;
    }

    auto new_v = dest.mergeVertex(*temp_v);
    vtx_map[v->_poolindex] = new_v->_poolindex;

  }
  for( auto p : _orderedPolys ){
    std::vector<vertex_ptr_t> newverts;
    for( auto v : p->_vertices ){
      auto it = vtx_map.find(v->_poolindex);
      OrkAssert(it!=vtx_map.end());
      auto newv = dest._vtxpool->_orderedVertices[it->second];
      newverts.push_back(newv);
    }
    auto newp = std::make_shared<poly>(newverts);
    dest.mergePoly(*newp);
  }

  dest.name = name;
  dest._mergeEdges = _mergeEdges;
  dest._annotations = _annotations;
  dest._aaBoxDirty = true;
  dest._surfaceArea = 0.0f;

}

///////////////////////////////////////////////////////////////////////////////

fvec3 submesh::center() const {
  size_t num_verts = _vtxpool->_orderedVertices.size();
  fvec3 center;
  for( auto v : _vtxpool->_orderedVertices ){
    center += v->mPos;
  }
  center *= 1.0f / float(num_verts);
  return center;
}

///////////////////////////////////////////////////////////////////////////////
float submesh::convexVolume() const {
  float volume = 0.0f;
  fvec3 c = center();
  for( auto p : _orderedPolys ){
    int numsides = p->_vertices.size();
    OrkAssert(numsides==3);
    const auto& v0 = p->_vertices[0]->mPos;
    const auto& v1 = p->_vertices[1]->mPos;
    const auto& v2 = p->_vertices[2]->mPos;

    float U = (v0-v1).length();
    float V = (v1-v2).length();
    float W = (v2-v0).length();

    float u = (v2-c).length();
    float v = (v0-c).length();
    float w = (v1-c).length();

    float usq = u*u;
    float vsq = v*v;
    float wsq = w*w;

    float sqU = U*U;
    float sqV = V*V;
    float sqW = W*W;
    float termA = vsq + wsq - sqU;
    float termB = wsq + usq - sqV;
    float termC = usq + vsq - sqW;

    //sqrt(4*u*u*v*v*w*w – u*u*(v*v + w*w – U*U)^2 – v*v(w*w + u*u – V*V)^2 – w*w(u*u + v*v – W*W)^2 + (u*u + v*v – W*W) * (w*w + u*u – V*V) * (v*v + w*w – U*U)) / 12

    float this_vol = sqrt(4.0f*usq*vsq*wsq - usq*termA*termA - vsq*termB*termB - wsq*termC*termC + (termC*termB*termA)) / 12.0f;
    volume += this_vol;
  }
  return volume;
}

///////////////////////////////////////////////////////////////////////////////
/*
void SubMesh::GenIndexBuffers( void )
{
    int inumvtx = _vtxpool->VertexPool.size();

    orkvector<int> TrianglePolyIndices;
    orkvector<int> QuadPolyIndices;

    FindNSidedPolys( TrianglePolyIndices, 3 );
    FindNSidedPolys( QuadPolyIndices, 4 );

    int inumtri( TrianglePolyIndices.size() );
    int inumquad( QuadPolyIndices.size() );

    mpBaseTriangleIndices = new U16[ inumtri*3 ];
    mpBaseQuadIndices = new U16[ inumquad*4 ];

    for( int itri=0; itri<inumtri; itri++ )
    {
        int iti = TrianglePolyIndices[itri];

        const poly & tri = RefPoly( iti );

        int i0 = tri.miVertices[0];
        int i1 = tri.miVertices[1];
        int i2 = tri.miVertices[2];

        OrkAssert( i0<inumvtx );
        OrkAssert( i1<inumvtx );
        OrkAssert( i2<inumvtx );

        mpBaseTriangleIndices[ (itri*3)+0 ] = U16(i0);
        mpBaseTriangleIndices[ (itri*3)+1 ] = U16(i1);
        mpBaseTriangleIndices[ (itri*3)+2 ] = U16(i2);
    }

    for( int iqua=0; iqua<inumquad; iqua++ )
    {
        int iqi = QuadPolyIndices[iqua];

        const poly & qu = RefPoly( iqi );

        int i0 = qu.miVertices[0];
        int i1 = qu.miVertices[1];
        int i2 = qu.miVertices[2];
        int i3 = qu.miVertices[3];

        OrkAssert( i0<inumvtx );
        OrkAssert( i1<inumvtx );
        OrkAssert( i2<inumvtx );
        OrkAssert( i3<inumvtx );

        mpBaseQuadIndices[ (iqua*4)+0 ] = U16(i0);
        mpBaseQuadIndices[ (iqua*4)+1 ] = U16(i1);
        mpBaseQuadIndices[ (iqua*4)+2 ] = U16(i2);
        mpBaseQuadIndices[ (iqua*4)+3 ] = U16(i3);
    }

}*/
} // namespace ork::meshutil
