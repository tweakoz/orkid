////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

namespace ork::lev2 {

struct RenderContextFrameData;
struct CompositingPassData;
struct RtGroup;
struct GfxTarget;

///////////////////////////////////////////////////////////////////////////////

struct FrameRenderer {
  typedef std::function<void()> rendermisccb_t;
  /////////////////////////////////////////////
  FrameRenderer(RenderContextFrameData& RCFD, rendermisccb_t cb);
  void renderMisc();
  RenderContextFrameData& framedata() { return _framedata; }
  /////////////////////////////////////////////
  RenderContextFrameData& _framedata;
  rendermisccb_t _rendermisccb;
};

///////////////////////////////////////////////////////////////////////////////

class CompositingPassData;

class FrameTechniqueBase {
public:
  FrameTechniqueBase(int iW, int iH);
  virtual ~FrameTechniqueBase() {}

  virtual void Render(FrameRenderer& ContextData) {}
  virtual RtGroup* GetFinalRenderTarget() const { return mpMrtFinal; }
  void Init(GfxTarget* targ);

  virtual void update(const CompositingPassData& CPD, int itargw, int itargh) {}

protected:
  int miW;
  int miH;
  RtGroup* mpMrtFinal;

private:
  virtual void DoInit(GfxTarget* targ) {}
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
