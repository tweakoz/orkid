///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyrigh 1996-2004, Michael T. Mayers
// See License at OrkidRoot/license.html or http://www.tweakoz.com/orkid/license.html
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/meshutil/clusterizer.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

XgmClusterizer::XgmClusterizer() {
}

///////////////////////////////////////////////////////////////////////////////

XgmClusterizer::~XgmClusterizer() {
}

///////////////////////////////////////////////////////////////////////////////

XgmClusterizerStd::XgmClusterizerStd() {
}

///////////////////////////////////////////////////////////////////////////////

XgmClusterizerStd::~XgmClusterizerStd() {
}

///////////////////////////////////////////////////////////////////////////////

bool XgmClusterizerStd::addTriangle(const XgmClusterTri& Triangle, const MeshConfigurationFlags& flags) {

  size_t iNumClusters = _clusters.size();
  bool do_skinned = _policy._skinned && flags._skinned;

  bool added = false;

  //////////////////////////////////////////////////////////////
  // SKINNED
  //////////////////////////////////////////////////////////////
  if( do_skinned ){
    for (size_t i = 0; i < iNumClusters; i++) {
      auto clusterbuilder = _clusters[i];
      added              = clusterbuilder->addTriangle(Triangle);
      if (added) {
        break;
      }
    }

    if (not added) { // start new cluster
      auto new_builder = std::make_shared<XgmSkinnedClusterBuilder>(*this);
      _clusters.push_back(new_builder);
      return new_builder->addTriangle(Triangle);
    }
  }
  //////////////////////////////////////////////////////////////
  // RIGID
  //////////////////////////////////////////////////////////////
  else{ 
      if(_clusters.size()==0){ 
        auto clusterbuilder = std::make_shared<XgmRigidClusterBuilder>(*this);
        _clusters.push_back(clusterbuilder);
      }
      auto clusterbuilder = *_clusters.rbegin();
      added              = clusterbuilder->addTriangle(Triangle);
      if( not added){ // start new cluster
        //printf( "STARTED NEW RIGID CLUSTER\n");
        clusterbuilder = std::make_shared<XgmRigidClusterBuilder>(*this);
        _clusters.push_back(clusterbuilder);
        added = clusterbuilder->addTriangle(Triangle);
      }
  }
  return added;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
