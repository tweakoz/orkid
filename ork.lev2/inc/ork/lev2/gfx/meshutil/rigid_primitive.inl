////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
#include <ork/kernel/datablock.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/meshutil/clusterizer.h>
#include <ork/lev2/gfx/scenegraph/scenegraph.h>

namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////
typedef orkmap<std::string, svar64_t> AnnotationMap;
struct XgmClusterizer;
struct XgmClusterizerDiced;
struct XgmClusterizerStd;
///////////////////////////////////////////////////////////////////////////////
/// RigidPrimitive SubMesh Primitive with V12N12B12T8C4 vertex format
///////////////////////////////////////////////////////////////////////////////

struct RigidPrimitiveBase {
  using idxbuf_t     = lev2::StaticIndexBuffer<uint16_t>;
  using idxbuf_ptr_t = std::shared_ptr<idxbuf_t>;

  struct PrimitiveGroup {
    idxbuf_ptr_t _idxbuffer;
    lev2::PrimitiveType _primtype = lev2::PrimitiveType::END;
  };

  using primgroup_ptr_t      = std::shared_ptr<PrimitiveGroup>;
  using primgroup_ptr_list_t = std::vector<primgroup_ptr_t>;

  virtual ~RigidPrimitiveBase() {}
  virtual lev2::callback_drawable_ptr_t createDrawable(lev2::fxpipeline_ptr_t pipeline) = 0;
  virtual lev2::callback_drawable_ptr_t createDrawable(lev2::material_ptr_t material) = 0;
  inline lev2::scenegraph::drawable_node_ptr_t createNode(
      std::string named, //
      lev2::scenegraph::layer_ptr_t layer,
      lev2::fxpipeline_ptr_t pipeline) {
    auto drw = createDrawable(pipeline);
    return layer->createDrawableNode(named, drw);
  }
  inline lev2::scenegraph::drawable_node_ptr_t createNode(
      std::string named, //
      lev2::scenegraph::layer_ptr_t layer,
      lev2::material_ptr_t material) {
    auto drw = createDrawable(material);
    return layer->createDrawableNode(named, drw);
  }

  lev2::fxpipeline_ptr_t _pipeline;
  lev2::material_ptr_t _material;
};

using rigidprimitive_ptr_t = std::shared_ptr<RigidPrimitiveBase>;

///////////////////////////////////////////////////////////////////////////////

struct RigidPrimitiveDrawableData : public lev2::DrawableData {

  RigidPrimitiveDrawableData();
  lev2::drawable_ptr_t createDrawable() const final;
  rigidprimitive_ptr_t _primitive;
  lev2::fxpipeline_ptr_t _pipeline;
  lev2::material_ptr_t _material;
};

using rigidprimitive_drawdata_ptr_t = std::shared_ptr<RigidPrimitiveDrawableData>;

