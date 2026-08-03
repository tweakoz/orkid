////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "gfxenv.h"
#include "txi.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

class ContextDummy;

///////////////////////////////////////////////////////////////////////////////

struct DummyDrawingInterface : public DrawingInterface {
  DummyDrawingInterface(ContextDummy& ctx);
};

class DummyFxInterface : public FxInterface {
public:
  void _doBeginFrame() final {
  }
  void _doEndFrame() final {
  }

  int BeginBlock(const FxShaderTechnique* tek, const RenderContextInstData& data) final {
    return 0;
  }
  void EndBlock() final {
  }
  void CommitParams(void) final {
  }

  const FxShaderTechnique* technique(FxShader* hfx, const std::string& name) final {
    return nullptr;
  }
  const FxShaderParam* parameter(FxShader* hfx, const std::string& name) final {
    return nullptr;
  }
  const FxUniformBlock* uniformBlock(FxShader* hfx, const std::string& name) final {
    return nullptr;
  }
  const FxShaderStorageBlock* storageBlock(FxShader* hfx, const std::string& name) final {
    return nullptr;
  }
  fxbuffer_member_constptr_t findStorageMember(fxparamstorageblock_constptr_t block, const std::string& member_name) final {
    return nullptr;
  }
  const FxComputeShader* computeShader(FxShader* hfx, const std::string& name) final {
    return nullptr;
  }

  void bindParamBool(const FxShaderParam* hpar, const bool bval) final {
  }
  void bindParamInt(const FxShaderParam* hpar, const int ival) final {
  }
  void bindParamVect2(const FxShaderParam* hpar, const fvec2& Vec) final {
  }
  void bindParamVect3(const FxShaderParam* hpar, const fvec3& Vec) final {
  }
  void bindParamVect4(const FxShaderParam* hpar, const fvec4& Vec) final {
  }
  void bindParamVect4Array(const FxShaderParam* hpar, const fvec4* Vec, const int icount) final {
  }
  void bindParamVect2Array(const FxShaderParam* hpar, const fvec2* Vec, const int icount) final {
  }
  void bindParamVect3Array(const FxShaderParam* hpar, const fvec3* Vec, const int icount) final {
  }
  void bindParamFloatArray(const FxShaderParam* hpar, const float* pfA, const int icnt) final {
  }
  void bindParamFloat(const FxShaderParam* hpar, float fA) final {
  }
  void bindParamMatrix(const FxShaderParam* hpar, const fmtx4& Mat) final {
  }
  void bindParamMatrix(const FxShaderParam* hpar, const fmtx3& Mat) final {
  }
  void bindParamMatrixArray(const FxShaderParam* hpar, const fmtx4* MatArray, int iCount) final {
  }
  void bindParamU32(const FxShaderParam* hpar, U32 uval) final {
  }
  void bindParamU64(const FxShaderParam* hpar, uint64_t uval) final {
  }
  void bindParamTexture(const FxShaderParam* hpar, const Texture* pTex) final {
  }
  void bindParamTextureArray(const FxShaderParam* hpar, const TextureArray* tex_array) final {
    
  }

  bool LoadFxShader(const AssetPath& pth, FxShader* ptex) final;

  fxsamplerset_constptr_t samplerSet(FxShader* hfx, const std::string& name) { return nullptr; }


  DummyFxInterface() {
  }
};


///////////////////////////////////////////////////////////////////////////////
struct DuComputeInterface : public ComputeInterface {};
///////////////////////////////////////////////////////////////////////////////

class DuMatrixStackInterface : public MatrixStackInterface {
  virtual fmtx4 Ortho(float left, float right, float top, float bottom, float fnear, float ffar);

public:
  DuMatrixStackInterface(Context& target)
      : MatrixStackInterface(target) {
  }
};

///////////////////////////////////////////////////////////////////////////////

class DuGeometryBufferInterface final : public GeometryBufferInterface {

  ///////////////////////////////////////////////////////////////////////
  // VtxBuf Interface

  void* LockVB(VertexBufferBase& VBuf, int ivbase, int icount) override;
  void UnLockVB(VertexBufferBase& VBuf) override;

  const void* LockVB(const VertexBufferBase& VBuf, int ivbase = 0, int icount = 0) override;
  void UnLockVB(const VertexBufferBase& VBuf) override;

  void ReleaseVB(VertexBufferBase& VBuf) override;

  //

  void* LockIB(IndexBufferBase& VBuf, int ivbase, int icount) override;
  void UnLockIB(IndexBufferBase& VBuf) override;

  const void* LockIB(const IndexBufferBase& VBuf, int ibase = 0, int icount = 0) override;
  void UnLockIB(const IndexBufferBase& VBuf) override;

  void ReleaseIB(IndexBufferBase& VBuf) override;

  //

