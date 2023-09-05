////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/gfx/rtgroup.h>

#include <ork/kernel/prop.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/rtti/downcast.h>
#include <ork/util/logger.h>

namespace ork { namespace lev2 {
static logchannel_ptr_t logchan_pickbuf = logger()->createChannel("PICKBUF", fvec3(0.8, 0.2, 0.5), true);

///////////////////////////////////////////////////////////////////////////////
PickBuffer::PickBuffer(ui::Surface* surf, Context* ctx, int w, int h)
    : _context(ctx)
    , _surface(surf)
    , _width(w)
    , _height(h) {
  Init();
}
///////////////////////////////////////////////////////////////////////////////
uint64_t PickBuffer::AssignPickId(const void* pobj) {
  uint64_t pid  = uint64_t(pobj);
  mPickIds[pid] = const_cast<void*>(pobj);
  return pid;
}
///////////////////////////////////////////////////////////////////////////////
void* PickBuffer::GetObjectFromPickId(uint64_t pid) {
  // printf("pickid <0x%zx>\n", pid);
  auto it           = mPickIds.find(pid);
  void* pobj = (it == mPickIds.end()) ? nullptr : it->second;
  return pobj;
}
///////////////////////////////////////////////////////////////////////////////
void PickBuffer::Init() {
  _rtgroup         = std::make_shared<RtGroup>(_context, _width, _height);
  _uimaterial      = new GfxMaterialUITextured(_context);
  auto buf0        = _rtgroup->createRenderTarget(lev2::EBufferFormat::RGBA32F);
  auto buf1        = _rtgroup->createRenderTarget(lev2::EBufferFormat::RGBA32F);
  buf0->_debugName = FormatString("Pickbuf::mrt0");
  buf1->_debugName = FormatString("Pickbuf::mrt1");
}
///////////////////////////////////////////////////////////////////////////////
void PickBuffer::resize(int w, int h) {
  // printf("resize pickbuf<%p> size<%d %d>\n", this, w, h);
  _rtgroup->Resize(w, h);
  _width  = w;
  _height = h;
}
///////////////////////////////////////////////////////////////////////////////
void PickBuffer::Draw(lev2::PixelFetchContext& ctx) {
  mPickIds.clear();

  auto tgt = context();
  tgt->makeCurrentContext();
  tgt->debugPushGroup("PickBufferDraw");
  auto mtxi = tgt->MTXI();
  auto fbi  = tgt->FBI();
  auto fxi  = tgt->FXI();
  //auto rsi  = tgt->RSI();

  int irtgw  = _rtgroup->width();
  int irtgh  = _rtgroup->height();
  int isurfw = _surface->width();
  int isurfh = _surface->height();
  if (irtgw != isurfw || irtgh != isurfh) {
    logchan_pickbuf->log("resize pickbuf size<%d %d>", isurfw, isurfh);
    _width  = isurfw;
    _height = isurfh;
    _rtgroup->Resize(isurfw, isurfh);
  }
  // printf("Begin Pickbuffer::Draw() surf<%p>\n", _surface);
  fbi->PushRtGroup(_rtgroup.get());
  fbi->EnterPickState(this);

  auto drwev = std::make_shared<ork::ui::DrawEvent>(tgt);

  //////////////////////////////////////////////////////////////
  // repaint the surface into the pickbuffer's rtgroup
  //////////////////////////////////////////////////////////////

  _surface->RePaintSurface(drwev);

  //////////////////////////////////////////////////////////////

  fbi->LeavePickState();
  fbi->PopRtGroup();
  tgt->debugPopGroup();
  // printf("End Pickbuffer::Draw()\n");
}
///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
