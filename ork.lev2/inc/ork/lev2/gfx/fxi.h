////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/lev2_types.h>

namespace ork::lev2 {

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// FxInterface (interface for dealing with FX materials)
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

class FxInterface {
public:
  void BeginFrame();

  virtual int BeginBlock(
      const FxShaderTechnique* tek, //
      const RenderContextInstData& data) = 0;
  virtual bool BindPass(int ipass)       = 0;
  virtual void EndPass()                 = 0;
  virtual void EndBlock()                = 0;
  virtual void CommitParams(void)        = 0;
  virtual void reset() {
  }
  virtual const FxShaderTechnique* technique(FxShader* hfx, const std::string& name)       = 0;
  virtual const FxShaderParam* parameter(FxShader* hfx, const std::string& name)           = 0;
  virtual const FxUniformBlock* uniformBlock(FxShader* hfx, const std::string& name) = 0;

  virtual const FxComputeShader* computeShader(FxShader* hfx, const std::string& name)     = 0;
  virtual const FxShaderStorageBlock* storageBlock(FxShader* hfx, const std::string& name) = 0;

  virtual void BindParamBool(const FxShaderParam* hpar, const bool bval)                          = 0;
  virtual void BindParamInt(const FxShaderParam* hpar, const int ival)                            = 0;
  virtual void BindParamVect2(const FxShaderParam* hpar, const fvec2& Vec)                        = 0;
  virtual void BindParamVect3(const FxShaderParam* hpar, const fvec3& Vec)                        = 0;
  virtual void BindParamVect4(const FxShaderParam* hpar, const fvec4& Vec)                        = 0;
  virtual void BindParamVect2Array(const FxShaderParam* hpar, const fvec2* Vec, const int icount) = 0;
  virtual void BindParamVect3Array(const FxShaderParam* hpar, const fvec3* Vec, const int icount) = 0;
  virtual void BindParamVect4Array(const FxShaderParam* hpar, const fvec4* Vec, const int icount) = 0;
  virtual void BindParamFloatArray(const FxShaderParam* hpar, const float* pfA, const int icnt)   = 0;
  virtual void BindParamFloat(const FxShaderParam* hpar, float fA)                                = 0;
  virtual void BindParamMatrix(const FxShaderParam* hpar, const fmtx4& Mat)                       = 0;
  virtual void BindParamMatrix(const FxShaderParam* hpar, const fmtx3& Mat)                       = 0;
  virtual void BindParamMatrixArray(const FxShaderParam* hpar, const fmtx4* MatArray, int iCount) = 0;
  virtual void BindParamU32(const FxShaderParam* hpar, uint32_t uval)                             = 0;
  virtual void BindParamCTex(const FxShaderParam* hpar, const Texture* pTex)                      = 0;
  virtual void BindParamU64(const FxShaderParam* hpar, uint64_t uval)                             = 0;
  void BindParamTex(const FxShaderParam* hpar, const lev2::TextureAsset* tex);

  //////////////////////////////////////////
  // new descriptorset api
  //////////////////////////////////////////

  virtual size_t numDescriptorSetBindPoints(fxtechnique_constptr_t tek) {
    return 0;
  }
  virtual fxdescriptorsetbindpoint_constptr_t descriptorSetBindPoint(fxtechnique_constptr_t tek, int slot_index) {
    return nullptr;
  }
  virtual void bindDescriptorSet(fxdescriptorsetbindpoint_constptr_t bindingpoint, fxdescriptorset_constptr_t the_set) {
  }

  //////////////////////////////////////////

  virtual bool LoadFxShader(const AssetPath& pth, FxShader* ptex) = 0;
  virtual FxShader* shaderFromShaderText(const std::string& name, const std::string& shadertext) {
    return nullptr;
  }

  static void Reset();

  virtual fxuniformbuffer_ptr_t createUniformBuffer(size_t length) {
    return nullptr;
  }
  virtual fxuniformbuffermapping_ptr_t mapUniformBuffer(fxuniformbuffer_ptr_t b, size_t base = 0, size_t length = 0) {
    return nullptr;
  }
  virtual void unmapUniformBuffer(fxuniformbuffermapping_ptr_t mapping) {
  }
  virtual void bindUniformBuffer(const FxUniformBlock* block, fxuniformbuffer_constptr_t buffer) {
  }

  FxInterface();
  virtual ~FxInterface() {
  }

  void pushRasterState(rasterstate_ptr_t rs);
  rasterstate_ptr_t popRasterState();
  virtual void _doPushRasterState(rasterstate_ptr_t rs) {
  }
  virtual rasterstate_ptr_t _doPopRasterState() {
    return nullptr;
  }

protected:
  FxShader* _activeShader;
  const FxShaderTechnique* _activeTechnique;

private:
  virtual void _doBeginFrame() {
  }
  virtual void _doEndFrame() {
  }
  virtual void DoOnReset() {
  }
};

} // namespace ork::lev2
