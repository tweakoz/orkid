///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyrigh 1996-2004, Michael T. Mayers
// See License at OrkidRoot/license.html or http://www.tweakoz.com/orkid/license.html
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/meshutil/meshutil_fixedgrid.h>
#include <ork/lev2/gfx/meshutil/clusterizer.h>
#include <ork/application/application.h>
#include <ork/lev2/gfx/meshutil/meshutil_stripper.h>

const bool gbFORCEDICE = true;
const int kDICESIZE    = 512;

using namespace ork::tool;

namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

XgmClusterBuilder::XgmClusterBuilder(const XgmClusterizer& clusterizer)
    : _vertexBuffer(NULL)
    , _clusterizer(clusterizer) {
}

///////////////////////////////////////////////////////////////////////////////

XgmClusterBuilder::~XgmClusterBuilder() {
}

///////////////////////////////////////////////////////////////////////////////

void XgmClusterBuilder::Dump(void) {
  /*orkprintf( "[CLUSDUMP] Cluster[%08x] NUBI %02d\n", this, GetNumUniqueBonIndices() );
  int iNumBones = _boneRegisterMap.size();
  orkprintf( "/////////////////////////////////////////\n" );
  orkprintf( "[CLUSDUMP] [Cluster %08x] [NumBones %d]\n", this, iNumBones );
  orkprintf( "//////////////////\n" );
  orkprintf( "[CLUSDUMP] " );
  /////////////////////////////////////////////////////////////////
  static int RegMap[256];
  for( orkmap<int,int>::const_iterator it=_boneRegisterMap.begin(); it!=_boneRegisterMap.end(); it++ )
  {	std::pair<int,int> BoneMapItem = *it;
      int BoneIDX = BoneMapItem.first;
      int BoneREG = BoneMapItem.second;
      RegMap[ BoneREG ] = BoneIDX;
      orkprintf( " B%02d:R%02d", BoneMapItem.first, BoneMapItem.second );
  }
  orkprintf( "\n[CLUSDUMP] " );
  ////////////////////////////////////////////////////////////////
  for( int r=0; r<iNumBones; r++ )
  {	orkprintf( " R%02d:B%02d", r, RegMap[r] );
  }
  orkprintf( "\n//////////////////\n" );*/
}

///////////////////////////////////////////////////////////////////////////////

void BuildXgmClusterPrimGroups(lev2::XgmCluster& XgmCluster, const std::vector<unsigned int>& TriangleIndices) {
  lev2::ContextDummy DummyTarget;

  const int imaxvtx = XgmCluster._vertexBuffer->GetNumVertices();

  // const ColladaExportPolicy* policy = ColladaExportPolicy::context();
  // TODO: Is this correct? Why?
  static const int WII_PRIM_GROUP_MAX_INDICES = 0xFFFF;

  ////////////////////////////////////////////////////////////
  // Build TriStrips

  meshutil::TriStripper MyStripper(TriangleIndices, 16, 4);

  bool bhastris = (MyStripper.GetTriIndices().size() > 0);

  int inumstripgroups = MyStripper.GetStripGroups().size();

  bool bhasstrips = (inumstripgroups > 0);

  int inumpg = inumstripgroups + int(bhastris);

  ////////////////////////////////////////////////////////////
  // Create PrimGroups

  XgmCluster.mpPrimGroups    = new ork::lev2::XgmPrimGroup[inumpg];
  XgmCluster.miNumPrimGroups = inumpg;

  ////////////////////////////////////////////////////////////

  int ipg = 0;

  ////////////////////////////////////////////////////////////
  if (bhasstrips)
  ////////////////////////////////////////////////////////////
  {
    const orkvector<meshutil::TriStripperPrimGroup>& StripGroups = MyStripper.GetStripGroups();
    for (int i = 0; i < inumstripgroups; i++) {
      const orkvector<unsigned int>& StripIndices = MyStripper.GetStripIndices(i);
      int inumidx                                 = StripIndices.size();

      /////////////////////////////////

      ork::lev2::StaticIndexBuffer<U16>* pidxbuf = new ork::lev2::StaticIndexBuffer<U16>(inumidx);
      U16* pidx                                  = (U16*)DummyTarget.GBI()->LockIB(*pidxbuf);
      OrkAssert(pidx != 0);
      {
        for (int ii = 0; ii < inumidx; ii++) {
          int index = StripIndices[ii];
          OrkAssert(index < imaxvtx);
          pidx[ii] = U16(index);
        }
      }
      DummyTarget.GBI()->UnLockIB(*pidxbuf);

      /////////////////////////////////

      ork::lev2::XgmPrimGroup& StripGroup = XgmCluster.mpPrimGroups[ipg++];

      StripGroup.miNumIndices = inumidx;
      StripGroup.mpIndices    = pidxbuf;
      StripGroup.mePrimType   = lev2::EPRIM_TRIANGLESTRIP;
    }
  }

  ////////////////////////////////////////////////////////////
  if (bhastris)
  ////////////////////////////////////////////////////////////
  {
    int inumidx = MyStripper.GetTriIndices().size();

    /////////////////////////////////////////////////////
    ork::lev2::StaticIndexBuffer<U16>* pidxbuf = new ork::lev2::StaticIndexBuffer<U16>(inumidx);
    U16* pidx                                  = (U16*)DummyTarget.GBI()->LockIB(*pidxbuf);
    OrkAssert(pidx != 0);
    for (int ii = 0; ii < inumidx; ii++) {
      pidx[ii] = U16(MyStripper.GetTriIndices()[ii]);
    }
    DummyTarget.GBI()->UnLockIB(*pidxbuf);
    /////////////////////////////////////////////////////

    ork::lev2::XgmPrimGroup& StripGroup = XgmCluster.mpPrimGroups[ipg++];

    StripGroup.miNumIndices = inumidx;
    StripGroup.mpIndices    = pidxbuf;
    StripGroup.mePrimType   = lev2::EPRIM_TRIANGLES;
  }
}

