////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/math/plane.h>
#include <ork/lev2/gfx/meshutil/submesh.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace meshutil {
///////////////////////////////////////////////////////////////////////////////

static const std::string gnomatch("");

///////////////////////////////////////////////////////////////////////////////

const std::string& AnnoMap::GetAnnotation(const std::string& annoname) const {
  orkmap<std::string, std::string>::const_iterator it = _annotations.find(annoname);
  if (it != _annotations.end()) {
    return it->second;
  }
  return gnomatch;
}

void AnnoMap::SetAnnotation(const std::string& key, const std::string& val) {
  _annotations[key] = val;
}

///////////////////////////////////////////////////////////////////////////////

AnnoMap* AnnoMap::Fork() const {
  AnnoMap* newannomap      = new AnnoMap;
  newannomap->_annotations = _annotations;
  return newannomap;
}

///////////////////////////////////////////////////////////////////////////////

AnnoMap::AnnoMap() {
  gAllAnnoSets.insert(this);
}

///////////////////////////////////////////////////////////////////////////////

AnnoMap::~AnnoMap() {
  gAllAnnoSets.erase(this);
}

orkset<AnnoMap*> AnnoMap::gAllAnnoSets;

///////////////////////////////////////////////////////////////////////////////

U64 annopolyposlut::HashItem(const submesh& tmesh, const poly& ply) const {
  boost::Crc64 crc64;
  int inumpv = ply.GetNumSides();
  for (int iv = 0; iv < inumpv; iv++) {
    int ivi           = ply.GetVertexID(iv);
    const vertex& vtx = tmesh.RefVertexPool().GetVertex(ivi);
    crc64.accumulateItem(vtx.mPos);
    crc64.accumulateItem(vtx.mNrm);
  }
  crc64.finish();
  return crc64.result();
}

///////////////////////////////////////////////////////////////////////////////

