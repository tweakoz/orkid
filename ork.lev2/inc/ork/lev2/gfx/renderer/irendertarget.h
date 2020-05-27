#pragma once

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

class IRenderTarget {
public:
  IRenderTarget();

  virtual int width()                                = 0;
  virtual int height()                                = 0;
  virtual void BeginFrame(FrameRenderer& frenderer) = 0;
  virtual void EndFrame(FrameRenderer& frenderer)   = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct RtGroupRenderTarget : public IRenderTarget {
  RtGroupRenderTarget(RtGroup* prtgroup);

  int width() final;
  int height() final;
  void BeginFrame(FrameRenderer& frenderer) final;
  void EndFrame(FrameRenderer& frenderer) final;

  RtGroup* _rtgroup;
};

///////////////////////////////////////////////////////////////////////////////

class UiViewportRenderTarget : public IRenderTarget {
public:
  UiViewportRenderTarget(ui::Viewport* pVP);

private:
  ui::Viewport* mpViewport;

  int width() final;
  int height() final;
  void BeginFrame(FrameRenderer& frenderer) final;
  void EndFrame(FrameRenderer& frenderer) final;
};

///////////////////////////////////////////////////////////////////////////////

class UiSurfaceRenderTarget : public IRenderTarget {
public:
  UiSurfaceRenderTarget(ui::Surface* pVP);

private:
  ui::Surface* mSurface;

  int width() final;
  int height() final;
  void BeginFrame(FrameRenderer& frenderer) final;
  void EndFrame(FrameRenderer& frenderer) final;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
