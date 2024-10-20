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

namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

XgmClusterBuilder::XgmClusterBuilder(const XgmClusterizer& clusterizer)
    : _clusterizer(clusterizer) {
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

void BuildXgmClusterPrimGroups(
    lev2::Context& context,
    lev2::xgmcluster_ptr_t xgm_cluster,
    const std::vector<unsigned int>& TriangleIndices,
    bool enable_tristrips) {

  const int imaxvtx = xgm_cluster->_vertexBuffer->GetNumVertices();

  // const ColladaExportPolicy* policy = ColladaExportPolicy::context();
  // TODO: Is this correct? Why?
  static const int WII_PRIM_GROUP_MAX_INDICES = 0xFFFF;

  ////////////////////////////////////////////////////////////
  // Disallow TriStrips
  ////////////////////////////////////////////////////////////

  if(not enable_tristrips){
    int inumidx = TriangleIndices.size();

    /////////////////////////////////////////////////////
    ork::lev2::StaticIndexBuffer<U16>* pidxbuf = new ork::lev2::StaticIndexBuffer<U16>(inumidx);
    U16* pidx                                  = (U16*)context.GBI()->LockIB(*pidxbuf);
    OrkAssert(pidx != 0);
    for (int ii = 0; ii < inumidx; ii++) {
      pidx[ii] = U16(TriangleIndices[ii]);
    }
    context.GBI()->UnLockIB(*pidxbuf);
    /////////////////////////////////////////////////////

    auto ListGroup = std::make_shared<ork::lev2::XgmPrimGroup>();
    xgm_cluster->_primgroups.push_back(ListGroup);

    ListGroup->miNumIndices = inumidx;
    ListGroup->mpIndices    = pidxbuf;
    ListGroup->mePrimType   = lev2::PrimitiveType::TRIANGLES;
  }

  ////////////////////////////////////////////////////////////
  // Allow TriStrips
  ////////////////////////////////////////////////////////////

  else {
  meshutil::TriStripper MyStripper(TriangleIndices, 16, 4);

  bool bhastris = (MyStripper.GetTriIndices().size() > 0);

  int inumstripgroups = MyStripper.GetStripGroups().size();

  bool bhasstrips = (inumstripgroups > 0);

  int inumpg = inumstripgroups + int(bhastris);

  ////////////////////////////////////////////////////////////
  // Create PrimGroups

  for (int ipg = 0; ipg < inumpg; ipg++) {
    auto pg = std::make_shared<ork::lev2::XgmPrimGroup>();
    xgm_cluster->_primgroups.push_back(pg);
  }

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
      U16* pidx                                  = (U16*)context.GBI()->LockIB(*pidxbuf);
      OrkAssert(pidx != 0);
      {
        for (int ii = 0; ii < inumidx; ii++) {
          int index = StripIndices[ii];
          OrkAssert(index < imaxvtx);
          pidx[ii] = U16(index);
        }
      }
      context.GBI()->UnLockIB(*pidxbuf);

      /////////////////////////////////

      auto stripgroup = xgm_cluster->primgroup(ipg++);

      stripgroup->miNumIndices = inumidx;
      stripgroup->mpIndices    = pidxbuf;
      stripgroup->mePrimType   = lev2::PrimitiveType::TRIANGLESTRIP;
    }
  }

  ////////////////////////////////////////////////////////////
  if (bhastris)
  ////////////////////////////////////////////////////////////
  {
    int inumidx = MyStripper.GetTriIndices().size();

    /////////////////////////////////////////////////////
    ork::lev2::StaticIndexBuffer<U16>* pidxbuf = new ork::lev2::StaticIndexBuffer<U16>(inumidx);
    U16* pidx                                  = (U16*)context.GBI()->LockIB(*pidxbuf);
    OrkAssert(pidx != 0);
    for (int ii = 0; ii < inumidx; ii++) {
      pidx[ii] = U16(MyStripper.GetTriIndices()[ii]);
    }
    context.GBI()->UnLockIB(*pidxbuf);
    /////////////////////////////////////////////////////

    auto ListGroup = xgm_cluster->primgroup(ipg++);

    ListGroup->miNumIndices = inumidx;
    ListGroup->mpIndices    = pidxbuf;
    ListGroup->mePrimType   = lev2::PrimitiveType::TRIANGLES;
  }
  }
}

///////////////////////////////////////////////////////////////////////////////

void buildXgmCluster( lev2::Context& context, 
                      lev2::xgmcluster_ptr_t xgm_cluster, 
                      clusterbuilder_ptr_t clusterbuilder,
                      bool enable_tristrips ) {

  if (!clusterbuilder->_vertexBuffer)
    return;

  // transfer VB to cluster
  xgm_cluster->_vertexBuffer = clusterbuilder->_vertexBuffer;

  const int imaxvtx = xgm_cluster->_vertexBuffer->GetNumVertices();

  // printf("imaxvtx<%d>\n", imaxvtx);

  /////////////////////////////////////////////////////////////
  // triangle indices come from the ClusterBuilder

  std::vector<unsigned int> TriangleIndices;
  std::vector<int> ToolMeshTriangles;

  clusterbuilder->_submesh.FindNSidedPolys(ToolMeshTriangles, 3);

  int inumtriangles = int(ToolMeshTriangles.size());

  for (int i = 0; i < inumtriangles; i++) {
    int itri_i = ToolMeshTriangles[i];

    const ork::meshutil::Polygon& ClusTri = clusterbuilder->_submesh.RefPoly(itri_i);

    TriangleIndices.push_back(ClusTri.vertexID(0));
    TriangleIndices.push_back(ClusTri.vertexID(1));
    TriangleIndices.push_back(ClusTri.vertexID(2));
  }

  /////////////////////////////////////////////////////////////

  BuildXgmClusterPrimGroups(context, xgm_cluster, TriangleIndices,enable_tristrips);

  xgm_cluster->mBoundingBox    = clusterbuilder->_submesh.aabox();
  xgm_cluster->mBoundingSphere = Sphere(xgm_cluster->mBoundingBox.Min(), xgm_cluster->mBoundingBox.Max());

  /////////////////////////////////////////////////////////////
  // bone -> matrix register mapping

  auto skinned_builder = std::dynamic_pointer_cast<XgmSkinnedClusterBuilder>(clusterbuilder);

  if (skinned_builder) {
    const auto& joint_map = skinned_builder->jointRegMap();
    xgm_cluster->_jointPaths.resize(joint_map.size());
    for (auto item : joint_map ) {
      const std::string& joint_path            = item.first;  
      int joint_register                       = item.second; // the shader register index the bone goes into
      xgm_cluster->_jointPaths[joint_register] = joint_path;
    }
  }

  /////////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
