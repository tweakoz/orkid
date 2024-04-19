#include <ork/lev2/gfx/scenegraph/sgnode_grid.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
ImplementReflectionX(ork::lev2::GridDrawableData, "GridDrawableData");
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct GridRenderImpl {

  GridRenderImpl(const GridDrawableData* grid) : _griddata(grid) {

  }
  ~GridRenderImpl(){
  }
  void gpuInit(lev2::Context* ctx) {

    auto load_req = std::make_shared<asset::LoadRequest>(_griddata->_colortexpath);
    auto texasset = asset::AssetManager<lev2::TextureAsset>::load(load_req);
    OrkAssert(texasset);

    _colortexture = texasset->GetTexture();
    OrkAssert(_colortexture);

    load_req = std::make_shared<asset::LoadRequest>(_griddata->_normaltexpath);
    texasset = asset::AssetManager<lev2::TextureAsset>::load(load_req);
    OrkAssert(texasset);

    _normaltexture = texasset->GetTexture();
    OrkAssert(_normaltexture);

    _pbrmaterial       = new PBRMaterial();
    _pbrmaterial->_shader_suffix = _griddata->_shader_suffix;
    _pbrmaterial->_shaderpath = "orkshader://grid";
    _pbrmaterial->_texColor = _colortexture;
    _pbrmaterial->_texNormal = _normaltexture;
    _pbrmaterial->gpuInit(ctx);
    _pbrmaterial->_metallicFactor  = 0.0f;
    _pbrmaterial->_roughnessFactor = 1.0f;
    _pbrmaterial->_baseColor       = fvec3(1, 1, 1);
    _pbrmaterial->_doubleSided = true;

    _fxcache = _pbrmaterial->pipelineCache();

    _initted                   = true;
  }
  void _render(const RenderContextInstData& RCID){
    auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
    auto context    = RCID.context();

    if (not _initted){
      gpuInit(context);
    }

    bool isPickState = context->FBI()->isPickState();

    auto RCFD = RCID.rcfd();

    const auto& CPD  = RCFD->topCPD();

    float extent = _griddata->_extent;
    fvec3 topl(-extent, 0, -extent);
    fvec3 topr(+extent, 0, -extent);
    fvec3 botr(+extent, 0, +extent);
    fvec3 botl(-extent, 0, +extent);

    float uvextent = extent / _griddata->_majorTileDim;

    auto uv_topl  = fvec2(-uvextent, -uvextent);
    auto uv_topr  = fvec2(+uvextent, -uvextent);
    auto uv_botr  = fvec2(+uvextent, +uvextent);
    auto uv_botl  = fvec2(-uvextent, +uvextent);
    auto normal   = fvec3(0.5, 0.5, 1);
    auto binormal = fvec3(0, 0, 1);

    auto v0 = SVtxV12N12B12T8C4(topl, normal, binormal, uv_topl, 0xffffffff);
    auto v1 = SVtxV12N12B12T8C4(topr, normal, binormal, uv_topr, 0xffffffff);
    auto v2 = SVtxV12N12B12T8C4(botr, normal, binormal, uv_botr, 0xffffffff);
    auto v3 = SVtxV12N12B12T8C4(botl, normal, binormal, uv_botl, 0xffffffff);

    auto& VB = GfxEnv::GetSharedDynamicVB2();
    VtxWriter<SVtxV12N12B12T8C4> vw;
    vw.Lock(context, &VB, 6);

    vw.AddVertex(v0);
    vw.AddVertex(v2);
    vw.AddVertex(v1);

    vw.AddVertex(v0);
    vw.AddVertex(v3);
    vw.AddVertex(v2);

    vw.UnLock(context);

    auto mtxi = context->MTXI();
    auto gbi  = context->GBI();
    mtxi->PushMMatrix(fmtx4::Identity());
    fvec4 modcolor = _griddata->_modcolor;
    if (isPickState) {
      auto pickBuf = context->FBI()->currentPickBuffer();
      //uint64_t pid = pickBuf ? pickBuf->AssignPickId((ork::Object*)nullptr) : 0;
      modcolor.setRGBAU64(uint64_t(0xffffffffffffffff));
    }
    context->PushModColor(modcolor);

    auto pipeline = _fxcache->findPipeline(RCID);
    OrkAssert(pipeline);
    pipeline->wrappedDrawCall(RCID, [&]() {
      gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES, 6);
    });

    context->PopModColor();
    mtxi->PopMMatrix();
  }
  static void renderGrid(RenderContextInstData& RCID) {
    auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
    renderable->GetDrawableDataA().getShared<GridRenderImpl>()->_render(RCID);
  }
  const GridDrawableData* _griddata;
  PBRMaterial* _pbrmaterial;

  texture_ptr_t _colortexture;
  texture_ptr_t _normaltexture;
  fxpipelinecache_constptr_t _fxcache;
  bool _initted = false;

};

///////////////////////////////////////////////////////////////////////////////

void GridDrawableData::describeX(class_t* c) {
}

///////////////////////////////////////////////////////////////////////////////

drawable_ptr_t GridDrawableData::createDrawable() const {

  auto impl = std::make_shared<GridRenderImpl>(this);

  auto rval = std::make_shared<CallbackDrawable>(nullptr);
  rval->SetRenderCallback(GridRenderImpl::renderGrid);
  rval->SetUserDataA(impl);
  rval->_sortkey = 10;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

GridDrawableData::GridDrawableData() {
  _colortexpath = "lev2://textures/gridcell_grey";
  _normaltexpath = "src://effect_textures/default_normal.dds";
}

///////////////////////////////////////////////////////////////////////////////

GridDrawableData::~GridDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
