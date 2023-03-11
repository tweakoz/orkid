////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/orklut.hpp>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/meshutil/submesh.h>

namespace ork::meshutil {
static const std::string gnomatch("");

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
    mJointNames[i]   = "";
    mJointWeights[i] = float(0.0f);
  }
}

////////////////////////////////////////////////////////////////

vertex::vertex(fvec3 pos, fvec3 nrm, fvec3 bin, fvec2 uv, fvec4 col)
    : vertex() {
  set(pos, nrm, bin, uv, col);
}

////////////////////////////////////////////////////////////////

void vertex::set(fvec3 pos, fvec3 nrm, fvec3 bin, fvec2 uv, fvec4 col) {
  mPos                = pos;
  mNrm                = nrm;
  mUV[0].mMapTexCoord = uv;
  mUV[0].mMapBiNormal = bin;
  mCol[0]             = col;
  miNumColors         = 1;
  miNumUvs            = 1;
}

////////////////////////////////////////////////////////////////

const fvec3& vertex::Pos() const {
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
  mPos = fvec3(0.0f, 0.0f, 0.0f);
  mNrm = fvec3(0.0f, 0.0f, 0.0f);
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

U64 vertex::Hash() const {
  boost::Crc64 crc64;
  crc64.accumulateItem(miNumWeights);
  crc64.accumulateItem(miNumColors);
  crc64.accumulateItem(miNumUvs);
  for (int i = 0; i < vertex::kmaxinfluences; i++) {
    int ilen = (int)mJointNames[i].length();
    if (ilen) {
      crc64.accumulateString(mJointNames[i]);
    }
  }
  crc64.accumulateItem(mCol);
  crc64.accumulateItem(mUV);
  crc64.accumulateItem(mJointWeights);
  crc64.accumulateItem(mNrm);
  crc64.accumulateItem(mPos);
  crc64.finish();
  return crc64.result();
}

////////////////////////////////////////////////////////////////

vertex_ptr_t edge::edgeVertex(int iv) const {
  switch (iv) {
    case 0:
      return _vertexA;
      break;
    case 1:
      return _vertexB;
      break;
    default:
      OrkAssert(false);
      break;
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////

edge::edge()
    : miNumConnectedPolys(0) {
  for (int i = 0; i < kmaxpolysperedge; i++)
    miConnectedPolys[i] = -1;
}

////////////////////////////////////////////////////////////////

edge::edge(vertex_ptr_t va, vertex_ptr_t vb)
    : _vertexA(va)
    , _vertexB(vb)
    , miNumConnectedPolys(0) {
  for (int i = 0; i < kmaxpolysperedge; i++)
    miConnectedPolys[i] = -1;
}

////////////////////////////////////////////////////////////////

int edge::GetNumConnectedPolys(void) const {
  return miNumConnectedPolys;
}

////////////////////////////////////////////////////////////////

int edge::GetConnectedPoly(int ip) const {
  OrkAssert(ip < miNumConnectedPolys);
  return miConnectedPolys[ip];
}

///////////////////////////////////////////////////////////////////////////////

void edge::ConnectToPoly(int ipoly) {
  OrkAssert(miNumConnectedPolys < kmaxpolysperedge);
  miConnectedPolys[miNumConnectedPolys++] = ipoly;
}

///////////////////////////////////////////////////////////////////////////////

U64 edge::GetHashKey(void) const {
  u64 uv = (_vertexA->_poolindex < _vertexB->_poolindex) //
               ? u64(_vertexA->_poolindex) | (u64(_vertexB->_poolindex) << 32)
               : u64(_vertexB->_poolindex) | (u64(_vertexA->_poolindex) << 32);
  return uv;
}

///////////////////////////////////////////////////////////////////////////////

bool edge::Matches(const edge& other) const {
  return ((_vertexA == other._vertexA) && (_vertexB == other._vertexB)) ||
         ((_vertexB == other._vertexA) && (_vertexA == other._vertexB)); // should we care about order here ?
}

////////////////////////////////////////////////////////////////

const AnnoMap* poly::GetAnnoMap() const {
  return mAnnotationSet;
}

////////////////////////////////////////////////////////////////

void poly::SetAnnoMap(const AnnoMap* pmap) {
  mAnnotationSet = pmap;
}

////////////////////////////////////////////////////////////////

int poly::GetNumSides(void) const {
  return _vertices.size();
}

////////////////////////////////////////////////////////////////

int poly::GetVertexID(int i) const {
  OrkAssert(i < GetNumSides());
  return _vertices[i]->_poolindex;
}

////////////////////////////////////////////////////////////////

poly::poly(vertex_ptr_t ia, vertex_ptr_t ib, vertex_ptr_t ic)
    : mAnnotationSet(0) {

  OrkAssert(ia);
  OrkAssert(ib);
  OrkAssert(ic);

  _vertices.push_back(ia);
  _vertices.push_back(ib);
  _vertices.push_back(ic);

  //auto eab = std::make_shared<edge>(ia,ib);
  //auto ebc = std::make_shared<edge>(ib,ic);
  //auto eca = std::make_shared<edge>(ic,ia);
  //for (int i = 4; i < kmaxsidesperpoly; i++) {
    //_edges[i] = nullptr;
  //}
}

////////////////////////////////////////////////////////////////

poly::poly(vertex_ptr_t ia, vertex_ptr_t ib, vertex_ptr_t ic, vertex_ptr_t id)
    : mAnnotationSet(0) {

  OrkAssert(ia);
  OrkAssert(ib);
  OrkAssert(ic);
  OrkAssert(id);

  _vertices.push_back(ia);
  _vertices.push_back(ib);
  _vertices.push_back(ic);
  _vertices.push_back(id);

  //for (int i = 4; i < kmaxsidesperpoly; i++) {
    //_edges[i] = nullptr;
  //}
}

////////////////////////////////////////////////////////////////

poly::poly(const std::vector<vertex_ptr_t>& vertices)
    : mAnnotationSet(0) {

  OrkAssert(vertices.size()<=5);
  OrkAssert(vertices.size()>=3);

  for( int i=0; i<vertices.size(); i++ ){
    auto in_vertex = vertices[i];
    OrkAssert(in_vertex);
    _vertices.push_back(in_vertex);
  }

  //for (int i = 4; i < kmaxsidesperpoly; i++) {
    //_edges[i] = nullptr;
  //}
}

///////////////////////////////////////////////////////////////////////////////
/*   // disabled during submesh refactor

int poly::VertexCW(int vert) const {
  for (int i = 0; i < miNumSides; i++) {
    if (miVertices[i] == vert)
      return miVertices[(i + miNumSides - 1) % miNumSides];
  }
  return -1;
}

///////////////////////////////////////////////////////////////////////////////

int poly::VertexCCW(int vert) const {
  for (int i = 0; i < miNumSides; i++) {
    if (miVertices[i] == vert)
      return miVertices[(i + 1) % miNumSides];
  }
  return -1;
}
*/
///////////////////////////////////////////////////////////////////////////////
vertex poly::ComputeCenter() const {
  int inumv = GetNumSides();
  vertex vcenter;
  vcenter.mPos = fvec4(0.0f, 0.0f, 0.0f);
  vcenter.mNrm = fvec4(0.0f, 0.0f, 0.0f);
  for (int ic = 0; ic < vertex::kmaxcolors; ic++) {
    vcenter.mCol[ic] = fvec4(0.0f, 0.0f, 0.0f);
  }
  for (int it = 0; it < vertex::kmaxuvs; it++) {
    vcenter.mUV[it].Clear();
  }
  float frecip = float(1.0f) / float(inumv);
  for (int iv = 0; iv < inumv; iv++) {
    auto v       = _vertices[iv];
    vcenter.mPos = vcenter.mPos + v->mPos;
    vcenter.mNrm = vcenter.mNrm + v->mNrm;
    for (int it = 0; it < vertex::kmaxuvs; it++) {
      vcenter.mUV[it] = vcenter.mUV[it] + (v->mUV[it] * frecip);
    }
    for (int ic = 0; ic < vertex::kmaxcolors; ic++) {
      vcenter.mCol[ic] += (v->mCol[ic] * frecip);
    }
  }
  vcenter.mPos = vcenter.mPos * frecip;
  vcenter.mNrm.normalizeInPlace();
  return vcenter;
}

///////////////////////////////////////////////////////////////////////////////

float poly::ComputeArea(const fmtx4& MatRange) const {
  float farea     = 0.0f;
  ork::fvec3 base = _vertices[0]->mPos.transform(MatRange);
  ork::fvec3 prev = _vertices[1]->mPos.transform(MatRange);
  // compute area polygon as area of triangle fan
  for (int i = 2; i < _vertices.size(); i++) {
    ork::fvec3 next = _vertices[i]->mPos.transform(MatRange);
    // area of triangle 1/2 length of cross product the vector of any two edges
    farea += (prev - base).crossWith(next - base).magnitude() * 0.5f;
    prev = next;
  }
  return farea;
}

///////////////////////////////////////////////////////////////////////////////

float poly::ComputeEdgeLength(const fmtx4& MatRange, int iedge) const {
  int inumvtx = _vertices.size();
  auto v0     = _vertices[(iedge + 0) % inumvtx];
  auto v1     = _vertices[(iedge + 1) % inumvtx];
  float elen  = (v0->mPos.transform(MatRange) - v1->mPos.transform(MatRange)).magnitude();
  return elen;
}

///////////////////////////////////////////////////////////////////////////////

fplane3 poly::computePlane() const{
  OrkAssert(_vertices.size()>=3);
  auto v0 = _vertices[0];
  auto v1 = _vertices[1];
  auto v2 = _vertices[2];
  return fplane3(v0->mPos,v1->mPos,v2->mPos);
}

///////////////////////////////////////////////////////////////////////////////

fvec3 poly::ComputeNormal() const {
  fvec3 rval(0, 0, 0);
  int inumvtx = _vertices.size();
  auto v0 = _vertices[0]->mPos;
  auto v1 = _vertices[1]->mPos;
  for (int i = 2; i < inumvtx; i++) {
    auto v2 = _vertices[i % inumvtx]->mPos;
    rval += (v0 - v1).crossWith(v2 - v1);
    v0 = v1;
    v1 = v2;
  }
  return rval.normalized();
}

///////////////////////////////////////////////////////////////////////////////

U64 poly::HashIndices(void) const {
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
  int inumv = GetNumSides();
  for (int i = 0; i < inumv; i++) {
    my_array.push_back(_vertices[i]->_poolindex);
  }
  bubblesort::doit(my_array.data(), inumv);
  boost::Crc64 crc64;
  crc64.accumulate((const void*)my_array.data(), sizeof(int) * inumv);
  U64 ucrc = crc64.result();
  return ucrc;
}

///////////////////////////////////////////////////////////////////////////////

const std::string& poly::GetAnnotation(const std::string& annoname) const {
  if (mAnnotationSet) {
    orkmap<std::string, std::string>::const_iterator it = mAnnotationSet->_annotations.find(annoname);
    if (it != mAnnotationSet->_annotations.end()) {
      return it->second;
    }
  }
  return gnomatch;
}

////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
