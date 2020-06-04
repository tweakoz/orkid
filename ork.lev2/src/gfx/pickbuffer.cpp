////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////
PickBuffer::PickBuffer(ui::Surface* surf, Context* ctx, int w, int h)
    : _context(ctx)
    , _surface(surf)
    , _width(w)
    , _height(h)
    , _inittex(true) {
  Init();
}
///////////////////////////////////////////////////////////////////////////////
uint64_t PickBuffer::AssignPickId(const ork::Object* pobj) {
  uint64_t pid  = uint64_t(pobj);
  mPickIds[pid] = const_cast<ork::Object*>(pobj);
  return pid;
}
///////////////////////////////////////////////////////////////////////////////
ork::Object* PickBuffer::GetObjectFromPickId(uint64_t pid) {
  printf("pickid <0x%zx>\n", pid);
  auto it           = mPickIds.find(pid);
  ork::Object* pobj = (it == mPickIds.end()) ? nullptr : it->second;
  return pobj;
}
///////////////////////////////////////////////////////////////////////////////
void PickBuffer::Init() {
  _rtgroup         = new lev2::RtGroup(_context, _width, _height);
  _uimaterial      = new ork::lev2::GfxMaterialUITextured(_context);
  auto buf0        = new ork::lev2::RtBuffer(lev2::ERTGSLOT0, lev2::EBufferFormat::RGBA16UI, _width, _height);
  auto buf1        = new ork::lev2::RtBuffer(lev2::ERTGSLOT1, lev2::EBufferFormat::RGBA32F, _width, _height);
  buf0->_debugName = FormatString("Pickbuf::mrt0");
  buf1->_debugName = FormatString("Pickbuf::mrt1");
  _rtgroup->SetMrt(0, buf0);
  _rtgroup->SetMrt(1, buf1);
}
///////////////////////////////////////////////////////////////////////////////
void PickBuffer::resize(int w, int h) {
  printf("resize pickbuf<%p> size<%d %d>\n", this, w, h);
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
  auto rsi  = tgt->RSI();

  int irtgw  = _rtgroup->width();
  int irtgh  = _rtgroup->height();
  int isurfw = _surface->width();
  int isurfh = _surface->height();
  if (irtgw != isurfw || irtgh != isurfh) {
    printf("resize pickbuf size<%d %d>\n", isurfw, isurfh);
    _width  = isurfw;
    _height = isurfh;
    _rtgroup->Resize(isurfw, isurfh);
  }
  printf("Begin Pickbuffer::Draw() surf<%p>\n", _surface);
  fbi->PushRtGroup(_rtgroup);
  fbi->EnterPickState(this);

  auto drwev = std::make_shared<ork::ui::DrawEvent>(tgt);
  _surface->RePaintSurface(drwev);
  fbi->LeavePickState();
  fbi->PopRtGroup();
  tgt->debugPopGroup();
  printf("End Pickbuffer::Draw()\n");
}
///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
