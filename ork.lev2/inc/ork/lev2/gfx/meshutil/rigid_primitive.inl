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
#include <ork/lev2/gfx/renderer/cull_debug.h> // ORKID_DISABLE_FRUSTUM_CULL debug lever
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/material_pbr.inl>
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
  using idxbuf_t     = lev2::StaticIndexBuffer<uint32_t>;
  using idxbuf_ptr_t = lev2::idxbufferbase_ptr_t; //std::shared_ptr<>;

  struct IPrimitiveGroup {};

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
  bool _stateDebugger = false;
};

using rigidprimitive_ptr_t = std::shared_ptr<RigidPrimitiveBase>;

///////////////////////////////////////////////////////////////////////////////

struct RigidPrimitiveDrawableData : public lev2::DrawableData {
  // Reflected only so the polymorphic SceneGraphNodeItemData::_drawabledata
  // slot serializes the concrete class name (otherwise the serializer
  // walks up to the abstract DrawableData base and the JSON can't load
  // back). None of the fields below round-trip — _primitive carries
  // GPU buffers, _pipeline is per-context, _material is also runtime —
  // so describeX is empty and the post-load step must rebuild the
  // primitive + material before render.
  DeclareConcreteX(RigidPrimitiveDrawableData, lev2::DrawableData);

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
  void renderInstancedEML(lev2::Context* context, size_t instance_count) const; /// draw instanced

  // GPU-driven instanced draw: instanceCount comes from `indirect_args` (a compute-written SSBO
  // holding a VkDrawIndexedIndirectCommand) rather than the CPU. args_stride advances the command
  // per primgroup (0 = one shared command, the single-primgroup cones/spheres case).
  void renderInstancedIndirectEML(
      lev2::Context* context,
      const lev2::FxShaderStorageBuffer* indirect_args,
      size_t args_stride = 0) const;

  // single-instance bound (LOCAL space) — union of the cluster AABBs + the enclosing sphere.
  // computed lazily from _gpuClusters; the instanced cull transforms it per instance by the
  // instance matrix (remember to scale the radius by the instance's max-axis scale).
  const AABox& localAABB()   const { _ensureBounds(); return _bound_aabb; }
  const fvec3& boundCenter() const { _ensureBounds(); return _bound_center; }
  float        boundRadius() const { _ensureBounds(); return _bound_radius; }

