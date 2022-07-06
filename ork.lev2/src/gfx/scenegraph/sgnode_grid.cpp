#include <ork/lev2/gfx/scenegraph/sgnode_grid.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/material_pbr.inl>
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
ImplementReflectionX(ork::lev2::GridDrawableData, "GridDrawableData");
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

void GridDrawableData::describeX(class_t* c) {
}

///////////////////////////////////////////////////////////////////////////////

griddrawableinstptr_t GridDrawableData::createInstance() const {
  return std::make_shared<GridDrawableInst>(*this);
}

///////////////////////////////////////////////////////////////////////////////

GridDrawableData::GridDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////

GridDrawableData::~GridDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////

struct GridRenderImpl {

  GridRenderImpl(GridDrawableInst* grid) : _gridinst(grid) {

  }
  ~GridRenderImpl(){
  }
  void gpuInit(lev2::Context* ctx) {

    const GridDrawableData& data  = _gridinst->_data;
    auto texasset = data._colorTexture;
    OrkAssert(texasset);
    _colortexture = texasset->GetTexture();
    OrkAssert(_colortexture);

    _pbrmaterial       = new PBRMaterial();
    _pbrmaterial->_shaderpath = "orkshader://grid";
    _pbrmaterial->_texColor = _colortexture;
    //_pbrmaterial->_enablePick         = true;
    _pbrmaterial->gpuInit(ctx);
    _pbrmaterial->_metallicFactor  = 0.0f;
    _pbrmaterial->_roughnessFactor = 1.0f;
    _pbrmaterial->_baseColor       = fvec3(1, 1, 1);

    _fwdmaterial = new FreestyleMaterial;
    _fwdmaterial->gpuInit(ctx, "orkshader://grid");
    _tekFWGRID = _fwdmaterial->technique("fwd_grid");
    _parMatFWMVP = _fwdmaterial->param("mvp");
    _initted                   = true;
  }
  void _render(const RenderContextInstData& RCID){
    auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._dagrenderable);
    auto context    = RCID.context();

    if (not _initted){
      gpuInit(context);
    }



    const GridDrawableData& data  = _gridinst->_data;

    bool isPickState = context->FBI()->isPickState();

    const RenderContextFrameData* RCFD = RCID._RCFD;

    bool is_forward = (RCFD->_renderingmodel=="FORWARD"_crcu);

    const auto& CPD  = RCFD->topCPD();

    float extent = data._extent;
    fvec3 topl(-extent, 0, -extent);
    fvec3 topr(+extent, 0, -extent);
    fvec3 botr(+extent, 0, +extent);
    fvec3 botl(-extent, 0, +extent);

    float uvextent = extent / data._majorTileDim;

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
    vw.AddVertex(v1);
    vw.AddVertex(v2);

    vw.AddVertex(v0);
    vw.AddVertex(v2);
    vw.AddVertex(v3);

    vw.UnLock(context);

    auto mtxi = context->MTXI();
    auto gbi  = context->GBI();
    mtxi->PushMMatrix(fmtx4::Identity());
    fvec4 modcolor = fcolor4::Green();
    if (isPickState) {
      auto pickBuf = context->FBI()->currentPickBuffer();
      //uint64_t pid = pickBuf ? pickBuf->AssignPickId((ork::Object*)nullptr) : 0;
      modcolor.setRGBAU64(uint64_t(0xffffffffffffffff));
    }
    context->PushModColor(modcolor);

    if( is_forward ){

      auto cammatrices = CPD.cameraMatrices();
      const fmtx4& PMTX_mono = cammatrices->_pmatrix;
      const fmtx4& VMTX_mono = cammatrices->_vmatrix;
      auto MVP               = VMTX_mono * PMTX_mono;

      _fwdmaterial->_rasterstate.SetBlending(Blending::ALPHA_ADDITIVE);
      _fwdmaterial->_rasterstate.SetDepthTest(EDepthTest::EDEPTHTEST_OFF);
      _fwdmaterial->_rasterstate.SetZWriteMask(false);
      _fwdmaterial->begin(_tekFWGRID,*RCFD);
      _fwdmaterial->bindParamMatrix(_parMatFWMVP, MVP);
      gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES, 6);
      _fwdmaterial->end(*RCFD);

    }
    else{
      gbi->DrawPrimitive(_pbrmaterial, vw, PrimitiveType::TRIANGLES, 6);
    }

    context->PopModColor();
    mtxi->PopMMatrix();
  }
  static void renderGrid(RenderContextInstData& RCID) {
    auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._dagrenderable);
    renderable->GetDrawableDataA().getShared<GridRenderImpl>()->_render(RCID);
  }
  GridDrawableInst* _gridinst;
  PBRMaterial* _pbrmaterial;
  FreestyleMaterial* _fwdmaterial;
  const FxShaderTechnique* _tekFWGRID = nullptr;
  const FxShaderParam* _parMatFWMVP = nullptr;

  texture_ptr_t _colortexture;
  bool _initted = false;

};

///////////////////////////////////////////////////////////////////////////////

GridDrawableInst::GridDrawableInst(const GridDrawableData& data)
    : _data(data) {
}

///////////////////////////////////////////////////////////////////////////////

GridDrawableInst::~GridDrawableInst() {
}

///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////

callback_drawable_ptr_t GridDrawableInst::createCallbackDrawable() {
  auto impl = _impl.makeShared<GridRenderImpl>(this);

  _rawdrawable = std::make_shared<CallbackDrawable>(nullptr);
  _rawdrawable->SetRenderCallback(GridRenderImpl::renderGrid);
  _rawdrawable->SetUserDataA(impl);
  _rawdrawable->SetSortKey(1000);
  return _rawdrawable;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
