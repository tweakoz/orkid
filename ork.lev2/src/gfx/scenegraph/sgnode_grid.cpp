#include <ork/lev2/gfx/scenegraph/sgnode_grid.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/lev2/gfx/image.h>
#include <ork/kernel/opq.h>
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
ImplementReflectionX(ork::lev2::GridDrawableData, "GridDrawableData");
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

GridDrawableImpl::GridDrawableImpl(const GridDrawableData* grid)
    : _griddata(grid) {
}
GridDrawableImpl::~GridDrawableImpl() {
}
void GridDrawableImpl::gpuInit(lev2::Context* ctx) {

  auto load_req = std::make_shared<asset::LoadRequest>(_griddata->_colortexpath);
  auto texasset = asset::AssetManager<lev2::TextureAsset>::load(load_req);
  OrkAssert(texasset);

  if (_griddata->_colorImage) {
    _color_image = _griddata->_colorImage;
  } else {
    _color_image = Image::createFromFile(_griddata->_colortexpath);
  }
  if (_griddata->_normalImage) {
    _normal_image = _griddata->_normalImage;
  } else {
    _normal_image = Image::createFromFile(_griddata->_normaltexpath);
  }
  if (_griddata->_mtlrufImage) {
    _mtlruf_image = _griddata->_mtlrufImage;
  } else {
    _mtlruf_image = Image::createFromFile(_griddata->_mtlruftexpath);
  }

  OrkAssert(_color_image);
  OrkAssert(_normal_image);
  OrkAssert(_mtlruf_image);

  _pbrmaterial                 = std::make_shared<PBRMaterial>();
  _pbrmaterial->_shader_suffix = _griddata->_shader_suffix;
  _pbrmaterial->_shaderpath    = "orkshader://grid";
  _pbrmaterial->assignImages(
      ctx,           // context
      _color_image,  // COLOR
      _normal_image, // NORMAL
      _mtlruf_image, // MTLRUF
      nullptr,       // EMISSIVE
      nullptr,       // AMBOCC
      true);         // conform
  _pbrmaterial->gpuInit(ctx);
  _pbrmaterial->_metallicFactor  = 0.0f;
  _pbrmaterial->_roughnessFactor = 1.0f;
  _pbrmaterial->_baseColor       = fvec3(1, 1, 1);
  _pbrmaterial->_doubleSided     = true;

  _fxcache = _pbrmaterial->pipelineCache();

  auto as_fstyle = _pbrmaterial->_as_freestyle;
  _paramAuxA     = as_fstyle->param("AuxA");
  _paramAuxB     = as_fstyle->param("AuxB");

  _initted = true;
}
void GridDrawableImpl::_render(const RenderContextInstData& RCID) {

  auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
  auto context    = RCID.context();

  if (not _initted) {
    gpuInit(context);
  }

  bool isPickState = context->FBI()->isPickState();

  auto RCFD = RCID.rcfd();

  const auto& CPD = RCFD->topCPD();

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
    // uint64_t pid = pickBuf ? pickBuf->AssignPickId((ork::Object*)nullptr) : 0;
    modcolor.setRGBAU64(uint64_t(0xffffffffffffffff));
  }
  context->PushModColor(modcolor);

  auto pipeline = _fxcache->findPipeline(RCID);
  OrkAssert(pipeline);

  pipeline->wrappedDrawCall(RCID, [&]() {
    pipeline->_set_typed_param(
        RCID, _paramAuxA, fvec4(_griddata->_intensityA, _griddata->_intensityB, _griddata->_intensityC, _griddata->_intensityD));
    pipeline->_set_typed_param(RCID, _paramAuxB, fvec4(_griddata->_lineWidth, 0, 0, 0));

    if (_griddata->_shader_suffix == "_V3") {
      // set additive
      _pbrmaterial->_rasterstate.SetBlending(Blending::ALPHA);
      context->RSI()->BindRasterState(_pbrmaterial->_rasterstate, true);
    }
    gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES, 6);
  });

  context->PopModColor();
  mtxi->PopMMatrix();
}

///////////////////////////////////////////////////////////////////////////////

void GridDrawableImpl::renderGrid(RenderContextInstData& RCID) { // static
  auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
  auto drawable   = renderable->_drawable;
  drawable->_implA.getShared<GridDrawableImpl>()->_render(RCID);
}

///////////////////////////////////////////////////////////////////////////////

void GridDrawableData::describeX(class_t* c) {
}

///////////////////////////////////////////////////////////////////////////////

drawable_ptr_t GridDrawableData::createDrawable() const {
  auto drw = std::make_shared<CallbackDrawable>(nullptr);
  auto impl = drw->_implA.makeShared<GridDrawableImpl>(this);
  drw->_sortkey = 10;
  drw->SetRenderCallback(GridDrawableImpl::renderGrid);
  return drw;
}

///////////////////////////////////////////////////////////////////////////////

GridDrawableData::GridDrawableData() {
  _colortexpath  = "src://effect_textures/white_64.dds";
  _mtlruftexpath = "src://effect_textures/white_64.dds";
  _normaltexpath = "src://effect_textures/default_normal.dds";
}

///////////////////////////////////////////////////////////////////////////////

GridDrawableData::~GridDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