inline RigidPrimitiveDrawableData::RigidPrimitiveDrawableData() {
}
inline lev2::drawable_ptr_t RigidPrimitiveDrawableData::createDrawable() const {
    if(_pipeline){
        return _primitive->createDrawable(_pipeline);
    }
    else if(_material){
        return _primitive->createDrawable(_material);
    }
    OrkAssert(false);
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

template <typename vtx_t> struct RigidPrimitive : public RigidPrimitiveBase {

  using vtxbuf_t      = lev2::StaticVertexBuffer<vtx_t>;
  using vtxbuf_ptr_t  = std::shared_ptr<vtxbuf_t>;
  using vtxbuf_list_t = std::vector<vtxbuf_t>;

  ////////////////////////

  struct PrimGroupCluster {
    vtxbuf_ptr_t _vtxbuffer;
    primgroup_ptr_list_t _primgroups;
    AABox _aabb;
    // Sphere _sphere;
  };

  using primgroupcluster_ptr_t = std::shared_ptr<PrimGroupCluster>;
  using cluster_ptr_list_t     = std::vector<primgroupcluster_ptr_t>;

  ////////////////////////

  RigidPrimitive();

  void fromSubMesh(const submesh& submesh, lev2::Context* context); /// generate from submesh using internal vertexbuffer
  void toClusterizer(const submesh& inp_submesh, meshutil::XgmClusterizerStd& clusterizer_out);

  void fromClusterizer(const XgmClusterizerStd& cluz, lev2::Context* context);
  void renderEML(lev2::Context* context) const; /// draw with context

  void renderUnitOrthoWithMaterial(lev2::Context* context, const SRect& vprect, lev2::GfxMaterial* pmat) const;

  void writeToChunks(const lev2::XgmSubMesh& xsubmesh, chunkfile::OutputStream* hdrstream, chunkfile::OutputStream* geostream);
  void clusterizerToChunks(
      XgmClusterizerStd& clusterizer,
      chunkfile::OutputStream* hdrstream, //
      chunkfile::OutputStream* geostream);

  void gpuLoadFromChunks(lev2::Context* context, chunkfile::InputStream* hdrstream, chunkfile::InputStream* geostream);
  void gpuLoadFromChunksA(lev2::Context* context, chunkfile::InputStream* hdrstream, chunkfile::InputStream* geostream);

  //////////////////////////////////////////////////////////////////////////////

  template <typename... A>                                                                                           //
  static lev2::callback_drawable_ptr_t makeDrawableAndPrimitive(lev2::fxpipeline_ptr_t pipeline, A&&... prim_args) { //
    auto prim                          = std::make_shared<RigidPrimitive>(std::forward<A>(prim_args)...);
    auto drw                           = prim->createDrawable(pipeline);
    drw->_properties["primitive"_crcu] = prim;
    return drw;
  }

  //////////////////////////////////////////////////////////////////////////////

  lev2::callback_drawable_ptr_t createDrawable(lev2::fxpipeline_ptr_t pipeline) final {

    OrkAssert(pipeline != nullptr);
    OrkAssert(pipeline->_technique != nullptr);

    _pipeline = pipeline;

    auto drw = std::make_shared<lev2::CallbackDrawable>(nullptr);
    drw->SetRenderCallback([=](lev2::RenderContextInstData& RCID) { //
      auto context = RCID.context();
      pipeline->wrappedDrawCall(
          RCID,                       //
          [this, context]() {         //
            this->renderEML(context); //
          });
    });
    return drw;
  }

  //////////////////////////////////////////////////////////////////////////////

  lev2::callback_drawable_ptr_t createDrawable(lev2::material_ptr_t material) final {

    OrkAssert(material != nullptr);
    //OrkAssert(material->_technique != nullptr);

    //_pipeline = pipeline;

    auto drw = std::make_shared<lev2::CallbackDrawable>(nullptr);


    drw->SetRenderCallback([=](lev2::RenderContextInstData& RCID) { //
      auto context = RCID.context();
      auto RCFD = RCID.rcfd();

      lev2::FxPipelinePermutation permu;
      permu._stereo = false;
      permu._instanced = false;
      permu._skinned = false;
      permu._is_picking = false;
      permu._has_vtxcolors = true;
      permu._rendering_model = RCFD->_renderingmodel._modelID;

      auto fxcache = material->pipelineCache();


      auto pipeline = fxcache->findPipeline(permu);
      OrkAssert(pipeline != nullptr);
      pipeline->wrappedDrawCall(
          RCID,                       //
          [this, context]() {         //
            this->renderEML(context); //
          });
    });
    return drw;
  }

  //////////////////////////////////////////////////////////////////////////////

  cluster_ptr_list_t _gpuClusters;
};
///////////////////////////////////////////////////////////////////////////////
template <typename vtx_t> RigidPrimitive<vtx_t>::RigidPrimitive() {
}
////////////////////////////////////////////////////////////////////////////////
template <typename vtx_t>
void RigidPrimitive<vtx_t>::fromClusterizer(const meshutil::XgmClusterizerStd& cluz, lev2::Context* context) {
  auto GBI = context->GBI();
  for (auto c : _gpuClusters) {
    auto vbuf = c->_vtxbuffer;
    GBI->ReleaseVB(*vbuf);
    for (auto pg : c->_primgroups) {
      auto ibuf = pg->_idxbuffer;
      GBI->ReleaseIB(*ibuf);
    }
  }
  _gpuClusters.clear();
  //////////////////////////////////////////////////////////////
  // create Indexed TriStripped Primitive Groups
  //////////////////////////////////////////////////////////////
  size_t inumclus = cluz.GetNumClusters();
  // printf("inumclus<%zu>\n", inumclus);
  // OrkAssert(inumclus <= 1);
  for (size_t icluster = 0; icluster < inumclus; icluster++) {
    auto clusterbuilder = cluz.GetCluster(icluster);
    clusterbuilder->buildVertexBuffer(*context, vtx_t::meFormat);
    auto xgmcluster = std::make_shared<lev2::XgmCluster>();
    buildXgmCluster(*context, xgmcluster, clusterbuilder, true);
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
template <typename vtx_t>
void RigidPrimitive<vtx_t>::toClusterizer(const submesh& inp_submesh, meshutil::XgmClusterizerStd& clusterizer_out) {
  submesh submeshTris;
  submeshTriangulate(inp_submesh, submeshTris);
  //////////////////////////////////////////////////////////////
  // Fill In ClusterBuilder from submesh triangle soup
  //////////////////////////////////////////////////////////////
  meshutil::MeshConfigurationFlags meshflags;
  int numverts  = submeshTris.numVertices();
  int inumpolys = submeshTris.numPolys(3);
  clusterizer_out.Begin();
  submeshTris.visitAllPolys([&](poly_const_ptr_t ply) {
    auto vtxa = ply->vertex(0);
    auto vtxb = ply->vertex(1);
    auto vtxc = ply->vertex(2);
    if (false) { // vtxa->mNrm.magnitude()==0){
      // TODO - fixme
      vtxa->mNrm                = vtxb->mNrm;
      vtxa->mUV[0].mMapBiNormal = vtxb->mUV[0].mMapBiNormal;
    }
    // printf( "vna<%g %g %g>\n", vtxa->mNrm.x, vtxa->mNrm.y, vtxa->mNrm.z );
    // printf( "vnb<%g %g %g>\n", vtxb->mNrm.x, vtxb->mNrm.y, vtxb->mNrm.z );
    // printf( "vnc<%g %g %g>\n", vtxc->mNrm.x, vtxc->mNrm.y, vtxc->mNrm.z );
    XgmClusterTri tri{*vtxa, *vtxb, *vtxc};
    clusterizer_out.addTriangle(tri, meshflags);
  });
  clusterizer_out.End();
}
////////////////////////////////////////////////////////////////////////////////
template <typename vtx_t> void RigidPrimitive<vtx_t>::fromSubMesh(const submesh& inp_submesh, lev2::Context* context) {
  meshutil::XgmClusterizerStd clusterizer;
  toClusterizer(inp_submesh, clusterizer);
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
      hdrstream->AddItem<lev2::PrimitiveType>(PG->mePrimType);
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
////////////////////////////////////////////////////////////////////////////////
template <typename vtx_t>
void RigidPrimitive<vtx_t>::clusterizerToChunks(
    XgmClusterizerStd& clusterizer,
    chunkfile::OutputStream* hdrstream, //
    chunkfile::OutputStream* geostream) {

  printf("GEOSIZE<%zu>\n", geostream->GetSize());

  size_t inumclus = clusterizer.GetNumClusters();
  hdrstream->AddItem<size_t>(inumclus);
  lev2::ContextDummy DummyTarget;

  for (size_t icluster = 0; icluster < inumclus; icluster++) {

    hdrstream->AddItem<size_t>("begin-cluster"_crcu);
    hdrstream->AddItem<size_t>(icluster);

    auto clusterbuilder = clusterizer.GetCluster(icluster);
    clusterbuilder->buildVertexBuffer(DummyTarget, vtx_t::meFormat);
    auto xgmcluster = std::make_shared<lev2::XgmCluster>();
    buildXgmCluster(DummyTarget, xgmcluster, clusterbuilder, true);
    primgroupcluster_ptr_t out_cluster = std::make_shared<PrimGroupCluster>();
    out_cluster->_vtxbuffer            = std::dynamic_pointer_cast<vtxbuf_t>(clusterbuilder->_vertexBuffer);
    auto VB                            = out_cluster->_vtxbuffer;
    size_t numverts                    = VB->GetNumVertices();
    size_t vtxsize                     = VB->GetVtxSize();
    size_t vertexdatalen               = numverts * vtxsize;
    size_t vertexdataoffset            = geostream->GetSize();
    auto vertexdata                    = DummyTarget.GBI()->LockVB(*VB);
    OrkAssert(vertexdata != nullptr);

    // printf( "vertexdataoffset<%zu>\n", vertexdataoffset );

    hdrstream->AddItem<lev2::EVtxStreamFormat>(vtx_t::meFormat);
    hdrstream->AddItem<size_t>(numverts);
    hdrstream->AddItem<size_t>(vtxsize);
    hdrstream->AddItem<size_t>(vertexdatalen);
    hdrstream->AddItem<size_t>(vertexdataoffset);
    geostream->Write((const uint8_t*)vertexdata, vertexdatalen);
    DummyTarget.GBI()->UnLockVB(*VB);

    out_cluster->_aabb = xgmcluster->mBoundingBox;

    size_t num_primgroups = xgmcluster->numPrimGroups();
    hdrstream->AddItem<size_t>(num_primgroups);
    for (size_t ipg = 0; ipg < num_primgroups; ipg++) {
      auto src_PG = xgmcluster->primgroup(ipg);

      hdrstream->AddItem<size_t>(ipg);
      hdrstream->AddItem<lev2::PrimitiveType>(src_PG->mePrimType);

      auto IB = src_PG->mpIndices;

      size_t numindices = src_PG->GetNumIndices();
      size_t ibufoffset = geostream->GetSize();

      hdrstream->AddItem<size_t>(numindices);
      hdrstream->AddItem<size_t>(ibufoffset);

      auto src_indexdata = (const uint16_t*)DummyTarget.GBI()->LockIB(*src_PG->GetIndexBuffer());
      geostream->Write(
          (const uint8_t*)src_indexdata, //
          numindices * sizeof(uint16_t));

      DummyTarget.GBI()->UnLockIB(*src_PG->GetIndexBuffer());
    }
    ////////////////////////////////////////////////////////////////
    hdrstream->AddItem<size_t>("end-cluster"_crcu);
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
  lev2::PrimitiveType primtype;
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
      hdrstream->GetItem<lev2::PrimitiveType>(primtype);
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
template <typename vtx_t>
void RigidPrimitive<vtx_t>::gpuLoadFromChunksA(
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
  lev2::PrimitiveType primtype;
  size_t numindices      = 0;
  size_t indexdataoffset = 0;
  ////////////////////////////////////////////////////////////////
  hdrstream->GetItem<size_t>(num_clusters);
  // printf( "num_clusters<%zu>\n", num_clusters );
  for (size_t icluster = 0; icluster < num_clusters; icluster++) {

    auto gpu_cluster = std::make_shared<PrimGroupCluster>();
    _gpuClusters.push_back(gpu_cluster);
    hdrstream->GetItem<size_t>(begin_lod_marker);
    OrkAssert(begin_lod_marker == "begin-cluster"_crcu);
    hdrstream->GetItem<size_t>(check_cluster);
    // hdrstream->GetItem<fvec3>(bbmin);
    // hdrstream->GetItem<fvec3>(bbmax);
    hdrstream->GetItem<lev2::EVtxStreamFormat>(streamfmt);
    hdrstream->GetItem<size_t>(numverts);
    hdrstream->GetItem<size_t>(vtxsize);
    hdrstream->GetItem<size_t>(vertexdatalen);
    hdrstream->GetItem<size_t>(vertexdataoffset);
    hdrstream->GetItem<size_t>(numprimgroups);
    // printf( "fmt<%zu> numverts<%zu> vtxsize<%zu> vertexdatalen<%zu> vertexdataoffset<%zu> numprimgroups<%zu>\n",
    //       size_t(streamfmt), numverts, vtxsize, vertexdatalen, vertexdataoffset, numprimgroups );
    auto vertexbufferdata = (const void*)geostream->GetDataAt(vertexdataoffset);

    auto VB            = lev2::VertexBufferBase::CreateVertexBuffer(streamfmt, numverts, true);
    auto gpuvtxpointer = (void*)context->GBI()->LockVB(*VB.get(), 0, numverts);
    memcpy(gpuvtxpointer, vertexbufferdata, vertexdatalen);
    context->GBI()->UnLockVB(*VB.get());

    gpu_cluster->_vtxbuffer = std::dynamic_pointer_cast<vtxbuf_t>(VB);

    for (size_t ipg = 0; ipg < numprimgroups; ipg++) {
      hdrstream->GetItem<size_t>(check_pgindex);
      OrkAssert(ipg == check_pgindex);
      hdrstream->GetItem<lev2::PrimitiveType>(primtype);
      hdrstream->GetItem<size_t>(numindices);
      hdrstream->GetItem<size_t>(indexdataoffset);

      // printf( "ipg<%zu> primtype<%d> numindices<%zu> indexdataoffset<%zu>\n",
      //       ipg, int(primtype), numindices, indexdataoffset );

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
    OrkAssert(end_lod_marker == "end-cluster"_crcu);
  }
}
////////////////////////////////////////////////////////////////////////////////
template <typename vtx_t> void RigidPrimitive<vtx_t>::renderEML(lev2::Context* context) const {
  auto gbi = context->GBI();
  for (auto& cluster : _gpuClusters) {
    for (auto& primgroup : cluster->_primgroups) {
      gbi->DrawIndexedPrimitiveEML(
          *cluster->_vtxbuffer.get(), //
          *primgroup->_idxbuffer.get(),
          primgroup->_primtype);
    }
  }
}
////////////////////////////////////////////////////////////////////////////////
template <typename vtx_t>
void RigidPrimitive<vtx_t>::renderUnitOrthoWithMaterial(lev2::Context* context, const SRect& vprect, lev2::GfxMaterial* pmat)
    const {
  lev2::SRasterState DefaultRasterState;
  auto mtxi = context->MTXI();
  auto fbi  = context->FBI();
  auto gbi  = context->GBI();

  lev2::ViewportRect vprectNew(vprect.miX, vprect.miY, vprect.miX2 - vprect.miX, vprect.miY2 - vprect.miY);

  mtxi->PushPMatrix(fmtx4::Identity());
  mtxi->PushVMatrix(fmtx4::Identity());
  mtxi->PushMMatrix(fmtx4::Identity());
  context->RSI()->BindRasterState(DefaultRasterState, true);
  fbi->pushViewport(vprectNew);
  fbi->pushScissor(vprectNew);
  { // Draw primitive with specified material
    int inumpasses = pmat->BeginBlock(context);
    for (int ipass = 0; ipass < inumpasses; ipass++) {
      bool bDRAW = pmat->BeginPass(context, ipass);
      for (auto& cluster : _gpuClusters) {
        for (auto& primgroup : cluster->_primgroups) {
          gbi->DrawIndexedPrimitiveEML(
              *cluster->_vtxbuffer.get(), //
              *primgroup->_idxbuffer.get(),
              primgroup->_primtype);
        }
      }
      pmat->EndPass(context);
    }
    pmat->EndBlock(context);
  }
  fbi->popScissor();
  fbi->popViewport();
  mtxi->PopPMatrix();
  mtxi->PopVMatrix();
  mtxi->PopMMatrix();
}
///////////////////////////////////////////////////////////////////////////////
using rigidprim_V12_t               = RigidPrimitive<lev2::VtxV12>;
using rigidprim_V12T8_t             = RigidPrimitive<lev2::VtxV12T8>;
using rigidprim_V12C4T16_t          = RigidPrimitive<lev2::SVtxV12C4T16>;
using rigidprim_SVtxV12N12T16_t     = RigidPrimitive<lev2::SVtxV12N12T16>;
using rigidprim_V12_ptr_t           = std::shared_ptr<rigidprim_V12_t>;
using rigidprim_V12T8_ptr_t         = std::shared_ptr<rigidprim_V12T8_t>;
using rigidprim_V12C4T16_ptr_t      = std::shared_ptr<rigidprim_V12C4T16_t>;
using rigidprim_SVtxV12N12T16_ptr_t = std::shared_ptr<rigidprim_SVtxV12N12T16_t>;
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
