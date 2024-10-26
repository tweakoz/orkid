////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/orklut.hpp>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/util/xxhash.inl>

namespace ork::meshutil {
static const std::string gnomatch("");
double _default_quantization = 1000.0;

////////////////////////////////////////////////////////////////

uvmapcoord::uvmapcoord() {
}

////////////////////////////////////////////////////////////////

void uvmapcoord::Clear(void) {
  mMapBiNormal = fvec3();
  mMapTangent  = fvec3();
  mMapTexCoord = fvec2();
}

///////////////////////////////////////////////////////////////////////////////

void uvmapcoord::lerp(const uvmapcoord& ina, const uvmapcoord& inb, float flerp) {
  mMapTexCoord.lerp(ina.mMapTexCoord, inb.mMapTexCoord, flerp);
  mMapBiNormal.lerp(ina.mMapBiNormal, inb.mMapBiNormal, flerp);
  mMapTangent.lerp(ina.mMapTangent, inb.mMapTangent, flerp);
  // return uvmapcoord( out );
}

///////////////////////////////////////////////////////////////////////////////

uvmapcoord uvmapcoord::operator+(const uvmapcoord& ina) const {
  uvmapcoord out;
  out.mMapTexCoord = mMapTexCoord + ina.mMapTexCoord;
  out.mMapBiNormal = mMapBiNormal + ina.mMapBiNormal;
  out.mMapTangent  = mMapTangent + ina.mMapTangent;
  return uvmapcoord(out);
}

///////////////////////////////////////////////////////////////////////////////

uvmapcoord uvmapcoord::operator*(float Scalar) const {
  uvmapcoord out;
  out.mMapTexCoord = mMapTexCoord * Scalar;
  out.mMapBiNormal = mMapBiNormal * Scalar;
  out.mMapTangent  = mMapTangent * Scalar;
  return uvmapcoord(out);
}

////////////////////////////////////////////////////////////////

vertex::vertex()
    : miNumWeights(0)
    , miNumColors(0)
    , miNumUvs(0) {
  for (int i = 0; i < kmaxcolors; i++) {
    mCol[i] = fvec4::White();
  }
  for (int i = 0; i < kmaxinfluences; i++) {
    _jointpaths[i]   = "";
    mJointWeights[i] = float(0.0f);
  }
}

////////////////////////////////////////////////////////////////

vertex::vertex(dvec3 pos, dvec3 nrm, fvec3 bin, fvec2 uv, fvec4 col)
    : vertex() {
  set(pos, nrm, bin, uv, col);
}

////////////////////////////////////////////////////////////////

vertex::vertex(fvec3 pos, fvec3 nrm, fvec3 bin, fvec2 uv, fvec4 col)
    : vertex() {
  set(pos, nrm, bin, uv, col);
}

////////////////////////////////////////////////////////////////

void vertex::clearAllExceptPosition() {
  mNrm         = dvec3(0, 0, 0);
  miNumWeights = 0;
  miNumColors  = 0;
  miNumUvs     = 0;
  for (int i = 0; i < kmaxcolors; i++) {
    mCol[i] = fvec4::White();
  }
  for (int i = 0; i < kmaxinfluences; i++) {
    _jointpaths[i]   = "";
    mJointWeights[i] = 0.0f;
  }
  for (int i = 0; i < kmaxuvs; i++) {
    mUV[i] = uvmapcoord();
  }
}

////////////////////////////////////////////////////////////////

vertex::vertex(const vertex& rhs) {
  _poolindex = 0xffffffff;

  mPos         = rhs.mPos;
  mNrm         = rhs.mNrm;
  miNumWeights = rhs.miNumWeights;
  miNumColors  = rhs.miNumColors;
  miNumUvs     = rhs.miNumUvs;

  for (int i = 0; i < kmaxinfluences; i++) {
    _jointpaths[i]   = rhs._jointpaths[i];
    mJointWeights[i] = rhs.mJointWeights[i];
  }
  for (int i = 0; i < kmaxcolors; i++) {
    mCol[i] = rhs.mCol[i];
  }
  for (int i = 0; i < kmaxuvs; i++) {
    mUV[i] = rhs.mUV[i];
  }
}

void vertex::dump(const std::string& name) const {
  printf(
      "vertex<%s> hash<%llx> pos<%.*e %.*e %.*e> nrm<%.*f %.*f %.*f> uv0<%.*f %.*f> col0<%.*f %.*f %.*f %.*f>\n",
      name.c_str(),
      hash(),
      10,
      mPos.x,
      10,
      mPos.y,
      10,
      mPos.z,
      3,
      mNrm.x,
      3,
      mNrm.y,
      3,
      mNrm.z,
      2,
      mUV[0].mMapTexCoord.x,
      2,
      mUV[0].mMapTexCoord.y,
      2,
      mCol[0].x,
      2,
      mCol[0].y,
      2,
      mCol[0].z,
      2,
      mCol[0].w);
}

////////////////////////////////////////////////////////////////

void vertex::set(dvec3 pos, dvec3 nrm, fvec3 bin, fvec2 uv, fvec4 col) {
  mPos                = pos;
  mNrm                = nrm;
  mUV[0].mMapTexCoord = uv;
  mUV[0].mMapBiNormal = bin;
  mCol[0]             = col;
  miNumColors         = 1;
  miNumUvs            = 1;
}

////////////////////////////////////////////////////////////////

void vertex::set(fvec3 pos, fvec3 nrm, fvec3 bin, fvec2 uv, fvec4 col) {
  mPos                = fvec3_to_dvec3(pos);
  mNrm                = fvec3_to_dvec3(nrm);
  mUV[0].mMapTexCoord = uv;
  mUV[0].mMapBiNormal = bin;
  mCol[0]             = col;
  miNumColors         = 1;
  miNumUvs            = 1;
}

////////////////////////////////////////////////////////////////

const dvec3& vertex::Pos() const {
  return mPos;
}

///////////////////////////////////////////////////////////////////////////////

void vertex::lerp(const vertex& a, const vertex& b, float flerp) {
  *this = b.lerp(a, flerp);
}

///////////////////////////////////////////////////////////////////////////////

vertex vertex::lerp(const vertex& vtx, float flerp) const {
  vertex vcenter;
  vcenter.mPos.lerp(vtx.mPos, mPos, flerp);
  vcenter.mNrm.lerp(vtx.mNrm, mNrm, flerp);
  vcenter.mNrm.normalizeInPlace();
  for (int ic = 0; ic < vertex::kmaxcolors; ic++) {
    vcenter.mCol[ic].lerp(vtx.mCol[ic], mCol[ic], flerp);
  }
  for (int it = 0; it < vertex::kmaxuvs; it++) {
    vcenter.mUV[it].lerp(vtx.mUV[it], mUV[it], flerp);
  }
  return vcenter;
}

///////////////////////////////////////////////////////////////////////////////

void vertex::Center(const vertex** pverts, int icnt) {
  mPos = dvec3(0.0f, 0.0f, 0.0f);
  mNrm = dvec3(0.0f, 0.0f, 0.0f);
  for (int i = 0; i < vertex::kmaxcolors; i++) {
    mCol[i] = fvec4(0.0f, 0.0f, 0.0f);
  }
  for (int i = 0; i < vertex::kmaxuvs; i++) {
    mUV[i].Clear();
  }
  float ficnt = 1.0f / float(icnt);
  for (int i = 0; i < icnt; i++) {
    mPos += pverts[i]->mPos * ficnt;
    mNrm += pverts[i]->mNrm * ficnt;
    for (int ic = 0; ic < vertex::kmaxcolors; ic++) {
      mCol[ic] += pverts[i]->mCol[ic] * ficnt;
    }
    for (int it = 0; it < vertex::kmaxuvs; it++) {
      mUV[it].mMapTexCoord += pverts[i]->mUV[it].mMapTexCoord * ficnt;
      mUV[it].mMapBiNormal += pverts[i]->mUV[it].mMapBiNormal * ficnt;
      mUV[it].mMapTangent += pverts[i]->mUV[it].mMapTangent * ficnt;
    }
  }
  mNrm.normalizeInPlace();
  for (int it = 0; it < vertex::kmaxuvs; it++) {
    mUV[it].mMapBiNormal.normalizeInPlace();
    mUV[it].mMapTangent.normalizeInPlace();
  }
}

///////////////////////////////////////////////////////////////////////////////

void vertex::center(const std::vector<vertex_ptr_t>& verts) {
  int icnt = verts.size();
  mPos     = dvec3(0.0f, 0.0f, 0.0f);
  mNrm     = dvec3(0.0f, 0.0f, 0.0f);
  for (int i = 0; i < vertex::kmaxcolors; i++) {
    mCol[i] = fvec4(0.0f, 0.0f, 0.0f);
  }
  for (int i = 0; i < vertex::kmaxuvs; i++) {
    mUV[i].Clear();
  }
  float ficnt = 1.0f / float(icnt);
  for (int i = 0; i < icnt; i++) {
    mPos += verts[i]->mPos * ficnt;
    mNrm += verts[i]->mNrm * ficnt;
    for (int ic = 0; ic < vertex::kmaxcolors; ic++) {
      mCol[ic] += verts[i]->mCol[ic] * ficnt;
    }
    for (int it = 0; it < vertex::kmaxuvs; it++) {
      mUV[it].mMapTexCoord += verts[i]->mUV[it].mMapTexCoord * ficnt;
      mUV[it].mMapBiNormal += verts[i]->mUV[it].mMapBiNormal * ficnt;
      mUV[it].mMapTangent += verts[i]->mUV[it].mMapTangent * ficnt;
    }
  }
  mNrm.normalizeInPlace();
  for (int it = 0; it < vertex::kmaxuvs; it++) {
    mUV[it].mMapBiNormal.normalizeInPlace();
    mUV[it].mMapTangent.normalizeInPlace();
  }
}

///////////////////////////////////////////////////////////////////////////////

uint64_t vertex::hash(double quantization) const {

  if(quantization==0.0)
    quantization = _default_quantization;

  XXH64HASH xxh;
  xxh.init();
  xxh.accumulateItem(miNumWeights);
  xxh.accumulateItem(miNumColors);
  xxh.accumulateItem(miNumUvs);
  for (int i = 0; i < miNumWeights; i++) {
    int ilen = (int)_jointpaths[i].length();
    if (ilen) {
      xxh.accumulateString(_jointpaths[i]);
    }
    xxh.accumulateItem(mJointWeights[i]);
  }
  for (int i = 0; i < miNumColors; i++) {
    xxh.accumulateItem(mCol[i].hash(quantization));
  }
  for (int i = 0; i < miNumUvs; i++) {
    const auto& UV = mUV[i];
    xxh.accumulateItem(UV.mMapBiNormal.hash(quantization));
    xxh.accumulateItem(UV.mMapTangent.hash(quantization));
    xxh.accumulateItem(UV.mMapTexCoord.hash(quantization));
  }
  uint64_t pos_hash = mPos.hash(quantization);
  uint64_t nrm_hash = mNrm.hash(quantization);
  xxh.accumulateItem(pos_hash);
  xxh.accumulateItem(nrm_hash);

  // printf( "pos<%.*e %.*e %.*e> pos_hash<0x%zx>\n", 10, mPos.x, 10, mPos.y, 10, mPos.z, pos_hash );
  xxh.finish();
  return xxh.result();
}

////////////////////////////////////////////////////////////////

const AnnoMap* Polygon::GetAnnoMap() const {
  return mAnnotationSet;
}

void Polygon::visitEdges(const std::function<void(edge_ptr_t)>& visitor) const {
  size_t num_verts = _vertices.size();
  for (int i = 0; i < num_verts; i++) {
    auto v0 = _vertices[i];
    auto v1 = _vertices[(i + 1) % num_verts];
    visitor(std::make_shared<edge>(v0, v1));
  }
}

////////////////////////////////////////////////////////////////

void Polygon::reverse() {
  std::reverse(_vertices.begin(), _vertices.end());
}

////////////////////////////////////////////////////////////////

bool Polygon::containsVertex(vertex_ptr_t v) const {
  auto as_const = std::const_pointer_cast<const struct vertex>(v);
  return containsVertex(as_const);
}

////////////////////////////////////////////////////////////////

bool Polygon::containsVertex(vertex_const_ptr_t v) const {
  for (auto v2 : _vertices) {
    if (v2 == v)
      return true;
  }
  return false;
}

bool Polygon::containsVertices(vertex_set_t vset) const {
  for (auto vitem : vset._the_map) {
    auto v = vitem.second;
    if (not containsVertex(v))
      return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////

bool Polygon::containsEdge(const edge& e, bool ordered) const {
  size_t num_verts = _vertices.size();
  for (int i = 0; i < num_verts; i++) {
    auto v0 = _vertices[i];
    auto v1 = _vertices[(i + 1) % num_verts];
    if ((v0 == e._vertexA) and (v1 == e._vertexB))
      return true;
    else if ((not ordered) and (v0 == e._vertexB) and (v1 == e._vertexA))
      return true;
  }
  return false;
}
bool Polygon::containsEdge(edge_ptr_t e, bool ordered) const {
  return containsEdge(*e, ordered);
}

////////////////////////////////////////////////////////////////

edge_vect_t Polygon::edges() const {
  edge_vect_t rval;
  size_t num_verts = _vertices.size();
  for (int i = 0; i < num_verts; i++) {
    auto va = _vertices[i];
    auto vb = _vertices[(i + 1) % num_verts];
    rval.push_back(std::make_shared<edge>(va, vb));
  }
  return rval;
}

////////////////////////////////////////////////////////////////

edge_ptr_t Polygon::edgeForVertices(vertex_ptr_t va, vertex_ptr_t vb) const {
  size_t num_verts = _vertices.size();
  for (int i = 0; i < num_verts; i++) {
    auto v0 = _vertices[i];
    auto v1 = _vertices[(i + 1) % num_verts];
    if ((v0 == va) and (v1 == vb))
      return std::make_shared<edge>(va, vb);
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////

void Polygon::SetAnnoMap(const AnnoMap* pmap) {
  mAnnotationSet = pmap;
}

////////////////////////////////////////////////////////////////

size_t Polygon::numSides() const {
  return _vertices.size();
}

////////////////////////////////////////////////////////////////

size_t Polygon::numVertices() const {
  return _vertices.size();
}

////////////////////////////////////////////////////////////////

int Polygon::vertexID(int i) const {
  OrkAssert(i < numVertices());
  return _vertices[i]->_poolindex;
}

////////////////////////////////////////////////////////////////

vertex_ptr_t Polygon::vertex(int i) const {
  OrkAssert(i < numVertices());
  return _vertices[i];
}

////////////////////////////////////////////////////////////////

dvec3 Polygon::vertexPos(int i) const {
  OrkAssert(i < numVertices());
  return _vertices[i]->mPos;
}

////////////////////////////////////////////////////////////////

dvec3 Polygon::vertexNormal(int i) const {
  OrkAssert(i < numVertices());
  return _vertices[i]->mNrm;
}

////////////////////////////////////////////////////////////////

void Polygon::visitVertices(const std::function<void(vertex_ptr_t)>& visitor) const {
  for (auto v : _vertices) {
    visitor(v);
  }
}

// void Polygon::addVertex(vertex_ptr_t v) {
// OrkAssert(_submeshIndex==-1); // make sure we have not merged this poly yet...
//_vertices.push_back(v);
//}

////////////////////////////////////////////////////////////////

Polygon::Polygon(vertex_ptr_t ia, vertex_ptr_t ib, vertex_ptr_t ic)
    : mAnnotationSet(0) {

  OrkAssert(ia);
  OrkAssert(ib);
  OrkAssert(ic);

  _vertices.push_back(ia);
  _vertices.push_back(ib);
  _vertices.push_back(ic);

  // auto eab = std::make_shared<edge>(ia,ib);
  // auto ebc = std::make_shared<edge>(ib,ic);
  // auto eca = std::make_shared<edge>(ic,ia);
  // for (int i = 4; i < kmaxsidesperpoly; i++) {
  //_edges[i] = nullptr;
  //}
}

////////////////////////////////////////////////////////////////

Polygon::Polygon(vertex_ptr_t ia, vertex_ptr_t ib, vertex_ptr_t ic, vertex_ptr_t id)
    : mAnnotationSet(0) {

  OrkAssert(ia);
  OrkAssert(ib);
  OrkAssert(ic);
  OrkAssert(id);

  _vertices.push_back(ia);
  _vertices.push_back(ib);
  _vertices.push_back(ic);
  _vertices.push_back(id);

  // for (int i = 4; i < kmaxsidesperpoly; i++) {
  //_edges[i] = nullptr;
  //}
}

////////////////////////////////////////////////////////////////

Polygon::Polygon(const std::vector<vertex_ptr_t>& vertices)
    : mAnnotationSet(0) {

  // OrkAssert(vertices.size()<=5);
  OrkAssert(vertices.size() >= 3);

  for (int i = 0; i < vertices.size(); i++) {
    auto in_vertex = vertices[i];
    OrkAssert(in_vertex);
    _vertices.push_back(in_vertex);
  }

  // for (int i = 4; i < kmaxsidesperpoly; i++) {
  //_edges[i] = nullptr;
  //}
}

///////////////////////////////////////////////////////////////////////////////
/*   // disabled during submesh refactor

int Polygon::VertexCW(int vert) const {
  for (int i = 0; i < miNumSides; i++) {
    if (miVertices[i] == vert)
      return miVertices[(i + miNumSides - 1) % miNumSides];
  }
  return -1;
}

///////////////////////////////////////////////////////////////////////////////

int Polygon::VertexCCW(int vert) const {
  for (int i = 0; i < miNumSides; i++) {
    if (miVertices[i] == vert)
      return miVertices[(i + 1) % miNumSides];
  }
  return -1;
}
*/
///////////////////////////////////////////////////////////////////////////////
vertex Polygon::computeCenter() const {
  int inumv = numSides();
  struct vertex vcenter;
  double frecip = double(1.0) / double(inumv);
  visitVertices([&](vertex_ptr_t v) {
    vcenter.mPos += v->mPos;
    vcenter.mNrm += v->mNrm;
    for (int it = 0; it < vertex::kmaxuvs; it++) {
      vcenter.mUV[it] = vcenter.mUV[it] + (v->mUV[it] * frecip);
    }
    for (int ic = 0; ic < vertex::kmaxcolors; ic++) {
      vcenter.mCol[ic] += (v->mCol[ic] * frecip);
    }
  });
  vcenter.mPos = vcenter.mPos * frecip;
  vcenter.mNrm.normalizeInPlace();
  return vcenter;
}

dvec3 Polygon::centerOfMass() const {

  struct tri {
    dvec3 a;
    dvec3 b;
    dvec3 c;
    dvec3 na;
    dvec3 nb;
    dvec3 nc;
    dvec3 center() const {
      return (a + b + c) * (1.0 / 3.0);
    }
    dvec3 avg_n() const {
      return (na + nb + nc).normalized();
    }
    double _area = 0.0;
  };

  std::vector<tri> tris;

  ork::dvec3 base   = _vertices[0]->mPos;
  ork::dvec3 prev   = _vertices[1]->mPos;
  double total_area = 0.0;
  for (int i = 2; i < _vertices.size(); i++) {
    ork::dvec3 next = _vertices[i]->mPos;
    tri t;
    t.c = base;
    t.a = prev;
    t.b = next;
    t._area += (prev - base).crossWith(next - base).magnitude() * 0.5;
    total_area += t._area;
    tris.push_back(t);
    prev = next;
  }

  dvec3 vcenter;
  for (auto t : tris) {

    dvec3 c           = t.center();
    double proportion = t._area / total_area;
    vcenter += c * proportion;
  }

  return vcenter;
}

///////////////////////////////////////////////////////////////////////////////

double Polygon::signedVolumeWithPoint(const dvec3& point) const {
  // compute volume of polygon as volume of tetrahedron fan
  double volume   = 0.0;
  ork::dvec3 base = _vertices[0]->mPos;
  ork::dvec3 prev = _vertices[1]->mPos;
  for (int i = 2; i < _vertices.size(); i++) {
    ork::dvec3 next = _vertices[i]->mPos;
    volume += (prev - base).crossWith(next - base).dotWith(point - base);
    prev = next;
  }
  return volume;
}

///////////////////////////////////////////////////////////////////////////////

double Polygon::computeArea(const dmtx4& MatRange) const {
  double farea    = 0.0f;
  ork::dvec3 base = _vertices[0]->mPos.transform(MatRange);
  ork::dvec3 prev = _vertices[1]->mPos.transform(MatRange);
  // compute area polygon as area of triangle fan
  for (int i = 2; i < _vertices.size(); i++) {
    ork::dvec3 next = _vertices[i]->mPos.transform(MatRange);
    // area of triangle 1/2 length of cross product the vector of any two edges
    farea += (prev - base).crossWith(next - base).magnitude() * 0.5;
    prev = next;
  }
  return farea;
}

///////////////////////////////////////////////////////////////////////////////

double Polygon::computeEdgeLength(const dmtx4& MatRange, int iedge) const {
  int inumvtx = _vertices.size();
  auto v0     = _vertices[(iedge + 0) % inumvtx];
  auto v1     = _vertices[(iedge + 1) % inumvtx];
  double elen = (v0->mPos.transform(MatRange) - v1->mPos.transform(MatRange)).magnitude();
  return elen;
}

///////////////////////////////////////////////////////////////////////////////

double Polygon::minEdgeLength(const dmtx4& MatRange) const {
  double min_len = 1e12;
  int numedges   = _vertices.size();
  for (int e = 0; e < numedges; e++) {
    double elen = computeEdgeLength(MatRange, e);
    if (elen < min_len)
      min_len = elen;
  }
  return min_len;
}

double Polygon::maxEdgeLength(const dmtx4& MatRange) const {
  double max_len = 0.0;
  int numedges   = _vertices.size();
  for (int e = 0; e < numedges; e++) {
    double elen = computeEdgeLength(MatRange, e);
    if (elen > max_len)
      max_len = elen;
  }
  return max_len;
}

///////////////////////////////////////////////////////////////////////////////

dplane3 Polygon::computePlane() const { // using CCW rules
  OrkAssert(_vertices.size() >= 3);
  auto n = computeNormal();
  auto v0  = _vertices[0];
  return dplane3(n, v0->mPos);
}

///////////////////////////////////////////////////////////////////////////////

dvec3 Polygon::computeNormal() const { // using CCW rules
  dvec3 rval(0, 0, 0);
  int inumvtx = _vertices.size();
  for (size_t i = 0; i < inumvtx; ++i) {
    dvec3 current = _vertices[i]->mPos;
    dvec3 next    = _vertices[(i + 1) % inumvtx]->mPos;
    dvec3 edge1 = next - current;
    dvec3 edge2 = _vertices[(i + 2) % inumvtx]->mPos - next;
    rval += edge1.crossWith(edge2);
  }
  return rval.normalized();
}

///////////////////////////////////////////////////////////////////////////////

uint64_t Polygon::hash(void) const {
  struct bubblesort {
    static void doit(int* array, int length) {
      int i, j, temp;
      int test; // use this only if unsure whether the list is already sorted or not
      for (i = length - 1; i > 0; i--) {
        test = 0;
        for (j = 0; j < i; j++) {
          if (array[j] > array[j + 1]) // compare neighboring elements
          {
            temp         = array[j]; // swap array[j] and array[j+1]
            array[j]     = array[j + 1];
            array[j + 1] = temp;
            test         = 1;
          }
        }
        if (test == 0)
          break; // will exit if the list is sorted!
      }
    }
  };
  std::vector<int> my_array;
  int inumv = numSides();
  for (int i = 0; i < inumv; i++) {
    my_array.push_back(_vertices[i]->_poolindex);
  }
  bubblesort::doit(my_array.data(), inumv);
  XXH64HASH xxh;
  xxh.init();
  xxh.accumulate((const void*)my_array.data(), sizeof(int) * inumv);
  xxh.finish();
  return xxh.result();
}

///////////////////////////////////////////////////////////////////////////////

const std::string& Polygon::GetAnnotation(const std::string& annoname) const {
  if (mAnnotationSet) {
    orkmap<std::string, std::string>::const_iterator it = mAnnotationSet->_annotations.find(annoname);
    if (it != mAnnotationSet->_annotations.end()) {
      return it->second;
    }
  }
  return gnomatch;
}

////////////////////////////////////////////////////////////////

MergedPolygon::MergedPolygon(vertex_ptr_t ia, vertex_ptr_t ib, vertex_ptr_t ic)
    : Polygon(ia, ib, ic) {
}
MergedPolygon::MergedPolygon(vertex_ptr_t ia, vertex_ptr_t ib, vertex_ptr_t ic, vertex_ptr_t id)
    : Polygon(ia, ib, ic, id) {
}
MergedPolygon::MergedPolygon(const std::vector<vertex_ptr_t>& vertices)
    : Polygon(vertices) {
}

////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
