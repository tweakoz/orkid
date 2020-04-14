////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/lev2_asset.h>

namespace ork::lev2 {

struct FreestyleMaterial;
using freestyle_mtl_ptr_t = std::shared_ptr<FreestyleMaterial>;

///////////////////////////////////////////////////////////////////////////////

struct FreestyleMaterial : public GfxMaterial {

  FreestyleMaterial();
  ~FreestyleMaterial() final;

  void dump() const;

  ////////////////////////////////////////////
  // legacy interface
  ////////////////////////////////////////////

  bool BeginPass(Context* targ, int iPass = 0) final;
  void EndPass(Context* targ) final;
  int BeginBlock(Context* targ, const RenderContextInstData& RCID) final;
  void EndBlock(Context* targ) final;
  void Init(Context* targ) final;
  void Update() final;

  ////////////////////////////////////////////
  // new interface (WIP)
  ////////////////////////////////////////////

  void begin(const RenderContextFrameData& RCFD);
  void begin(const FxShaderTechnique* tek, const RenderContextFrameData& RCFD);
  void begin(const FxShaderTechnique* tekMono, const FxShaderTechnique* tekStereo, const RenderContextFrameData& RCFD);
  void end(const RenderContextFrameData& RCFD);
  void gpuInit(Context* targ, const AssetPath& assetname);
  void gpuInitFromShaderText(Context* targ, const std::string& shadername, const std::string& shadertext);

  int materialInstanceBeginBlock(materialinst_ptr_t minst, const RenderContextInstData& RCID) final;
  bool materialInstanceBeginPass(materialinst_ptr_t minst, const RenderContextInstData& RCID, int ipass) final;
  void materialInstanceEndPass(materialinst_ptr_t minst, const RenderContextInstData& RCID) final;
  void materialInstanceEndBlock(materialinst_ptr_t minst, const RenderContextInstData& RCID) final;

  const FxShaderTechnique* technique(std::string named);
  const FxShaderParam* param(std::string named);
  const FxShaderParamBlock* paramBlock(std::string named);

  ////////////////////////////////////////////
#if defined(ENABLE_COMPUTE_SHADERS)
  const FxShaderStorageBlock* storageBlock(std::string named);
  const FxComputeShader* computeShader(std::string named);
#endif
  ////////////////////////////////////////////

  void setInstanceMvpParams(materialinst_ptr_t minst, std::string monocam, std::string stereocamL, std::string stereocamR);
  void commit();
  void bindTechnique(const FxShaderTechnique* tek);
  void bindParamInt(const FxShaderParam* par, int value);
  void bindParamFloat(const FxShaderParam* par, float value);
  void bindParamCTex(const FxShaderParam* par, const Texture* tex);
  void bindParamVec2(const FxShaderParam* par, const fvec2& v);
  void bindParamVec3(const FxShaderParam* par, const fvec3& v);
  void bindParamVec4(const FxShaderParam* par, const fvec4& v);
  void bindParamMatrix(const FxShaderParam* par, const fmtx4& m);
  void bindParamMatrix(const FxShaderParam* par, const fmtx3& m);
  void bindParamMatrixArray(const FxShaderParam* par, const fmtx4* m, size_t len);

  ////////////////////////////////////////////

  FxShader* _shader = nullptr;

  std::set<const FxShaderTechnique*> _techniques;
  std::set<const FxShaderParam*> _params;
  std::set<const FxShaderParamBlock*> _paramBlocks;

  ////////////////////////////////////////////

  Context* _initialTarget = nullptr;

#if defined(ENABLE_COMPUTE_SHADERS)
  std::set<const FxShaderStorageBlock*> _storageBlocks;
  std::set<const FxComputeShader*> _computeShaders;
#endif
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
