////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/orklut.hpp>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/meshutil/submesh.h>

template class ork::orklut<std::string, ork::meshutil::submesh*>;

namespace ork::meshutil {

const vertexpool vertexpool::EmptyPool;

/////////////////////////////////////////////////////////////////////////

submesh::submesh(const vertexpool& vpool)
    : mvpool(vpool)
    , mfSurfaceArea(0)
    , mbMergeEdges(true) {
  // mMergedPolys.reserve(32<<10);
  // if( mbMergeEdges )
  {
    // mEdges.reserve(32<<10);
  }
  for (int i = 0; i < kmaxsidesperpoly; i++) {
    mPolyTypeCounter[i] = 0;
  }
}

submesh::~submesh() {
  static size_t gc1 = 0;
  static size_t gc2 = 0;
  static size_t gc3 = 0;
  static size_t gc4 = 0;
  static size_t gc5 = 0;
  static size_t gc6 = 0;

  size_t ic1 = mpolyhashmap.size();
  size_t ic2 = mEdgeMap.size();
  size_t ic3 = mvpool.VertexPoolMap.size();
  size_t ic4 = mvpool.VertexPool.size();
  size_t ic5 = mEdges.size();
  size_t ic6 = mMergedPolys.size();
  gc1 += ic1;

  gc2 += ic2;
  gc3 += ic3;
  gc4 += ic4;
  gc5 += ic5;
  gc6 += ic6;
  size_t is1 = sizeof(std::pair<U64, int>);
  size_t is2 = sizeof(std::pair<int, int>);
  size_t is3 = is1;
  size_t is4 = sizeof(vertex);
  size_t is5 = sizeof(edge);
  size_t is6 = sizeof(poly);

  // orkprintf( "///////////////////////////////////\n" );
  // orkprintf( "polyhash cnt<%d:%d> tot<%d:%d>\n", ic1,ic1*is1, gc1,gc1*is1 );
  // orkprintf( "polys cnt<%d:%d> tot<%d:%d>\n", ic6,ic6*is6, gc6,gc6*is6 );
  // orkprintf( "edgemap cnt<%d:%d> tot<%d:%d>\n", ic2,ic2*is2, gc2,gc2*is2 );
  // orkprintf( "edges cnt<%d:%d> tot<%d:%d>\n", ic5,ic5*is5, gc5,gc5*is5 );
  // orkprintf( "vpoolmap cnt<%d:%d> tot<%d:%d>\n", ic3,ic3*is3, gc3,gc3*is3 );
  // orkprintf( "vpool cnt<%d:%d> tot<%d:%d>\n", ic4,ic4*is4, gc4,gc4*is4 );
  // orkprintf( "///////////////////////////////////\n" );
}

svar64_t submesh::annotation(const char* annokey) const {
  static const char* defret("");
  auto it = mAnnotations.find(std::string(annokey));
  if (it != mAnnotations.end()) {
    return (*it).second;
  }
  return defret;
}

void submesh::MergeAnnos(const AnnotationMap& mrgannos, bool boverwrite) {
  for (AnnotationMap::const_iterator it = mrgannos.begin(); it != mrgannos.end(); it++) {
    const std::string& key = it->first;
    const auto& val        = it->second;

    AnnotationMap::iterator itf = mAnnotations.find(key);
    if (itf == mAnnotations.end()) {
      mAnnotations[key] = val;
    } else if (boverwrite) {
      itf->second = val;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void submesh::ImportPolyAnnotations(const annopolylut& apl) {
  int inumpolys = (int)mMergedPolys.size();
  for (int ip = 0; ip < inumpolys; ip++) {
    poly& ply           = mMergedPolys[ip];
    const AnnoMap* amap = apl.Find(*this, ply);
    if (amap) {
      ply.SetAnnoMap(amap);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void submesh::ExportPolyAnnotations(annopolylut& apl) const {
  int inumpolys = (int)mMergedPolys.size();
  for (int ip = 0; ip < inumpolys; ip++) {
    const poly& ply     = mMergedPolys[ip];
    U64 uhash           = apl.HashItem(*this, ply);
    const AnnoMap* amap = ply.GetAnnoMap();
    apl.mAnnoMap[uhash] = amap;
  }
}

///////////////////////////////////////////////////////////////////////////////

const AABox& submesh::aabox() const {
  if (mAABoxDirty) {
    mAABox.BeginGrow();
    int inumvtx = (int)RefVertexPool().GetNumVertices();
    for (int i = 0; i < inumvtx; i++) {
      const vertex& v = RefVertexPool().GetVertex(i);
      mAABox.Grow(v.mPos);
    }
    mAABox.EndGrow();
    mAABoxDirty = false;
  }
  return mAABox;
}

///////////////////////////////////////////////////////////////////////////////

const edge& submesh::RefEdge(U64 edgekey) const {
  OrkAssert(edgekey != poly::Inv);
  HashU64IntMap::const_iterator it = mEdgeMap.find(edgekey);
  OrkAssert(it != mEdgeMap.end());
  int index = it->second;
  OrkAssert(index < int(mEdges.size()));
  return mEdges[index];
}

///////////////////////////////////////////////////////////////////////////////

int submesh::MergeVertex(const vertex& vtx, int idx) {
  mAABoxDirty = true;
  return mvpool.MergeVertex(vtx, idx);
}

///////////////////////////////////////////////////////////////////////////////

poly& submesh::RefPoly(int i) {
  OrkAssert(orkvector<int>::size_type(i) < mMergedPolys.size());
  return mMergedPolys[i];
}

///////////////////////////////////////////////////////////////////////////////

const poly& submesh::RefPoly(int i) const {
  OrkAssert(orkvector<int>::size_type(i) < mMergedPolys.size());
  return mMergedPolys[i];
}

///////////////////////////////////////////////////////////////////////////////

const orkvector<poly>& submesh::RefPolys() const {
  return mMergedPolys;
}

/////////////////////////////////////////////////////////////////////////

void submesh::FindNSidedPolys(orkvector<int>& output, int inumsides) const {
  int inump = (int)mMergedPolys.size();
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
    iret = (int)mMergedPolys.size();
  } else {
    OrkAssert(inumsides < kmaxsidesperpoly);
    iret = mPolyTypeCounter[inumsides];
  }
  return iret;
}

///////////////////////////////////////////////////////////////////////////////

void submesh::GetEdges(const poly& ply, orkvector<edge>& Edges) const {
  int icnt  = 0;
  int icntf = 0;
  for (int is = 0; is < ply.GetNumSides(); is++) {
    U64 ue                           = ply.mEdges[is];
    HashU64IntMap::const_iterator it = mEdgeMap.find(ue);
    if (it != mEdgeMap.end()) {
      int ie = it->second;
      Edges.push_back(mEdges[ie]);
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

const U64 submesh::GetEdgeBetween(int aind, int bind) const {
  const poly& a = RefPoly(aind);
  const poly& b = RefPoly(bind);
  for (int eaind = 0; eaind < a.miNumSides; eaind++)
    for (int ebind = 0; ebind < b.miNumSides; ebind++)
      if (a.mEdges[eaind] == b.mEdges[ebind])
        return a.mEdges[eaind];
  return poly::Inv;
}

///////////////////////////////////////////////////////////////////////////////

void submesh::GetConnectedPolys(const edge& ed, orkset<int>& output) const {
  U64 keyA                             = ed.GetHashKey();
  HashU64IntMap::const_iterator itfind = mEdgeMap.find(keyA);
  if (itfind != mEdgeMap.end()) {
    int ie              = itfind->second;
    const edge& edfound = mEdges[ie];
    int inump           = edfound.GetNumConnectedPolys();
    for (int ip = 0; ip < inump; ip++) {
      int ipi = edfound.GetConnectedPoly(ip);
      output.insert(ipi);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void submesh::MergeSubMesh(const submesh& inp_mesh) {
  float ftimeA     = float(OldSchool::GetRef().GetLoResTime());
  int inumpingroup = inp_mesh.GetNumPolys();
  for (int i = 0; i < inumpingroup; i++) {
    const poly& ply = inp_mesh.RefPoly(i);
    int inumpv      = ply.GetNumSides();
    poly NewPoly;
    NewPoly.miNumSides = inumpv;
    for (int iv = 0; iv < inumpv; iv++) {
      int ivi                = ply.GetVertexID(iv);
      const vertex& vtx      = inp_mesh.RefVertexPool().GetVertex(ivi);
      int inewvi             = MergeVertex(vtx);
      NewPoly.miVertices[iv] = inewvi;
    }
    NewPoly.SetAnnoMap(ply.GetAnnoMap());
    MergePoly(NewPoly);
  }
  float ftimeB = float(OldSchool::GetRef().GetLoResTime());
  float ftime  = (ftimeB - ftimeA);
  orkprintf("<<PROFILE>> <<submesh::MergeSubMesh %f seconds>>\n", ftime);
}

///////////////////////////////////////////////////////////////////////////////

void submesh::MergePoly(const poly& ply) {
  int ipolyindex = GetNumPolys();
  poly nply      = ply;
  int inumv      = ply.GetNumSides();
  OrkAssert(inumv <= kmaxsidesperpoly);
  ///////////////////////////////
  // zero area poly removal
  switch (inumv) {
    case 3: {
      if ((ply.miVertices[0] == ply.miVertices[1]) || (ply.miVertices[1] == ply.miVertices[2]) ||
          (ply.miVertices[2] == ply.miVertices[0])) {
        orkprintf("Mesh::MergePoly() removing zero area tri<%d %d %d>\n", ply.miVertices[0], ply.miVertices[1], ply.miVertices[2]);

        return;
      }
    }
    case 4: {
      if ((ply.miVertices[0] == ply.miVertices[1]) || (ply.miVertices[0] == ply.miVertices[2]) ||
          (ply.miVertices[0] == ply.miVertices[3]) || (ply.miVertices[1] == ply.miVertices[2]) ||
          (ply.miVertices[1] == ply.miVertices[3]) || (ply.miVertices[2] == ply.miVertices[3])) {
        orkprintf(
            "Mesh::MergePoly() removing zero area quad<%d %d %d %d>\n",
            ply.miVertices[0],
            ply.miVertices[1],
            ply.miVertices[2],
            ply.miVertices[3]);

        return;
      }
    }
      // TODO n-sided polys
  }
  //////////////////////////////
  // dupe check
  U64 ucrc                      = ply.HashIndices();
  HashU64IntMap::iterator itfhm = mpolyhashmap.find(ucrc);
  ///////////////////////////////
  if (itfhm == mpolyhashmap.end()) // no match
  {
    int inewpi         = (int)mMergedPolys.size();
    mpolyhashmap[ucrc] = inewpi;
    //////////////////////////////////////////////////
    // connect to vertices
    for (int i = 0; i < inumv; i++) {
      int iv      = ply.miVertices[i];
      vertex& vtx = mvpool.GetVertex(iv);
      // vtx.ConnectToPoly(inewpi);
    }
    //////////////////////////////////////////////////
    // add edges
    if (mbMergeEdges) {
      for (int i = 0; i < inumv; i++) {
        int i0  = (i);
        int i1  = (i + 1) % inumv;
        int iv0 = ply.GetVertexID(i0);
        int iv1 = ply.GetVertexID(i1);
        edge Edge(iv0, iv1);
        nply.mEdges[i] = MergeEdge(Edge, ipolyindex);
      }
    }
    nply.SetAnnoMap(ply.GetAnnoMap());
    mMergedPolys.push_back(nply);
    //////////////////////////////////////////////////
    // add n sided counters
    mPolyTypeCounter[inumv]++;
    //////////////////////////////////////////////////
    float farea = ply.ComputeArea(mvpool, ork::fmtx4::Identity());
    mfSurfaceArea += farea;
  }
  mAABoxDirty = true;
}

///////////////////////////////////////////////////////////////////////////////

U64 submesh::MergeEdge(const edge& ed, int ipolyindex) {
  U64 crcA                             = ed.GetHashKey();
  HashU64IntMap::const_iterator itfind = mEdgeMap.find(crcA);

  int ieee = -1;

  if (mEdgeMap.end() != itfind) {
    ieee        = itfind->second;
    edge& other = mEdges[ieee];
    U64 crcB    = other.GetHashKey();
    OrkAssert(ed.Matches(other));
  } else {
    ieee = (int)mEdges.size();
    mEdges.push_back(ed);
    mEdgeMap[crcA] = ieee;
  }
  if (ipolyindex >= 0) {
    mEdges[ieee].ConnectToPoly(ipolyindex);
  }

  mAABoxDirty = true;
  return crcA;
}

///////////////////////////////////////////////////////////////////////////////
// addPoly helper methods
///////////////////////////////////////////////////////////////////////////////

void submesh::addQuad(fvec3 p0, fvec3 p1, fvec3 p2, fvec3 p3, fvec2 uv0, fvec2 uv1, fvec2 uv2, fvec2 uv3, fvec4 c) {
  vertex muvtx[4];
  fvec3 p0p1 = (p1 - p0).Normal();
  fvec3 p0p2 = (p2 - p0).Normal();
  fvec3 nrm  = p0p1.Cross(p0p2);
  // todo compute tangent space from uv gradients
  fvec3 bin = p0p1;
  muvtx[0].set(p0, nrm, bin, uv0, c);
  muvtx[1].set(p1, nrm, bin, uv1, c);
  muvtx[2].set(p2, nrm, bin, uv2, c);
  muvtx[3].set(p3, nrm, bin, uv3, c);

  int i0 = MergeVertex(muvtx[0]);
  int i1 = MergeVertex(muvtx[1]);
  int i2 = MergeVertex(muvtx[2]);
  int i3 = MergeVertex(muvtx[3]);
  MergePoly(poly(i0, i1, i2, i3));
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
  fvec3 p0p1 = (p1 - p0).Normal();
  fvec3 bin  = p0p1;
  muvtx[0].set(p0, n0, bin, uv0, c);
  muvtx[1].set(p1, n1, bin, uv1, c);
  muvtx[2].set(p2, n2, bin, uv2, c);
  muvtx[3].set(p3, n3, bin, uv3, c);

  int i0 = MergeVertex(muvtx[0]);
  int i1 = MergeVertex(muvtx[1]);
  int i2 = MergeVertex(muvtx[2]);
  int i3 = MergeVertex(muvtx[3]);
  MergePoly(poly(i0, i1, i2, i3));
}
///////////////////////////////////////////////////////////////////////////////
/*
void SubMesh::GenIndexBuffers( void )
{
    int inumvtx = RefVertexPool().VertexPool.size();

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
