////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/gfx/camera.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>

namespace ork::lev2 {

  ///////////////////////////////////////////////////////////////////////////////

  struct FrameRenderer {
    typedef std::function<void()> rendercb_t;
    RenderContextFrameData _framedata;
    FrameRenderer() { _render=[](){}; }
    void Render() { _render(); }
    rendercb_t _render;
    RenderContextFrameData& framedata() { return _framedata; }
  };

  class IRenderTarget {
  public:
    IRenderTarget();

    virtual int GetW()                                = 0;
    virtual int GetH()                                = 0;
    virtual void BeginFrame(FrameRenderer& frenderer) = 0;
    virtual void EndFrame(FrameRenderer& frenderer)   = 0;
  };

  class RtGroupRenderTarget : public IRenderTarget {
  public:
    RtGroupRenderTarget(RtGroup* prtgroup);

  private:
    RtGroup* mpRtGroup;

    int GetW();
    int GetH();
    void BeginFrame(FrameRenderer& frenderer);
    void EndFrame(FrameRenderer& frenderer);
  };

  class UiViewportRenderTarget : public IRenderTarget {
  public:
    UiViewportRenderTarget(ui::Viewport* pVP);

  private:
    ui::Viewport* mpViewport;

    int GetW();
    int GetH();
    void BeginFrame(FrameRenderer& frenderer);
    void EndFrame(FrameRenderer& frenderer);
  };

  class UiSurfaceRenderTarget : public IRenderTarget {
  public:
    UiSurfaceRenderTarget(ui::Surface* pVP);

  private:
    ui::Surface* mSurface;

    int GetW();
    int GetH();
    void BeginFrame(FrameRenderer& frenderer);
    void EndFrame(FrameRenderer& frenderer);
  };

///////////////////////////////////////////////////////////////////////////////

class FrameTechniqueBase {
public:
  FrameTechniqueBase(int iW, int iH);
  virtual ~FrameTechniqueBase() {}

  virtual void Render(FrameRenderer& ContextData) {}
  virtual RtGroup* GetFinalRenderTarget() const { return mpMrtFinal; }
  void Init(GfxTarget* targ);

protected:
  int miW;
  int miH;
  RtGroup* mpMrtFinal;

private:
  virtual void DoInit(GfxTarget* targ) {}
};

///////////////////////////////////////////////////////////////////////////////

} //namespace ork::lev2 {
