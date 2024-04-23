////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/group.h>

namespace ork { namespace ui {

////////////////////////////////////////////////////////////////////
// surface : optionally image backed group
//  can redraw without repainting when clean!
////////////////////////////////////////////////////////////////////

struct Surface : public Group {
public:
  Surface(const std::string& name, int x, int y, int w, int h, fcolor3 color, F32 depth);

  void SurfaceRender(lev2::RenderContextFrameData& fd, const std::function<void()>& l);
  void Clear();

  fcolor3& GetClearColorRef(void) {
    return _clearColor;
  }
  F32 GetClearDepth(void) {
    return mfClearDepth;
  }

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

  bool mbClear;
  fcolor3 _clearColor;
  F32 mfClearDepth;
  lev2::rtgroup_ptr_t _rtgroup;
  bool mNeedsSurfaceRepaint;
  lev2::PickBuffer* _pickbuffer;

  void_lambda_t _postRenderCallback;
  bool _aspect_from_rtgroup = false;
  bool _decouple_from_ui_size = false;
  int _decoupled_width = 0;
  int _decoupled_height = 0;

protected:
  void _doGpuInit(lev2::Context* pTARG) override;
  void RenderCached();
  void _doOnResized(void) override;
  virtual void DoSurfaceResize() {
  }
  void DoDraw(ui::drawevent_constptr_t drwev) override;
  virtual void DoRePaintSurface(ui::drawevent_constptr_t drwev) {
  }
};

}} // namespace ork::ui
