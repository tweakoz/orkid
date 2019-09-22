#pragma once

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Immediate Mode Interface
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

class ImmInterface {
public:
  inline CVtxBuffer<SVtxV4C4>& RefUIQuadBuffer(void) { return mVtxBufUIQuad; }
  inline CVtxBuffer<SVtxV12C4T16>& RefUITexQuadBuffer(void) { return mVtxBufUITexQuad; }
  inline CVtxBuffer<SVtxV12C4T16>& RefTextVB(void) { return mVtxBufText; }

protected:
  ImmInterface(GfxTarget& target);

  GfxTarget& mTarget;
  DynamicVertexBuffer<SVtxV4C4> mVtxBufUILine;
  DynamicVertexBuffer<SVtxV4C4> mVtxBufUIQuad;
  DynamicVertexBuffer<SVtxV12C4T16> mVtxBufUITexQuad;
  DynamicVertexBuffer<SVtxV12C4T16> mVtxBufText;

private:
  virtual void DoBeginFrame() = 0;
  virtual void DoEndFrame()   = 0;
};

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Raster State Interface
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

class RasterStateInterface {
public:
  RasterStateInterface(GfxTarget& target);

  SRasterState& GetRasterState(void) { return mCurrentState; }
  SRasterState& RefUIRasterState(void) { return mUIRasterState; }
  virtual void BindRasterState(const SRasterState& rState, bool bForce = false) = 0;

  SRasterState& GetLastState(void) { return mLastState; }

  virtual void SetZWriteMask(bool bv)             = 0;
  virtual void SetRGBAWriteMask(bool rgb, bool a) = 0;
  virtual void SetBlending(EBlending eVal)        = 0;
  virtual void SetDepthTest(EDepthTest eVal)      = 0;
  virtual void SetCullTest(ECullTest eVal)        = 0;
  virtual void SetScissorTest(EScissorTest eVal)  = 0;

protected:
  GfxTarget& _target;
  SRasterState mUIRasterState;
  SRasterState mCurrentState;
  SRasterState mLastState;
};
