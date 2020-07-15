////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/gfxmaterial.h>

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

class GfxMaterialUI final : public GfxMaterial {
  RttiDeclareAbstract(GfxMaterialUI, GfxMaterial);
  //////////////////////////////////////////////////////////////////////////////

public:
  GfxMaterialUI(Context* pTarg = 0);
  ~GfxMaterialUI();

  void Update(void) override {
  }

  void gpuInit(Context* context) override;

  bool BeginPass(Context* pTARG, int iPass = 0) override;
  void EndPass(Context* pTARG) override;
  int BeginBlock(Context* pTARG, const RenderContextInstData& MatCtx) override;
  void EndBlock(Context* pTARG) override;

  void SetUIColorMode(UiColorMode emod) {
    meUIColorMode = emod;
  }
  UiColorMode GetUIColorMode(void) {
    return meUIColorMode;
  }

  enum EType { ETYPE_STANDARD = 0, ETYPE_CIRCLE };

  void SetType(EType etyp) {
    meType = etyp;
  }

  //////////////////////////////////////////////////////////////////////////////

protected:
  fvec4 PosScale;
  fvec4 PosBias;
  fvec4 Color;
  fxshaderasset_ptr_t _shaderasset;
  FxShader* _shader;
  UiColorMode meUIColorMode;
  const FxShaderTechnique* hTekMod;
  const FxShaderTechnique* hTekVtx;
  const FxShaderTechnique* hTekModVtx;
  const FxShaderTechnique* hTekCircle;
  const FxShaderParam* hVPW;
  const FxShaderParam* hBias;
  const FxShaderParam* hScale;
  const FxShaderParam* hTransform;
  const FxShaderParam* hModColor;
  const FxShaderParam* hColorMap;
  const FxShaderParam* hCircleInnerRadius;
  const FxShaderParam* hCircleOuterRadius;
  EType meType;
};

using uimaterial_ptr_t = std::shared_ptr<GfxMaterialUI>;

///////////////////////////////////////////////////////////////////////////////

class GfxMaterialUIText final : public GfxMaterial {
  //////////////////////////////////////////////////////////////////////////////

public:
  static void ClassInit() {
  }

  GfxMaterialUIText(Context* pTarg = 0);

  void Update(void) override {
  }
  void gpuInit(Context* context) override;

  bool BeginPass(Context* pTARG, int iPass = 0) override;
  void EndPass(Context* pTARG) override;
  int BeginBlock(Context* pTARG, const RenderContextInstData& MatCtx) override;
  void EndBlock(Context* pTARG) override;

  //////////////////////////////////////////////////////////////////////////////

protected:
  fvec4 PosScale;
  fvec4 PosBias;
  fvec4 TexScale;
  fvec4 TexColor;

  fxshaderasset_ptr_t _shaderasset;
  FxShader* _shader;
  const FxShaderTechnique* hTek;
  const FxShaderParam* hVPW;
  const FxShaderParam* hBias;
  const FxShaderParam* hScale;
  const FxShaderParam* hUVScale;
  const FxShaderParam* hTransform;
  const FxShaderParam* hModColor;
  const FxShaderParam* hColorMap;
};

///////////////////////////////////////////////////////////////////////////////

class GfxMaterialUITextured final : public GfxMaterial {
public:
  static void ClassInit();
  GfxMaterialUITextured(Context* pTarg = 0, const std::string& Technique = "uitextured");
  void gpuInit(Context* context) override;
  void gpuInit(Context* context, const std::string& Technique);
  void Update(void) override {
  }
  bool BeginPass(Context* pTARG, int iPass = 0) override;
  void EndPass(Context* pTARG) override;
  int BeginBlock(Context* pTARG, const RenderContextInstData& MatCtx) override;
  void EndBlock(Context* pTARG) override;

  void EffectInit(void);

protected:
  std::string mTechniqueName;

  fvec4 Color;
  fxshaderasset_ptr_t _shaderasset;
  FxShader* _shader;

  const FxShaderTechnique* hTek;
  const FxShaderParam* hVPW;
  const FxShaderParam* hBias;
  const FxShaderParam* hScale;
  const FxShaderParam* hTransform;
  const FxShaderParam* hModColor;
  const FxShaderParam* hColorMap;
};

uimaterial_ptr_t defaultUIMaterial();

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
