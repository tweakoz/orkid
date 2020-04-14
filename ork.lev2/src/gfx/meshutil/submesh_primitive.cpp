////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/orklut.hpp>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/meshutil/clusterizer.h>
#include <ork/lev2/gfx/gfxenv_enum.h>

namespace ork::meshutil {

RigidPrimitive::RigidPrimitive() {
}

////////////////////////////////////////////////////////////////////////////////

void RigidPrimitive::fromClusterizer(const meshutil::XgmClusterizerStd& cluz, lev2::Context* context) {
  //////////////////////////////////////////////////////////////
  // create Indexed TriStripped Primitive Groups
  //////////////////////////////////////////////////////////////
  size_t inumclus = cluz.GetNumClusters();
  printf("inumclus<%zu>\n", inumclus);
  OrkAssert(inumclus <= 1);
  for (size_t icluster = 0; icluster < inumclus; icluster++) {
    auto clusterbuilder = cluz.GetCluster(icluster);
    clusterbuilder->buildVertexBuffer(*context, lev2::EVtxStreamFormat::V12N12B12T8C4);
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

void RigidPrimitive::fromSubMesh(const submesh& submesh, lev2::Context* context) {
  //////////////////////////////////////////////////////////////
  // Fill In ClusterBuilder from submesh triangle soup
  //////////////////////////////////////////////////////////////
  meshutil::XgmClusterizerStd clusterizer;
  meshutil::MeshConfigurationFlags meshflags;
  const auto& vpool = submesh.RefVertexPool();
  int numverts      = vpool.GetNumVertices();
  int inumpolys     = submesh.GetNumPolys(3);
  clusterizer.Begin();
  for (int p = 0; p < inumpolys; p++) {
    const auto& poly   = submesh.RefPoly(p);
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

void RigidPrimitive::draw(lev2::Context* context) const {
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

} // namespace ork::meshutil