  // index count of the FIRST primgroup — seeds the indirect command's indexCount (the cull only
  // writes instanceCount). Assumes a single-primgroup mesh (cones/spheres); multi-primgroup meshes
  // would need one command per primgroup (see renderInstancedIndirectEML's args_stride).
  size_t indexCountFirstPrimGroup() const {
    for (auto& cluster : _gpuClusters)
      for (auto& primgroup : cluster->_primgroups)
        return primgroup->_idxbuffer->GetNumIndices();
    return 0;
  }

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
    if(0)printf("DEBUG: Pipeline<%p> technique<%p>\n", (void*)pipeline.get(), (void*)pipeline->_technique);
    if(pipeline->_technique==nullptr){
      printf( "Bad Pipeline! Pipeline exists but technique is null\n");
      pipeline->dump();
      OrkAssert(false);
    }

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
  void installInCallbackDrawable(lev2::callback_drawable_wkptr_t drw, lev2::material_ptr_t material){
    OrkAssert(material != nullptr);
    bool is_alpha = false;
    if (auto as_pbr = std::dynamic_pointer_cast<lev2::PBRMaterial>(material)) {
      is_alpha = as_pbr->_alphaBlend;
    }
    drw.lock()->SetRenderCallback([this,material,is_alpha](lev2::RenderContextInstData& RCID) { //
      auto context = RCID.context();
      auto RCFD = RCID.rcfd();
      lev2::FxPipelinePermutation permu;
      // single-pass stereo: hand-built permutation, so the stereo bit must be read off the
      // active CPD the way FxPipelineCache::findPipeline(RCID) does. Pinned false, every
      // drawable on this path takes the _MO technique inside a stereo pass and both eye
      // layers receive the SAME image — zero parallax, no validation error, no crash.
      permu._stereo = (RCFD and RCFD->hasCPD()) ? RCFD->topCPD().isSinglePassStereo() : false;
      permu._instanced = false;
      permu._skinned = false;
      permu._is_picking = false;
      permu._has_vtxcolors = true;
      permu._is_alpha = is_alpha;
      // cloud-shadow fill: this permutation is hand-built, so it must pick the flag up off the
      // active CPD the way FxPipelineCache::findPipeline(RCID) does (same shape as the cascade
      // shadow-pass read in the instanced primitive below) — the cloud decks draw through THIS
      // path, and without it they take the full forward technique into the cookie pass.
      permu._is_sun_cookie = RCFD ? RCFD->topCPD()._sunCookiePass : false;
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
  }
  //////////////////////////////////////////////////////////////////////////////
  void installInCallbackDrawable(lev2::callback_drawable_wkptr_t drw, lev2::fxpipeline_ptr_t pipeline){
    OrkAssert(pipeline != nullptr);
    drw.lock()->SetRenderCallback([this,pipeline](lev2::RenderContextInstData& RCID) { //
      auto context = RCID.context();
      pipeline->wrappedDrawCall(
          RCID,                       //
          [this, context]() {         //
            this->renderEML(context); //
          });
    });
  }
  //////////////////////////////////////////////////////////////////////////////

  lev2::callback_drawable_ptr_t createDrawable(lev2::material_ptr_t material) final {
    auto drw = std::make_shared<lev2::CallbackDrawable>(nullptr);
    installInCallbackDrawable(drw, material);
    return drw;
  }

  //////////////////////////////////////////////////////////////////////////////

  cluster_ptr_list_t _gpuClusters;

  // union the per-cluster AABBs into one local-space box, then derive the enclosing sphere
  // (center = box center, radius = half the diagonal). Lazy: all three cluster-build paths
  // (fromSubMesh / fromClusterizer / gpuLoadFromChunks) just push clusters; bounds resolve on
  // first query. _gpuClusters is fixed after build, so a one-shot compute is safe.
  void _ensureBounds() const {
    if (_bound_valid)
      return;
    AABox box;
    box.BeginGrow();
    for (auto& c : _gpuClusters) {
      box.Grow(c->_aabb.Min());
      box.Grow(c->_aabb.Max());
    }
    box.EndGrow();
    _bound_aabb   = box;
    _bound_center = box.center();
    _bound_radius = (box.Max() - box.center()).length(); // half-diagonal encloses the box
    _bound_valid  = true;
  }
  mutable AABox _bound_aabb;
  mutable fvec3 _bound_center = fvec3(0, 0, 0);
  mutable float _bound_radius = 0.0f;
  mutable bool  _bound_valid  = false;
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
      auto src_index_buffer = src_PG->GetIndexBuffer();
      auto src_indexdata   = (const void*) context->GBI()->LockIB(*src_index_buffer);
      gpu_prim->_idxbuffer = std::make_shared<idxbuf_t>(numindices);
      size_t sizeof_orig_index = src_index_buffer->indexSize();
      auto gpuindexptr     = (void*)context->GBI()->LockIB(*gpu_prim->_idxbuffer.get());
      switch(sizeof_orig_index){
        case 2:{ // upcast to 32 bit indices
          auto src_indexdata16 = (const uint16_t*)src_indexdata;
          for(size_t i=0; i<numindices; i++){
            uint32_t idx = src_indexdata16[i];
            ((uint32_t*)gpuindexptr)[i] = idx;
          }
          break;
        }
        case 4:{
          memcpy(gpuindexptr, src_indexdata, numindices * sizeof_orig_index);
          break;
        }
        default:
          OrkAssert(false);
          break;
      }
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
      size_t indexsize  = PG->mpIndices->indexSize();
      auto indexdata    = DummyTarget.GBI()->LockIB(*PG->mpIndices);
      OrkAssert(indexdata != nullptr);
      hdrstream->AddItem<size_t>(ipg);
      hdrstream->AddItem<lev2::PrimitiveType>(PG->mePrimType);
      hdrstream->AddItem<size_t>(indexsize);
      hdrstream->AddItem<size_t>(numindices);
      hdrstream->AddItem<size_t>(ibufoffset);
      geostream->Write(
          (const uint8_t*)indexdata, //
          numindices * indexsize);
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
      auto idxbuf = src_PG->GetIndexBuffer();

      hdrstream->AddItem<size_t>(ipg);
      hdrstream->AddItem<lev2::PrimitiveType>(src_PG->mePrimType);

      auto IB = src_PG->mpIndices;

      size_t indexsize = idxbuf->indexSize();
      size_t numindices = src_PG->GetNumIndices();
      size_t ibufoffset = geostream->GetSize();

      hdrstream->AddItem<size_t>(indexsize);
      hdrstream->AddItem<size_t>(numindices);
      hdrstream->AddItem<size_t>(ibufoffset);

      auto src_indexdata = (const uint32_t*)DummyTarget.GBI()->LockIB(*src_PG->GetIndexBuffer());
      geostream->Write(
          (const uint8_t*)src_indexdata, //
          numindices * indexsize);

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
  size_t indexsize       = 0;
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
      hdrstream->GetItem<size_t>(indexsize);
      hdrstream->GetItem<size_t>(numindices);
      hdrstream->GetItem<size_t>(indexdataoffset);
      auto indexbufferdata = (const void*) geostream->GetDataAt(indexdataoffset);

      auto gpu_prim = std::make_shared<PrimitiveGroup>();
      gpu_cluster->_primgroups.push_back(gpu_prim);

      gpu_prim->_primtype  = primtype;
      gpu_prim->_idxbuffer = std::make_shared<idxbuf_t>(numindices);
      auto gpuindexptr     = (uint32_t*)context->GBI()->LockIB(*gpu_prim->_idxbuffer.get());
      // idxbuf_t is always uint32_t but the chunk data may be 16-bit — upcast if so.
      // Without this, the trailing half of the dst buffer is left uninitialized
      // (zero-valued after allocation) and draws triangles with stray indices
      // pointing at vertex 0 — hence the "geometry radiating from origin" glitch.
      switch (indexsize) {
        case 2: {
          auto src16 = (const uint16_t*)indexbufferdata;
          for (size_t i = 0; i < numindices; i++) {
            gpuindexptr[i] = uint32_t(src16[i]);
          }
          break;
        }
        case 4:
          memcpy(gpuindexptr, indexbufferdata, numindices * 4);
          break;
        default:
          OrkAssert(false);
          break;
      }
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
  size_t indexsize      = 0;
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
      hdrstream->GetItem<size_t>(indexsize);
      hdrstream->GetItem<size_t>(numindices);
      hdrstream->GetItem<size_t>(indexdataoffset);

      // printf( "ipg<%zu> primtype<%d> numindices<%zu> indexdataoffset<%zu>\n",
      //       ipg, int(primtype), numindices, indexdataoffset );

      auto indexbufferdata = (const void*)geostream->GetDataAt(indexdataoffset);

      auto gpu_prim = std::make_shared<PrimitiveGroup>();
      gpu_cluster->_primgroups.push_back(gpu_prim);

      gpu_prim->_primtype  = primtype;
      gpu_prim->_idxbuffer = std::make_shared<idxbuf_t>(numindices);
      auto gpuindexptr     = (uint32_t*)context->GBI()->LockIB(*gpu_prim->_idxbuffer.get());
      // idxbuf_t is always uint32_t but the chunk data may be 16-bit — upcast if so.
      // See matching fix in gpuLoadFromChunksA / fromClusterizer.
      switch (indexsize) {
        case 2: {
          auto src16 = (const uint16_t*)indexbufferdata;
          for (size_t i = 0; i < numindices; i++) {
            gpuindexptr[i] = uint32_t(src16[i]);
          }
          break;
        }
        case 4:
          memcpy(gpuindexptr, indexbufferdata, numindices * 4);
          break;
        default:
          OrkAssert(false);
          break;
      }
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
      if(_stateDebugger){
        context->stateDebugger();
      }
      gbi->DrawIndexedPrimitiveEML(
          *cluster->_vtxbuffer.get(), //
          *primgroup->_idxbuffer.get(),
          primgroup->_primtype);
    }
  }
}
////////////////////////////////////////////////////////////////////////////////
template <typename vtx_t> void RigidPrimitive<vtx_t>::renderInstancedEML(lev2::Context* context, size_t instance_count) const {
  auto gbi = context->GBI();
  for (auto& cluster : _gpuClusters) {
    for (auto& primgroup : cluster->_primgroups) {
      gbi->DrawInstancedIndexedPrimitiveEML(
          *cluster->_vtxbuffer.get(),
          *primgroup->_idxbuffer.get(),
          primgroup->_primtype,
          instance_count);
    }
  }
}
////////////////////////////////////////////////////////////////////////////////
template <typename vtx_t>
void RigidPrimitive<vtx_t>::renderInstancedIndirectEML(
    lev2::Context* context,
    const lev2::FxShaderStorageBuffer* indirect_args,
    size_t args_stride) const {
  auto gbi          = context->GBI();
  size_t cmd_offset = 0; // byte offset of this primgroup's command within indirect_args
  for (auto& cluster : _gpuClusters) {
    for (auto& primgroup : cluster->_primgroups) {
      gbi->DrawInstancedIndexedPrimitiveIndirectEML(
          *cluster->_vtxbuffer.get(),
          *primgroup->_idxbuffer.get(),
          primgroup->_primtype,
          indirect_args,
          cmd_offset);
      cmd_offset += args_stride; // 0 -> every primgroup reads command 0 (single-command meshes)
    }
  }
}
////////////////////////////////////////////////////////////////////////////////
template <typename vtx_t>
void RigidPrimitive<vtx_t>::renderUnitOrthoWithMaterial(lev2::Context* context, const SRect& vprect, lev2::GfxMaterial* pmat)
    const {
  auto mtxi = context->MTXI();
  auto fbi  = context->FBI();
  auto gbi  = context->GBI();

  lev2::ViewportRect vprectNew(vprect.miX, vprect.miY, vprect.miX2 - vprect.miX, vprect.miY2 - vprect.miY);
  //printf("vprect<%d %d %d %d>\n", vprect.miX, vprect.miY, vprect.miX2, vprect.miY2);
  mtxi->PushPMatrix(fmtx4::Identity());
  mtxi->PushVMatrix(fmtx4::Identity());
  mtxi->PushMMatrix(fmtx4::Identity());
  context->FXI()->applyRasterState(*(pmat->_rasterstate));
  fbi->pushViewport(vprectNew);
  fbi->pushScissor(vprectNew);
  { // Draw primitive with specified material
    int inumpasses = pmat->BeginBlock(context);
    for (auto& cluster : _gpuClusters) {
      for (auto& primgroup : cluster->_primgroups) {
        gbi->DrawIndexedPrimitiveEML(
            *cluster->_vtxbuffer.get(), //
            *primgroup->_idxbuffer.get(),
            primgroup->_primtype);
      }
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
using rigidprim_V12N12T16_t         = RigidPrimitive<lev2::SVtxV12N12T16>;
using rigidprim_V12_ptr_t           = std::shared_ptr<rigidprim_V12_t>;
using rigidprim_V12T8_ptr_t         = std::shared_ptr<rigidprim_V12T8_t>;
using rigidprim_V12C4T16_ptr_t      = std::shared_ptr<rigidprim_V12C4T16_t>;
using rigidprim_V12N12T16_ptr_t     = std::shared_ptr<rigidprim_V12N12T16_t>;
using rigidprim_V12N12B12T8C4_t     = meshutil::RigidPrimitive<lev2::SVtxV12N12B12T8C4>;
using rigidprim_V12N12B12T8C4_ptr_t = std::shared_ptr<rigidprim_V12N12B12T8C4_t>;
///////////////////////////////////////////////////////////////////////////////

template <typename vtx_t>
struct InstancedRigidPrimitiveDrawable final : public lev2::InstancedDrawable {

  InstancedRigidPrimitiveDrawable() = default;

  void bindPrimitive(std::shared_ptr<RigidPrimitive<vtx_t>> prim, lev2::material_ptr_t material) {
    _primitive = prim;
    _material = material;
    _fxcache = material->pipelineCache();
    _matrices_only = material->instancedMatricesOnly(); // -> FWD_CT_NM_IM_NI_MO (matrices from storage_inst_mtx)
  }

  void gpuInit(lev2::Context* ctx) const {
    auto FXI = ctx->FXI();
    // matrices-only: COUNT-SIZED matrices buffers (count*64) + a matrices-only cull (no colors), so
    // memory scales with the instance count instead of the fixed k_max_instances combined buffer
    // (matrices 64 + colors 16 + pickids 8 per instance). Otherwise: the combined fixed buffer.
    size_t inst_bytes = _matrices_only ? ((_count > 0 ? size_t(_count) : size_t(1)) * 64) : k_ssbo_total_size;
    _instanceSSBO = FXI->createStorageBuffer(inst_bytes);
    if (_cullEnabled) {
      // survivors mirrors the instance SSBO layout (matrices[|colors|pickids]) so the VS reads it
      // identically when bound to _parInstanceBlock. args = VkDrawIndexedIndirectCommand (20B);
      // cullparams = mat4 vp + vec4 bound + uint count (96B, std430-padded).
      _survivorsSSBO  = FXI->createStorageBuffer(inst_bytes);
      _argsSSBO       = FXI->createStorageBuffer(32);
      _cullParamsSSBO = FXI->createStorageBuffer(96);
      // cascade-cull fix: a SECOND survivor set for the sun-shadow passes (union-sun-culled). The eye
      // set above is untouched by the shadow cull, so the color pass stays byte-identical.
      _survivorsSSBOShadow  = FXI->createStorageBuffer(inst_bytes);
      _argsSSBOShadow       = FXI->createStorageBuffer(32);
      _cullParamsSSBOShadow = FXI->createStorageBuffer(96);
      auto shader = _matrices_only
          ? FXI->shaderFromShaderText("instance_cull_mtxonly", _cull_shader_text_mtxonly())
          : FXI->shaderFromShaderText("instance_cull", _cull_shader_text());
      _cullShader = FXI->computeShader(shader, "cs_cull");
    }
  }

  void enqueueToRenderQueue(lev2::drawqueueitem_constptr_t item, lev2::IRenderer* renderer) const override {
    auto context = renderer->GetTarget();
    if (!_instanceSSBO) {
      gpuInit(context);
    }
    lev2::CallbackRenderable& renderable = renderer->enqueueCallback();
    renderable._drawable = nullptr;
    renderable._pickID = _pickID;
    renderable._sortkey = _sortkey;
    renderable._instanced = true;
    // CallbackRenderables come from a pooled fixedvector that does not reset
    // slots between frames; per-instance matrices already carry world-space
    // positions, so force identity here to avoid inheriting a stale world
    // matrix from a prior renderable that happened to land on the same slot.
    renderable.SetMatrix(fmtx4());
    renderable.SetModColor(fcolor4::White());
    renderable.SetDrawableDataA(GetUserDataA());
    renderable.SetDrawableDataB(GetUserDataB());
    bool is_alpha = false;
    if (auto as_pbr = std::dynamic_pointer_cast<lev2::PBRMaterial>(_material)) {
      is_alpha = as_pbr->_alphaBlend;
    }
    renderable.SetRenderCallback([this, is_alpha](lev2::RenderContextInstData& RCID) {
      auto context = RCID.context();
      auto RCFD = RCID.rcfd();
      auto FXI = context->FXI();
      OrkAssert(_matrices_only or _count <= k_max_instances);  // matrices-only is count-sized (uncapped)
      if (not _cullEnabled) {
        // non-cull path: upload this frame's candidate instances here. (When culling, the upload
        // happens once per frame in onGpuUpdate — view-independent — and the cull compacts them.)
        auto instances_copy = _idbuf_pool.begin_pull();
        if (_matrices_only) {
          auto ssbo_mapped = FXI->mapStorageBuffer(_instanceSSBO, 0, size_t(_count) * 64, lev2::BufferMapAccess::WRITE_ONLY);
          memcpy(ssbo_mapped->_mappedaddr, instances_copy->_worldmatrices.data(), _count * 64);
          ssbo_mapped->unmap();
        } else {
          auto ssbo_mapped = FXI->mapStorageBuffer(_instanceSSBO, 0, k_ssbo_total_size, lev2::BufferMapAccess::WRITE_ONLY);
          char* base_ptr = (char*)ssbo_mapped->_mappedaddr;
          memcpy(base_ptr + k_ssbo_offset_matrices, instances_copy->_worldmatrices.data(), _count * 64);
          memcpy(base_ptr + k_ssbo_offset_colors, instances_copy->_modcolors.data(), _count * 16);
          memcpy(base_ptr + k_ssbo_offset_pickids, instances_copy->_pickids.data(), _count * 8);
          ssbo_mapped->unmap();
        }
        _idbuf_pool.end_pull(instances_copy);
      }
      lev2::FxPipelinePermutation permu;
      // same hand-built-permutation read as the non-instanced twin above: pinned false, an
      // instanced draw inside a stereo pass takes its _MO technique and writes the SAME
      // image into both eye layers.
      permu._stereo = (RCFD and RCFD->hasCPD()) ? RCFD->topCPD().isSinglePassStereo() : false;
      permu._instanced = true;
      permu._skinned = false;
      permu._is_picking = false;
      permu._has_vtxcolors = not _matrices_only;   // matrices-only VS (vif_FWDTEST) has no vtxcolor input
      permu._is_alpha = is_alpha;
      permu._instanced_matrices_only = _matrices_only;  // -> FWD_CT_NM_IM_NI_MO (dynamic storage_inst_mtx)
      permu._rendering_model = RCFD->_renderingmodel._modelID;
      auto pipeline = _fxcache->findPipeline(permu);
      OrkAssert(pipeline);
      // cascade-cull fix: a sun-cascade depth pass reads the SHADOW survivor set (union-sun-culled);
      // every other pass (color, spot depth, probe) reads the eye set. Keyed off the active CPD flag
      // set by _update_sun_cascades. Non-shadow path is byte-identical to before.
      bool shadow_pass = false;
      if (_cullEnabled and RCFD)
        shadow_pass = RCFD->topCPD()._sunCascadeShadowPass;
      auto survivors = shadow_pass ? _survivorsSSBOShadow : _survivorsSSBO;
      auto drawargs  = shadow_pass ? _argsSSBOShadow      : _argsSSBO;
      pipeline->wrappedDrawCall(RCID, [&]() {
        if (pipeline->_parInstanceBlock) {
          // cull path binds the COMPACTED survivors (the VS reads the same layout by gl_InstanceIndex)
          FXI->bindStorageBuffer(pipeline->_parInstanceBlock, _cullEnabled ? survivors : _instanceSSBO);
        }
        if (_cullEnabled)
          _primitive->renderInstancedIndirectEML(context, drawargs); // instanceCount from the cull
        else
          _primitive->renderInstancedEML(context, _count);
      });
      RCID._isInstanced = false;
    });
  }

  //////////////////////////////////////////////////////////////////////////////
  // GPU frustum cull (opt-in via enableCull). VIEW-INDEPENDENT upload of the candidate instances
  // once per frame (onGpuUpdate, no camera), then a PER-VIEWPORT cull compute (onPreRender, with
  // that VP's CameraMatrices) compacts survivors + writes the indirect draw's instanceCount; the
  // render callback above draws indirect from the survivors. Compute runs on its own dispatch
  // phase (submit+wait), so it completes before the render pass reads the results.
  //////////////////////////////////////////////////////////////////////////////

  void enableCull(bool e) { _cullEnabled = e; }

  void onGpuUpdate(lev2::Context* ctx) const override {
    if (not _cullEnabled)
      return;
    if (not _instanceSSBO)
      gpuInit(ctx);
    OrkAssert(_matrices_only or _count <= k_max_instances);  // matrices-only is count-sized (uncapped)
    // Read the candidate instances straight from _instancedata (the source set by the caller),
    // NOT the _idbuf_pool triple buffer: the pool is published in enqueueOnLayer, which runs AFTER
    // onGpuUpdate/onPreRender, so pulling here would get stale/empty data. _instancedata is sized
    // to _count by resize(). (Fine for the static scatter case; dynamic instances would need a
    // pre-enqueue publish.)
    if (not _instancedata)
      return;
    size_t n = _count;
    if (n > _instancedata->_worldmatrices.size())
      n = _instancedata->_worldmatrices.size();
    if (n == 0)
      return;
    auto FXI = ctx->FXI();
    if (_matrices_only) {
      auto m = FXI->mapStorageBuffer(_instanceSSBO, 0, n * 64, lev2::BufferMapAccess::WRITE_ONLY);
      memcpy(m->_mappedaddr, _instancedata->_worldmatrices.data(), n * 64);
      m->unmap();
    } else {
      auto m = FXI->mapStorageBuffer(_instanceSSBO, 0, k_ssbo_total_size, lev2::BufferMapAccess::WRITE_ONLY);
      char* base = (char*)m->_mappedaddr;
      memcpy(base + k_ssbo_offset_matrices, _instancedata->_worldmatrices.data(), n * 64);
      memcpy(base + k_ssbo_offset_colors,   _instancedata->_modcolors.data(),     n * 16);
      memcpy(base + k_ssbo_offset_pickids,  _instancedata->_pickids.data(),       n * 8);
      m->unmap();
    }
  }

  void onPreRender(lev2::Context* ctx, const lev2::CameraMatrices& cammtx) const override {
    // EYE cull -> the eye survivor/args set (consumed by the color + spot depth passes).
    _cullInto(ctx, cammtx, _survivorsSSBO, _argsSSBO, _cullParamsSSBO);
  }

  // cascade-cull fix: sun-shadow cull -> the SHADOW set, consumed by the cascade depth passes. Runs
  // once per frame from Scene::shadowCull with the prologue's UNION sun camera (superset of every
  // cascade slice), so off-view casters that the eye cull dropped are kept for the shadow passes.
  void onShadowPreRender(lev2::Context* ctx, const lev2::CameraMatrices& cammtx) const override {
    _cullInto(ctx, cammtx, _survivorsSSBOShadow, _argsSSBOShadow, _cullParamsSSBOShadow);
  }

  bool wantsShadowCull() const override { return _cullEnabled; }

  // shared cull dispatch: compacts candidate instances (from _instanceSSBO) that pass cammtx's frustum
  // into `survivors`, writing the indirect instanceCount into `args`. Identical work for eye vs shadow;
  // only the target buffer set + camera differ. begin/endDispatchPhase are reentrant, so this nests
  // harmlessly inside Scene::shadowCull's outer phase (one submit for all drawables).
  void _cullInto(
      lev2::Context* ctx,
      const lev2::CameraMatrices& cammtx,
      lev2::FxShaderStorageBuffer* survivors,
      lev2::FxShaderStorageBuffer* args,
      lev2::FxShaderStorageBuffer* cullParams) const {
    if (not _cullEnabled or not _cullShader or not cullParams)
      return;
    auto FXI = ctx->FXI();
    auto CI  = ctx->CI();
    // cullparams: VP (mat4 @0), bound (cx,cy,cz,radius @64), count (@80).
    const auto& vp  = cammtx.GetVPMatrix();
    auto bc         = _primitive->boundCenter();
    float bound[4]  = {bc.x, bc.y, bc.z, _primitive->boundRadius()};
    uint32_t count  = uint32_t(_count);
    // u_p0 (@84): ORKID_DISABLE_FRUSTUM_CULL flag. 1 -> cs_cull skips the 6 plane tests (every instance
    // compacted as visible). Cached bool, no cost when unset; the shader defaults to full frustum when 0.
    uint32_t disable_frustum = lev2::cullFrustumDisabled() ? 1u : 0u;
    auto pm = FXI->mapStorageBuffer(cullParams, 0, 96, lev2::BufferMapAccess::WRITE_ONLY);
    char* pb = (char*)pm->_mappedaddr;
    memcpy(pb + 0,  vp.asArray(), 64);
    memcpy(pb + 64, bound, 16);
    memcpy(pb + 80, &count, 4);
    memcpy(pb + 84, &disable_frustum, 4);
    pm->unmap();
    // args = VkDrawIndexedIndirectCommand: seed indexCount, reset instanceCount to 0 (atomicAdd).
    uint32_t seed[5] = {uint32_t(_primitive->indexCountFirstPrimGroup()), 0u, 0u, 0u, 0u};
    auto am = FXI->mapStorageBuffer(args, 0, 32, lev2::BufferMapAccess::WRITE_ONLY);
    memcpy(am->_mappedaddr, seed, sizeof(seed));
    am->unmap();
    // dispatch the cull on its own phase (endDispatchPhase submits + waits -> done before render).
    int groups = (int(_count) + 63) / 64;
    if (groups < 1)
      return;
    CI->beginDispatchPhase();
    CI->bindStorageBuffer(_cullShader, 0, _instanceSSBO);
    CI->bindStorageBuffer(_cullShader, 1, survivors);
    CI->bindStorageBuffer(_cullShader, 2, cullParams);
    CI->bindStorageBuffer(_cullShader, 3, args);
    CI->dispatchCompute(_cullShader, groups, 1, 1);
    CI->endDispatchPhase();
  }

  // The instance-cull compute (headless-validated by llgfx/test_compute_cull.py). Reads candidate
  // world matrices, frustum-tests each instance's bound sphere (local center+radius via the matrix,
  // radius x max-axis scale) vs the VP, and stream-compacts survivors into the output SSBO with
  // atomicAdd on the indirect command's instanceCount. The instance-array size is taken from
  // k_max_instances (the SINGLE source of truth — still must equal storage_instancing in stdtools.i2).
  static std::string _cull_shader_text() {
    std::string t = R"SHADER(
fxconfig fxcfg_default {}
storage_interface sif_in (descriptor_set 0) {
  buffer layout(std430) bin { mat4 in_mtx[$NMAX$]; vec4 in_col[$NMAX$]; };
}
storage_interface sif_out (descriptor_set 0) {
  buffer layout(std430) bout { mat4 out_mtx[$NMAX$]; vec4 out_col[$NMAX$]; };
}
storage_interface sif_par (descriptor_set 0) {
  buffer layout(std430) bpar { mat4 u_vp; vec4 u_bound; uint u_count; uint u_p0; uint u_p1; uint u_p2; };
}
storage_interface sif_arg (descriptor_set 0) {
  buffer layout(std430) barg { uint a_indexCount; uint a_instanceCount; uint a_firstIndex; uint a_vertexOffset; uint a_firstInstance; };
}
compute_interface iface_cull {
  storage { sif_in sif_out sif_par sif_arg }
  inputs { layout(local_size_x = 64, local_size_y = 1, local_size_z = 1); }
}
compute_shader cs_cull : iface_cull {
  uint i = gl_GlobalInvocationID.x;
  if (i >= u_count) { return; }
  mat4 M = in_mtx[i];
  vec3 c = (M * vec4(u_bound.xyz, 1.0)).xyz;
  float r = u_bound.w * max(length(M[0].xyz), max(length(M[1].xyz), length(M[2].xyz)));
  vec4 rx = vec4(u_vp[0].x, u_vp[1].x, u_vp[2].x, u_vp[3].x);
  vec4 ry = vec4(u_vp[0].y, u_vp[1].y, u_vp[2].y, u_vp[3].y);
  vec4 rz = vec4(u_vp[0].z, u_vp[1].z, u_vp[2].z, u_vp[3].z);
  vec4 rw = vec4(u_vp[0].w, u_vp[1].w, u_vp[2].w, u_vp[3].w);
  bool inside = true;
  // u_p0 == 1 is the ORKID_DISABLE_FRUSTUM_CULL flag (host-stamped): skip the frustum reject so every
  // instance is stream-compacted as visible.
  if (u_p0 == 0u) {
    vec4 pl0 = rw + rx; vec4 pl1 = rw - rx;
    vec4 pl2 = rw + ry; vec4 pl3 = rw - ry;
    vec4 pl4 = rz;      vec4 pl5 = rw - rz;
    if ((dot(pl0.xyz, c) + pl0.w) < (-r * length(pl0.xyz))) { inside = false; }
    if ((dot(pl1.xyz, c) + pl1.w) < (-r * length(pl1.xyz))) { inside = false; }
    if ((dot(pl2.xyz, c) + pl2.w) < (-r * length(pl2.xyz))) { inside = false; }
    if ((dot(pl3.xyz, c) + pl3.w) < (-r * length(pl3.xyz))) { inside = false; }
    if ((dot(pl4.xyz, c) + pl4.w) < (-r * length(pl4.xyz))) { inside = false; }
    if ((dot(pl5.xyz, c) + pl5.w) < (-r * length(pl5.xyz))) { inside = false; }
  }
  if (inside) {
    uint slot = atomicAdd(a_instanceCount, 1u);
    out_mtx[slot] = M;
    out_col[slot] = in_col[i];
  }
}
)SHADER";
    const std::string n = std::to_string(k_max_instances);
    for (size_t p = t.find("$NMAX$"); p != std::string::npos; p = t.find("$NMAX$", p))
      t.replace(p, 6, n);
    return t;
  }
  // MATRICES-ONLY cull: in/out are SIZE-LESS runtime arrays (`mat4 x[];`) so the candidate + survivor
  // buffers are count-sized (no k_max_instances cap, no per-instance color). Same frustum test; writes
  // only out_mtx. Pairs with the FWD_CT_NM_IM_NI_MO render variant. (Requires shadlang [] runtime arrays.)
  static const char* _cull_shader_text_mtxonly() {
    return R"SHADER(
fxconfig fxcfg_default {}
storage_interface sif_in (descriptor_set 0) {
  buffer layout(std430) bin { mat4 in_mtx[]; };
}
storage_interface sif_out (descriptor_set 0) {
  buffer layout(std430) bout { mat4 out_mtx[]; };
}
storage_interface sif_par (descriptor_set 0) {
  buffer layout(std430) bpar { mat4 u_vp; vec4 u_bound; uint u_count; uint u_p0; uint u_p1; uint u_p2; };
}
storage_interface sif_arg (descriptor_set 0) {
  buffer layout(std430) barg { uint a_indexCount; uint a_instanceCount; uint a_firstIndex; uint a_vertexOffset; uint a_firstInstance; };
}
compute_interface iface_cull {
  storage { sif_in sif_out sif_par sif_arg }
  inputs { layout(local_size_x = 64, local_size_y = 1, local_size_z = 1); }
}
compute_shader cs_cull : iface_cull {
  uint i = gl_GlobalInvocationID.x;
  if (i >= u_count) { return; }
  mat4 M = in_mtx[i];
  vec3 c = (M * vec4(u_bound.xyz, 1.0)).xyz;
  float r = u_bound.w * max(length(M[0].xyz), max(length(M[1].xyz), length(M[2].xyz)));
  vec4 rx = vec4(u_vp[0].x, u_vp[1].x, u_vp[2].x, u_vp[3].x);
  vec4 ry = vec4(u_vp[0].y, u_vp[1].y, u_vp[2].y, u_vp[3].y);
  vec4 rz = vec4(u_vp[0].z, u_vp[1].z, u_vp[2].z, u_vp[3].z);
  vec4 rw = vec4(u_vp[0].w, u_vp[1].w, u_vp[2].w, u_vp[3].w);
  bool inside = true;
  // u_p0 == 1 is the ORKID_DISABLE_FRUSTUM_CULL flag (host-stamped): skip the frustum reject so every
  // instance is stream-compacted as visible.
  if (u_p0 == 0u) {
    vec4 pl0 = rw + rx; vec4 pl1 = rw - rx;
    vec4 pl2 = rw + ry; vec4 pl3 = rw - ry;
    vec4 pl4 = rz;      vec4 pl5 = rw - rz;
    if ((dot(pl0.xyz, c) + pl0.w) < (-r * length(pl0.xyz))) { inside = false; }
    if ((dot(pl1.xyz, c) + pl1.w) < (-r * length(pl1.xyz))) { inside = false; }
    if ((dot(pl2.xyz, c) + pl2.w) < (-r * length(pl2.xyz))) { inside = false; }
    if ((dot(pl3.xyz, c) + pl3.w) < (-r * length(pl3.xyz))) { inside = false; }
    if ((dot(pl4.xyz, c) + pl4.w) < (-r * length(pl4.xyz))) { inside = false; }
    if ((dot(pl5.xyz, c) + pl5.w) < (-r * length(pl5.xyz))) { inside = false; }
  }
  if (inside) {
    uint slot = atomicAdd(a_instanceCount, 1u);
    out_mtx[slot] = M;
  }
}
)SHADER";
  }

  std::shared_ptr<RigidPrimitive<vtx_t>> _primitive;
  lev2::material_ptr_t _material;
  lev2::fxpipelinecache_constptr_t _fxcache;
  bool _cullEnabled = false;
  // _matrices_only is inherited from InstancedDrawable (set in bindPrimitive from the material).
  mutable lev2::FxShaderStorageBuffer* _survivorsSSBO  = nullptr;
  mutable lev2::FxShaderStorageBuffer* _argsSSBO       = nullptr;
  mutable lev2::FxShaderStorageBuffer* _cullParamsSSBO = nullptr;
  // cascade-cull fix: the sun-shadow survivor set (union-sun frustum), consumed by the cascade depth
  // passes; the eye set above is consumed by color + spot passes.
  mutable lev2::FxShaderStorageBuffer* _survivorsSSBOShadow  = nullptr;
  mutable lev2::FxShaderStorageBuffer* _argsSSBOShadow       = nullptr;
  mutable lev2::FxShaderStorageBuffer* _cullParamsSSBOShadow = nullptr;
  mutable const lev2::FxComputeShader* _cullShader     = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
