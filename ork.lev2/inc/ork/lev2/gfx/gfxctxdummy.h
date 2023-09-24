////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "gfxenv.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

class ContextDummy;

///////////////////////////////////////////////////////////////////////////////

struct DummyDrawingInterface : public DrawingInterface {
  DummyDrawingInterface(ContextDummy& ctx);
};

class DummyFxInterface : public FxInterface {
public:
  void _doBeginFrame() final {
  }

  int BeginBlock(const FxShaderTechnique* tek, const RenderContextInstData& data) final {
    return 0;
  }
  bool BindPass(int ipass) final {
    return false;
  }
  void EndPass() final {
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
  const FxShaderParamBlock* parameterBlock(FxShader* hfx, const std::string& name) final {
    return nullptr;
  }
#if defined(ENABLE_SHADER_STORAGE)
  const FxShaderStorageBlock* storageBlock(FxShader* hfx, const std::string& name) final {
    return nullptr;
  }
#endif
#if defined(ENABLE_COMPUTE_SHADERS)
  const FxComputeShader* computeShader(FxShader* hfx, const std::string& name) final {
    return nullptr;
  }
#endif

  void BindParamBool(const FxShaderParam* hpar, const bool bval) final {
  }
  void BindParamInt(const FxShaderParam* hpar, const int ival) final {
  }
  void BindParamVect2(const FxShaderParam* hpar, const fvec2& Vec) final {
  }
  void BindParamVect3(const FxShaderParam* hpar, const fvec3& Vec) final {
  }
  void BindParamVect4(const FxShaderParam* hpar, const fvec4& Vec) final {
  }
  void BindParamVect4Array(const FxShaderParam* hpar, const fvec4* Vec, const int icount) final {
  }
  void BindParamVect2Array(const FxShaderParam* hpar, const fvec2* Vec, const int icount) final {
  }
  void BindParamVect3Array(const FxShaderParam* hpar, const fvec3* Vec, const int icount) final {
  }
  void BindParamFloatArray(const FxShaderParam* hpar, const float* pfA, const int icnt) final {
  }
  void BindParamFloat(const FxShaderParam* hpar, float fA) final {
  }
  void BindParamMatrix(const FxShaderParam* hpar, const fmtx4& Mat) final {
  }
  void BindParamMatrix(const FxShaderParam* hpar, const fmtx3& Mat) final {
  }
  void BindParamMatrixArray(const FxShaderParam* hpar, const fmtx4* MatArray, int iCount) final {
  }
  void BindParamU32(const FxShaderParam* hpar, U32 uval) final {
  }
  void BindParamU64(const FxShaderParam* hpar, uint64_t uval) final {
  }
  void BindParamCTex(const FxShaderParam* hpar, const Texture* pTex) final {
  }

  bool LoadFxShader(const AssetPath& pth, FxShader* ptex) final;

  DummyFxInterface() {
  }
};

///////////////////////////////////////////////////////////////////////////////
/*
struct DuRasterStateInterface : public RasterStateInterface {
  DuRasterStateInterface(Context& target);
  void BindRasterState(const SRasterState& rState, bool bForce = false) final {
  }
  void SetZWriteMask(bool bv) final {
  }
  void SetRGBAWriteMask(bool rgb, bool a) final {
  }
  RGBAMask SetRGBAWriteMask(const RGBAMask& newmask) final {
    return _curmask;
  }
  void SetBlending(Blending eVal) final {
  }
  void SetDepthTest(EDepthTest eVal) final {
  }
  void SetCullTest(ECullTest eVal) final {
  }
  void setScissorTest(EScissorTest eVal) final {
  }

public:
};*/

///////////////////////////////////////////////////////////////////////////////
#if defined(ENABLE_COMPUTE_SHADERS)
struct DuComputeInterface : public ComputeInterface {};
#endif
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

  void* LockVB(VertexBufferBase& VBuf, int ivbase, int icount) final;
  void UnLockVB(VertexBufferBase& VBuf) final;

  const void* LockVB(const VertexBufferBase& VBuf, int ivbase = 0, int icount = 0) final;
  void UnLockVB(const VertexBufferBase& VBuf) final;

  void ReleaseVB(VertexBufferBase& VBuf) final;

  //

  void* LockIB(IndexBufferBase& VBuf, int ivbase, int icount) final;
  void UnLockIB(IndexBufferBase& VBuf) final;

  const void* LockIB(const IndexBufferBase& VBuf, int ibase = 0, int icount = 0) final;
  void UnLockIB(const IndexBufferBase& VBuf) final;

  void ReleaseIB(IndexBufferBase& VBuf) final;

  //

  void DrawPrimitiveEML(
      const VertexBufferBase& VBuf, //
      PrimitiveType eType,
      int ivbase,
      int ivcount) final;

  void
  DrawIndexedPrimitiveEML(const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, PrimitiveType eType)
      final;

#if defined(ENABLE_COMPUTE_SHADERS)
  void DrawPrimitiveEML(
      const FxShaderStorageBuffer* SSBO, //
      PrimitiveType eType = PrimitiveType::NONE,
      int ivbase          = 0,
      int ivcount         = 0) final;
#endif

  void DrawInstancedIndexedPrimitiveEML(
      const VertexBufferBase& VBuf,
      const IndexBufferBase& IdxBuf,
      PrimitiveType eType,
      size_t instance_count) final;

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
  }
  RtGroup* _popRtGroup() final {
    return nullptr;
  }

  ///////////////////////////////////////////////////////

  void _setViewport(int iX, int iY, int iW, int iH) final {
  }
  void _setScissor(int iX, int iY, int iW, int iH) final {
  }
  void _clearColorAndDepth(const fcolor4& rCol, float fdepth) final {
  }
  void _clearDepth(float fdepth) final {
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
  DuTextureInterface(context_rawptr_t target)
      : TextureInterface(target) {
  }

  void TexManInit(void) final {
  }

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
  // RasterStateInterface* RSI() final {
  // return &mRsI;
  //}
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

#if defined(ENABLE_COMPUTE_SHADERS)
  ComputeInterface* CI() final {
    return &mCI;
  }
#endif

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
  void initializeOffscreenContext(DisplayBuffer* pBuf) final;          // make a pbuffer
  void initializeLoaderContext() final;
  void _doResizeMainSurface(int iW, int iH) final;

  ///////////////////////////////////////////////////////////////////////

private:
  DummyFxInterface mFxI;
  DuMatrixStackInterface mMtxI;
  // DuRasterStateInterface mRsI;
  DuGeometryBufferInterface mGbI;
  DuTextureInterface mTxI;
  DuFrameBufferInterface mFbI;
  DummyDrawingInterface mDWI;

#if defined(ENABLE_COMPUTE_SHADERS)
  DuComputeInterface mCI;
#endif
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
