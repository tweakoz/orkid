////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "gfxenv.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

class DummyFxInterface : public FxInterface {
public:
  void DoBeginFrame() final {}

  int BeginBlock(FxShader* hfx, const RenderContextInstData& data) final { return 0; }
  bool BindPass(FxShader* hfx, int ipass) final { return false; }
  bool BindTechnique(FxShader* hfx, const FxShaderTechnique* htek) final { return false; }
  void EndPass(FxShader* hfx) final {}
  void EndBlock(FxShader* hfx) final {}
  void CommitParams(void) final {}

  const FxShaderTechnique* technique(FxShader* hfx, const std::string& name) final { return nullptr; }
  const FxShaderParam* parameter(FxShader* hfx, const std::string& name) final { return nullptr; }
  const FxShaderParamBlock* parameterBlock(FxShader* hfx, const std::string& name) final { return nullptr; }
  #if defined(ENABLE_SHADER_STORAGE)
  const FxShaderStorageBlock* storageBlock(FxShader* hfx, const std::string& name) final { return nullptr; }
  #endif
  #if defined(ENABLE_COMPUTE_SHADERS)
  const FxComputeShader* computeShader(FxShader* hfx, const std::string& name) final { return nullptr; }
  #endif

  void BindParamBool(FxShader* hfx, const FxShaderParam* hpar, const bool bval) final {}
  void BindParamInt(FxShader* hfx, const FxShaderParam* hpar, const int ival) final {}
  void BindParamVect2(FxShader* hfx, const FxShaderParam* hpar, const fvec2& Vec) final {}
  void BindParamVect3(FxShader* hfx, const FxShaderParam* hpar, const fvec3& Vec) final {}
  void BindParamVect4(FxShader* hfx, const FxShaderParam* hpar, const fvec4& Vec) final {}
  void BindParamVect4Array(FxShader* hfx, const FxShaderParam* hpar, const fvec4* Vec, const int icount) final {}
  void BindParamFloatArray(FxShader* hfx, const FxShaderParam* hpar, const float* pfA, const int icnt) final {}
  void BindParamFloat(FxShader* hfx, const FxShaderParam* hpar, float fA) final {}
  void BindParamMatrix(FxShader* hfx, const FxShaderParam* hpar, const fmtx4& Mat) final {}
  void BindParamMatrix(FxShader* hfx, const FxShaderParam* hpar, const fmtx3& Mat) final {}
  void BindParamMatrixArray(FxShader* hfx, const FxShaderParam* hpar, const fmtx4* MatArray, int iCount) final {}
  void BindParamU32(FxShader* hfx, const FxShaderParam* hpar, U32 uval) final {}
  void BindParamCTex(FxShader* hfx, const FxShaderParam* hpar, const Texture* pTex) final {}

  bool LoadFxShader(const AssetPath& pth, FxShader* ptex) final;

  DummyFxInterface() {}
};

///////////////////////////////////////////////////////////////////////////////

struct DuRasterStateInterface : public RasterStateInterface {
  DuRasterStateInterface(GfxTarget& target);
  void BindRasterState(const SRasterState& rState, bool bForce = false) override {}
  void SetZWriteMask(bool bv) override {}
  void SetRGBAWriteMask(bool rgb, bool a) override {}
  void SetBlending(EBlending eVal) override {}
  void SetDepthTest(EDepthTest eVal) override {}
  void SetCullTest(ECullTest eVal) override {}
  void SetScissorTest(EScissorTest eVal) override {}

public:
};

///////////////////////////////////////////////////////////////////////////////
#if defined (ENABLE_COMPUTE_SHADERS)
struct DuComputeInterface : public ComputeInterface {
};
#endif
///////////////////////////////////////////////////////////////////////////////

class DuMatrixStackInterface : public MatrixStackInterface {
  virtual fmtx4 Ortho(float left, float right, float top, float bottom, float fnear, float ffar);

public:
  DuMatrixStackInterface(GfxTarget& target) : MatrixStackInterface(target) {}
};

///////////////////////////////////////////////////////////////////////////////

class DuGeometryBufferInterface : public GeometryBufferInterface {
  ///////////////////////////////////////////////////////////////////////
  // VtxBuf Interface

  virtual void* LockVB(VertexBufferBase& VBuf, int ivbase, int icount);
  virtual void UnLockVB(VertexBufferBase& VBuf);

  virtual const void* LockVB(const VertexBufferBase& VBuf, int ivbase = 0, int icount = 0);
  virtual void UnLockVB(const VertexBufferBase& VBuf);

