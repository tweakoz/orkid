#include <ork/lev2/gfx/scenegraph/sgnode_geoclipmap.h>
#include <ork/lev2/gfx/meshutil/geoclipmap.h>
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/kernel/datablock.h>
#include <ork/kernel/datacache.h>

///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
ImplementReflectionX(ork::lev2::ClipMapDrawableData, "ClipMapDrawableData");
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct ClipMapRenderImpl {

  ClipMapRenderImpl(const ClipMapDrawableData* gpd)
      : _data(gpd) {
  }
  ~ClipMapRenderImpl() {
  }
  void gpuInit(lev2::Context* ctx) {

    using namespace meshutil;

    auto params       = std::make_shared<geoclipmap::Parameters>();
    params->_levels   = _data->_levels;
    params->_ringSize = _data->_ringSize;
    params->_baseQuadSize = _data->_baseQuadSize;
    auto cmaphasher   = DataBlock::createHasher();
    cmaphasher->accumulateString("ClipMapRenderImpl::mesh"); // identifier
    cmaphasher->accumulateItem<float>(1.0);                  // version code
    cmaphasher->accumulateItem<float>(1.0);                  // salt
    cmaphasher->accumulateItem<int>(params->_levels);
    cmaphasher->accumulateItem<int>(params->_ringSize);
    cmaphasher->accumulateItem<float>(params->_baseQuadSize);
    cmaphasher->accumulateItem<int>(params->_circle);
    cmaphasher->finish();
    uint64_t cmaphash = cmaphasher->result();
    // logchan_pbrgen->log("brdfIntegrationMap hashkey<%zx>", cmaphash);
    auto dblock = DataBlockCache::findDataBlock(cmaphash);

    auto read_from_datablock = [&](datablock_ptr_t dblock) -> bool {
      chunkfile::DefaultLoadAllocator allocator;
      chunkfile::Reader chunkreader(dblock, allocator);

      auto hdrstream = chunkreader.GetStream("header");
      auto geostream = chunkreader.GetStream("geometry");

      _mesh_primitive = std::make_shared<rigidprim_SVtxV12N12T16_t>();
      _mesh_primitive->gpuLoadFromChunksA(ctx, hdrstream, geostream);
      return true;
    };

    if (dblock) {
      read_from_datablock(dblock);
    } else {
      printf("generating primitive...\n");
      auto generator  = std::make_shared<geoclipmap::Generator>(params);
      auto subm       = generator->generateClipmaps();
      _mesh_primitive = std::make_shared<rigidprim_SVtxV12N12T16_t>();
      meshutil::XgmClusterizerStd clusterizer;
      _mesh_primitive->toClusterizer(*subm, clusterizer);

      //_mesh_primitive->fromSubMesh(*subm,ctx);
      chunkfile::Writer chunkwriter("clipmapgeom");
      auto hdrstream = chunkwriter.AddStream("header");
      auto geostream = chunkwriter.AddStream("geometry");
      _mesh_primitive->clusterizerToChunks(clusterizer, hdrstream, geostream);
      dblock = std::make_shared<DataBlock>();
      chunkwriter.writeToDataBlock(dblock);
      DataBlockCache::setDataBlock(cmaphash, dblock);
      {
        auto read_dblock = DataBlockCache::findDataBlock(cmaphash);
        read_from_datablock(read_dblock);
      }
    }

    if (_data->_material) {
      _pbrmaterial  = _data->_material;
      _fxcache      = _pbrmaterial->pipelineCache();
      _as_freestyle = _pbrmaterial->_as_freestyle;
      //_paramMYM = _as_freestyle->param("my_m");
    } else {
      _pbrmaterial = std::make_shared<PBRMaterial>();
      _pbrmaterial->gpuInit(ctx);
      _pbrmaterial->_metallicFactor  = 0.0f;
      _pbrmaterial->_roughnessFactor = 1.0f;
      _pbrmaterial->_baseColor       = fvec3(1, 1, 1);
      _pbrmaterial->_doubleSided     = true;

      _fxcache = _pbrmaterial->pipelineCache();
    }

    _initted = true;
  }
  void _render(const RenderContextInstData& RCID) {

    auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
    auto context    = RCID.context();

    if (not _initted) {
      gpuInit(context);
    }

    bool isPickState = context->FBI()->isPickState();

    auto RCFD = RCID.rcfd();

    bool is_depth_prepass = RCFD->_renderingmodel._modelID == "DEPTH_PREPASS"_crcu;

    auto& CPD = RCFD->topCPD();

    auto mtxi     = context->MTXI();
    auto gbi      = context->GBI();
    auto fxi      = context->FXI();
    auto pipeline = _fxcache->findPipeline(RCID);
    OrkAssert(pipeline);

    auto mcams             = CPD._cameraMatrices;
    const fmtx4& PMTX_mono = mcams->_pmatrix;
    fmtx4 vmtx_mono        = mcams->_vmatrix;
    fmtx4 v_offset;
    fvec3 eye_pos = vmtx_mono.inverse().translation();
    fvec3 eye_dir = vmtx_mono.inverse().zNormal();
    fvec3 center = fvec3(eye_pos.x, 0.0f, eye_pos.z);
    //center += fvec3(eye_dir.x, 0.0f, eye_dir.z);// * powf(eye_pos.y, 1.0) * -1.0f;
    v_offset.setTranslation(center);

    auto mut_renderable          = const_cast<CallbackRenderable*>(renderable);
    auto orig_matrix             = mut_renderable->_worldMatrix;
    mut_renderable->_worldMatrix = (v_offset * orig_matrix);

    pipeline->wrappedDrawCall(RCID, [&]() {
      //_as_freestyle->bindParamMatrix(_paramMYM, orig_matrix);
      _mesh_primitive->renderEML(context);
    });
    mut_renderable->_worldMatrix = orig_matrix;
  }
  static void renderClipMap(RenderContextInstData& RCID) {
    auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
    renderable->GetDrawableDataA().getShared<ClipMapRenderImpl>()->_render(RCID);
  }
  const ClipMapDrawableData* _data;
  pbrmaterial_ptr_t _pbrmaterial;

  texture_ptr_t _colortexture;
  fxpipelinecache_constptr_t _fxcache;
  meshutil::rigidprim_SVtxV12N12T16_ptr_t _mesh_primitive;
  fxparam_constptr_t _paramMYM      = nullptr;
  freestyle_mtl_ptr_t _as_freestyle = nullptr;

  bool _initted = false;
};

///////////////////////////////////////////////////////////////////////////////

void ClipMapDrawableData::describeX(class_t* c) {
}

///////////////////////////////////////////////////////////////////////////////

drawable_ptr_t ClipMapDrawableData::createDrawable() const {

  auto rval = std::make_shared<CallbackDrawable>(nullptr);
  rval->SetRenderCallback(ClipMapRenderImpl::renderClipMap);
  auto impl      = rval->_implA.makeShared<ClipMapRenderImpl>(this);
  rval->_sortkey = 10;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

ClipMapDrawableData::ClipMapDrawableData() {
  //_colortexpath = "lev2://textures/gridcell_grey";
}

///////////////////////////////////////////////////////////////////////////////

ClipMapDrawableData::~ClipMapDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
