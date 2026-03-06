////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/lev2_asset.h>
#include <cmath>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::GfxMaterialUI, "MaterialUI")

namespace ork { namespace lev2 {

// Singleton UI materials - same instance works on all contexts that share VkDevice
static uimaterial_ptr_t g_default_ui_material;
static uitexmaterial_ptr_t g_default_uitex_material;

uimaterial_ptr_t defaultUIMaterial() {
  if (!g_default_ui_material) {
    g_default_ui_material = std::make_shared<GfxMaterialUI>(contextForCurrentThread());
  }
  return g_default_ui_material;
}
uitexmaterial_ptr_t defaultUITextureMaterial() {
  if (!g_default_uitex_material) {
    g_default_uitex_material = std::make_shared<GfxMaterialUITextured>(contextForCurrentThread());
  }
  return g_default_uitex_material;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUI::Describe() {
}

/////////////////////////////////////////////////////////////////////////

GfxMaterialUI::~GfxMaterialUI() {
}

/////////////////////////////////////////////////////////////////////////

GfxMaterialUI::GfxMaterialUI(Context* pTarg)
    : meType(ETYPE_STANDARD)
    , meUIColorMode(UiColorMode::MOD) {
  miNumPasses = 1;
  _rasterstate->setBlendingMacro(BlendingMacro::OFF);
  _rasterstate->setDepthTest(EDepthTest::OFF);
  _rasterstate->setWriteMaskZ(false);
  _rasterstate->setWriteMaskRGB(true);
  _rasterstate->setWriteMaskA(true);
  _rasterstate->setCullTest(ECullTest::OFF);

  if (pTarg) {
    gpuInit(pTarg);
  }
}

/////////////////////////////////////////////////////////////////////////

uimaterial_ptr_t GfxMaterialUI::clone() const {
  auto newmat = std::make_shared<GfxMaterialUI>();

  newmat->cloneStateFrom(*this);

  newmat->_shaderasset = this->_shaderasset;
  newmat->_shader = this->_shader;

  newmat->_rasterstate = this->_rasterstate->clone();

  newmat->hTekMod = this->hTekMod;
  newmat->hTekVtx = this->hTekVtx;
  newmat->hTekModVtx = this->hTekModVtx;
  newmat->hTekCircle = this->hTekCircle;

  newmat->hVPW = this->hVPW;
  newmat->hBias = this->hBias;
  newmat->hScale = this->hScale;
  newmat->hTransform = this->hTransform;
  newmat->hModColor = this->hModColor;
  newmat->hColorMap = this->hColorMap;
  newmat->hCircleInnerRadius = this->hCircleInnerRadius;
  newmat->hCircleOuterRadius = this->hCircleOuterRadius;

  newmat->meType = this->meType;
  newmat->meUIColorMode = this->meUIColorMode;

  newmat->PosScale = this->PosScale;
  newmat->PosBias = this->PosBias;
  newmat->Color = this->Color;

  return newmat;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUI::gpuInit(ork::lev2::Context* pTarg) {
  // printf( "_shader<%p>\n", _shader );

    auto mtl_load_req = std::make_shared<asset::LoadRequest>();
    mtl_load_req->_asset_path = "orkshader://ui";
    _shaderasset = asset::AssetManager<FxShaderAsset>::load(mtl_load_req);
    _shader      = _shaderasset->GetFxShader();
    // printf( "HMODFX<%p> pTarg<%p>\n", _shader, pTarg );
    OrkAssertI(_shader != 0, "did you copy the shaders folder!\n");

  hTekMod = pTarg->FXI()->technique(_shader, "uidev_modcolor");

  // OrkAssert(hTekMod);
  hTekVtx    = pTarg->FXI()->technique(_shader, "ui_vtx");
  hTekModVtx = pTarg->FXI()->technique(_shader, "ui_vtxmod");
  hTekCircle = pTarg->FXI()->technique(_shader, "uicircle");

  hTransform = pTarg->FXI()->parameter(_shader, "mvp");
  hModColor  = pTarg->FXI()->parameter(_shader, "ModColor");
}

/////////////////////////////////////////////////////////////////////////

int GfxMaterialUI::BeginBlock(Context* pTarg, const RenderContextInstData& MatCtx) {
  const FxShaderTechnique* htek = 0;

  if(nullptr==hTekMod){
    gpuInit(pTarg);
  }

  htek = hTekMod;
  switch (meType) {
    case ETYPE_STANDARD: {
      switch (meUIColorMode) {
        default:
        case UiColorMode::MOD:
          htek = hTekMod;
          break;
        case UiColorMode::VTX:
          htek = hTekVtx;
          break;
        case UiColorMode::MODVTX:
          htek = hTekModVtx;
          break;
      }
      break;
    }
    case ETYPE_CIRCLE:
      htek = hTekCircle;
      break;
    default:
      OrkAssert(false);
  }

  int inumpasses = pTarg->FXI()->BeginBlock(htek, MatCtx);
  ///////////////////////////////
  ///////////////////////////////

  const fmtx4& MatMVP = pTarg->MTXI()->RefMVPMatrix();

  // Debug: check for NaN in MVP and log context info
  static int nan_log_count = 0;
  const float* mvpdata = MatMVP.asArray();
  if (nan_log_count < 5 && (std::isnan(mvpdata[0]) || std::isnan(mvpdata[1]))) {
    printf("GfxMaterialUI::BeginBlock NaN! pTarg=%p MTXI=%p mvp=[%g %g %g %g]\n",
           (void*)pTarg, (void*)pTarg->MTXI(),
           mvpdata[0], mvpdata[1], mvpdata[2], mvpdata[3]);
    nan_log_count++;
  }

  ///////////////////////////////

  //pTarg->FXI()->BindPass(iPass);
  pTarg->FXI()->bindParamMatrix(hTransform, MatMVP);
  pTarg->FXI()->bindParamVect4(hModColor, pTarg->RefModColor());
  pTarg->FXI()->applyRasterState(*_rasterstate);
  pTarg->FXI()->CommitParams();
  return inumpasses;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUI::wrappedDraw(Context* context, void_lambda_t drawcb){
  auto rcfd = std::make_shared<RenderContextFrameData>(context);
  RenderContextInstData RCID(rcfd);
  this->BeginBlock(context, RCID);
  drawcb();
  this->EndBlock(context);
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUI::EndBlock(Context* pTarg) {
  pTarg->FXI()->EndBlock();
}

/////////////////////////////////////////////////////////////////////////

GfxMaterialUIText::GfxMaterialUIText(Context* pTarg)
    : hTek(0)
    , hTransform(0)
    , hModColor(0)
    , hColorMap(0) {
  _rasterstate->setBlendingMacro(BlendingMacro::OFF);
  _rasterstate->setDepthTest(EDepthTest::ALWAYS);
  _rasterstate->setWriteMaskZ(false);
  _rasterstate->setCullTest(ECullTest::OFF);

  miNumPasses = 1;

  auto mtl_load_req = std::make_shared<asset::LoadRequest>();
  mtl_load_req->_asset_path = "orkshader://ui";

  _shaderasset = asset::AssetManager<FxShaderAsset>::load(mtl_load_req);
  _shader      = _shaderasset->GetFxShader();
  if (pTarg) {
    gpuInit(pTarg);
  }
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUIText::gpuInit(ork::lev2::Context* pTarg) {
  hTek = pTarg->FXI()->technique(_shader, "uitext");

  hTransform = pTarg->FXI()->parameter(_shader, "mvp");
  hModColor  = pTarg->FXI()->parameter(_shader, "ModColor");
  hColorMap  = pTarg->FXI()->parameter(_shader, "ColorMap");

  _rasterstate->setDepthTest(ork::lev2::EDepthTest::OFF);
}

/////////////////////////////////////////////////////////////////////////

int GfxMaterialUIText::BeginBlock(Context* pTarg, const RenderContextInstData& MatCtx) {
  int inumpasses = pTarg->FXI()->BeginBlock(hTek, MatCtx);
  ///////////////////////////////

  const fmtx4& MatMVP = pTarg->MTXI()->RefMVPMatrix();

  pTarg->FXI()->bindParamMatrix(hTransform, MatMVP);

  ///////////////////////////////

  pTarg->FXI()->bindParamTexture(hColorMap, GetTexture(ETEXDEST_DIFFUSE).mpTexture);
  pTarg->FXI()->bindParamVect4(hModColor, pTarg->RefModColor());
  pTarg->FXI()->CommitParams();
  pTarg->FXI()->applyRasterState(*_rasterstate);
  return inumpasses;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUIText::EndBlock(Context* pTarg) {
  pTarg->FXI()->EndBlock();
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUIText::UpdateMVPMatrix(Context* context) {
  const fmtx4& MatMVP = context->MTXI()->RefMVPMatrix();
  context->FXI()->bindParamMatrix(hTransform, MatMVP);
  context->FXI()->CommitParams();
}

/////////////////////////////////////////////////////////////////////////

GfxMaterialUITextured::GfxMaterialUITextured(Context* pTarg, const std::string& Technique)
    : _techniqueName(Technique) {
  miNumPasses = 1;
  _rasterstate->setBlendingMacro(BlendingMacro::OFF);
  _rasterstate->setDepthTest(EDepthTest::OFF);
  _rasterstate->setCullTest(ECullTest::OFF);
  _rasterstate->_force = true;

  if (pTarg) {
    gpuInit(pTarg);
  }
}

void GfxMaterialUITextured::ClassInit() {
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUITextured::EffectInit(void) {
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUITextured::gpuInit(ork::lev2::Context* pTarg) {
  if (hTek == nullptr) {

    auto mtl_load_req = std::make_shared<asset::LoadRequest>();
    mtl_load_req->_asset_path = "orkshader://ui";

    _shaderasset = asset::AssetManager<FxShaderAsset>::load(mtl_load_req);
    _shader      = _shaderasset->GetFxShader();

    hTek = pTarg->FXI()->technique(_shader, _techniqueName);
    hTekStereo = pTarg->FXI()->technique(_shader, "uitextured_stereo");
    //printf("HMODFX<%p> pTarg<%p> hTek<%p>\n", (void*) _shader, (void*) pTarg, (void*) hTek);

    hTransform = pTarg->FXI()->parameter(_shader, "mvp");
    hModColor  = pTarg->FXI()->parameter(_shader, "ModColor");
    hColorMap  = pTarg->FXI()->parameter(_shader, "ColorMap");
  }
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUITextured::gpuInit(ork::lev2::Context* pTarg, const std::string& Technique) {
  if (hTek == nullptr) {
    _techniqueName = Technique;
    gpuInit(pTarg);
  }
}

/////////////////////////////////////////////////////////////////////////

int GfxMaterialUITextured::BeginBlock(Context* pTarg, const RenderContextInstData& RCID) {
  auto rcfd2 = pTarg->topRenderContextFrameData();
  OrkAssert(rcfd2);
  const auto& CPD = rcfd2->topCPD();
  int inumpasses = pTarg->FXI()->BeginBlock(hTek, RCID);
  const fmtx4& MatMVP = pTarg->MTXI()->RefMVPMatrix();
  auto texture = GetTexture(ETEXDEST_DIFFUSE).mpTexture;
  OrkAssert(texture != nullptr);
  pTarg->FXI()->bindParamMatrix(hTransform, MatMVP);
  pTarg->FXI()->bindParamTexture(hColorMap, texture);
  pTarg->FXI()->bindParamVect4(hModColor, pTarg->RefModColor());
  pTarg->FXI()->applyRasterState(*_rasterstate);
  pTarg->FXI()->CommitParams();
  return inumpasses;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUITextured::EndBlock(Context* pTarg) {
  pTarg->FXI()->EndBlock();
}

}} // namespace ork::lev2
