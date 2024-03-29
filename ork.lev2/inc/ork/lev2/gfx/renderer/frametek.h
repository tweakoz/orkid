////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

namespace ork::lev2 {

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

struct FrameTechniqueBase {
public:
  FrameTechniqueBase(int iW, int iH);
  virtual ~FrameTechniqueBase() {}

  virtual void Render(FrameRenderer& ContextData) {}
  virtual RtGroup* GetFinalRenderTarget() const { return mpMrtFinal; }
  void Init(Context* targ);

  virtual void update(const CompositingPassData& CPD, int itargw, int itargh) {}

protected:
  int miW;
  int miH;
  RtGroup* mpMrtFinal;

private:
  virtual void DoInit(Context* targ) {}
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
