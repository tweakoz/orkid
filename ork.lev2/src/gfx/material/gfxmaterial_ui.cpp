////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/lev2_asset.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::GfxMaterialUI, "MaterialUI")

namespace ork { namespace lev2 {

uimaterial_ptr_t defaultUIMaterial() {
  static auto mtl = std::make_shared<GfxMaterialUI>(GfxEnv::GetRef().loadingContext());
  return mtl;
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
    , hTekMod(nullptr)
    , hTekVtx(nullptr)
    , hTekModVtx(nullptr)
    , hTekCircle(nullptr)
    , hTransform(nullptr)
    , hModColor(nullptr)
    , meUIColorMode(UiColorMode::MOD) {
  miNumPasses = 1;
  _rasterstate.SetShadeModel(ESHADEMODEL_SMOOTH);
  _rasterstate.SetAlphaTest(EALPHATEST_OFF);
  _rasterstate.SetBlending(Blending::OFF);
  _rasterstate.SetDepthTest(EDEPTHTEST_OFF);
  _rasterstate.SetZWriteMask(false);
  _rasterstate.SetCullTest(ECULLTEST_OFF);

  _shaderasset = asset::AssetManager<FxShaderAsset>::load("orkshader://ui");
  _shader      = _shaderasset->GetFxShader();
  // printf( "HMODFX<%p> pTarg<%p>\n", _shader, pTarg );
  OrkAssertI(_shader != 0, "did you copy the shaders folder!\n");

  if (pTarg) {
    gpuInit(pTarg);
  }
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUI::gpuInit(ork::lev2::Context* pTarg) {
  // printf( "_shader<%p>\n", _shader );

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
  return inumpasses;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUI::EndBlock(Context* pTarg) {
  pTarg->FXI()->EndBlock();
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUI::EndPass(Context* pTarg) {
  pTarg->FXI()->EndPass();
}

/////////////////////////////////////////////////////////////////////////

bool GfxMaterialUI::BeginPass(Context* pTarg, int iPass) {
  ///////////////////////////////
  pTarg->RSI()->BindRasterState(_rasterstate);
  ///////////////////////////////

  const fmtx4& MatMVP = pTarg->MTXI()->RefMVPMatrix();

  ///////////////////////////////

  pTarg->FXI()->BindPass(iPass);
  pTarg->FXI()->BindParamMatrix(hTransform, MatMVP);
  pTarg->FXI()->BindParamVect4(hModColor, pTarg->RefModColor());
  pTarg->FXI()->CommitParams();

  return true;
}

/////////////////////////////////////////////////////////////////////////

GfxMaterialUIText::GfxMaterialUIText(Context* pTarg)
    : hTek(0)
    , hTransform(0)
    , hModColor(0)
    , hColorMap(0) {
  _rasterstate.SetAlphaTest(EALPHATEST_GREATER, 0.0f);
  _rasterstate.SetBlending(Blending::OFF);
  _rasterstate.SetDepthTest(EDEPTHTEST_ALWAYS);
  _rasterstate.SetZWriteMask(false);
  _rasterstate.SetCullTest(ECULLTEST_OFF);

  miNumPasses = 1;

  _shaderasset = asset::AssetManager<FxShaderAsset>::load("orkshader://ui");
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

  _rasterstate.SetDepthTest(ork::lev2::EDEPTHTEST_OFF);
}

/////////////////////////////////////////////////////////////////////////

int GfxMaterialUIText::BeginBlock(Context* pTarg, const RenderContextInstData& MatCtx) {
  int inumpasses = pTarg->FXI()->BeginBlock(hTek, MatCtx);
  return inumpasses;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUIText::EndBlock(Context* pTarg) {
  pTarg->FXI()->EndBlock();
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUIText::EndPass(Context* pTarg) {
  pTarg->FXI()->EndPass();
}

/////////////////////////////////////////////////////////////////////////

bool GfxMaterialUIText::BeginPass(Context* pTarg, int iPass) {
  pTarg->FXI()->BindPass(iPass);

  ///////////////////////////////
  SRasterState& RasterState = _rasterstate; // pTarg->RSI()->RefUIRasterState();

  // RasterState.SetAlphaTest( EALPHATEST_GREATER, 0.0f );
  // pTarg->RSI()->BindRasterState( RasterState );

  ///////////////////////////////

  const fmtx4& MatMVP = pTarg->MTXI()->RefMVPMatrix();

  pTarg->FXI()->BindParamMatrix(hTransform, MatMVP);

  ///////////////////////////////

  pTarg->FXI()->BindParamCTex(hColorMap, GetTexture(ETEXDEST_DIFFUSE).mpTexture);
  pTarg->FXI()->BindParamVect4(hModColor, pTarg->RefModColor());
  pTarg->FXI()->CommitParams();

  return true;
}

/////////////////////////////////////////////////////////////////////////

GfxMaterialUITextured::GfxMaterialUITextured(Context* pTarg, const std::string& Technique)
    : hTek(nullptr)
    , mTechniqueName(Technique)
    , hTransform(nullptr)
    , hModColor(nullptr)
    , hColorMap(nullptr) {
  miNumPasses = 1;
  _rasterstate.SetShadeModel(ESHADEMODEL_SMOOTH);
  _rasterstate.SetAlphaTest(EALPHATEST_OFF);
  _rasterstate.SetBlending(Blending::OFF);
  _rasterstate.SetDepthTest(EDEPTHTEST_LEQUALS);
  _rasterstate.SetCullTest(ECULLTEST_OFF);

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
    _shaderasset = asset::AssetManager<FxShaderAsset>::load("orkshader://ui");
    _shader      = _shaderasset->GetFxShader();

    hTek = pTarg->FXI()->technique(_shader, mTechniqueName);
    printf("HMODFX<%p> pTarg<%p> hTek<%p>\n", _shader, pTarg, hTek);

    hTransform = pTarg->FXI()->parameter(_shader, "mvp");
    hModColor  = pTarg->FXI()->parameter(_shader, "ModColor");
    hColorMap  = pTarg->FXI()->parameter(_shader, "ColorMap");
  }
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUITextured::gpuInit(ork::lev2::Context* pTarg, const std::string& Technique) {
  if (hTek == nullptr) {
    mTechniqueName = Technique;
    gpuInit(pTarg);
  }
}

/////////////////////////////////////////////////////////////////////////

int GfxMaterialUITextured::BeginBlock(Context* pTarg, const RenderContextInstData& MatCtx) {
  int inumpasses = pTarg->FXI()->BeginBlock(hTek, MatCtx);
  return inumpasses;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUITextured::EndBlock(Context* pTarg) {
  pTarg->FXI()->EndBlock();
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUITextured::EndPass(Context* pTarg) {
  pTarg->FXI()->EndPass();
}

////////////////////////////////////////////////////////////2/////////////

bool GfxMaterialUITextured::BeginPass(Context* pTarg, int iPass) {
  ///////////////////////////////
  pTarg->RSI()->BindRasterState(_rasterstate);
  ///////////////////////////////

  const fmtx4& MatMVP = pTarg->MTXI()->RefMVPMatrix();

  pTarg->FXI()->BindPass(iPass);
  pTarg->FXI()->BindParamMatrix(hTransform, MatMVP);
  pTarg->FXI()->BindParamCTex(hColorMap, GetTexture(ETEXDEST_DIFFUSE).mpTexture);
  pTarg->FXI()->BindParamVect4(hModColor, pTarg->RefModColor());
  pTarg->FXI()->CommitParams();
  return true;
}

}} // namespace ork::lev2
