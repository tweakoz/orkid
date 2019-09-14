////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/gfxmaterial.h>

namespace ork { namespace lev2 {
//
///////////////////////////////////////////////////////////////////////////////

class GfxMaterial3DSolid : public GfxMaterial {
  RttiDeclareConcrete(GfxMaterial3DSolid, GfxMaterial);

public:
  static void ClassInit();
  GfxMaterial3DSolid(GfxTarget* pTARG = 0);
  GfxMaterial3DSolid(
      GfxTarget* pTARG, const char* puserfx, const char* pusertek, bool allowcompilefailure = false, bool unmanaged = false);
  ~GfxMaterial3DSolid() final{};

  void SetVolumeTexture(Texture* ptex) { mVolumeTexture = ptex; }
  void SetTexture(Texture* ptex) { mCurrentTexture = ptex; }
  void SetTexture2(Texture* ptex) { mCurrentTexture2 = ptex; }
  void SetTexture3(Texture* ptex) { mCurrentTexture3 = ptex; }
  void SetTexture4(Texture* ptex) { mCurrentTexture4 = ptex; }

  void SetUser0(const fvec4& vuser) { mUser0 = vuser; }
  void SetUser1(const fvec4& vuser) { mUser1 = vuser; }
  void SetUser2(const fvec4& vuser) { mUser2 = vuser; }
  void SetUser3(const fvec4& vuser) { mUser3 = vuser; }

  void SetUserFx(const char* puserfx, const char* pusertek) {
    mUserFxName  = puserfx;
    mUserTekName = pusertek;
  }

  bool IsUserFxOk() const;

  ////////////////////////////////////////////

  enum EColorMode {
    EMODE_VERTEXMOD_COLOR = 0,
    EMODE_VERTEX_COLOR,
    EMODE_TEX_COLOR,
    EMODE_TEXMOD_COLOR,
    EMODE_TEXTEXMOD_COLOR,
    EMODE_TEXVERTEX_COLOR,
    EMODE_MOD_COLOR,
    EMODE_INTERNAL_COLOR,
    EMODE_USER,
  };

  ////////////////////////////////////////////

  EColorMode GetColorMode() const { return meColorMode; }
  void SetColorMode(EColorMode emode) { meColorMode = emode; }
  void SetColor(const fvec4& color) { Color = color; }
  const fvec4& GetColor() const { return Color; }
  void SetNoiseAmp(const fvec4& color) { mNoiseAmp = color; }
  void SetNoiseFreq(const fvec4& color) { mNoiseFreq = color; }
  void SetNoiseShift(const fvec4& color) { mNoiseShift = color; }

  ////////////////////////////////////////////

  void SetWRotMatrix(const fmtx3& wrot) { mRotMatrix = wrot; }
  void SetAuxMatrix(const fmtx4& mtx) { mMatAux = mtx; }

  bool BeginPass(GfxTarget* pTARG, int iPass = 0) final;
  void EndPass(GfxTarget* pTARG) final;
  int BeginBlock(GfxTarget* pTARG, const RenderContextInstData& MatCtx) final;
  void EndBlock(GfxTarget* pTARG) final;
  void Init(GfxTarget* pTarg) final;

  bool _enablePick = false;

protected:
  void Update(void) final {}
  void SetMaterialProperty(const char* prop, const char* val) final;

  EColorMode meColorMode;
  fvec4 mNoiseAmp;
  fvec4 mNoiseFreq;
  fvec4 mNoiseShift;
  fvec4 Color;
  fvec4 mUser0;
  fvec4 mUser1;
  fvec4 mUser2;
  fvec4 mUser3;
  FxShader* hModFX;
  Texture* mVolumeTexture;
  Texture* mCurrentTexture;
  Texture* mCurrentTexture2;
  Texture* mCurrentTexture3;
  Texture* mCurrentTexture4;
  std::string mUserFxName;
  std::string mUserTekName;
  fmtx4 mMatAux;
  fmtx3 mRotMatrix;
  bool mUnManaged;
  bool mAllowCompileFailure;

  const FxShaderTechnique* hTekUser;
  const FxShaderTechnique* hTekTexColor;
  const FxShaderTechnique* hTekTexModColor;
  const FxShaderTechnique* hTekTexTexModColor;
  const FxShaderTechnique* hTekTexVertexColor;
  const FxShaderTechnique* hTekVertexColor;
  const FxShaderTechnique* hTekVertexModColor;
  const FxShaderTechnique* hTekModColor;
  const FxShaderTechnique* hTekPick;

  const FxShaderParam* hMatM;
  const FxShaderParam* hMatV;
  const FxShaderParam* hMatP;
  const FxShaderParam* hMatMV;
  const FxShaderParam* hMatMVP;
  const FxShaderParam* hMatMVPL;
  const FxShaderParam* hMatMVPR;
  const FxShaderParam* hMatMVPC;
  const FxShaderParam* hMatAux;
  const FxShaderParam* hMatRot;
  const FxShaderParam* hVolumeMap;
  const FxShaderParam* hColorMap;
  const FxShaderParam* hColorMap2;
  const FxShaderParam* hColorMap3;
  const FxShaderParam* hColorMap4;
  const FxShaderParam* hParamUser0;
  const FxShaderParam* hParamUser1;
  const FxShaderParam* hParamUser2;
  const FxShaderParam* hParamUser3;
  const FxShaderParam* hParamModColor;
  const FxShaderParam* hParamTime;
  const FxShaderParam* hParamNoiseShift;
  const FxShaderParam* hParamNoiseFreq;
  const FxShaderParam* hParamNoiseAmp;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