///////////////////////////////////////////////////////////////////////////////

void buildTriStripXgmCluster(lev2::XgmCluster& XgmCluster, const XgmClusterBuilder* clusterbuilder) {
  if (!clusterbuilder->_vertexBuffer)
    return;

  XgmCluster._vertexBuffer = clusterbuilder->_vertexBuffer;

  const int imaxvtx = XgmCluster._vertexBuffer->GetNumVertices();

  /////////////////////////////////////////////////////////////
  // triangle indices come from the ClusterBuilder

  std::vector<unsigned int> TriangleIndices;
  std::vector<int> ToolMeshTriangles;

  clusterbuilder->_submesh.FindNSidedPolys(ToolMeshTriangles, 3);

  int inumtriangles = int(ToolMeshTriangles.size());

  for (int i = 0; i < inumtriangles; i++) {
    int itri_i = ToolMeshTriangles[i];

    const ork::meshutil::poly& ClusTri = clusterbuilder->_submesh.RefPoly(itri_i);

    TriangleIndices.push_back(ClusTri.GetVertexID(0));
    TriangleIndices.push_back(ClusTri.GetVertexID(1));
    TriangleIndices.push_back(ClusTri.GetVertexID(2));
  }

  /////////////////////////////////////////////////////////////

  BuildXgmClusterPrimGroups(XgmCluster, TriangleIndices);

  XgmCluster.mBoundingBox    = clusterbuilder->_submesh.aabox();
  XgmCluster.mBoundingSphere = Sphere(XgmCluster.mBoundingBox.Min(), XgmCluster.mBoundingBox.Max());

  /////////////////////////////////////////////////////////////
  // bone -> matrix register mapping

  auto skinner = dynamic_cast<const XgmSkinnedClusterBuilder*>(clusterbuilder);

  if (skinner) {
    const orkmap<std::string, int>& BoneMap = skinner->RefBoneRegMap();

    int inumjointsmapped = BoneMap.size();

    XgmCluster.mJoints.resize(inumjointsmapped);

    for (orkmap<std::string, int>::const_iterator it = BoneMap.begin(); it != BoneMap.end(); it++) {
      const std::string& JointName      = it->first;  // the index of the bone in the skeleton
      int JointRegister                 = it->second; // the shader register index the bone goes into
      XgmCluster.mJoints[JointRegister] = AddPooledString(JointName.c_str());
    }
  }

  /////////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
