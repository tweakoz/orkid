////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/group.h>
#include <ork/lev2/gfx/material_freestyle.h>

namespace ork { namespace ui {

////////////////////////////////////////////////////////////////////
// surface : optionally image backed group
//  can redraw without repainting when clean!
////////////////////////////////////////////////////////////////////

struct Surface : public Group {
public:
  Surface(const std::string& name, int x, int y, int w, int h, fcolor4 color, F32 depth);

  void SurfaceRender(lev2::RenderContextFrameData& fd, const std::function<void()>& l);

  void BeginSurface(lev2::Context* ptarg);
  void EndSurface(lev2::Context* ptarg);

  void GetPixel(int ix, int iy, lev2::PixelFetchContext& ctx);

  void RePaintSurface(ui::drawevent_constptr_t drwev);

  void decoupleFromUiSize(int w, int h);

  void MarkSurfaceDirty() {
    mNeedsSurfaceRepaint = true;
    SetDirty();
  }
  lev2::PickBuffer* pickbuffer() {
    return _pickbuffer;
  }

  // REPAINT CONTRACT (the wait primitive for anything reading _rtgroup).
  //
  // A surface repaints ONLY on a frame where it is dirty (DoDraw), so the
  // render loop can run an unbounded number of frames without this RTG
  // changing — an offscreen/headless loop runs thousands of frames per second
  // against an update thread that dirties it at its own rate. That makes a
  // FRAME COUNT useless as a "my state change is now in the pixels" wait: a
  // 12-frame settle can contain zero repaints.
  //
  // The DEPTH is ONE: the first repaint after a state change renders that
  // state (the render path reads scene/PBR state live at bind time — e.g.
  // CommonStuff::activeRadianceMaps() in material_pbr_pipeline.cpp), there is
  // no extra frame of pipelining to absorb. So the correct wait for "the RTG
  // now shows the change I just made" is: sample _repaintCount when you make
  // the change, then wait until it has ADVANCED. Monotonic, never reset.
  std::atomic<uint64_t> _repaintCount{0};

  bool _flipY = false;
  bool mbClear;
  fcolor4 _clearColor;
  F32 mfClearDepth;
  lev2::rtgroup_ptr_t _rtgroup;
  bool mNeedsSurfaceRepaint;
  bool _alwaysRepaint = false;
  lev2::PickBuffer* _pickbuffer;

  void_lambda_t _postRenderCallback;
  bool _aspect_from_rtgroup = false;
  bool _decouple_from_ui_size = false;
  int _decoupled_width = 0;
  int _decoupled_height = 0;

  // SSAA: 0=off, 1=2x2, 2=3x3, 3=4x4, 4=5x5, 5=6x6
  int _supersample = 0;

protected:
  void _doGpuInit(lev2::Context* pTARG) override;
  void RenderCached();
  void _doOnResized(void) override;
  virtual void DoSurfaceResize() {
  }
  void DoDraw(ui::drawevent_constptr_t drwev) override;
  virtual void DoRePaintSurface(ui::drawevent_constptr_t drwev) {
  }

  // SSAA resolve resources (initialized lazily when _supersample > 0)
  lev2::rtgroup_ptr_t _ssaa_resolve_rtg;
  lev2::FreestyleMaterial _ssaa_blit_mtl;
  const lev2::FxShaderTechnique* _ssaa_tek[6] = {};
  lev2::fxparam_constptr_t _ssaa_par_mvp;
  lev2::fxparam_constptr_t _ssaa_par_colormap;
  lev2::fxparam_constptr_t _ssaa_par_flipy;
  lev2::fxparam_constptr_t _ssaa_par_flipx;
  lev2::fxparam_constptr_t _ssaa_par_vpdim;
  bool _ssaa_initialized = false;
  void _initSsaa(lev2::Context* ctx);
  void _ssaaResolve(lev2::Context* ctx, int dst_w, int dst_h);
  lev2::Texture* _resolvedTexture(); // returns resolved or direct texture
};

}} // namespace ork::ui
