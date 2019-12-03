////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/renderer/compositormaterial.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/enum_serializer.inl>
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
CompositingMaterial::CompositingMaterial()
    : hModFX(nullptr)
    , hMapA(nullptr)
    , hMapB(nullptr)
    , hMapC(nullptr)
    , hBiasA(nullptr)
    , hBiasB(nullptr)
    , hBiasC(nullptr)
    , hTekOp2AmulB(nullptr)
    , hTekOp2AdivB(nullptr)
    , hTekBoverAplusC(nullptr)
    , hTekAplusBplusC(nullptr)
    , hTekAlerpBwithC(nullptr)
    , hTekCurrent(nullptr)
    , mCurrentTextureA(nullptr)
    , mCurrentTextureB(nullptr)
    , mCurrentTextureC(nullptr)
    , mBiasA(0.0f, 0.0f, 0.0f, 0.0f)
    , mBiasB(0.0f, 0.0f, 0.0f, 0.0f)
    , mLevelA(1.0f, 1.0f, 1.0f, 1.0f)
    , mLevelB(1.0f, 1.0f, 1.0f, 1.0f)
    , mLevelC(1.0f, 1.0f, 1.0f, 1.0f) {}
/////////////////////////////////////////////////
CompositingMaterial::~CompositingMaterial() {}
/////////////////////////////////////////////////
void CompositingMaterial::Init(lev2::GfxTarget* pTarg) {
  if (0 == hModFX) {
    hModFX = asset::AssetManager<lev2::FxShaderAsset>::Load("orkshader://compositor")->GetFxShader();

    auto fxi = pTarg->FXI();

    hMatMVP = fxi->parameter(hModFX, "MatMVP");
    hBiasA  = fxi->parameter(hModFX, "BiasA");
    hBiasB  = fxi->parameter(hModFX, "BiasB");
    hBiasC  = fxi->parameter(hModFX, "BiasC");

    hLevelA = fxi->parameter(hModFX, "LevelA");
    hLevelB = fxi->parameter(hModFX, "LevelB");
    hLevelC = fxi->parameter(hModFX, "LevelC");

    hMapA = fxi->parameter(hModFX, "MapA");
    hMapB = fxi->parameter(hModFX, "MapB");
    hMapC = fxi->parameter(hModFX, "MapC");

    hTekOp2AmulB = fxi->technique(hModFX, "Op2AmulB");
    hTekOp2AdivB = fxi->technique(hModFX, "Op2AdivB");

    hTekBoverAplusC = fxi->technique(hModFX, "BoverAplusC");
    hTekAplusBplusC = fxi->technique(hModFX, "AplusBplusC");
    hTekAlerpBwithC = fxi->technique(hModFX, "AlerpBwithC");

    hTekAsolo = fxi->technique(hModFX, "Asolo");
    hTekBsolo = fxi->technique(hModFX, "Bsolo");
    hTekCsolo = fxi->technique(hModFX, "Csolo");

    _rasterstate.SetCullTest(ork::lev2::ECULLTEST_OFF);
    _rasterstate.SetAlphaTest(ork::lev2::EALPHATEST_OFF);
    _rasterstate.SetDepthTest(ork::lev2::EDEPTHTEST_OFF);
  }
}
/////////////////////////////////////////////////
void CompositingMaterial::SetTechnique(const std::string& tek) {
  if (tek == "Op2AmulB")
    hTekCurrent = hTekOp2AmulB;
  if (tek == "Op2AdivB")
    hTekCurrent = hTekOp2AdivB;

  if (tek == "BoverAplusC")
    hTekCurrent = hTekBoverAplusC;
  if (tek == "AplusBplusC")
    hTekCurrent = hTekAplusBplusC;
  if (tek == "AlerpBwithC")
    hTekCurrent = hTekAlerpBwithC;

  if (tek == "Asolo")
    hTekCurrent = hTekAsolo;
  if (tek == "Bsolo")
    hTekCurrent = hTekBsolo;
  if (tek == "Csolo")
    hTekCurrent = hTekCsolo;
}
/////////////////////////////////////////////////
bool CompositingMaterial::BeginPass(lev2::GfxTarget* pTarg, int iPass) {
  // printf("CompositorMtl draw\n");

  pTarg->RSI()->BindRasterState(_rasterstate);
  pTarg->FXI()->BindPass(hModFX, iPass);
  pTarg->FXI()->BindParamMatrix(hModFX, hMatMVP, pTarg->MTXI()->RefMVPMatrix());

  pTarg->FXI()->BindParamVect4(hModFX, hLevelA, mLevelA);
  pTarg->FXI()->BindParamVect4(hModFX, hLevelB, mLevelB);
  pTarg->FXI()->BindParamVect4(hModFX, hLevelC, mLevelC);

  pTarg->FXI()->BindParamVect4(hModFX, hBiasA, mBiasA);
  pTarg->FXI()->BindParamVect4(hModFX, hBiasB, mBiasB);
  pTarg->FXI()->BindParamVect4(hModFX, hBiasC, mBiasC);

  if (mCurrentTextureA && hMapA) {
    pTarg->FXI()->BindParamCTex(hModFX, hMapA, mCurrentTextureA);
  }
  if (mCurrentTextureB && hMapB) {
    pTarg->FXI()->BindParamCTex(hModFX, hMapB, mCurrentTextureB);
  }
  if (mCurrentTextureC && hMapC) {
    pTarg->FXI()->BindParamCTex(hModFX, hMapC, mCurrentTextureC);
  }

  pTarg->FXI()->CommitParams();
  return true;
}
/////////////////////////////////////////////////
void CompositingMaterial::EndPass(lev2::GfxTarget* pTarg) { pTarg->FXI()->EndPass(hModFX); }
/////////////////////////////////////////////////
int CompositingMaterial::BeginBlock(lev2::GfxTarget* pTarg, const lev2::RenderContextInstData& MatCtx) {
  pTarg->FXI()->BindTechnique(hModFX, hTekCurrent);
  int inumpasses = pTarg->FXI()->BeginBlock(hModFX, MatCtx);
  return inumpasses;
}
/////////////////////////////////////////////////
void CompositingMaterial::EndBlock(lev2::GfxTarget* pTarg) { pTarg->FXI()->EndBlock(hModFX); }
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
