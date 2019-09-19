////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/kernel/opq.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/ui/ui.h>
#include <ork/reflect/enum_serializer.inl>

#include <ork/reflect/RegisterProperty.h>

namespace ork::lev2 {

/////////////////////////////////////////////////////////////////////////

GetPixelContext::GetPixelContext() : mAsBuffer(nullptr), mRtGroup(nullptr), miMrtMask(0), mUserData(nullptr) {
  for (int i = 0; i < kmaxitems; i++) {
    mPickColors[i] = fcolor4(0.0f, 0.0f, 0.0f, 0.0f);
    mUsage[i] = EPU_FLOAT;
  }
}

/////////////////////////////////////////////////////////////////////////

ork::rtti::ICastable* GetPixelContext::GetObject(PickBufferBase* pb, int ichan) const {
  if (nullptr == pb)
    return nullptr;

  auto pid = (uint64_t)GetPointer(ichan);
  void* uobj = pb->GetObjectFromPickId(pid);
  if (0 != uobj) {
    ork::rtti::ICastable* pObj = reinterpret_cast<ork::rtti::ICastable*>(uobj);
    return pObj;
  }
  return 0;
}

/////////////////////////////////////////////////////////////////////////

void* GetPixelContext::GetPointer(int ichan) const {
  const fvec4& TestColor = mPickColors[ichan];
  uint64_t uobj = TestColor.GetRGBAU64();
  if (0 != uobj) {
    void* pObj = reinterpret_cast<void*>(uobj);
    return pObj;
  }
  return nullptr;
}

} // namespace ork::lev2
