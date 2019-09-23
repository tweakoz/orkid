#pragma once

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

class IRenderTarget {
public:
  IRenderTarget();

  virtual int GetW()                                = 0;
  virtual int GetH()                                = 0;
  virtual void BeginFrame(FrameRenderer& frenderer) = 0;
  virtual void EndFrame(FrameRenderer& frenderer)   = 0;
};

///////////////////////////////////////////////////////////////////////////////

class RtGroupRenderTarget : public IRenderTarget {
public:
  RtGroupRenderTarget(RtGroup* prtgroup);

private:
  RtGroup* mpRtGroup;

  int GetW() final;
  int GetH() final;
  void BeginFrame(FrameRenderer& frenderer) final;
  void EndFrame(FrameRenderer& frenderer) final;
};

///////////////////////////////////////////////////////////////////////////////

class UiViewportRenderTarget : public IRenderTarget {
public:
  UiViewportRenderTarget(ui::Viewport* pVP);

private:
  ui::Viewport* mpViewport;

  int GetW() final;
  int GetH() final;
  void BeginFrame(FrameRenderer& frenderer) final;
  void EndFrame(FrameRenderer& frenderer) final;
};

///////////////////////////////////////////////////////////////////////////////

class UiSurfaceRenderTarget : public IRenderTarget {
public:
  UiSurfaceRenderTarget(ui::Surface* pVP);

private:
  ui::Surface* mSurface;

  int GetW() final;
  int GetH() final;
  void BeginFrame(FrameRenderer& frenderer) final;
  void EndFrame(FrameRenderer& frenderer) final;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
