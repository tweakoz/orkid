///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyrigh 1996-2004, Michael T. Mayers
// See License at OrkidRoot/license.html or http://www.tweakoz.com/orkid/license.html
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/meshutil/clusterizer.h>

using namespace ork::tool;

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

XgmClusterizer::XgmClusterizer() {
}

///////////////////////////////////////////////////////////////////////////////

XgmClusterizer::~XgmClusterizer() {
  for (auto item : _clusters)
    delete item;
}

///////////////////////////////////////////////////////////////////////////////

XgmClusterizerStd::XgmClusterizerStd() {
}

///////////////////////////////////////////////////////////////////////////////

XgmClusterizerStd::~XgmClusterizerStd() {
}

///////////////////////////////////////////////////////////////////////////////

bool XgmClusterizerStd::AddTriangle(const XgmClusterTri& Triangle, const MaterialGroup* cmg) {

  size_t iNumClusters = _clusters.size();

  bool bAdded = false;

  for (size_t i = 0; i < iNumClusters; i++) {
    XgmClusterBuilder* pClus = _clusters[i];
    bAdded                   = pClus->addTriangle(Triangle);
    if (bAdded) {
      break;
    }
  }

  if (false == bAdded) // start new cluster
  {
    XgmClusterBuilder* pNewCluster = 0;

    bool do_skinned = _policy._skinned && cmg->mMeshConfigurationFlags.mbSkinned;

    printf("do_skinned<%d>\n", int(do_skinned));

    if (do_skinned) {
      pNewCluster = new XgmSkinnedClusterBuilder(*this);
    } else {
      pNewCluster = new XgmRigidClusterBuilder(*this);
    }

    _clusters.push_back(pNewCluster);
    return pNewCluster->addTriangle(Triangle);
  }
  return bAdded;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