  void DrawPrimitiveEML(
      const VertexBufferBase& VBuf, //
      PrimitiveType eType,
      int ivbase,
      int ivcount) override;

  void
  DrawIndexedPrimitiveEML(const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, PrimitiveType eType)
      override;

  void DrawPrimitiveEML(
      const FxShaderStorageBuffer* SSBO, //
      PrimitiveType eType,
      int ivbase           = 0,
      int ivcount          = 0) final;

  void DrawInstancedIndexedPrimitiveEML(
      const VertexBufferBase& VBuf,
      const IndexBufferBase& IdxBuf,
      PrimitiveType eType,
      size_t instance_count) override;

  void DrawInstancedIndexedPrimitiveEML(
      const VertexBufferBase& VBuf,
      const IndexBufferBase& IdxBuf,
      PrimitiveType eType,
      size_t instance_count,
      size_t first_instance) override;

  void DrawInstancedIndexedPrimitiveIndirectEML(
      const VertexBufferBase& VBuf,
      const IndexBufferBase& IdxBuf,
      PrimitiveType eType,
      const FxShaderStorageBuffer* indirect_args,
      size_t args_offset = 0) override;

  void DrawIndirectEML(
      PrimitiveType eType,
      const FxShaderStorageBuffer* indirect_args,
      size_t args_offset = 0) override;

  void DrawIndexedIndirectEML(
      const FxShaderStorageBuffer* index_buffer,
      PrimitiveType eType,
      const FxShaderStorageBuffer* indirect_args,
      size_t args_offset = 0,
      int index_size = 4) override;

  void DrawMeshTasksEML(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;

  void DrawMeshTasksIndirectEML(const FxShaderStorageBuffer* indirect_args, size_t args_offset = 0) override;

  //////////////////////////////////////////////

public:
  DuGeometryBufferInterface(ContextDummy& ctx);
  ContextDummy& _ducontext;
};

///////////////////////////////////////////////////////////////////////////////

class DuFrameBufferInterface : public FrameBufferInterface {
public:
  DuFrameBufferInterface(Context& target);
  ~DuFrameBufferInterface();
  void _pushRtGroup(RtGroup* Base) final {
    _active_rtgroup = Base;
  }
  void _popRtGroup() final {
  }
  ///////////////////////////////////////////////////////

  void _setViewport(int iX, int iY, int iW, int iH) final {
  }
  void _setScissor(int iX, int iY, int iW, int iH) final {
  }

  void GetPixel(const fvec4& rAt, PixelFetchContext& ctx) final {
  }

  //////////////////////////////////////////////

  void _doBeginFrame(void) final {
  }
  void _doEndFrame(void) final {
  }

  void msaaBlit(rtgroup_ptr_t src, rtgroup_ptr_t dst) final {

  }

protected:
};

///////////////////////////////////////////////////////////////////////////////

class DuTextureInterface : public TextureInterface {
public:
  DuTextureInterface(Context& ctx);

  bool destroyTexture(texture_ptr_t ptex) final {
    return false;
  }

  void generateMipMaps(Texture* ptex) final {
  }
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class ContextDummy final : public Context {
  DeclareConcreteX(ContextDummy, Context);

  friend class GfxEnv;

  ///////////////////////////////////////////////////////////////////////

public:
  ContextDummy();
  ~ContextDummy();

  ///////////////////////////////////////////////////////////////////////
  // VtxBuf Interface

  bool SetDisplayMode(DisplayMode* mode) final;

  //////////////////////////////////////////////
  // FX Interface

  FxInterface* FXI() final {
    return &mFxI;
  }
  MatrixStackInterface* MTXI() final {
    return &mMtxI;
  }
  GeometryBufferInterface* GBI() final {
    return &mGbI;
  }
  TextureInterface* TXI() final {
    return &mTxI;
  }
  FrameBufferInterface* FBI() final {
    return &mFbI;
  }
  DrawingInterface* DWI() final {
    return &mDWI;
  }

  ComputeInterface* CI() final {
    return &mCI;
  }

  //////////////////////////////////////////////

private:
  //////////////////////////////////////////////
  // GfxHWContext Concrete Interface
  //////////////////////////////////////////////

  void _doBeginFrame(void) final {
  }
  void _doEndFrame(void) final {
  }
  void initializeWindowContext(Window* pWin, CTXBASE* pctxbase) final; // make a window
  void initializeOffscreenContext(DisplayBuffer* pBuf) final;        // make a pbuffer
  void initializeLoaderContext() final;
  void _doResizeMainSurface(int iW, int iH) final;

  ///////////////////////////////////////////////////////////////////////

private:
  DummyFxInterface mFxI;
  DuMatrixStackInterface mMtxI;
  DuGeometryBufferInterface mGbI;
  DuTextureInterface mTxI;
  DuFrameBufferInterface mFbI;
  DummyDrawingInterface mDWI;
  DuComputeInterface mCI;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