const AnnoMap* annopolylut::Find(const submesh& tmesh, const poly& ply) const {
  const AnnoMap* rval                            = 0;
  U64 uhash                                      = HashItem(tmesh, ply);
  orkmap<U64, const AnnoMap*>::const_iterator it = mAnnoMap.find(uhash);
  if (it != mAnnoMap.end()) {
    rval = it->second;
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void vertex::Lerp(const vertex& a, const vertex& b, float flerp) {
  *this = b.Lerp(a, flerp);
}

///////////////////////////////////////////////////////////////////////////////

vertex vertex::Lerp(const vertex& vtx, float flerp) const {
  vertex vcenter;
  vcenter.mPos.Lerp(vtx.mPos, mPos, flerp);
  vcenter.mNrm.Lerp(vtx.mNrm, mNrm, flerp);
  vcenter.mNrm.Normalize();
  for (int ic = 0; ic < vertex::kmaxcolors; ic++) {
    vcenter.mCol[ic].Lerp(vtx.mCol[ic], mCol[ic], flerp);
  }
  for (int it = 0; it < vertex::kmaxuvs; it++) {
    vcenter.mUV[it].Lerp(vtx.mUV[it], mUV[it], flerp);
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
  mNrm.Normalize();
  for (int it = 0; it < vertex::kmaxuvs; it++) {
    mUV[it].mMapBiNormal.Normalize();
    mUV[it].mMapTangent.Normalize();
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

vertex poly::ComputeCenter(const vertexpool& vpool) const {
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
  vcenter.mNrm.Normalize();
  return vcenter;
}

///////////////////////////////////////////////////////////////////////////////

float poly::ComputeArea(const vertexpool& vpool, const fmtx4& MatRange) const {
  float farea     = 0.0f;
  ork::fvec3 base = _vertices[0]->mPos.Transform(MatRange);
  ork::fvec3 prev = _vertices[1]->mPos.Transform(MatRange);
  // compute area polygon as area of triangle fan
  for (int i = 2; i < miNumSides; i++) {
    ork::fvec3 next = _vertices[i]->mPos.Transform(MatRange);
    // area of triangle 1/2 length of cross product the vector of any two edges
    farea += (prev - base).Cross(next - base).Mag() * 0.5f;
    prev = next;
  }
  return farea;
}

///////////////////////////////////////////////////////////////////////////////

float poly::ComputeEdgeLength(const vertexpool& vpool, const fmtx4& MatRange, int iedge) const {
  int inumvtx = miNumSides;
  auto v0     = _vertices[(iedge + 0) % miNumSides];
  auto v1     = _vertices[(iedge + 1) % miNumSides];
  float elen  = (v0->mPos.Transform(MatRange) - v1->mPos.Transform(MatRange)).Mag();
  return elen;
}

///////////////////////////////////////////////////////////////////////////////

fvec3 poly::ComputeNormal(const vertexpool& vpool) const {
  fvec3 rval(0, 0, 0);
  auto v0 = _vertices[0]->mPos;
  auto v1 = _vertices[1]->mPos;
  for (int i = 2; i < miNumSides; i++) {
    auto v2 = _vertices[i % miNumSides]->mPos;
    rval += (v0 - v1).Cross(v2 - v1);
    v0 = v1;
    v1 = v2;
  }
  return rval.Normal();
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
  int myarray[kmaxsidesperpoly];
  int inumv = GetNumSides();
  for (int i = 0; i < inumv; i++) {
    myarray[i] = _vertices[i]->_poolindex;
  }
  for (int i = inumv; i < kmaxsidesperpoly; i++) {
    myarray[i] = 0;
  }
  bubblesort::doit(myarray, inumv);
  boost::Crc64 crc64;
  crc64.accumulate((const void*)&myarray[0], sizeof(int) * inumv);
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

///////////////////////////////////////////////////////////////////////////////

void uvmapcoord::Lerp(const uvmapcoord& ina, const uvmapcoord& inb, float flerp) {
  mMapTexCoord.Lerp(ina.mMapTexCoord, inb.mMapTexCoord, flerp);
  mMapBiNormal.Lerp(ina.mMapBiNormal, inb.mMapBiNormal, flerp);
  mMapTangent.Lerp(ina.mMapTangent, inb.mMapTangent, flerp);
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

///////////////////////////////////////////////////////////////////////////////

vertexpool::vertexpool() {
}

///////////////////////////////////////////////////////////////////////////////

vertex_ptr_t vertexpool::newMergeVertex(const vertex& vtx) {
  vertex_ptr_t rval;
  U64 vhash                        = vtx.Hash();
  HashU64IntMap::const_iterator it = VertexPoolMap.find(vhash);
  if (VertexPoolMap.end() != it) {
    int iother = it->second;
    rval       = VertexPool[iother];
    // boost::Crc64 otherCRC = boost::crc64( (const void *) & vtx, sizeof( vertex ) );
    // U32 otherCRC = Crc32( (const unsigned char *) & OtherVertex, sizeof( vertex ) );
    // OrkAssert( Crc32::DoesDataMatch( & vtx, & OtherVertex, sizeof( vertex ) ) );
  } else {
    int ipv                = (int)VertexPool.size();
    auto new_vertex        = std::make_shared<vertex>(vtx);
    rval                   = new_vertex;
    new_vertex->_poolindex = uint32_t(VertexPool.size());
    VertexPool.push_back(new_vertex);
    VertexPoolMap[vhash] = ipv;
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

int vertexpool::MergeVertex(const vertex& vtx, int inidx) {
  int ioutidx                      = -1;
  U64 vhash                        = vtx.Hash();
  HashU64IntMap::const_iterator it = VertexPoolMap.find(vhash);
  if (VertexPoolMap.end() != it) {
    int iother       = it->second;
    auto OtherVertex = VertexPool[iother];
    // boost::Crc64 otherCRC = boost::crc64( (const void *) & vtx, sizeof( vertex ) );
    // U32 otherCRC = Crc32( (const unsigned char *) & OtherVertex, sizeof( vertex ) );
    // OrkAssert( Crc32::DoesDataMatch( & vtx, & OtherVertex, sizeof( vertex ) ) );
    ioutidx = iother;
  } else {
    int ipv                = (int)VertexPool.size();
    auto new_vertex        = std::make_shared<vertex>(vtx);
    new_vertex->_poolindex = uint32_t(VertexPool.size());
    VertexPool.push_back(new_vertex);
    VertexPoolMap[vhash] = ipv;
    ioutidx              = ipv;
  }
  return ioutidx;
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
