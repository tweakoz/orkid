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

material_ptr_t defaultUIMaterial() {
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
    , meUIColorMode(EUICOLOR_MOD) {
  miNumPasses = 1;
  _rasterstate.SetShadeModel(ESHADEMODEL_SMOOTH);
  _rasterstate.SetAlphaTest(EALPHATEST_OFF);
  _rasterstate.SetBlending(EBLENDING_OFF);
  _rasterstate.SetDepthTest(EDEPTHTEST_LEQUALS);
  _rasterstate.SetZWriteMask(false);
  _rasterstate.SetCullTest(ECULLTEST_OFF);

  hModFX = asset::AssetManager<FxShaderAsset>::Load("orkshader://ui")->GetFxShader();
  // printf( "HMODFX<%p> pTarg<%p>\n", hModFX, pTarg );
  OrkAssertI(hModFX != 0, "did you copy the shaders folder!\n");

  if (pTarg) {
    Init(pTarg);
  }
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUI::Init(ork::lev2::Context* pTarg) {
  // printf( "hModFX<%p>\n", hModFX );

  hTekMod = pTarg->FXI()->technique(hModFX, "uidev_modcolor");

  // OrkAssert(hTekMod);
  hTekVtx    = pTarg->FXI()->technique(hModFX, "ui_vtx");
  hTekModVtx = pTarg->FXI()->technique(hModFX, "ui_vtxmod");
  hTekCircle = pTarg->FXI()->technique(hModFX, "uicircle");

  hTransform = pTarg->FXI()->parameter(hModFX, "mvp");
  hModColor  = pTarg->FXI()->parameter(hModFX, "ModColor");
}

/////////////////////////////////////////////////////////////////////////

int GfxMaterialUI::BeginBlock(Context* pTarg, const RenderContextInstData& MatCtx) {
  const FxShaderTechnique* htek = 0;

  htek = hTekMod;
  switch (meType) {
    case ETYPE_STANDARD: {
      switch (meUIColorMode) {
        default:
        case EUICOLOR_MOD:
          htek = hTekMod;
          break;
        case EUICOLOR_VTX:
          htek = hTekVtx;
          break;
        case EUICOLOR_MODVTX:
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

  pTarg->FXI()->BindTechnique(hModFX, htek);
  int inumpasses = pTarg->FXI()->BeginBlock(hModFX, MatCtx);
  return inumpasses;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUI::EndBlock(Context* pTarg) {
  pTarg->FXI()->EndBlock(hModFX);
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUI::EndPass(Context* pTarg) {
  pTarg->FXI()->EndPass(hModFX);
}

/////////////////////////////////////////////////////////////////////////

bool GfxMaterialUI::BeginPass(Context* pTarg, int iPass) {
  ///////////////////////////////
  pTarg->RSI()->BindRasterState(_rasterstate);
  ///////////////////////////////

  const fmtx4& MatMVP = pTarg->MTXI()->RefMVPMatrix();

  ///////////////////////////////

  pTarg->FXI()->BindPass(hModFX, iPass);
  pTarg->FXI()->BindParamMatrix(hModFX, hTransform, MatMVP);
  pTarg->FXI()->BindParamVect4(hModFX, hModColor, pTarg->RefModColor());
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
  _rasterstate.SetBlending(EBLENDING_OFF);
  _rasterstate.SetDepthTest(EDEPTHTEST_ALWAYS);
  _rasterstate.SetZWriteMask(false);
  _rasterstate.SetCullTest(ECULLTEST_OFF);

  miNumPasses = 1;

  hModFX = asset::AssetManager<FxShaderAsset>::Load("orkshader://ui")->GetFxShader();

  if (pTarg) {
    Init(pTarg);
  }
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUIText::Init(ork::lev2::Context* pTarg) {
  hTek = pTarg->FXI()->technique(hModFX, "uitext");

  hTransform = pTarg->FXI()->parameter(hModFX, "mvp");
  hModColor  = pTarg->FXI()->parameter(hModFX, "ModColor");
  hColorMap  = pTarg->FXI()->parameter(hModFX, "ColorMap");

  _rasterstate.SetDepthTest(ork::lev2::EDEPTHTEST_OFF);
}

/////////////////////////////////////////////////////////////////////////

int GfxMaterialUIText::BeginBlock(Context* pTarg, const RenderContextInstData& MatCtx) {
  pTarg->FXI()->BindTechnique(hModFX, hTek);
  int inumpasses = pTarg->FXI()->BeginBlock(hModFX, MatCtx);
  return inumpasses;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUIText::EndBlock(Context* pTarg) {
  pTarg->FXI()->EndBlock(hModFX);
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUIText::EndPass(Context* pTarg) {
  pTarg->FXI()->EndPass(hModFX);
}

/////////////////////////////////////////////////////////////////////////

bool GfxMaterialUIText::BeginPass(Context* pTarg, int iPass) {
  pTarg->FXI()->BindPass(hModFX, iPass);

  ///////////////////////////////
  SRasterState& RasterState = _rasterstate; // pTarg->RSI()->RefUIRasterState();

  // RasterState.SetAlphaTest( EALPHATEST_GREATER, 0.0f );
  // pTarg->RSI()->BindRasterState( RasterState );

  ///////////////////////////////

  const fmtx4& MatMVP = pTarg->MTXI()->RefMVPMatrix();

  pTarg->FXI()->BindParamMatrix(hModFX, hTransform, MatMVP);

  ///////////////////////////////

  pTarg->FXI()->BindParamCTex(hModFX, hColorMap, GetTexture(ETEXDEST_DIFFUSE).mpTexture);
  pTarg->FXI()->BindParamVect4(hModFX, hModColor, pTarg->RefModColor());
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
  _rasterstate.SetBlending(EBLENDING_OFF);
  _rasterstate.SetDepthTest(EDEPTHTEST_LEQUALS);
  _rasterstate.SetCullTest(ECULLTEST_OFF);

  mTechniqueName = Technique;

  if (pTarg) {
    Init(pTarg);
  }
}

void GfxMaterialUITextured::ClassInit() {
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUITextured::EffectInit(void) {
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUITextured::Init(ork::lev2::Context* pTarg) {
  hModFX = asset::AssetManager<FxShaderAsset>::Load("orkshader://ui")->GetFxShader();
  // printf( "HMODFX<%p> pTarg<%p>\n", hModFX, pTarg );
  hTek = pTarg->FXI()->technique(hModFX, mTechniqueName);

  hTransform = pTarg->FXI()->parameter(hModFX, "mvp");
  hModColor  = pTarg->FXI()->parameter(hModFX, "ModColor");
  hColorMap  = pTarg->FXI()->parameter(hModFX, "ColorMap");
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUITextured::Init(ork::lev2::Context* pTarg, const std::string& Technique) {
  mTechniqueName = Technique;
  Init(pTarg);
}

/////////////////////////////////////////////////////////////////////////

int GfxMaterialUITextured::BeginBlock(Context* pTarg, const RenderContextInstData& MatCtx) {
  pTarg->FXI()->BindTechnique(hModFX, hTek);
  int inumpasses = pTarg->FXI()->BeginBlock(hModFX, MatCtx);
  return inumpasses;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUITextured::EndBlock(Context* pTarg) {
  pTarg->FXI()->EndBlock(hModFX);
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUITextured::EndPass(Context* pTarg) {
  pTarg->FXI()->EndPass(hModFX);
}

////////////////////////////////////////////////////////////2/////////////

bool GfxMaterialUITextured::BeginPass(Context* pTarg, int iPass) {
  ///////////////////////////////
  pTarg->RSI()->BindRasterState(_rasterstate);
  ///////////////////////////////

  const fmtx4& MatMVP = pTarg->MTXI()->RefMVPMatrix();

  pTarg->FXI()->BindPass(hModFX, iPass);
  pTarg->FXI()->BindParamMatrix(hModFX, hTransform, MatMVP);
  pTarg->FXI()->BindParamCTex(hModFX, hColorMap, GetTexture(ETEXDEST_DIFFUSE).mpTexture);
  pTarg->FXI()->BindParamVect4(hModFX, hModColor, pTarg->RefModColor());
  pTarg->FXI()->CommitParams();
  return true;
}

}} // namespace ork::lev2
