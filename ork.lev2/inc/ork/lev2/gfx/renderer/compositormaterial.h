////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/renderer/frametek.h>

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

class CompositingMaterial final : public lev2::GfxMaterial {
public:
  CompositingMaterial();
  ~CompositingMaterial();
  /////////////////////////////////////////////////
  void Update(void) {
  }
  void gpuInit(lev2::Context* context) final;
  bool BeginPass(lev2::Context* pTARG, int iPass = 0) final;
  void EndPass(lev2::Context* pTARG) final;
  int BeginBlock(lev2::Context* pTARG, const lev2::RenderContextInstData& MatCtx) final;
  void EndBlock(lev2::Context* pTARG) final;
  /////////////////////////////////////////////////
  void SetTextureA(lev2::Texture* ptex) {
    mCurrentTextureA = ptex;
  }
  void SetTextureB(lev2::Texture* ptex) {
    mCurrentTextureB = ptex;
  }
  void SetTextureC(lev2::Texture* ptex) {
    mCurrentTextureC = ptex;
  }
  void SetLevelA(const fvec4& la) {
    mLevelA = la;
  }
  void SetLevelB(const fvec4& lb) {
    mLevelB = lb;
  }
  void SetLevelC(const fvec4& lc) {
    mLevelC = lc;
  }
  void SetBiasA(const fvec4& ba) {
    mBiasA = ba;
  }
  void SetBiasB(const fvec4& bb) {
    mBiasB = bb;
  }
  void SetBiasC(const fvec4& bc) {
    mBiasC = bc;
  }
  void SetTechnique(const std::string& tek);
  /////////////////////////////////////////////////
  lev2::Texture* mCurrentTextureA = nullptr;
  lev2::Texture* mCurrentTextureB = nullptr;
  lev2::Texture* mCurrentTextureC = nullptr;

  const lev2::FxShaderTechnique* hTekOp2AmulB = nullptr;
  const lev2::FxShaderTechnique* hTekOp2AdivB = nullptr;

  const lev2::FxShaderTechnique* hTekBoverAplusC = nullptr;
  const lev2::FxShaderTechnique* hTekAplusBplusC = nullptr;
  const lev2::FxShaderTechnique* hTekAlerpBwithC = nullptr;
  const lev2::FxShaderTechnique* hTekAsolo = nullptr;
  const lev2::FxShaderTechnique* hTekBsolo = nullptr;
  const lev2::FxShaderTechnique* hTekCsolo = nullptr;

  const lev2::FxShaderTechnique* hTekCurrent = nullptr;

  const lev2::FxShaderParam* hMapA = nullptr;
  const lev2::FxShaderParam* hMapB = nullptr;
  const lev2::FxShaderParam* hLevelA = nullptr;
  const lev2::FxShaderParam* hLevelB = nullptr;
  const lev2::FxShaderParam* hLevelC = nullptr;
  const lev2::FxShaderParam* hBiasA = nullptr;
  const lev2::FxShaderParam* hBiasB = nullptr;
  const lev2::FxShaderParam* hBiasC = nullptr;
  const lev2::FxShaderParam* hMapC = nullptr;
  const lev2::FxShaderParam* hMatMVP = nullptr;
  lev2::FxShader* _shader = nullptr;

  fxshaderasset_ptr_t _shaderasset;

  fvec4 mBiasA;
  fvec4 mBiasB;
  fvec4 mBiasC;
  fvec4 mLevelA;
  fvec4 mLevelB;
  fvec4 mLevelC;
};

} // namespace ork::lev2
