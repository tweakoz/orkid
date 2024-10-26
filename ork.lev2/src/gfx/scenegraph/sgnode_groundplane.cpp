#include <ork/lev2/gfx/scenegraph/sgnode_groundplane.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
ImplementReflectionX(ork::lev2::GroundPlaneDrawableData, "GroundPlaneDrawableData");
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct GroundPlaneRenderImpl {

  GroundPlaneRenderImpl(const GroundPlaneDrawableData* gpd) : _grounddata(gpd) {

  }
  ~GroundPlaneRenderImpl(){
  }
  void gpuInit(lev2::Context* ctx) {

    if( _grounddata->_pipeline_color ){
        _pipeline_color = _grounddata->_pipeline_color;
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
        _pbrmaterial->_doubleSided = true;
        
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

    auto RCFD = RCID.rcfd();

    bool is_depth_prepass = RCFD->_renderingmodel._modelID=="DEPTH_PREPASS"_crcu;

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

    fvec3 dpos1 = topr - topl;
    fvec3 dpos2 = botl - topl;
    fvec2 duv1 = uv_topr - uv_topl;
    fvec2 duv2 = uv_botl - uv_topl;
    float r = 1.0f / (duv1.x * duv2.y - duv1.y * duv2.x);
    fvec3 tangent = ((dpos1 * duv2.y - dpos2 * duv1.y)*r).normalized();
    fvec3 binormal = ((dpos2 * duv1.x - dpos1 * duv2.x)*r).normalized();


    //auto binormal = fvec3(0, 0, -1);

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

    fxpipeline_ptr_t pipeline = nullptr;

    if(_pipeline_color and not is_depth_prepass){
      pipeline = _pipeline_color;
    }
    else{
      OrkAssert(_fxcache);
      pipeline = _fxcache->findPipeline(RCID);
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
  fxpipeline_ptr_t _pipeline_color;

  texture_ptr_t _colortexture;
  fxpipelinecache_constptr_t _fxcache;
  bool _initted = false;

};

///////////////////////////////////////////////////////////////////////////////

void GroundPlaneDrawableData::describeX(class_t* c) {
}

///////////////////////////////////////////////////////////////////////////////

drawable_ptr_t GroundPlaneDrawableData::createDrawable() const {


  auto rval = std::make_shared<CallbackDrawable>(nullptr);
  rval->SetRenderCallback(GroundPlaneRenderImpl::renderGroundPlane);
  auto impl = rval->_implA.makeShared<GroundPlaneRenderImpl>(this);
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
