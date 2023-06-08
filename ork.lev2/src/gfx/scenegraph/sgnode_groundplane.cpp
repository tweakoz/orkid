#include <ork/lev2/gfx/scenegraph/sgnode_groundplane.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/material_pbr.inl>
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
ImplementReflectionX(ork::lev2::GroundPlaneDrawableData, "GroundPlaneDrawableData");
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct GroundPlaneRenderImpl {

  GroundPlaneRenderImpl(const GroundPlaneDrawableData* grid) : _grounddata(grid) {

  }
  ~GroundPlaneRenderImpl(){
  }
  void gpuInit(lev2::Context* ctx) {

    if( _grounddata->_pipeline ){
        _pipeline = _grounddata->_pipeline;
    }
    else if( _grounddata->_material ){
        _pbrmaterial = _grounddata->_material;
        _fxcache = _pbrmaterial->pipelineCache();
    }
    else{
        _pbrmaterial       = std::make_shared<PBRMaterial>();
        _pbrmaterial->gpuInit(ctx);
        _pbrmaterial->_metallicFactor  = 0.0f;
        _pbrmaterial->_roughnessFactor = 1.0f;
        _pbrmaterial->_baseColor       = fvec3(1, 1, 1);
        _fxcache = _pbrmaterial->pipelineCache();
    }

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

    const auto& CPD  = RCFD->topCPD();

    float extent = _grounddata->_extent;
    fvec3 topl(-extent, 0, -extent);
    fvec3 topr(+extent, 0, -extent);
    fvec3 botr(+extent, 0, +extent);
    fvec3 botl(-extent, 0, +extent);

    float uvextent = 0.5f; //extent / _grounddata->_majorTileDim;

    auto uv_topl  = fvec2(-uvextent, -uvextent);
    auto uv_topr  = fvec2(+uvextent, -uvextent);
    auto uv_botr  = fvec2(+uvextent, +uvextent);
    auto uv_botl  = fvec2(-uvextent, +uvextent);
    auto normal   = fvec3(0, 1, 0);
    auto binormal = fvec3(1, 0, 0);

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
    fvec4 modcolor = fcolor4::White();
    if (isPickState) {
      auto pickBuf = context->FBI()->currentPickBuffer();
      //uint64_t pid = pickBuf ? pickBuf->AssignPickId((ork::Object*)nullptr) : 0;
      modcolor.setRGBAU64(uint64_t(0xffffffffffffffff));
    }
    context->PushModColor(modcolor);

    auto pipeline = _pipeline;
    if( nullptr == pipeline ){
       _fxcache->findPipeline(RCID);
    }
    OrkAssert(pipeline);
    pipeline->wrappedDrawCall(RCID, [&]() {
      gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES, 6);
    });

    context->PopModColor();
    mtxi->PopMMatrix();
  }
  static void renderGroundPlane(RenderContextInstData& RCID) {
    auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
    renderable->GetDrawableDataA().getShared<GroundPlaneRenderImpl>()->_render(RCID);
  }
  const GroundPlaneDrawableData* _grounddata;
  pbrmaterial_ptr_t _pbrmaterial;
  fxpipeline_ptr_t _pipeline;

  texture_ptr_t _colortexture;
  fxpipelinecache_constptr_t _fxcache;
  bool _initted = false;

};

///////////////////////////////////////////////////////////////////////////////

void GroundPlaneDrawableData::describeX(class_t* c) {
}

///////////////////////////////////////////////////////////////////////////////

drawable_ptr_t GroundPlaneDrawableData::createDrawable() const {

  auto impl = std::make_shared<GroundPlaneRenderImpl>(this);

  auto rval = std::make_shared<CallbackDrawable>(nullptr);
  rval->SetRenderCallback(GroundPlaneRenderImpl::renderGroundPlane);
  rval->SetUserDataA(impl);
  rval->_sortkey = 10;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

GroundPlaneDrawableData::GroundPlaneDrawableData() {
  //_colortexpath = "lev2://textures/gridcell_grey";
}

///////////////////////////////////////////////////////////////////////////////

GroundPlaneDrawableData::~GroundPlaneDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
