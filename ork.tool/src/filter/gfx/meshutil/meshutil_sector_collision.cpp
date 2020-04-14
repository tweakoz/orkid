////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// DAE(Collada) -> EFC(edge adjacency / fixed grid) metacollision converter
///////////////////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/math/plane.h>
#include <orktool/filter/gfx/meshutil/meshutil_tool.h>
#include <ork/kernel/string/StringBlock.h>
#include <ork/file/chunkfile.h>
#include <orktool/filter/gfx/collada/collada.h>
#include <ork/math/plane.hpp>
#include <ork/util/endian.h>

#if 1
#include <BulletCollision/btBulletCollisionCommon.h>
#include <pkg/ent/bullet_sector.h>

//#include <IL/il.h>

#define SECTOR_DEBUG_SPAM (0)
#define DEBUG_DRAW (0)
#define DRAW_STRAIGHTS (0)
#define DRAW_TRIANGLES (0)
#define DRAW_TRACK (0)

using namespace ork::tool;

///////////////////////////////////////////////////////////////////////////////
namespace ork::tool::meshutil {
///////////////////////////////////////////////////////////////////////////////

class SectorWalker;

bool SECOutput(const file::Path& outpath, const SectorWalker& mesh);

///////////////////////////////////////////////////////////////////////////////

// Cloned enum so that we don't have to type ent::bullet:: in front of every face type

enum {
  FACE_START = 1,
  FACE_BOTTOM,
};

///////////////////////////////////////////////////////////////////////////////

/*template<typename Type, Type DefaultValue>
class SectorFaces
{
public:
    SectorFaces() : mFaces(5, DefaultValue) { }

    Type& operator[](u32 index) { return mFaces[index]; }
    Type operator[](u32 index) const { return mFaces[index]; }

    Type *Bottom

private:
    orkvector<Type> mFaces;
};*/

typedef orkvector<int> SectorPolys;

typedef orkvector<ent::bullet::Sector> SectorArray;

///////////////////////////////////////////////////////////////////////////////

class SectorWalker {
public:
  SectorWalker(const ToolMesh& mesh, ent::bullet::SectorData& sectorsTarget, const std::string& secname);

  bool ComputeSectors();

  bool AddMidlineInfo(const ork::meshutil::submesh& mesh);
  bool AddKillInfo(const ork::meshutil::submesh& mesh);

  bool IsValidMesh() const {
    return mValidMesh;
  }

private:
  const auto& Poly(int polyIndex) const {
    return mSubmesh->RefPoly(polyIndex);
  }
  const fvec3& VertexPos(int vertIndex) const {
    return mSubmesh->RefVertexPool().GetVertex(vertIndex).mPos;
  }
  fvec3 ComputeNormal(int polyIndex) const;
  fvec3 ComputeCenter(int polyIndex) const {
    return Poly(polyIndex).ComputeCenter(mSubmesh->RefVertexPool()).mPos;
  }
  fplane3 ComputePlane(int polyIndex) const {
    fplane3 plane;
    plane.CalcFromNormalAndOrigin(ComputeNormal(polyIndex), ComputeCenter(polyIndex));
    return plane;
  }
  U64 PortalKey(int sector, int portal);

  bool IsValid(int polyIndex) const {
    return polyIndex >= 0;
  }
  bool IsPent(int polyIndex) const {
    return Poly(polyIndex).GetNumSides() == 5;
  }
  bool IsQuad(int polyIndex) const {
    return Poly(polyIndex).GetNumSides() == 4;
  }
  bool IsTriangle(int polyIndex) const {
    return Poly(polyIndex).GetNumSides() == 3;
  }
  bool IsStart(int polyIndex) const {
    return Poly(polyIndex).GetAnnotation("material") == "sector_start";
  }
  bool IsBottom(int polyIndex) const {
    return Poly(polyIndex).GetAnnotation("material") == "sector_bottom";
  }

  void InitPortal(ent::bullet::SectorPortal& portal, int bl, int tl, int tr, int br);

  bool GatherSectorPolysStep(SectorPolys& polys, int first, int prev, int cur, int n) const;
  bool GatherSectorPolys(SectorPolys& polys, int top) const;
  bool ComputeSectorDefinition(ent::bullet::Sector& sector, const SectorPolys& polys);
  bool ComputeSectorAdjacency(int index, const SectorPolys& polys);
  bool ComputeSectorTrackPositions();

  ork::meshutil::Mesh mMesh;
  ork::meshutil::Mesh mBoundsMesh;
  bool mValidMesh;
  orkset<int> mVisitedPolys;
  std::string mSecName;

  ork::meshutil::submesh* mSubmesh;

  orkvector<int> mPolyOwners;

  orkvector<int> mOutVertIDs;

  orkvector<int> mGravVertIDs;

  int mStartPoly;

  int getOutVertID(int vert) {
    if (mOutVertIDs[vert] == -1) {
      mOutVertIDs[vert] = mOutVerts.size();
      mOutVerts.push_back(mSubmesh->RefVertexPool().GetVertex(vert).mPos);
    }
    return mOutVertIDs[vert];
  }

  ent::bullet::SectorData& mTarget;

  SectorArray& mSectors;
  orkvector<ent::bullet::SectorVert>& mOutVerts;

  int mStartSector;

