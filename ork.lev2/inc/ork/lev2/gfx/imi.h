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
