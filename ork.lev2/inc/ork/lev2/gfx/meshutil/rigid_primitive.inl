////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
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
#include <ork/kernel/varmap.inl>

#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/targetinterfaces.h>
#include <unordered_map>
#include <ork/kernel/datablock.inl>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/meshutil/clusterizer.h>

namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////
typedef orkmap<std::string, svar64_t> AnnotationMap;
struct XgmClusterizer;
struct XgmClusterizerDiced;
struct XgmClusterizerStd;
///////////////////////////////////////////////////////////////////////////////
/// RigidPrimitive SubMesh Primitive with V12N12B12T8C4 vertex format
///////////////////////////////////////////////////////////////////////////////
template <typename vtx_t> struct RigidPrimitive {

  using idxbuf_t      = lev2::StaticIndexBuffer<uint16_t>;
  using vtxbuf_t      = lev2::StaticVertexBuffer<vtx_t>;
  using vtxbuf_ptr_t  = std::shared_ptr<vtxbuf_t>;
  using vtxbuf_list_t = std::vector<vtxbuf_t>;
  using idxbuf_ptr_t  = std::shared_ptr<idxbuf_t>;

  struct PrimitiveGroup {
    idxbuf_ptr_t _idxbuffer;
    lev2::EPrimitiveType _primtype = lev2::EPrimitiveType::NONE;
  };
  using primgroup_ptr_t      = std::shared_ptr<PrimitiveGroup>;
  using primgroup_ptr_list_t = std::vector<primgroup_ptr_t>;
  struct PrimGroupCluster {
    vtxbuf_ptr_t _vtxbuffer;
    primgroup_ptr_list_t _primgroups;
  };

  using primgroupcluster_ptr_t = std::shared_ptr<PrimGroupCluster>;
  using cluster_ptr_list_t     = std::vector<primgroupcluster_ptr_t>;

  RigidPrimitive();

  void fromSubMesh(const submesh& submesh, lev2::Context* context); /// generate from submesh using internal vertexbuffer
  void fromClusterizer(const XgmClusterizerStd& cluz, lev2::Context* context);
  void draw(lev2::Context* context) const; /// draw with context

  cluster_ptr_list_t _gpuClusters;
};
///////////////////////////////////////////////////////////////////////////////
template <typename vtx_t> RigidPrimitive<vtx_t>::RigidPrimitive() {
}
////////////////////////////////////////////////////////////////////////////////
template <typename vtx_t>
void RigidPrimitive<vtx_t>::fromClusterizer(const meshutil::XgmClusterizerStd& cluz, lev2::Context* context) {
  //////////////////////////////////////////////////////////////
  // create Indexed TriStripped Primitive Groups
  //////////////////////////////////////////////////////////////
  size_t inumclus = cluz.GetNumClusters();
  printf("inumclus<%zu>\n", inumclus);
  OrkAssert(inumclus <= 1);
  for (size_t icluster = 0; icluster < inumclus; icluster++) {
    auto clusterbuilder = cluz.GetCluster(icluster);
    clusterbuilder->buildVertexBuffer(*context, vtx_t::meFormat);
    lev2::XgmCluster xgmcluster;
    buildTriStripXgmCluster(*context, xgmcluster, clusterbuilder);
    primgroupcluster_ptr_t out_cluster = std::make_shared<PrimGroupCluster>();
    out_cluster->_vtxbuffer            = std::dynamic_pointer_cast<vtxbuf_t>(clusterbuilder->_vertexBuffer);
    for (size_t ipg = 0; ipg < xgmcluster.numPrimGroups(); ipg++) {
      auto src_PG         = xgmcluster.primgroup(ipg);
      auto gpu_prim       = std::make_shared<PrimitiveGroup>();
      gpu_prim->_primtype = src_PG->GetPrimType();
      out_cluster->_primgroups.push_back(gpu_prim);
      size_t numindices    = src_PG->GetNumIndices();
      auto src_indexdata   = (const uint16_t*)context->GBI()->LockIB(*src_PG->GetIndexBuffer());
      gpu_prim->_idxbuffer = std::make_shared<idxbuf_t>(numindices);
      auto gpuindexptr     = (void*)context->GBI()->LockIB(*gpu_prim->_idxbuffer.get());
      memcpy(gpuindexptr, src_indexdata, numindices * sizeof(uint16_t));
      context->GBI()->UnLockIB(*gpu_prim->_idxbuffer.get());
      context->GBI()->UnLockIB(*src_PG->GetIndexBuffer());
    }
    _gpuClusters.push_back(out_cluster);
  } // for (size_t icluster = 0; icluster < inumclus; icluster++) {
}
////////////////////////////////////////////////////////////////////////////////
template <typename vtx_t> void RigidPrimitive<vtx_t>::fromSubMesh(const submesh& inp_submesh, lev2::Context* context) {
  submesh submeshTris;
  submeshTriangulate(inp_submesh, submeshTris);
  //////////////////////////////////////////////////////////////
  // Fill In ClusterBuilder from submesh triangle soup
  //////////////////////////////////////////////////////////////
  meshutil::XgmClusterizerStd clusterizer;
  meshutil::MeshConfigurationFlags meshflags;
  const auto& vpool = submeshTris.RefVertexPool();
  int numverts      = vpool.GetNumVertices();
  int inumpolys     = submeshTris.GetNumPolys(3);
  clusterizer.Begin();
  for (int p = 0; p < inumpolys; p++) {
    const auto& poly   = submeshTris.RefPoly(p);
    const vertex& vtxa = vpool.GetVertex(poly.miVertices[0]);
    const vertex& vtxb = vpool.GetVertex(poly.miVertices[1]);
    const vertex& vtxc = vpool.GetVertex(poly.miVertices[2]);
    XgmClusterTri tri{vtxa, vtxb, vtxc};
    clusterizer.addTriangle(tri, meshflags);
  }
  clusterizer.End();
  //////////////////////////////////////////////////////////////
  // build primitives
  //////////////////////////////////////////////////////////////
  fromClusterizer(clusterizer, context);
}
////////////////////////////////////////////////////////////////////////////////
template <typename vtx_t> void RigidPrimitive<vtx_t>::draw(lev2::Context* context) const {
  auto gbi = context->GBI();
  for (auto cluster : _gpuClusters) {
    for (auto primgroup : cluster->_primgroups) {
      gbi->DrawIndexedPrimitiveEML(
          *cluster->_vtxbuffer.get(), //
          *primgroup->_idxbuffer.get(),
          primgroup->_primtype);
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
