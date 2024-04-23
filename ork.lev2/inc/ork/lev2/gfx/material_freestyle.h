////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/lev2_asset.h>

namespace ork::lev2 {


///////////////////////////////////////////////////////////////////////////////

struct FreestyleMaterial final : public GfxMaterial {

  static fxpipeline_ptr_t _createFxPipeline(const FxPipelinePermutation& permu, const FreestyleMaterial* mtl);

  FreestyleMaterial();
  ~FreestyleMaterial();

  void dump() const;

  ////////////////////////////////////////////
  // legacy interface
  ////////////////////////////////////////////

  bool BeginPass(Context* targ, int iPass = 0) override;
  void EndPass(Context* targ) override;
  int BeginBlock(Context* targ, const RenderContextInstData& RCID) override;
  void EndBlock(Context* targ) override;
  void gpuInit(Context* targ) override;
  void Update() override;

  ////////////////////////////////////////////
  // new interface (WIP)
  ////////////////////////////////////////////

  fxpipelinecache_constptr_t _doFxPipelineCache(fxpipelinepermutation_set_constptr_t perms) const final;

  void begin(const FxShaderTechnique* tek, rcfd_ptr_t RCFD);
  void begin(const FxShaderTechnique* tekMono, const FxShaderTechnique* tekStereo, rcfd_ptr_t RCFD);
  void end(rcfd_ptr_t RCFD);
  void gpuInit(Context* targ, const AssetPath& assetname);
  void gpuInitFromShaderText(Context* targ, const std::string& shadername, const std::string& shadertext);

  const FxShaderTechnique* technique(std::string named);
  fxparam_constptr_t param(std::string named);
  const FxShaderParamBlock* paramBlock(std::string named);

  ////////////////////////////////////////////
#if defined(ENABLE_COMPUTE_SHADERS)
  const FxShaderStorageBlock* storageBlock(std::string named);
  const FxComputeShader* computeShader(std::string named);
#endif
  ////////////////////////////////////////////

  void commit();
  void bindTechnique(const FxShaderTechnique* tek);
  void bindParamInt(fxparam_constptr_t par, int value);
  void bindParamFloat(fxparam_constptr_t par, float value);
  void bindParamFloatArray(fxparam_constptr_t par, const float* value, size_t len);
  void bindParamCTex(fxparam_constptr_t par, const Texture* tex);
  void bindParamVec2(fxparam_constptr_t par, const fvec2& v);
  void bindParamVec3(fxparam_constptr_t par, const fvec3& v);
  void bindParamVec4(fxparam_constptr_t par, const fvec4& v);
  void bindParamQuat(fxparam_constptr_t par, const fquat& v);
  void bindParamPlane(fxparam_constptr_t par, const fplane& v);
  void bindParamVec2Array(fxparam_constptr_t par, const fvec2* v, size_t len);
  void bindParamVec3Array(fxparam_constptr_t par, const fvec3* v, size_t len);
  void bindParamVec4Array(fxparam_constptr_t par, const fvec4* v, size_t len);
  void bindParamU64(fxparam_constptr_t par, uint64_t v); // binds as uvec4 (4 32bit uint vector)
  void bindParamMatrix(fxparam_constptr_t par, const fmtx4& m);
  void bindParamMatrix(fxparam_constptr_t par, const fmtx3& m);
  void bindParamMatrixArray(fxparam_constptr_t par, const fmtx4* m, size_t len);

  ////////////////////////////////////////////

  fxshaderasset_ptr_t _shaderasset;
  FxShader* _shader                     = nullptr;
  const FxShaderTechnique* _selectedTEK = nullptr;

  std::set<const FxShaderTechnique*> _techniques;
  std::set<fxparam_constptr_t> _params;
  std::set<const FxShaderParamBlock*> _paramBlocks;

  ////////////////////////////////////////////

  Context* _initialTarget = nullptr;

#if defined(ENABLE_COMPUTE_SHADERS)
  std::set<const FxShaderStorageBlock*> _storageBlocks;
  std::set<const FxComputeShader*> _computeShaders;
#endif

};

freestyle_mtl_ptr_t createShaderFromFile(lev2::Context* ctx, std::string debugname, file::Path shader_path);

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
