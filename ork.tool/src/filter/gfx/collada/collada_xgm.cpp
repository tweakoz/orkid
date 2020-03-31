///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/application/application.h>
#if defined(USE_FCOLLADA)
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/prop.h>

#include <orktool/filter/gfx/collada/collada.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/meshutil/clusterizer.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::tool::meshutil {

///////////////////////////////////////////////////////////////////////////////

bool CColladaModel::BuildXgmSkeleton(void) {
  int inumjoints = mSkeleton.size();

  ork::lev2::XgmSkeleton& XgmSkel = mXgmModel.skeleton();

  mXgmModel.SetSkinned(isSkinned());

  if (inumjoints > 1) {
    XgmSkel.mBindShapeMatrix = mBindShapeMatrix;

    XgmSkel.resize(inumjoints);

    ////////////////////////////////////////////////////////
    // assign linearized joint indices
    ////////////////////////////////////////////////////////

    int ijntindex = 0;

    for (orkmap<std::string, ork::lev2::XgmSkelNode*>::iterator it = mSkeleton.begin(); it != mSkeleton.end(); it++) {
      ork::lev2::XgmSkelNode* SkelNode = (*it).second;
      SkelNode->miSkelIndex            = ijntindex++;
    }

    ////////////////////////////////////////////////////////
    // setup hierarchy
    ////////////////////////////////////////////////////////

    for (orkmap<std::string, ork::lev2::XgmSkelNode*>::iterator it = mSkeleton.begin(); it != mSkeleton.end(); it++) {
      const std::string& JointName          = (*it).first;
      ork::lev2::XgmSkelNode* SkelNode      = (*it).second;
      const ork::lev2::XgmSkelNode* ParNode = SkelNode->_parent;
      int idx                               = SkelNode->miSkelIndex;
      int pidx                              = ParNode ? ParNode->miSkelIndex : -1;
      PoolString JointNameSidx              = AddPooledString(JointName.c_str());
      XgmSkel.AddJoint(idx, pidx, JointNameSidx);
      XgmSkel.RefInverseBindMatrix(idx) = SkelNode->_bindMatrixInverse;
      XgmSkel.RefJointMatrix(idx)       = SkelNode->_jointMatrix;
    }

    ////////////////////////////////////////////////////////
  } else {
    return true;
  }

  XgmSkel.mTopNodesMatrix = mTopNodesMatrix;

  /////////////////////////////////////
  // flatten the skeleton
  /////////////////////////////////////

  XgmSkel.miRootNode = mSkeletonRoot ? mSkeletonRoot->miSkelIndex : -1;

  if (mSkeletonRoot) {
    orkstack<lev2::XgmSkelNode*> NodeStack;
    NodeStack.push(mSkeletonRoot);
    while (false == NodeStack.empty()) {
      lev2::XgmSkelNode* ParNode = NodeStack.top();
      int iparentindex           = ParNode->miSkelIndex;
      NodeStack.pop();
      int inumchildren = ParNode->mChildren.size();
      for (int ic = 0; ic < inumchildren; ic++) {
        lev2::XgmSkelNode* Child = ParNode->mChildren[ic];
        int ichildindex          = Child->miSkelIndex;

        lev2::XgmBone Bone = {iparentindex, ichildindex};

        XgmSkel.addBone(Bone);
        NodeStack.push(Child);
      }
    }
  }
  XgmSkel.mpRootNode = mSkeletonRoot;
  // XgmSkel.dump();
  return true;
}

///////////////////////////////////////////////////////////////////////////////

struct TexSetter {
  orkmap<file::Path, ork::lev2::TextureAsset*> mTextureMap;

  ork::lev2::TextureAsset*
  get(const ork::meshutil::MaterialChannel& MatChanIn, const std::string& model_directory, std::string& ActualFileName) {
    ork::lev2::TextureAsset* htexture = 0;

    if (MatChanIn.mTextureName.length()) {
      file::Path::NameType texname = MatChanIn.mTextureName.c_str();
      file::Path::NameType texext  = FileEnv::filespec_to_extension(MatChanIn.mTextureName.c_str());

      texname.replace_in_place("\\", "/");

      printf("model_directory<%s>\n", model_directory.c_str());
      printf("texname1<%s>\n", texname.c_str());

      auto it_slash     = texname.find("/");
      auto it_src_slash = texname.find("src/");
      const auto NPOS   = file::Path::NameType::npos;

      printf("texname2<%s>\n", texname.c_str());
      printf("it_slash<%zx>\n", it_slash);
      printf("it_src_slash<%zx>\n", it_src_slash);

      ///////////////////////////////////////////////////////////////
      // find data/src or data\\src in texture path
      ///////////////////////////////////////////////////////////////

      if (NPOS == it_slash) {
        ActualFileName = texname.c_str();
      } else if (NPOS != it_src_slash) {
        it_src_slash += 4;
        texname        = texname.substr(it_src_slash, texname.length() - it_src_slash);
        ActualFileName = CreateFormattedString("data/src/%s", texname.c_str());
      } else {
        std::string mdir = model_directory;

        if (mdir.find("data/pc/") == 0) {
          mdir = CreateFormattedString("data/src/%s", mdir.substr(8, mdir.length() - 8).c_str());
        }
        ActualFileName = CreateFormattedString("%s/%s", mdir.c_str(), texname.c_str());
      }

      ///////////////////////////////////////////////////////////////
      // if texture exists, assign it..
      ///////////////////////////////////////////////////////////////

      file::Path PathToTexture(ActualFileName.c_str());

      auto itt = mTextureMap.find(PathToTexture);

      if (mTextureMap.end() == itt) {
        if (FileEnv::DoesFileExist(PathToTexture)) {
          lev2::ContextDummy DummyTarget;
          ork::lev2::TextureAsset* pl2tex = new ork::lev2::TextureAsset;
          ork::lev2::Texture* ptex        = pl2tex->GetTexture();
          bool bOK                        = DummyTarget.TXI()->LoadTexture(PathToTexture, ptex);
          if (bOK) {
            printf("loaded texture<%s>\n", PathToTexture.c_str());
            pl2tex->SetName(ork::AddPooledString(PathToTexture.c_str()));
            ptex->_varmap.makeValueForKey<std::string>("abspath") = PathToTexture.c_str();
            htexture                                              = pl2tex;
            mTextureMap[PathToTexture]                            = pl2tex;
          }
        }
      } else {
        htexture = itt->second;
      }
    }
    return htexture;
  }
};

