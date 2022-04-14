
////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/util/crc.h>
#include <ork/util/crc64.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/box.h>
#include <algorithm>
#include <ork/kernel/Array.h>

#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <unordered_map>
#include <ork/lev2/gfx/meshutil/meshutil.h>

namespace ork::meshutil {

struct XgmClusterizer;

///////////////////////////////////////////////////////////////////////////////

struct ClusterizerPolicy {
  bool _skinned           = false;
  int _maxBonesPerCluster = 24;
};

///////////////////////////////////////////////////////////////////////////////

struct XgmClusterTri {
  vertex _vertex[3];
};

///////////////////////////////////////////////////////////////////////////////
/// XgmClusterBuilder : responsible for building a single cluster
///////////////////////////////////////////////////////////////////////////////

struct XgmClusterBuilder {

  //////////////////////////////////////////////////
  XgmClusterBuilder(const XgmClusterizer& clusterizer);
  virtual ~XgmClusterBuilder();
  //////////////////////////////////////////////////
  virtual bool addTriangle(const XgmClusterTri& Triangle)                               = 0;
  virtual void buildVertexBuffer(lev2::Context& context, lev2::EVtxStreamFormat format) = 0;
  //////////////////////////////////////////////////
  void Dump(void);
  ///////////////////////////////////////////////////////////////////
  // Build Vertex Buffers
  ///////////////////////////////////////////////////////////////////
  submesh _submesh;
  lev2::vtxbufferbase_ptr_t _vertexBuffer;
  const XgmClusterizer& _clusterizer;
};

typedef std::shared_ptr<XgmClusterBuilder> clusterbuilder_ptr_t;

///////////////////////////////////////////////////////////////////////////////

struct XgmSkinnedClusterBuilder : public XgmClusterBuilder {
  XgmSkinnedClusterBuilder(const XgmClusterizer& clusterizer);
  /////////////////////////////////////////////////
  const orkmap<std::string, int>& RefBoneRegMap() const {
    return _boneRegisterMap;
  }

  bool addTriangle(const XgmClusterTri& Triangle) final;
  void buildVertexBuffer(lev2::Context& context, lev2::EVtxStreamFormat format) final; // virtual

  int FindNewBoneIndex(const std::string& BoneName);
  void BuildVertexBuffer_V12N12T8I4W4(lev2::Context& context);
  void BuildVertexBuffer_V12N12B12T8I4W4(lev2::Context& context);
  void BuildVertexBuffer_V12N6I1T4(lev2::Context& context);

  orkmap<std::string, int> _boneRegisterMap;
};

///////////////////////////////////////////////////////////////////////////////

struct XgmRigidClusterBuilder : public XgmClusterBuilder {
  XgmRigidClusterBuilder(const XgmClusterizer& clusterizer);
  bool addTriangle(const XgmClusterTri& Triangle) final;
  void buildVertexBuffer(lev2::Context& context, lev2::EVtxStreamFormat format) final;
};

///////////////////////////////////////////////////////////////////////////////
/// XgmClusterizer : responsible for partitioning an input mesh into a
//    set of clusters based on some policies
///////////////////////////////////////////////////////////////////////////////

struct XgmClusterizer {
  ///////////////////////////////////////////////////////
  XgmClusterizer();
  virtual ~XgmClusterizer();
  ///////////////////////////////////////////////////////
  virtual bool addTriangle(const XgmClusterTri& Triangle, const MeshConfigurationFlags& flags) = 0;
  virtual void Begin() {
  }
  virtual void End() {
  }
  ///////////////////////////////////////////////////////
  size_t GetNumClusters() const {
    return _clusters.size();
  }
  clusterbuilder_ptr_t GetCluster(int idx) const {
    return _clusters[idx];
  }

  std::vector<clusterbuilder_ptr_t> _clusters;
  ClusterizerPolicy _policy;
  ///////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

struct XgmClusterizerDiced : public XgmClusterizer {
  ///////////////////////////////////////////////////////
  XgmClusterizerDiced();
  virtual ~XgmClusterizerDiced();
  ///////////////////////////////////////////////////////
  bool addTriangle(const XgmClusterTri& Triangle, const MeshConfigurationFlags& flags);
  void Begin(); // virtual
  void End();   // virtual
  ///////////////////////////////////////////////////////

  submesh _preDicedMesh;
};

///////////////////////////////////////////////////////////////////////////////

struct XgmClusterizerStd : public XgmClusterizer {
  XgmClusterizerStd();
  virtual ~XgmClusterizerStd();
  bool addTriangle(const XgmClusterTri& Triangle, const MeshConfigurationFlags& flags);
};

///////////////////////////////////////////////////////////////////////////////

void buildXgmCluster(lev2::Context& context, 
                     lev2::xgmcluster_ptr_t xgm_cluster, 
                     clusterbuilder_ptr_t pclusbuilder,
                     bool enable_tristrips);

} // namespace ork::meshutil
