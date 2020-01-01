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

#if defined(ORK_CONFIG_QT)
#include <ork/lev2/qtui/qtui.h>
#endif
#include <ork/kernel/prop.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/rtti/downcast.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::PickBufferBase, "ork::lev2::PickBufferBase");

namespace ork { namespace lev2 {

void PickBufferBase::Describe() {}

PickBufferBase::PickBufferBase(lev2::OffscreenBuffer* Parent, int iX, int iY, int iW, int iH, EPickBufferType etyp)
    : ork::lev2::OffscreenBuffer(Parent, iX, iY, iW, iH, lev2::EBUFFMT_RGBA8, lev2::ETGTTYPE_EXTBUFFER), meType(etyp), mbInitTex(true),
      mpPickRtGroup(new lev2::RtGroup(context(), iW, iH)) {
  mpUIMaterial = new ork::lev2::GfxMaterialUITextured(context());
}

uint64_t PickBufferBase::AssignPickId(ork::Object* pobj) {
	uint64_t pid = uint64_t(pobj);
  mPickIds[pid] = pobj;
  return pid;
}
ork::Object* PickBufferBase::GetObjectFromPickId(uint64_t pid) {
  printf("pickid <0x%llx>\n", pid);
  auto it = mPickIds.find(pid);
  ork::Object* pobj = (it == mPickIds.end()) ? nullptr : it->second;
  return pobj;
}

void PickBufferBase::Init() {
  auto buf0 = new ork::lev2::RtBuffer(mpPickRtGroup, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA32F, miWidth, miHeight);
  auto buf1 = new ork::lev2::RtBuffer(mpPickRtGroup, lev2::ETGTTYPE_MRT1, lev2::EBUFFMT_RGBA32F, miWidth, miHeight);
  buf0->_debugName = FormatString("Pickbuf::mrt0");
  buf0->_debugName = FormatString("Pickbuf::mrt1");
  mpPickRtGroup->SetMrt(0,buf0);
  mpPickRtGroup->SetMrt(1,buf1);
}

}} // namespace ork::lev2