///////////////////////////////////////////////////////////////////////////////

static TexSetter gtex_setter;

///////////////////////////////////////////////////////////////////////////////

void CColladaModel::BuildXgmTriStripMesh(lev2::XgmMesh& XgmMesh, SColladaMesh* ColMesh) {
  int inummatgroups = ColMesh->RefMatGroups().size();

  orkvector<ork::meshutil::MaterialGroup*> clustersets;

  for (int imat = 0; imat < inummatgroups; imat++) {
    ork::meshutil::MaterialGroup* ColMatGroup = ColMesh->RefMatGroups()[imat];
    clustersets.push_back(ColMatGroup);
  }

  /////////////////////////////////////////////////////////////////////////////////////

  int inumclusset = int(clustersets.size());

  XgmMesh.ReserveSubMeshes(inumclusset);

  for (int imat = 0; imat < inumclusset; imat++) {
    lev2::XgmSubMesh& XgmClusSet = *new lev2::XgmSubMesh;
    XgmMesh.AddSubMesh(&XgmClusSet);

    ork::meshutil::MaterialGroup* ColMatGroup = clustersets[imat];

    XgmClusSet.mLightMapPath = ColMatGroup->mLightMapPath;
    XgmClusSet.mbVertexLit   = ColMatGroup->mbVertexLit;
    ///////////////////////////////////////////////

    // if (ColMatGroup->meMaterialClass == meshutil::ToolMaterialGroup::EMATCLASS_FX) {
    // ConfigureFxMaterial(this, ColMatGroup, XgmClusSet);
    //} else if (ColMatGroup->meMaterialClass == meshutil::ToolMaterialGroup::EMATCLASS_STANDARD) {
    // ConfigureStdMaterial(this, ColMatGroup, XgmClusSet);
    //} else
    { OrkAssert(false); }

    ColMatGroup->ComputeVtxStreamFormat();

    mXgmModel.AddMaterial(XgmClusSet.GetMaterial());

    ///////////////////////////////////////////////

    int inumclus = ColMatGroup->GetClusterizer()->GetNumClusters();

    XgmClusSet.miNumClusters = inumclus;

    XgmClusSet.mpClusters = new lev2::XgmCluster[inumclus];

    for (int ic = 0; ic < inumclus; ic++) {
      ork::meshutil::XgmClusterBuilder* clusterbuilder = ColMatGroup->GetClusterizer()->GetCluster(ic);

      auto format = ColMatGroup->GetVtxStreamFormat();
      clusterbuilder->buildVertexBuffer(format);

      lev2::XgmCluster& XgmClus = XgmClusSet.mpClusters[ic];
      buildTriStripXgmCluster(XgmClus, clusterbuilder);

      int inumclusjoints = XgmClus.mJoints.size();
      for (int ib = 0; ib < inumclusjoints; ib++) {
        const PoolString JointName                     = XgmClus.mJoints[ib];
        orklut<PoolString, int>::const_iterator itfind = mXgmModel.skeleton().mmJointNameMap.find(JointName);
        int iskelindex                                 = (*itfind).second;
        XgmClus.mJointSkelIndices.push_back(iskelindex);
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

bool CColladaModel::BuildXgmTriStripModel(void) {
  int inummeshes = mMeshIdMap.size();

  mXgmModel.ReserveMeshes(inummeshes);

  std::string FindMesh = "fg_2_1_3_ground_SG_ground_GeoDaeId";
  int imesh            = 0;
  for (orkmap<std::string, ColMeshRec*>::iterator it = mMeshIdMap.begin(); it != mMeshIdMap.end(); it++) {
    if (it->first == FindMesh) {
      orkprintf("found <%s>\n", FindMesh.c_str());
    }
    lev2::XgmMesh& XgmMesh = *new lev2::XgmMesh;
    SColladaMesh* ColMesh  = it->second->mcolmesh;
    PoolString MeshName    = AddPooledString(ColMesh->meshName().c_str());

    mXgmModel.AddMesh(MeshName, &XgmMesh);

    XgmMesh.SetMeshName(MeshName);

    BuildXgmTriStripMesh(XgmMesh, ColMesh);

    imesh++;
  }
  mXgmModel.SetBoundingAA_XYZ(mAABoundXYZ);
  mXgmModel.SetBoundingAA_WHD(mAABoundWHD);
  mXgmModel.SetBoundingCenter(mAABoundXYZ + (mAABoundWHD * float(0.5f)));
  mXgmModel.SetBoundingRadius(mAABoundWHD.Mag() * float(0.5f));

  return (inummeshes > 0);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::tool::meshutil

///////////////////////////////////////////////////////////////////////////////

#endif // USE_FCOLLADA
