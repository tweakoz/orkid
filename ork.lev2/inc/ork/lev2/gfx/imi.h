////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Immediate Mode Interface
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

namespace ork::lev2{

class ImmInterface {
public:
  inline CVtxBuffer<SVtxV4C4>& RefUIQuadBuffer(void) { return mVtxBufUIQuad; }
  inline CVtxBuffer<SVtxV12C4T16>& RefUITexQuadBuffer(void) { return mVtxBufUITexQuad; }
  inline CVtxBuffer<SVtxV12C4T16>& RefTextVB(void) { return mVtxBufText; }

protected:
  ImmInterface(Context& target);

  Context& mTarget;
  DynamicVertexBuffer<SVtxV4C4> mVtxBufUILine;
  DynamicVertexBuffer<SVtxV4C4> mVtxBufUIQuad;
  DynamicVertexBuffer<SVtxV12C4T16> mVtxBufUITexQuad;
  DynamicVertexBuffer<SVtxV12C4T16> mVtxBufText;

private:
  virtual void _doBeginFrame() = 0;
  virtual void _doEndFrame()   = 0;
};

} //namespace ork::lev2{