  int mNumSectors;
};

///////////////////////////////////////////////////////////////////////////////

#define ParseVarArgs(buffer)                                                                                                       \
  va_list args;                                                                                                                    \
  va_start(args, format);                                                                                                          \
  vsnprintf(buffer, sizeof(buffer), format, args);                                                                                 \
  va_end(args);                                                                                                                    \
  buffer[sizeof(buffer) - 1] = '\0';

///////////////////////////////////////////////////////////////////////////////

static void AssertFailed(const char* secName, const fvec3& pos, const char* format, ...) {
  char buffer[1024];
  ParseVarArgs(buffer);

  if (secName == NULL)
    secName = "NULL";

  orkerrorlog(
      "ERROR: <%s> Requirement '%s' failed near <%+0.1f, %+0.1f, %+0.1f>\n", secName, buffer, pos.GetX(), pos.GetY(), pos.GetZ());
}

///////////////////////////////////////////////////////////////////////////////

static void AssertFailed(const char* secName, const char* format, ...) {
  char buffer[1024];
  ParseVarArgs(buffer);

  if (secName == NULL)
    secName = "NULL";

  orkerrorlog("ERROR: <%s> Requirement '%s' failed\n", secName, buffer);
}

///////////////////////////////////////////////////////////////////////////////

#define SimpleAssert(expression, format, ...)                                                                                      \
  if (!(expression)) {                                                                                                             \
    AssertFailed(mSecName.c_str(), format, ##__VA_ARGS__);                                                                         \
    return;                                                                                                                        \
  }

///////////////////////////////////////////////////////////////////////////////

#define SimpleAssertReturn(expression, format, ...)                                                                                \
  if (!(expression)) {                                                                                                             \
    AssertFailed(mSecName.c_str(), format, ##__VA_ARGS__);                                                                         \
    return false;                                                                                                                  \
  }

///////////////////////////////////////////////////////////////////////////////

#define PolyAssert(expression, poly, format, ...)                                                                                  \
  if (!(expression)) {                                                                                                             \
    AssertFailed(mSecName.c_str(), ComputeCenter(poly), "(poly %d) " format, poly, ##__VA_ARGS__);                                 \
    return;                                                                                                                        \
  }

///////////////////////////////////////////////////////////////////////////////

#define PolyAssertReturn(expression, poly, format, ...)                                                                            \
  if (!(expression)) {                                                                                                             \
    AssertFailed(mSecName.c_str(), ComputeCenter(poly), "(poly %d) " format, poly, ##__VA_ARGS__);                                 \
    return false;                                                                                                                  \
  }
///////////////////////////////////////////////////////////////////////////////

#define LocAssertReturn(expression, loc, format, ...)                                                                              \
  if (!(expression)) {                                                                                                             \
    AssertFailed(mSecName.c_str(), loc, format, ##__VA_ARGS__);                                                                    \
    return false;                                                                                                                  \
  }

///////////////////////////////////////////////////////////////////////////////

SectorWalker::SectorWalker(const ToolMesh& mesh, ent::bullet::SectorData& target, const std::string& secname)
    : mMesh()
    , mValidMesh(false)
    , mNumSectors(0)
    , mStartSector(-1)
    , mSubmesh(NULL)
    , mTarget(target)
    , mSectors(target.mSectors)
    , mOutVerts(target.mSectorVerts)
    , mSecName(secname) {
  mMesh.CopyMaterialsFromToolMesh(mesh);
  mMesh.MergeToolMeshAs(mesh, "all");
  mSubmesh = mMesh.FindSubMesh("all");

  //	SimpleAssert(mStartGroup, "At least one poly in the start group");
  //	SimpleAssert(mBottomGroup, "At least one poly in the bottom group");

  //	SimpleAssert(!mMesh.FindSubMeshFromMaterialName("sector_end"), "No sector_end polys (obsolete)");

  /*mCapGroups.push_back(mFrontGroup);
  mCapGroups.push_back(mStartGroup);
  mCapGroups.push_back(mSplitPrimaryGroup);
  mCapGroups.push_back(mSplitSecondaryGroup);

  mStraightCapGroups.push_back(mFrontGroup);
  mStraightCapGroups.push_back(mStartGroup);*/

  unsigned int numStartPolys  = 0;
  unsigned int numBottomPolys = 0;

  for (int polyIndex = 0; polyIndex < mSubmesh->GetNumPolys(); ++polyIndex) {
    if (IsBottom(polyIndex))
      numBottomPolys++;

    if (IsStart(polyIndex)) {
      mStartPoly = polyIndex;
      numStartPolys++;
    }
  }

  const unsigned int minimumSectors = 3;

  // Error check the input data
  SimpleAssert(numStartPolys == 1, "Exactly one start poly (there are %d)", numStartPolys);
  SimpleAssert(
      numBottomPolys >= minimumSectors, "Track has more than %d bottom polys (there are %d)", minimumSectors, numBottomPolys);

  for (int polyIndex = 0; polyIndex < mSubmesh->GetNumPolys(); ++polyIndex) {
    const auto& thePoly = mSubmesh->RefPoly(polyIndex);

    PolyAssert(thePoly.GetNumSides() != 3, polyIndex, "No triangles in sector geometry");

    orkvector<ork::meshutil::edge> edges;
    mSubmesh->GetEdges(thePoly, edges);
    for (orkvector<ork::meshutil::edge>::const_iterator edgeIter = edges.begin(); edgeIter != edges.end(); edgeIter++) {
      orkset<int> connectedPolys;
      mSubmesh->GetConnectedPolys(*edgeIter, connectedPolys);
      const unsigned int numConnectedPolys = connectedPolys.size();

      PolyAssert(numConnectedPolys >= 2, polyIndex, "Edge is connected to at least 2 polys (connected to %d)", numConnectedPolys);
      PolyAssert(numConnectedPolys <= 3, polyIndex, "Edge is connected to at most 3 polys (connected to %d)", numConnectedPolys);

      if (numConnectedPolys == 3) {
        bool onStart = false;
        for (orkset<int>::const_iterator polyIter = connectedPolys.begin(); polyIter != connectedPolys.end(); polyIter++)
          if (*polyIter == mStartPoly)
            onStart = true;
        PolyAssert(onStart, polyIndex, "Edge with 3 polys three polys must be connted to start");
      }
    }
  }

  /////////////////////////////////////////////////////

  mPolyOwners.resize(mSubmesh->GetNumPolys());
  for (int i = 0; i < mSubmesh->GetNumPolys(); i++)
    mPolyOwners[i] = -1;

  mNumSectors = numBottomPolys;

  mValidMesh = true;
}

///////////////////////////////////////////////////////////////////////////////

#define GetConnectedAssert(expression, format, ...)                                                                                \
  if (!(expression)) {                                                                                                             \
    AssertFailed(mSecName.c_str(), ComputeCenter(polyIndex), format, ##__VA_ARGS__);                                               \
    return -1;                                                                                                                     \
  }

///////////////////////////////////////////////////////////////////////////////

fvec3 SectorWalker::ComputeNormal(int polyIndex) const {
  fvec3 rval(0, 0, 0);

  const auto& ThePoly = mSubmesh->RefPoly(polyIndex);
  const auto& vpool   = mSubmesh->RefVertexPool();

  for (int i = 0; i < ThePoly.GetNumSides(); i++) {
    const fvec3& v0 = vpool.GetVertex(ThePoly.GetVertexID((i + 0) % ThePoly.GetNumSides())).mPos;
    const fvec3& v1 = vpool.GetVertex(ThePoly.GetVertexID((i + 1) % ThePoly.GetNumSides())).mPos;
    const fvec3& v2 = vpool.GetVertex(ThePoly.GetVertexID((i + 2) % ThePoly.GetNumSides())).mPos;
    rval += (v0 - v1).Cross(v2 - v1);
  }

  return rval.Normal();
}

///////////////////////////////////////////////////////////////////////////////

bool SectorWalker::GatherSectorPolysStep(SectorPolys& polys, int first, int prev, int cur, int n) const {
  // disabled during submesh refactor
  /*if (n == 0)
    return cur == first;
  const auto& thePoly = Poly(cur);
  vertex_ptr_t at;
  vertex_ptr_t end;
  if (prev >= 0) {
    auto baseEdge = mSubmesh->edgeBetween(prev, cur);

    at = thePoly.VertexCCW(baseEdge->edgeVertex(0)->_poolindex);

    if (at == baseEdge->edgeVertex(1)->_poolindex) {
      at  = thePoly.VertexCCW(at);
      end = thePoly.VertexCW(baseEdge->edgeVertex(0)->_poolindex);
    } else {
      end = thePoly.VertexCW(baseEdge->edgeVertex(1)->_poolindex);
    }
  } else {
    at = end = thePoly.GetVertexID(0);
  }

  do {
    int next           = thePoly.VertexCCW(at);
    const auto& atEdge = mSubmesh->RefEdge(ork::meshutil::edge(at, next).GetHashKey());
    for (int i = 0; i < atEdge.GetNumConnectedPolys(); i++) {
      int nextPoly = atEdge.GetConnectedPoly(i);
      if (nextPoly == cur)
        continue;
      if (IsStart(nextPoly))
        continue;
      if (n > 1 && IsBottom(nextPoly))
        continue;
      if (GatherSectorPolysStep(polys, first, cur, nextPoly, n - 1)) {
        polys.push_back(cur);
        return true;
      }
    }

    at = next;
  } while (at != end);*/
  return false;
}

bool SectorWalker::GatherSectorPolys(SectorPolys& polys, int botPolyIndex) const {
  OrkAssertI(botPolyIndex >= 0, "GatherSectorPolys: Invalid bottom");
  if (botPolyIndex < 0)
    return false;

  bool gathered = GatherSectorPolysStep(polys, botPolyIndex, -1, botPolyIndex, 4);

  PolyAssertReturn(gathered, botPolyIndex, "Found walls and ceiling for base");

  // gathered polys come out [side, top, side, bottom]
  // shuffle to get [bottom, side, side, top];
  int tmp  = polys[1];
  polys[1] = polys[0];
  polys[0] = polys[3];
  polys[3] = tmp;

  PolyAssertReturn(
      Poly(polys[0]).GetNumSides() == Poly(polys[3]).GetNumSides(), botPolyIndex, "Bottom and top have same edge count");
  PolyAssertReturn(Poly(polys[1]).GetNumSides() == Poly(polys[2]).GetNumSides(), botPolyIndex, "Sides have same edge count");

  bool isHSplit = Poly(polys[0]).GetNumSides() == 5;
  bool isVSplit = Poly(polys[1]).GetNumSides() == 5;

  PolyAssertReturn(!(isVSplit && isHSplit), botPolyIndex, "Sector is not both hsplit and vsplit");

  // sanity check
  PolyAssertReturn(polys.size() == 4, botPolyIndex, "sector has four polys");

  return true;
}

///////////////////////////////////////////////////////////////////////////////

#define AdjacencyAssert(expression, sub, format, ...)                                                                              \
  if (!(expression)) {                                                                                                             \
    AssertFailed(mSecName.c_str(), ComputeCenter(sub, polys[FACE_TOP]), format, ##__VA_ARGS__);                                    \
    return false;                                                                                                                  \
  }

///////////////////////////////////////////////////////////////////////////////

void SectorWalker::InitPortal(ent::bullet::SectorPortal& portal, int bl, int tl, int tr, int br) {
  // disabled during submesh refactor
  // portal.mCornerVerts[ent::bullet::PORTAL_CORNER_BL] = getOutVertID(bl);
  // portal.mCornerVerts[ent::bullet::PORTAL_CORNER_TL] = getOutVertID(tl);
  // portal.mCornerVerts[ent::bullet::PORTAL_CORNER_TR] = getOutVertID(tr);
  // portal.mCornerVerts[ent::bullet::PORTAL_CORNER_BR] = getOutVertID(br);
  // ork::meshutil::poly tmp(bl, tl, tr, br);
  // portal.mPlane         = fplane3(tmp.ComputeNormal(mSubmesh->RefVertexPool()),
  // tmp.ComputeCenter(mSubmesh->RefVertexPool()).mPos); portal.mTrackProgress = -1; // uncomputed

  // int startCheck        = Poly(mStartPoly).VertexCCW(bl);
  // if (startCheck == br) {
  // portal.mTrackProgress = 1;
  //} else if (startCheck == tl) {
  // portal.mTrackProgress = 0;
  //}
}

///////////////////////////////////////////////////////////////////////////////

bool SectorWalker::ComputeSectorDefinition(ent::bullet::Sector& sector, const SectorPolys& polys) {
  const auto& bot = Poly(polys[0]);
  const auto& top = Poly(polys[3]);
  /*
    // disabled during submesh refactor
    //	bool isPre = false, isPost = false;
    {
       auto startedge = mSubmesh->edgeBetween(polys[0], mStartPoly);
       if (startedge != nullptr) {
         uint64_t startedge_hash = startedge->GetHashKey();
         int pt1                 = (int)(startedge_hash & 0x00FFFFFFFF);
         int pt2                 = (int)((startedge_hash >> 32) & 0x00FFFFFFFF);
         // looking at start, we want pt1 to be bottom-left and pt2 to be bottom-right
         if (bot.VertexCW(pt1) != pt2) {
           int tmp = pt2;
           pt2     = pt1;
           pt1     = tmp;
         }
         PolyAssertReturn(bot.VertexCW(pt1) == pt2, mStartPoly, "Failed to normalize start poly corners (internal - tell inio)");
         if (Poly(mStartPoly).VertexCW(pt1) == pt2) {
           //	isPre = true;
           sector.FlagStart();
         } else {
           PolyAssertReturn(Poly(mStartPoly).VertexCCW(pt1) == pt2, mStartPoly, "post sector didn't check out (internal - tell
     inio)");
           //	isPost = true;
         }
       }
     }
     bool isSplit = IsPent(polys[0]) || IsPent(polys[1]);

     int bl = bot.miVertices[0];
     for (;; bl = bot.VertexCCW(bl)) {
       int tl = Poly(polys[1]).VertexCCW(bl);
       if (tl == -1)
         tl = Poly(polys[2]).VertexCCW(bl);
       if (tl != -1) {
         int tr = top.VertexCCW(tl);
         int br = Poly(polys[1]).VertexCCW(tr);
         if (br == -1)
           br = Poly(polys[2]).VertexCCW(tr);
         if (br != -1) {
           if (bot.VertexCCW(br) == bl)
             break;
         }
       }
       PolyAssertReturn(bot.VertexCCW(bl) != bot.miVertices[0], polys[0], "Failed to find singular portal");
     }

     // singular portal is alwys considered the input of a sector.  Looking out it, the "right" side
     // of the sector is to our left so the bottom-left vertex should be on it.
     bool flipped = (Poly(polys[2]).VertexCW(bl) == -1);

     const auto& left  = Poly(polys[flipped ? 2 : 1]);
     const auto& right = Poly(polys[flipped ? 1 : 2]);

     PolyAssertReturn(right.VertexCCW(bl) != -1, polys[0], "Bottom-left of portal 0 is on right wall (internal - tell inio)");

     if (left.miNumSides == 4) {
       // not a V split, could be an H split

       int tl   = right.VertexCCW(bl);
       int stop = bot.VertexCCW(bl);

       int numPortals = 0;

       for (; bl != stop; bl = bot.VertexCW(bl), tl = top.VertexCCW(tl)) {
         if (left.VertexCCW(bl) == bot.VertexCW(bl) || right.VertexCCW(bl) == bot.VertexCW(bl))
           continue;

         int br = bot.VertexCW(bl);
         int tr = top.VertexCCW(tl);
         InitPortal(sector.mPortals[numPortals], bl, tl, tr, br);
         numPortals++;

         PolyAssertReturn(numPortals <= 3, polys[0], "At most 3 portals (internal - tell inio)");
       }

       PolyAssertReturn(numPortals >= 2, polys[0], "At least 2 portals (internal - tell inio)");

       if (numPortals == 3) {
         sector.mFlags |= ork::ent::bullet::SECTORFLAG_SPLITH;

         // 2---3
         // |   |
         // 1---4

         fvec3 v1, v2, v3, v4;
         v1.Lerp(
             mOutVerts[sector.mPortals[0].mCornerVerts[ent::bullet::PORTAL_CORNER_BL]].mPos,
             mOutVerts[sector.mPortals[0].mCornerVerts[ent::bullet::PORTAL_CORNER_BR]].mPos,
             0.5);
         v2.Lerp(
             mOutVerts[sector.mPortals[0].mCornerVerts[ent::bullet::PORTAL_CORNER_TL]].mPos,
             mOutVerts[sector.mPortals[0].mCornerVerts[ent::bullet::PORTAL_CORNER_TR]].mPos,
             0.5);
         v3 = mOutVerts[sector.mPortals[1].mCornerVerts[ent::bullet::PORTAL_CORNER_TR]].mPos;
         v4 = mOutVerts[sector.mPortals[1].mCornerVerts[ent::bullet::PORTAL_CORNER_BR]].mPos;
         sector.mSplitPlane.CalcFromNormalAndOrigin(
             ((v3 - v2) + (v4 - v1)).Cross((v2 - v1) + (v3 - v4)).Normal(), (v1 + v2 + v3 + v4) * (1.0f / 4.0f));
       }

     } else {
       // v split

       int br = bot.VertexCW(bl);
       int tl = right.VertexCCW(bl);
       int tr = left.VertexCW(br);

       InitPortal(sector.mPortals[0], bl, tl, tr, br);

       bl = left.VertexCCW(br);
       br = bot.VertexCW(bl);
       tl = left.VertexCCW(bl);
       tr = right.VertexCW(br);

       InitPortal(sector.mPortals[1], bl, tl, tr, br);

       bl = tl;
       br = tr;
       tl = left.VertexCCW(bl);
       tr = right.VertexCW(br);

       InitPortal(sector.mPortals[2], bl, tl, tr, br);

       sector.mFlags |= ork::ent::bullet::SECTORFLAG_SPLITV;

       // 2---3
       // |   |
       // 1---4

       fvec3 v1, v2, v3, v4;
       v1.Lerp(
           mOutVerts[sector.mPortals[0].mCornerVerts[ent::bullet::PORTAL_CORNER_BR]].mPos,
           mOutVerts[sector.mPortals[0].mCornerVerts[ent::bullet::PORTAL_CORNER_TR]].mPos,
           0.5);
       v2.Lerp(
           mOutVerts[sector.mPortals[0].mCornerVerts[ent::bullet::PORTAL_CORNER_BL]].mPos,
           mOutVerts[sector.mPortals[0].mCornerVerts[ent::bullet::PORTAL_CORNER_BL]].mPos,
           0.5);
       v3 = mOutVerts[sector.mPortals[1].mCornerVerts[ent::bullet::PORTAL_CORNER_TL]].mPos;
       v4 = mOutVerts[sector.mPortals[1].mCornerVerts[ent::bullet::PORTAL_CORNER_TR]].mPos;
       sector.mSplitPlane.CalcFromNormalAndOrigin(
           ((v3 - v2) + (v4 - v1)).Cross((v2 - v1) + (v3 - v4)).Normal(), (v1 + v2 + v3 + v4) * (1.0f / 4.0f));
     }
     */
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool SectorWalker::ComputeSectorAdjacency(int index, const SectorPolys& polys) {
  ent::bullet::Sector& sector = mSectors[index];

  orkset<int> adjpolys;

  for (int i = 0; i < 4; i++)
    mSubmesh->GetAdjacentPolys(polys[i], adjpolys);

  orkset<int> adjsectors;

  for (orkset<int>::const_iterator polyIter = adjpolys.begin(); polyIter != adjpolys.end(); polyIter++) {
    if (IsStart(*polyIter))
      continue;
    int sectorind = mPolyOwners[*polyIter];
    if (sectorind == index)
      continue;
    adjsectors.insert(sectorind);
  }

  if (sector.IsSplit()) {
    PolyAssertReturn(adjsectors.size() == 3, polys[0], "split sector has 3 adjacent sectors (had %d)", adjsectors.size());
  } else {
    if (adjsectors.size() != 2) {
      for (orkset<int>::const_iterator polyIter = adjpolys.begin(); polyIter != adjpolys.end(); polyIter++) {
        if (IsStart(*polyIter))
          continue;
        int sectorind = mPolyOwners[*polyIter];
        if (sectorind == index)
          continue;
        fvec3 pos = Poly(*polyIter).ComputeCenter(mSubmesh->RefVertexPool()).mPos;
        //	orkerrorlog("adjacent sector %d via poly @ <%f,%f,%f>\n", sectorind, pos.GetX(), pos.GetY(), pos.GetZ());
      }
    }
    // PolyAssertReturn(adjsectors.size() == 2 , polys[0], "normal sector has 2 adjacent sectors (had %d)", adjsectors.size());
  }

  for (int nearportal = 0; nearportal < (sector.IsSplit() ? 3 : 2); nearportal++) {
    bool match = false;

    ent::bullet::SectorPortal& portal = sector.mPortals[nearportal];
    for (orkset<int>::const_iterator sectIter = adjsectors.begin(); sectIter != adjsectors.end(); sectIter++) {

      ent::bullet::Sector& othersector = mSectors[*sectIter];

      for (int farportal = 0; farportal < (othersector.IsSplit() ? 3 : 2); farportal++) {
        ent::bullet::SectorPortal& otherportal = othersector.mPortals[farportal];
        const int nverts                       = ent::bullet::NUM_PORTAL_CORNERS;
        for (int offset = 0; offset < nverts; offset++) {
          if (portal.mCornerVerts[0] != otherportal.mCornerVerts[offset])
            continue;
          if (portal.mCornerVerts[1] != otherportal.mCornerVerts[(offset + nverts - 1) % nverts])
            continue;
          if (portal.mCornerVerts[2] != otherportal.mCornerVerts[(offset + nverts - 2) % nverts])
            continue;
          if (portal.mCornerVerts[3] != otherportal.mCornerVerts[(offset + nverts - 3) % nverts])
            continue;
          portal.mNeighbor = *sectIter;
          match            = true;
          break;
        }
        if (match)
          break;
      }
      if (match)
        break;
    }
    if (!match)
      match = false;
    LocAssertReturn(match, mOutVerts[portal.mCornerVerts[ent::bullet::PORTAL_CORNER_BL]].mPos, "could not find adjacent sector");
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////

struct ProgressRelaxationNode {
  int sbefore, safter;
  float progress;
  float progressSum, progressWeight;
  ProgressRelaxationNode(int s1, int s2)
      : sbefore(s1)
      , safter(s2)
      , progress(0.5)
      , progressSum(0)
      , progressWeight(0) {
  }
  float collect() {
    if (progressWeight == 0)
      return 0;
    float oldprogress = progress;
    progress          = progressSum / progressWeight;
    progressSum       = 0;
    progressWeight    = 0;
    oldprogress -= progress;
    return oldprogress * oldprogress;
  }
};

struct ProgressRelaxationEdge {
  ProgressRelaxationNode *n1, *n2;
  float weight;
  ProgressRelaxationEdge(ProgressRelaxationNode* in1, ProgressRelaxationNode* in2, float len)
      : n1(in1)
      , n2(in2)
      , weight(1 / len) {
  }
  void apply() {
    n1->progressSum += n2->progress * weight;
    n1->progressWeight += weight;
    n2->progressSum += n1->progress * weight;
    n2->progressWeight += weight;
  }
};

U64 SectorWalker::PortalKey(int sector, int portal) {
  int othersec = mSectors[sector].mPortals[portal].mNeighbor;
  if (othersec < sector)
    return ((U64)othersec) | (((U64)sector) << 32);
  return ((U64)sector) | (((U64)othersec) << 32);
}

bool SectorWalker::ComputeSectorTrackPositions() {
  ProgressRelaxationNode start(0, 0);
  ProgressRelaxationNode end(0, 0);
  orkvector<ProgressRelaxationNode> nodes;
  orkvector<ProgressRelaxationEdge> edges;
  orkmap<U64, int> nodemap;

  start.progress = 0;
  end.progress   = 1;

  nodes.reserve(mSectors.size() * 3);
  edges.reserve(mSectors.size() * 2);

  for (int snum = 0; snum < (int)mSectors.size(); snum++) {
    for (int pnum = 0; pnum < mSectors[snum].NumPortals(); pnum++) {
      int othersec = mSectors[snum].mPortals[pnum].mNeighbor;
      if (othersec < snum)
        continue;
      nodemap[PortalKey(snum, pnum)] = nodes.size();
      nodes.push_back(ProgressRelaxationNode(snum, othersec));
    }

    ProgressRelaxationNode *a, *b;
    float len;

    a = &nodes[nodemap[PortalKey(snum, 0)]];
    if (mSectors[snum].mPortals[0].mTrackProgress == 0)
      a = &start;
    if (mSectors[snum].mPortals[0].mTrackProgress == 1)
      a = &end;
    b = &nodes[nodemap[PortalKey(snum, 1)]];
    if (mSectors[snum].mPortals[1].mTrackProgress == 0)
      b = &start;
    if (mSectors[snum].mPortals[1].mTrackProgress == 1)
      b = &end;
    len = (mSectors[snum].mPortals[0].GetCenter(mOutVerts) - mSectors[snum].mPortals[1].GetCenter(mOutVerts)).Mag();

    if (!mSectors[snum].IsSplit()) {
      edges.push_back(ProgressRelaxationEdge(a, b, len));

    } else {
      edges.push_back(ProgressRelaxationEdge(a, b, len));

      b = &nodes[nodemap[PortalKey(snum, 2)]];
      if (mSectors[snum].mPortals[2].mTrackProgress == 0)
        b = &start;
      if (mSectors[snum].mPortals[2].mTrackProgress == 1)
        b = &end;
      len = (mSectors[snum].mPortals[0].GetCenter(mOutVerts) - mSectors[snum].mPortals[2].GetCenter(mOutVerts)).Mag();
      edges.push_back(ProgressRelaxationEdge(a, b, len));
    }
  }

  float err = 0;
  for (int outeriter = 0; outeriter < 300; outeriter++) {
    for (int iterations = 0; iterations < 100; iterations++) {
      for (orkvector<ProgressRelaxationEdge>::iterator itor = edges.begin(); itor != edges.end(); itor++)
        itor->apply();

      err = 0;
      for (orkvector<ProgressRelaxationNode>::iterator itor = nodes.begin(); itor != nodes.end(); itor++)
        err += itor->collect();
    }
    //	err = sqrt(err/nodes.size());
    //	printf("relaxing: %.2e\n", err);
    if (err < 1.0e-15)
      break;
  }

  ork::msleep(10000);

  for (int snum = 0; snum < (int)mSectors.size(); snum++) {
    for (int pnum = 0; pnum < (mSectors[snum].IsSplit() ? 3 : 2); pnum++) {
      int othersec = mSectors[snum].mPortals[pnum].mNeighbor;

      if (mSectors[snum].mPortals[pnum].mTrackProgress < 0)
        mSectors[snum].mPortals[pnum].mTrackProgress = nodes[nodemap[PortalKey(snum, pnum)]].progress;
    }
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool SectorWalker::ComputeSectors() {
  if (false == mValidMesh)
    return false;

  orkvector<SectorPolys> sectorsPolys;

  mOutVertIDs.resize(mSubmesh->RefVertexPool().GetNumVertices());
  for (size_t i = 0; i < mSubmesh->RefVertexPool().GetNumVertices(); i++) {
    mOutVertIDs[i] = -1;
  }

  // Gather the polygons for every sector
  // const orkset<int>& botPolys = mBottomGroup->mGroupPolys;

  for (size_t polyIndex = 0; polyIndex < size_t(mSubmesh->GetNumPolys()); ++polyIndex) {
    if (!IsBottom(polyIndex))
      continue;
    SectorPolys polys;
    bool gathered = GatherSectorPolys(polys, polyIndex);
    PolyAssertReturn(gathered, polyIndex, "Gathered sector polys for sector");

    sectorsPolys.push_back(polys);
  }

  // Insert each sector's index into the 'owners' set of every poly that it uses
  for (unsigned int sector = 0; sector < sectorsPolys.size(); ++sector) {
    const SectorPolys& polys = sectorsPolys[sector];
    for (int face = 0; face < (int)polys.size(); ++face)
      if (IsValid(polys[face]))
        mPolyOwners[polys[face]] = sector;
  }

  // Calculate planes, basis and connection info for every sector
  for (orkvector<SectorPolys>::const_iterator polysIt = sectorsPolys.begin(); polysIt != sectorsPolys.end(); ++polysIt) {
    ent::bullet::Sector sector;
    bool computed = ComputeSectorDefinition(sector, *polysIt);
    PolyAssertReturn(computed, (*polysIt)[0], "Computed sector planes and basis");

    if (sector.IsStart())
      mStartSector = mSectors.size();

    mSectors.push_back(sector);
  }

  // Compute adjacency info for every sector
  for (unsigned int sector = 0; sector < sectorsPolys.size(); ++sector) {
    bool computed = ComputeSectorAdjacency(sector, sectorsPolys[sector]);
    PolyAssertReturn(computed, sectorsPolys[sector][0], "Computed sector adjacency info for sector");
  }

  /*	// Copy verts adjacency info for every sector
      for (unsigned int sector = 0; sector < sectorsPolys.size(); ++sector)
      {
          bool computed = ComputeSectorAdjacency(sector, sectorsPolys[sector]);
          PolyAssertReturn(computed, sectorsPolys[sector][0], "Computed sector adjacency info for sector");
      };*/

  bool computed = ComputeSectorTrackPositions();
  SimpleAssertReturn(computed, "Computed Track Sector Percentages");

  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool SectorWalker::AddMidlineInfo(const ork::meshutil::submesh& mesh) {
  orkvector<ork::meshutil::vertex_ptr_t> vertexMap;
  /*
  vertexMap.resize(mesh.RefVertexPool().GetNumVertices());
    for (size_t i = 0; i < mesh.RefVertexPool().GetNumVertices(); i++)
      vertexMap[i] = nullptr;
    for (int secnum = 0; secnum < (int)mSectors.size(); secnum++) {
      ent::bullet::Sector& sector = mSectors[secnum];
      sector.mMidlineTriStart     = mTarget.mMidlineTris.size();
      for (int polynum = 0; polynum < mesh.GetNumPolys(); polynum++) {
        const auto& thePoly = mesh.RefPoly(polynum);
        PolyAssertReturn(
            thePoly.miNumSides == 3, polynum, "Gravity mesh only has triangles (importer should force, tell inio/tweak)");
        bool in = false;
        for (int vert = 0; vert < 3; vert++) {
          auto check_vtx = thePoly._vertices[vert]->mPos;
          if (sector.containsPoint(mTarget, check_vtx) {
            in = true;
            break;
          }
        }
        if (in) {
          for (int vertind = 0; vertind < 3; vertind++) {
            auto check_vtx = thePoly._vertices[vertind]->mPos;
            if (check_vtx == nullptr) {
              vertexMap[thePoly.miVertices[vertind]] = mTarget.mMidlineVerts.size();
              const auto& vert                       = mesh.RefVertexPool().GetVertex(thePoly.miVertices[vertind]);
              mTarget.mMidlineVerts.push_back(ent::bullet::MidlineVert(
                  vert.mPos, 2 * (vert.mCol[0].GetX() - 0.5f), 2 * vert.mCol[0].GetY(), vert.mCol[0].GetZ()));
            }
            mTarget.mMidlineTris.push_back(vertexMap[thePoly.miVertices[vertind]]);
          }
        }
      }
      sector.mMidlineTriCount = (mTarget.mMidlineTris.size() - sector.mMidlineTriStart) / 3;
    }*/
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool SectorWalker::AddKillInfo(const ork::meshutil::submesh& mesh) {
  /*
  orkvector<ork::meshutil::vertex_ptr_t> vertexMap;
  vertexMap.resize(mesh.RefVertexPool().GetNumVertices());
  for (size_t i = 0; i < mesh.RefVertexPool().GetNumVertices(); i++)
    vertexMap[i] = -1;
  for (size_t secnum = 0; secnum < mSectors.size(); secnum++) {
    ent::bullet::Sector& sector = mSectors[secnum];
    sector.mKillTriStart        = mTarget.mKillTris.size() / 3;
    for (int polynum = 0; polynum < mesh.GetNumPolys(); polynum++) {
      const auto& thePoly = mesh.RefPoly(polynum);
      PolyAssertReturn(thePoly.miNumSides == 3, polynum, "Kill mesh only has triangles (importer should force, tell inio/tweak)");
      bool in = false;
      for (int vert = 0; vert < 3; vert++) {
        if (sector.containsPoint(mTarget, mesh.RefVertexPool().GetVertex(thePoly.miVertices[vert]).mPos)) {
          in = true;
          break;
        }
      }
      if (in) {
        for (int vertind = 0; vertind < 3; vertind++) {
          auto check_vtx = thePoly._vertices[vertind];
          if (vertexMap[check_vtx->_poolindex] == nullptr) {
            vertexMap[thePoly.miVertices[vertind]] = mTarget.mKillVerts.size();
            const auto& vert                       = mesh.RefVertexPool().GetVertex(thePoly.miVertices[vertind]);
            mTarget.mKillVerts.push_back(vert.mPos);
          }
          mTarget.mKillTris.push_back(vertexMap[thePoly.miVertices[vertind]]);
        }
      }
    }
    sector.mKillTriCount = mTarget.mKillTris.size() / 3 - sector.mKillTriStart;
  }*/
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void ToolmeshToVertexIndexArrays(
    const ork::meshutil::submesh& triangles,
    orkvector<fvec3>& verts,
    orkvector<int>& indices,
    orkvector<int>& flags) {
  /*	Mesh triangles;
      rawMesh.Triangulate(&triangles);*/

  const int numTriangles = triangles.GetNumPolys();
  const int numIndices   = numTriangles * 3;
  const int numVerts     = triangles.RefVertexPool().GetNumVertices();

  verts.resize(numVerts);
  indices.resize(numIndices);
  flags.resize(numTriangles);

  const std::string materialkey("material");
  const std::string wallname("wall");
  const std::string slowname("slow");
  const std::string slipperyname("slippery");

  // Gather mVerts and mIndices from the tooltriangles
  for (int vertIdx = 0; vertIdx < numVerts; ++vertIdx) {
    verts[vertIdx] = triangles.RefVertexPool().GetVertex(vertIdx).Pos();
  }

  for (int triIdx = 0; triIdx < numTriangles; ++triIdx) {
    int polyflags = 0;

    const auto& thePoly    = triangles.RefPoly(triIdx);
    const std::string& mat = thePoly.GetAnnotation(materialkey);

    if (mat == wallname)
      polyflags |= ork::ent::bullet::COLLISIONFLAG_WALL;
    if (mat == slowname)
      polyflags |= ork::ent::bullet::COLLISIONFLAG_SLOW;
    if (mat == slipperyname)
      polyflags |= ork::ent::bullet::COLLISIONFLAG_SLIPPERY;

    flags[triIdx] = polyflags;
    for (int vert = 0; vert < 3; ++vert) {
      int iv      = triIdx * 3 + vert;
      int iout    = thePoly.GetVertexID(vert);
      indices[iv] = iout;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
void DrawTrackArea(file::Path path,
                   ent::bullet::Track& track,
                   const fvec3& origin,
                   const fvec3& uBasis,
                   const fvec3& vBasis,
                   float scale, int outputSize)
{
    #pragma pack(push, 1)
    struct Pixel
    {
        Pixel() : r(128), g(128), b(128) { }
        u8 r, g, b;
    };
    #pragma pack(pop)

    Pixel* pixels = new Pixel[outputSize * outputSize];

    for (int x = 0; x < outputSize; ++x)
    {
        for (int y = 0; y < outputSize; ++y)
        {
            const float tu = (float(y) / float(outputSize));
            const float tv = (float(x) / float(outputSize));

            const float u = -scale + 2 * scale * tu;
            const float v = -scale + 2 * scale * tv;

            fvec3 pos = origin + u * uBasis + v * vBasis;

            const int posSector = track.FindSectorByPoint(pos);
            if (posSector >= 0)
            {
                fvec3 xDir, yDir, zDir;
                track.GetInterpolatedBasis(posSector, pos, xDir, yDir, zDir);
                Pixel& pixel = pixels[x + y * outputSize];
            #if 0
                //Color based on sector only (map coloring)
                pixel.r = u8((posSector % 197) * 17 & 0xFF);
                pixel.g = u8((posSector % 211) * 31 & 0xFF);
                pixel.b = u8((posSector % 131) * 53 & 0xFF);
            #else
                //Color based on interpolated basis
                pixel.r = u8((zDir.GetX() + 1.0f) * 100);
                pixel.g = u8((zDir.GetY() + 1.0f) * 100);
                pixel.b = u8((zDir.GetZ() + 1.0f) * 100);
            #endif
            }
        }
    }

    ilInit();
    ILuint image;
    ilGenImages(1, &image);
    ilBindImage(image);
    if (!ilTexImage(outputSize, outputSize, 1, 3, IL_RGB, IL_UNSIGNED_BYTE, pixels))
    {
        ILenum error = ilGetError();
        orkprintf("Failed to create image (%04X)\n", error);
    }

    ilEnable(IL_FILE_OVERWRITE);
    if (!ilSaveImage(path.c_str()))
    {
        ILenum error = ilGetError();
        orkprintf("Failed to save to %s (%04X)\n", path.c_str(), error);
    }

    delete[] pixels;
}

///////////////////////////////////////////////////////////////////////////////

void DrawSector(file::Path path, ent::bullet::Track& track, int sectorIdx)
{
    const ent::bullet::Sector& sector = track.RefSector(sectorIdx);

    const fvec3 origin = sector.mCenter;
    const fvec3 uBasis = sector.mDirX;
    const fvec3 vBasis = sector.mDirZ;

    int outputSize = 256;

    const char* sectorTypeName = "";
    float sectorWidth = (sector.mPlaneCenters[ent::bullet::FACE_LEFT] - sector.mPlaneCenters[ent::bullet::FACE_RIGHT]).Mag();
    float sectorLength = 1.0f;
#if DRAW_STRAIGHTS || DRAW_TRIANGLES
    switch (sector.mType)
    {
# if DRAW_STRAIGHTS
    case ent::bullet::SECTORTYPE_STRAIGHT:
        sectorLength = (sector.mPlaneCenters[ent::bullet::FACE_FRONT] - sector.mPlaneCenters[ent::bullet::FACE_BACK]).Mag();
        sectorWidth /= 2.0f;
        sectorTypeName = "straight";
        break;
# endif
# if DRAW_TRIANGLES
    case ent::bullet::SECTORTYPE_JOIN:
        sectorLength = 4.0f * (sector.mPlaneCenters[ent::bullet::FACE_FRONT] - origin).Mag();
        sectorTypeName = "join";
        break;
    case ent::bullet::SECTORTYPE_SPLIT:
        sectorLength = 4.0f * (sector.mPlaneCenters[ent::bullet::FACE_BACK] - origin).Mag();
        sectorTypeName = "split";
        break;
# endif
    }
#endif

    const float scale = max(sectorLength, sectorWidth);
    file::Path outpath = path.ToAbsoluteFolder() + CreateFormattedString("sector%03d-%s.png",
        sectorIdx, sectorTypeName).c_str();

    orkprintf("Drawing sector\n");
    DrawTrackArea(outpath, track, origin, uBasis, vBasis, scale, outputSize);
}

///////////////////////////////////////////////////////////////////////////////

void DebugSectorBasisInterp(file::Path path)
{
    ent::bullet::Track track;

    fmtx4 transform;
    transform.RotateX(1.5f);
    transform.RotateY(1.8f);
    transform.RotateZ(2.9f);
    transform.SetTranslation(fvec3(100, 200, 300));

    track.Load(path, transform);

    ork::AABox bounds;
    bounds.BeginGrow();
    for (int sector = 0; sector < track.GetNumSectors(); ++sector)
    {
        DrawSector(path, track, sector);
        for (int face = 0; face < ent::bullet::NUM_FACES; ++face)
            bounds.Grow(track.RefSector(sector).mPlaneCenters[face]);
    }
    bounds.EndGrow();

    fvec3 min = bounds.Min();
    fvec3 max = bounds.Max();
    fvec3 origin;
    origin.Lerp(min, max, 0.5f);
    min.SetY(0.0f);
    max.SetY(0.0f);
    float scale = (min - max).Mag() / 2.0f;

#if DRAW_TRACK
    orkprintf("Drawing track\n");
    DrawTrackArea(path.ToAbsoluteFolder() + "track.png", track, origin, fvec3(1.0f, 0.0f, 0.0f), fvec3(0.0f, 0.0f, 1.0f), scale,
1024); #endif
}
*/
///////////////////////////////////////////////////////////////////////////////

#define ConverterAssert(expression, format, ...)                                                                                   \
  if (!(expression)) {                                                                                                             \
    AssertFailed(inPath.c_str(), format, ##__VA_ARGS__);                                                                           \
    return false;                                                                                                                  \
  }

///////////////////////////////////////////////////////////////////////////////
void DebugSectorBasisInterp(file::Path path);

bool DAEToSECCollision(const tokenlist& options) {
  bool rval = true;

  float ftimeA = float(OldSchool::GetRef().GetLoResTime());

  ork::tool::FilterOptMap OptionsMap;
  OptionsMap.SetDefault("--in", "coldae_in.dae");
  OptionsMap.SetDefault("--out", "coldae_out.sec");
  OptionsMap.SetOptions(options);

  std::string ttv_in  = OptionsMap.GetOption("--in")->GetValue();
  std::string ttv_out = OptionsMap.GetOption("--out")->GetValue();

  file::Path inPath(ttv_in.c_str());
  file::Path outPath(ttv_out.c_str());

  ork::file::Path::SmallNameType ext = inPath.GetExtension();

  if (ext != "dae")
    return false;

  ColladaExportPolicy policy;
  policy.mNormalPolicy.meNormalPolicy          = ColladaNormalPolicy::ENP_ALLOW_SPLIT_NORMALS;
  policy.mReadComponentsPolicy.mReadComponents = ColladaReadComponentsPolicy::EPOLICY_READCOMPONENTS_POSITION;
  policy.mTriangulation.SetPolicy(ColladaTriangulationPolicy::ECTP_DONT_TRIANGULATE);

  ent::bullet::TrackSaveData saveData;

  // Read sectors from the DAE file and grab sector info as well as last resort collision data
  DaeReadOpts sectorReadOptions;
  sectorReadOptions.mReadLayers.insert("sectors");
  ToolMesh sectorMesh;
  sectorMesh.ReadFromDaeFile(inPath, sectorReadOptions);
  // std::string outp = std::string(inPath.GetName().c_str())+std::string("_sect");
  // ork::file::Path outpth( inPath.c_str() );
  // outpth.SetFile( outp.c_str() );
  // sectorMesh.WriteToWavefrontObj( outpth );

  //	ConverterAssert(sectorMesh.GetNumPolys() != 0, "There are polys in the 'sectors' layer");

  // Read gravity data from the DAE file

  ToolMesh gravityMesh;
  {
    ColladaExportPolicy gravityPolicy;
    gravityPolicy.mNormalPolicy.meNormalPolicy          = ColladaNormalPolicy::ENP_ALLOW_SPLIT_NORMALS;
    gravityPolicy.mReadComponentsPolicy.mReadComponents = (ColladaReadComponentsPolicy::ReadComponents)(
        ColladaReadComponentsPolicy::EPOLICY_READCOMPONENTS_POSITION | ColladaReadComponentsPolicy::EPOLICY_READCOMPONENTS_COLOR0);
    gravityPolicy.mTriangulation.SetPolicy(ColladaTriangulationPolicy::ECTP_TRIANGULATE);
    DaeReadOpts gravityReadOptions;
    gravityReadOptions.mReadLayers.insert("gravity");
    gravityMesh.ReadFromDaeFile(inPath, gravityReadOptions);
    // std::string outp = std::string(inPath.GetName().c_str())+std::string("_grav");
    // ork::file::Path outpth( inPath.c_str() );
    // outpth.SetFile( outp.c_str() );
    // gravityMesh.WriteToWavefrontObj( outpth );
  }

  // Read kill data from the DAE file

  ToolMesh killMesh;
  {
    ColladaExportPolicy killPolicy;
    killPolicy.mNormalPolicy.meNormalPolicy = ColladaNormalPolicy::ENP_ALLOW_SPLIT_NORMALS;
    killPolicy.mReadComponentsPolicy.mReadComponents =
        (ColladaReadComponentsPolicy::ReadComponents)(ColladaReadComponentsPolicy::EPOLICY_READCOMPONENTS_POSITION);
    killPolicy.mTriangulation.SetPolicy(ColladaTriangulationPolicy::ECTP_TRIANGULATE);
    DaeReadOpts killReadOptions;
    killReadOptions.mReadLayers.insert("kill");
    killMesh.ReadFromDaeFile(inPath, killReadOptions);
    // std::string outp = std::string(inPath.GetName().c_str())+std::string("_kill");
    // ork::file::Path outpth( inPath.c_str() );
    // outpth.SetFile( outp.c_str() );
    // killMesh.WriteToWavefrontObj( outpth );
  }

  float ftimeB = float(OldSchool::GetRef().GetLoResTime());
  float ftime  = (ftimeB - ftimeA);
  orkprintf("<<PROFILE>> <<DAEToSECCollision::LoadDaes %f seconds>>\n", ftime);

  // empty gravity info is OK

  ////////////////////////////////////////////////////////

  ftimeA = float(OldSchool::GetRef().GetLoResTime());

  if (0 == sectorMesh.numSubMeshes()) {
    orkprintf("ERROR: 0 ork::meshutil::submeshes found in sector mesh<%s>\n", inPath.c_str());
    return false;
  }

  SectorWalker meshWalker(sectorMesh, saveData, inPath.c_str());

  rval &= meshWalker.ComputeSectors();
  ConverterAssert(rval, "SectorWalker was able to compute sectors");

  ftimeB = float(OldSchool::GetRef().GetLoResTime());
  ftime  = (ftimeB - ftimeA);
  orkprintf("<<PROFILE>> <<DAEToSECCollision::ComputeSectors %f seconds>>\n", ftime);

  ////////////////////////////////////////////////////////

  ftimeA = float(OldSchool::GetRef().GetLoResTime());

  ToolMesh gravityMerged;
  if (gravityMesh.numSubMeshes()) {
    gravityMerged.MergeToolMeshAs(gravityMesh, "all");
    rval &= meshWalker.AddMidlineInfo(*gravityMerged.FindSubMesh("all"));
    ConverterAssert(rval, "SectorWalker was able to add midline info to sectors");
  }

  ToolMesh killMerged;
  if (killMesh.numSubMeshes()) {
    killMerged.MergeToolMeshAs(killMesh, "all");
    rval &= meshWalker.AddKillInfo(*killMerged.FindSubMesh("all"));
    ConverterAssert(rval, "SectorWalker was able to add kill info to sectors");
  }

  ftimeB = float(OldSchool::GetRef().GetLoResTime());
  ftime  = (ftimeB - ftimeA);
  orkprintf("<<PROFILE>> <<DAEToSECCollision::GravKill %f seconds>>\n", ftime);

  ////////////////////////////////////////////////////////

  if (rval) {
    ftimeA = float(OldSchool::GetRef().GetLoResTime());

    // Read track collision data from the DAE file
    ColladaExportPolicy collisionPolicy;
    collisionPolicy.mNormalPolicy.meNormalPolicy = ColladaNormalPolicy::ENP_ALLOW_SPLIT_NORMALS;
    collisionPolicy.mReadComponentsPolicy.mReadComponents =
        (ColladaReadComponentsPolicy::ReadComponents)(ColladaReadComponentsPolicy::EPOLICY_READCOMPONENTS_POSITION);
    collisionPolicy.mTriangulation.SetPolicy(ColladaTriangulationPolicy::ECTP_TRIANGULATE);

    DaeReadOpts collisionReadOptions;
    collisionReadOptions.mReadLayers.insert("collision");
    ToolMesh collisionMesh;
    collisionMesh.ReadFromDaeFile(inPath, collisionReadOptions);

    ftimeB = float(OldSchool::GetRef().GetLoResTime());
    ftime  = (ftimeB - ftimeA);
    orkprintf("<<PROFILE>> <<DAEToSECCollision::ReadCol %f seconds>>\n", ftime);

    ftimeA = float(OldSchool::GetRef().GetLoResTime());
    //		ConverterAssert(sectorMesh.GetNumPolys() != 0, "There are polys in the 'sectors' layer");

    ToolMesh merged;
    merged.MergeToolMeshAs(collisionMesh, "all");
    ToolmeshToVertexIndexArrays(*merged.FindSubMesh("all"), saveData.mTrackVerts, saveData.mTrackIndices, saveData.mTrackFlags);

    rval &= ent::bullet::Track::Save(outPath, saveData);

    ftimeB = float(OldSchool::GetRef().GetLoResTime());
    ftime  = (ftimeB - ftimeA);
    orkprintf("<<PROFILE>> <<DAEToSECCollision::End %f seconds>>\n", ftime);

#if DEBUG_DRAW
    DebugSectorBasisInterp(outPath);
#endif
  }

  return rval;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::tool::meshutil
///////////////////////////////////////////////////////////////////////////////
#endif