  virtual void ReleaseVB(VertexBufferBase& VBuf);

  //

  virtual void* LockIB(IndexBufferBase& VBuf, int ivbase, int icount);
  virtual void UnLockIB(IndexBufferBase& VBuf);

  virtual const void* LockIB(const IndexBufferBase& VBuf, int ibase = 0, int icount = 0);
  virtual void UnLockIB(const IndexBufferBase& VBuf);

  virtual void ReleaseIB(IndexBufferBase& VBuf);

  //

  virtual void DrawPrimitive(const VertexBufferBase& VBuf, EPrimitiveType eType = EPRIM_NONE, int ivbase = 0, int ivcount = 0);
  virtual void DrawIndexedPrimitive(const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, EPrimitiveType eType = EPRIM_NONE,
                                    int ivbase = 0, int ivcount = 0);
  virtual void DrawPrimitiveEML(const VertexBufferBase& VBuf, EPrimitiveType eType = EPRIM_NONE, int ivbase = 0, int ivcount = 0);
  virtual void DrawIndexedPrimitiveEML(const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf,
                                       EPrimitiveType eType = EPRIM_NONE, int ivbase = 0, int ivcount = 0);

  //////////////////////////////////////////////

public:
};

///////////////////////////////////////////////////////////////////////////////

class DuFrameBufferInterface : public FrameBufferInterface {
public:
  DuFrameBufferInterface(GfxTarget& target);
  ~DuFrameBufferInterface();

  virtual void SetRtGroup(RtGroup* Base) {}

  ///////////////////////////////////////////////////////

  virtual void SetViewport(int iX, int iY, int iW, int iH) {}
  virtual void SetScissor(int iX, int iY, int iW, int iH) {}
  virtual void Clear(const fcolor4& rCol, float fdepth) {}

  virtual void GetPixel(const fvec4& rAt, PixelFetchContext& ctx) {}

  //////////////////////////////////////////////

  virtual void DoBeginFrame(void) {}
  virtual void DoEndFrame(void) {}

protected:
};

///////////////////////////////////////////////////////////////////////////////

class DuTextureInterface : public TextureInterface {
public:
  void TexManInit(void) final {}

  bool DestroyTexture(Texture* ptex) final { return false; }
  bool LoadTexture(const AssetPath& fname, Texture* ptex) final;
  void SaveTexture(const ork::AssetPath& fname, Texture* ptex) final {}
  void generateMipMaps(Texture* ptex) final {}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class GfxTargetDummy : public GfxTarget {
  RttiDeclareConcrete(GfxTargetDummy, GfxTarget);

  friend class GfxEnv;

  ///////////////////////////////////////////////////////////////////////

public:
  GfxTargetDummy();
  ~GfxTargetDummy() final;

  ///////////////////////////////////////////////////////////////////////
  // VtxBuf Interface

  bool SetDisplayMode(DisplayMode* mode) final;

  //////////////////////////////////////////////
  // FX Interface

  FxInterface* FXI() final { return &mFxI; }
  RasterStateInterface* RSI() final { return &mRsI; }
  MatrixStackInterface* MTXI() final { return &mMtxI; }
  GeometryBufferInterface* GBI() final { return &mGbI; }
  TextureInterface* TXI() final { return &mTxI; }
  FrameBufferInterface* FBI() final { return &mFbI; }

  #if defined(ENABLE_COMPUTE_SHADERS)
  ComputeInterface* CI() final { return &mCI; }
  #endif
  
  //////////////////////////////////////////////

private:
  //////////////////////////////////////////////
  // GfxHWContext Concrete Interface

  void DoBeginFrame(void) final {}
  void DoEndFrame(void) final {}
  void InitializeContext(GfxWindow* pWin, CTXBASE* pctxbase) final; // make a window
  void InitializeContext(GfxBuffer* pBuf) final;                    // make a pbuffer
  void resize(int iX, int iY, int iW, int iH) {}

  ///////////////////////////////////////////////////////////////////////

  void SetSize(int ix, int iy, int iw, int ih) final;

private:
  DummyFxInterface mFxI;
  DuMatrixStackInterface mMtxI;
  DuRasterStateInterface mRsI;
  DuGeometryBufferInterface mGbI;
  DuTextureInterface mTxI;
  DuFrameBufferInterface mFbI;

#if defined(ENABLE_COMPUTE_SHADERS)
  DuComputeInterface mCI;
#endif
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
