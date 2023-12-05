#include <ork/lev2/gfx/scenegraph/sgnode_billboard.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
ImplementReflectionX(ork::lev2::BillboardDrawableData, "BillboardDrawableData");
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct BillboardRenderImpl {

  BillboardRenderImpl(const BillboardDrawableData* bbd) : _bbdata(bbd) {

  }
  ~BillboardRenderImpl(){
  }
  void gpuInit(lev2::Context* ctx) {

    auto load_req = std::make_shared<asset::LoadRequest>(_bbdata->_colortexpath);
    auto texasset = asset::AssetManager<lev2::TextureAsset>::load(load_req);
    OrkAssert(texasset);

    _colortexture = texasset->GetTexture();
    OrkAssert(_colortexture);

    _material       = new GfxMaterialUITextured();
    _material->SetTexture(ETEXDEST_DIFFUSE,_colortexture.get());
    _material->gpuInit(ctx,"uitextured");
    _material->_rasterstate.SetBlending(Blending::ALPHA);
    _initted                   = true;
  }
  void _render(const RenderContextInstData& RCID){
    auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
    auto context    = RCID.context();

    if (not _initted){
      gpuInit(context);
    }

    bool isPickState = context->FBI()->isPickState();

    const RenderContextFrameData* RCFD = RCID._RCFD;

    auto mtxi = context->MTXI();
    auto gbi  = context->GBI();
    auto rsi  = context->RSI();
    auto fbi  = context->FBI();

    mtxi->PushMMatrix(fmtx4::Identity());
    mtxi->PushVMatrix(fmtx4::Identity());
    mtxi->PushPMatrix(fmtx4::Identity());
    fbi->pushViewport(0,0,3840,1920);
    fbi->pushScissor(0,0,3840,1920);
    fvec4 modcolor = fcolor4::Green();
    context->PushModColor(modcolor);
    rsi->SetDepthTest(lev2::EDepthTest::OFF);
    rsi->SetCullTest(lev2::ECullTest::OFF);

    auto& VB = GfxEnv::GetSharedDynamicVB2();

    //////////////////////////////////////////
    float Z = 0.0f;
    fvec3 topl(-1, -1, Z);
    fvec3 topr(+1, -1, Z);
    fvec3 botr(+1, +1, Z);
    fvec3 botl(-1, +1, Z);

    auto uv_topl  = fvec2(0.0f, 0.0f);
    auto uv_topr  = fvec2(1, 0.0f);
    auto uv_botr  = fvec2(1, 1);
    auto uv_botl  = fvec2(0.0f, 1);
    auto normal   = fvec3(0, 1, 0);
    auto binormal = fvec3(1, 0, 0);

    uint32_t color = 0xffffffff;
    float alpha    = _bbdata->_alpha;
    color          = (color & 0x00ffffff) | (uint32_t(alpha * 255.0f) << 24);
    auto v0 = SVtxV12N12B12T8C4(topl, normal, binormal, uv_topl, color);
    auto v1 = SVtxV12N12B12T8C4(topr, normal, binormal, uv_topr, color);
    auto v2 = SVtxV12N12B12T8C4(botr, normal, binormal, uv_botr, color);
    auto v3 = SVtxV12N12B12T8C4(botl, normal, binormal, uv_botl, color);

    VtxWriter<SVtxV12N12B12T8C4> vw;
    vw.Lock(context, &VB, 6);
    vw.AddVertex(v0);
    vw.AddVertex(v1);
    vw.AddVertex(v2);
    vw.AddVertex(v0);
    vw.AddVertex(v3);
    vw.AddVertex(v2);
    vw.UnLock(context);
    gbi->DrawPrimitive(_material,vw, PrimitiveType::TRIANGLES, 6);
    //////////////////////////////////////////

    fbi->popViewport();
    fbi->popScissor();
    context->PopModColor();
    mtxi->PopPMatrix();
    mtxi->PopVMatrix();
    mtxi->PopMMatrix();
  }
  static void renderBB(RenderContextInstData& RCID) {
    auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
    renderable->GetDrawableDataA().getShared<BillboardRenderImpl>()->_render(RCID);
  }
  const BillboardDrawableData* _bbdata;
  GfxMaterialUITextured* _material;
  texture_ptr_t _colortexture;
  bool _initted = false;

};

///////////////////////////////////////////////////////////////////////////////

void BillboardDrawableData::describeX(class_t* c) {
}

///////////////////////////////////////////////////////////////////////////////

drawable_ptr_t BillboardDrawableData::createDrawable() const {

  auto impl = std::make_shared<BillboardRenderImpl>(this);

  auto rval = std::make_shared<CallbackDrawable>(nullptr);
  rval->SetRenderCallback(BillboardRenderImpl::renderBB);
  rval->SetUserDataA(impl);
  rval->_sortkey = 10000;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

BillboardDrawableData::BillboardDrawableData() {
  _colortexpath = "lev2://textures/gridcell_grey";
}

///////////////////////////////////////////////////////////////////////////////

BillboardDrawableData::~BillboardDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
