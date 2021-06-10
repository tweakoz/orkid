////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <unordered_map>
#include <deque>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/pch.h>
#include <ork/lev2/gfx/orksl/container.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2::metal {
class ContextMetal;
}

namespace ork::lev2::metal::fx {

///////////////////////////////////////////////////////////////////////////////

class Interface final : public FxInterface {
public:
  void _doBeginFrame() override;

  int BeginBlock(fxtechnique_constptr_t tek, const RenderContextInstData& data) override;
  bool BindPass(int ipass) override;
  void EndPass() override;
  void EndBlock() override;
  void CommitParams(void) override;

  const FxShaderTechnique* technique(FxShader* hfx, const std::string& name) override;
  const FxShaderParam* parameter(FxShader* hfx, const std::string& name) override;
  const FxShaderParamBlock* parameterBlock(FxShader* hfx, const std::string& name) override;
#if defined(ENABLE_COMPUTE_SHADERS)
  const FxComputeShader* computeShader(FxShader* hfx, const std::string& name) override;
#endif
#if defined(ENABLE_SHADER_STORAGE)
  const FxShaderStorageBlock* storageBlock(FxShader* hfx, const std::string& name) override;
#endif

  void BindParamBool(const FxShaderParam* hpar, const bool bval) override;
  void BindParamInt(const FxShaderParam* hpar, const int ival) override;
  void BindParamVect2(const FxShaderParam* hpar, const fvec2& Vec) override;
  void BindParamVect3(const FxShaderParam* hpar, const fvec3& Vec) override;
  void BindParamVect4(const FxShaderParam* hpar, const fvec4& Vec) override;
  void BindParamVect4Array(const FxShaderParam* hpar, const fvec4* Vec, const int icount) override;
  void BindParamFloatArray(const FxShaderParam* hpar, const float* pfA, const int icnt) override;
  void BindParamFloat(const FxShaderParam* hpar, float fA) override;
  void BindParamMatrix(const FxShaderParam* hpar, const fmtx4& Mat) override;
  void BindParamMatrix(const FxShaderParam* hpar, const fmtx3& Mat) override;
  void BindParamMatrixArray(const FxShaderParam* hpar, const fmtx4* MatArray, int iCount) override;
  void BindParamU32(const FxShaderParam* hpar, uint32_t uval) override;
  void BindParamCTex(const FxShaderParam* hpar, const Texture* pTex) override;
  void BindParamU64(const FxShaderParam* hpar, uint64_t uval) override;

  bool LoadFxShader(const AssetPath& pth, FxShader* ptex) override;
  FxShader* shaderFromShaderText(const std::string& name, const std::string& shadertext) override;

  Interface(ContextMetal& metal_ctx);

  void BindContainerToAbstract(orksl::rootcontainer_ptr_t pcont, FxShader* fxh);

  orksl::rootcontainer_ptr_t GetActiveEffect() const {
    return _active_effect;
  }

  bool compileAndLink(orksl::rootcontainer_ptr_t container);
  bool compilePipelineVTG(orksl::rootcontainer_ptr_t container);
  bool compilePipelineNVTM(orksl::rootcontainer_ptr_t container);

  // ubo
  FxShaderParamBuffer* createParamBuffer(size_t length) final;
  parambuffermappingptr_t mapParamBuffer(FxShaderParamBuffer* b, size_t base, size_t length) final;
  void unmapParamBuffer(FxShaderParamBufferMapping* mapping) final;
  void bindParamBlockBuffer(const FxShaderParamBlock* block, FxShaderParamBuffer* buffer) final;

private:
  typedef std::function<void(int iloc, GLenum checktype)> stdparambinder_t;

  void _stdbindparam(const FxShaderParam* hpar, const stdparambinder_t& binder);

  orksl::rootcontainer_ptr_t _active_effect;
  const orksl::Pass* mLastPass;
  FxShaderTechnique* mhCurrentTek;

  ContextMetal& mTarget;
};

#if defined(ENABLE_COMPUTE_SHADERS)

struct ComputeInterface : public lev2::ComputeInterface {

  ComputeInterface(ContextMetal& metal_ctx);
  ContextMetal& _targetGL;
  Interface* _fxi                          = nullptr;
  PipelineCompute* _currentComputePipeline = nullptr;

  void dispatchCompute(const FxComputeShader* shader, uint32_t numgroups_x, uint32_t numgroups_y, uint32_t numgroups_z) final;

  void dispatchComputeIndirect(const FxComputeShader* shader, int32_t* indirect) final;

  FxShaderStorageBuffer* createStorageBuffer(size_t length) final;
  storagebuffermappingptr_t mapStorageBuffer(FxShaderStorageBuffer* b, size_t base = 0, size_t length = 0) final;
  void unmapStorageBuffer(FxShaderStorageBufferMapping* mapping) final;
  void bindStorageBuffer(const FxComputeShader* shader, uint32_t binding_index, FxShaderStorageBuffer* buffer) final;
  void bindImage(const FxComputeShader* shader, uint32_t binding_index, Texture* tex, ImageBindAccess access) final;

  PipelineCompute* createComputePipe(ComputeShader* csh);
  void bindComputeShader(ComputeShader* csh);
};
#endif

orksl::rootcontainer_ptr_t LoadFxFromFile(const AssetPath& pth);

} // namespace ork::lev2::metal::fx

///////////////////////////////////////////////////////////////////////////////
