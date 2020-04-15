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
    AABox _aabb;
    // Sphere _sphere;
  };

  using primgroupcluster_ptr_t = std::shared_ptr<PrimGroupCluster>;
  using cluster_ptr_list_t     = std::vector<primgroupcluster_ptr_t>;

  RigidPrimitive();

  void fromSubMesh(const submesh& submesh, lev2::Context* context); /// generate from submesh using internal vertexbuffer
  void fromClusterizer(const XgmClusterizerStd& cluz, lev2::Context* context);
  void renderEML(lev2::Context* context) const; /// draw with context

  void writeToChunks(const lev2::XgmSubMesh& xsubmesh, chunkfile::OutputStream* hdrstream, chunkfile::OutputStream* geostream);
  void gpuLoadFromChunks(lev2::Context* context, chunkfile::InputStream* hdrstream, chunkfile::InputStream* geostream);

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
    auto xgmcluster = std::make_shared<lev2::XgmCluster>();
    buildTriStripXgmCluster(*context, xgmcluster, clusterbuilder);
    primgroupcluster_ptr_t out_cluster = std::make_shared<PrimGroupCluster>();
    out_cluster->_vtxbuffer            = std::dynamic_pointer_cast<vtxbuf_t>(clusterbuilder->_vertexBuffer);
    out_cluster->_aabb                 = xgmcluster->mBoundingBox;
    // out_cluster->_sphere               = xgmcluster->mBoundingSphere;
    for (size_t ipg = 0; ipg < xgmcluster->numPrimGroups(); ipg++) {
      auto src_PG         = xgmcluster->primgroup(ipg);
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
    const auto& poly = submeshTris.RefPoly(p);
    auto vtxa        = poly._vertices[0];
    auto vtxb        = poly._vertices[1];
    auto vtxc        = poly._vertices[2];
    XgmClusterTri tri{*vtxa, *vtxb, *vtxc};
    clusterizer.addTriangle(tri, meshflags);
  }
  clusterizer.End();
  //////////////////////////////////////////////////////////////
  // build primitives
  //////////////////////////////////////////////////////////////
  fromClusterizer(clusterizer, context);
}
////////////////////////////////////////////////////////////////////////////////
template <typename vtx_t>
void RigidPrimitive<vtx_t>::writeToChunks(
    const lev2::XgmSubMesh& xsubmesh,
    chunkfile::OutputStream* hdrstream, //
    chunkfile::OutputStream* geostream) {
  lev2::ContextDummy DummyTarget;
  size_t inumclus = xsubmesh._clusters.size();
  hdrstream->AddItem<size_t>(inumclus);
  for (size_t icluster = 0; icluster < inumclus; icluster++) {
    auto cluster = xsubmesh._clusters[icluster];
    // XgmCluster xgmcluster;
    // buildTriStripXgmCluster(DummyTarget, xgmcluster, clusterbuilder);
    ////////////////////////////////////////////////////////////////
    hdrstream->AddItem<size_t>("begin-sector-lod"_crcu);
    hdrstream->AddItem<size_t>(icluster);
    // printf("write icluster<%zu>\n", icluster);
    hdrstream->AddItem<fvec3>(cluster->mBoundingBox.Min());
    hdrstream->AddItem<fvec3>(cluster->mBoundingBox.Max());
    ////////////////////////////////////////////////////////////////
    auto VB                 = cluster->_vertexBuffer;
    size_t numverts         = VB->GetNumVertices();
    size_t vtxsize          = VB->GetVtxSize();
    size_t vertexdatalen    = numverts * vtxsize;
    size_t vertexdataoffset = geostream->GetSize();
    auto vertexdata         = DummyTarget.GBI()->LockVB(*VB);
    OrkAssert(vertexdata != nullptr);
    hdrstream->AddItem<lev2::EVtxStreamFormat>(vtx_t::meFormat);
    hdrstream->AddItem<size_t>(numverts);
    hdrstream->AddItem<size_t>(vtxsize);
    hdrstream->AddItem<size_t>(vertexdatalen);
    hdrstream->AddItem<size_t>(vertexdataoffset);
    geostream->Write((const uint8_t*)vertexdata, vertexdatalen);
    DummyTarget.GBI()->UnLockVB(*VB);
    ////////////////////////////////////////////////////////////////
    hdrstream->AddItem<size_t>(cluster->_primgroups.size());
    for (size_t ipg = 0; ipg < cluster->_primgroups.size(); ipg++) {
      auto PG           = cluster->_primgroups[ipg];
      size_t ibufoffset = geostream->GetSize();
      size_t numindices = PG->mpIndices->GetNumIndices();
      auto indexdata    = DummyTarget.GBI()->LockIB(*PG->mpIndices);
      OrkAssert(indexdata != nullptr);
      hdrstream->AddItem<size_t>(ipg);
      hdrstream->AddItem<lev2::EPrimitiveType>(PG->mePrimType);
      hdrstream->AddItem<size_t>(numindices);
      hdrstream->AddItem<size_t>(ibufoffset);
      geostream->Write(
          (const uint8_t*)indexdata, //
          numindices * sizeof(uint16_t));
      DummyTarget.GBI()->UnLockIB(*PG->mpIndices);
    }
    ////////////////////////////////////////////////////////////////
    hdrstream->AddItem<size_t>("end-sector-lod"_crcu);
  }
}
template <typename vtx_t>
void RigidPrimitive<vtx_t>::gpuLoadFromChunks(
    lev2::Context* context,
    chunkfile::InputStream* hdrstream,
    chunkfile::InputStream* geostream) {
  size_t num_clusters     = 0;
  size_t begin_lod_marker = 0;
  size_t end_lod_marker   = 0;
  size_t check_cluster    = 0;
  fvec3 bbmin, bbmax;
  lev2::EVtxStreamFormat streamfmt;
  size_t numverts         = 0;
  size_t vtxsize          = 0;
  size_t vertexdatalen    = 0;
  size_t vertexdataoffset = 0;
  size_t numprimgroups    = 0;
  size_t check_pgindex    = 0;
  lev2::EPrimitiveType primtype;
  size_t numindices      = 0;
  size_t indexdataoffset = 0;
  ////////////////////////////////////////////////////////////////
  hdrstream->GetItem<size_t>(num_clusters);
  for (size_t icluster = 0; icluster < num_clusters; icluster++) {

    auto gpu_cluster = std::make_shared<PrimGroupCluster>();
    _gpuClusters.push_back(gpu_cluster);

    hdrstream->GetItem<size_t>(begin_lod_marker);
    OrkAssert(begin_lod_marker == "begin-sector-lod"_crcu);
    hdrstream->GetItem<size_t>(check_cluster);
    hdrstream->GetItem<fvec3>(bbmin);
    hdrstream->GetItem<fvec3>(bbmax);
    hdrstream->GetItem<lev2::EVtxStreamFormat>(streamfmt);
    hdrstream->GetItem<size_t>(numverts);
    hdrstream->GetItem<size_t>(vtxsize);
    hdrstream->GetItem<size_t>(vertexdatalen);
    hdrstream->GetItem<size_t>(vertexdataoffset);
    hdrstream->GetItem<size_t>(numprimgroups);

    auto vertexbufferdata = (const void*)geostream->GetDataAt(vertexdataoffset);

    auto VB            = lev2::VertexBufferBase::CreateVertexBuffer(streamfmt, numverts, true);
    auto gpuvtxpointer = (void*)context->GBI()->LockVB(*VB.get(), 0, numverts);
    memcpy(gpuvtxpointer, vertexbufferdata, vertexdatalen);
    context->GBI()->UnLockVB(*VB.get());

    gpu_cluster->_vtxbuffer = std::dynamic_pointer_cast<vtxbuf_t>(VB);

    for (size_t ipg = 0; ipg < numprimgroups; ipg++) {
      hdrstream->GetItem<size_t>(check_pgindex);
      OrkAssert(ipg == check_pgindex);
      hdrstream->GetItem<lev2::EPrimitiveType>(primtype);
      hdrstream->GetItem<size_t>(numindices);
      hdrstream->GetItem<size_t>(indexdataoffset);
      auto indexbufferdata = (const uint16_t*)geostream->GetDataAt(indexdataoffset);

      auto gpu_prim = std::make_shared<PrimitiveGroup>();
      gpu_cluster->_primgroups.push_back(gpu_prim);

      gpu_prim->_primtype  = primtype;
      gpu_prim->_idxbuffer = std::make_shared<idxbuf_t>(numindices);
      auto gpuindexptr     = (void*)context->GBI()->LockIB(*gpu_prim->_idxbuffer.get());
      memcpy(gpuindexptr, indexbufferdata, numindices * sizeof(uint16_t));
      context->GBI()->UnLockIB(*gpu_prim->_idxbuffer.get());
    }
    hdrstream->GetItem<size_t>(end_lod_marker);
    OrkAssert(end_lod_marker == "end-sector-lod"_crcu);
  }
}
////////////////////////////////////////////////////////////////////////////////
template <typename vtx_t> void RigidPrimitive<vtx_t>::renderEML(lev2::Context* context) const {
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
