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

vertexpool::vertexpool() {
}

///////////////////////////////////////////////////////////////////////////////

vertex_ptr_t vertexpool::newMergeVertex(const vertex& vtx) {
  vertex_ptr_t rval;
  U64 vhash = vtx.Hash();
  auto it   = _vtxmap.find(vhash);
  if (_vtxmap.end() != it) {
    rval = it->second;
    // boost::Crc64 otherCRC = boost::crc64( (const void *) & vtx, sizeof( vertex ) );
    // U32 otherCRC = Crc32( (const unsigned char *) & OtherVertex, sizeof( vertex ) );
    // OrkAssert( Crc32::DoesDataMatch( & vtx, & OtherVertex, sizeof( vertex ) ) );
  } else {
    rval             = std::make_shared<vertex>(vtx);
    rval->_poolindex = uint32_t(_orderedVertices.size());
    _orderedVertices.push_back(rval);
    _vtxmap[vhash] = rval;
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
