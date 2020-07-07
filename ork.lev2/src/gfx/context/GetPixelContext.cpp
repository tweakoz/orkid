////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
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

#include <ork/reflect/properties/register.h>

namespace ork::lev2 {

/////////////////////////////////////////////////////////////////////////

PixelFetchContext::PixelFetchContext()
    : mRtGroup(nullptr)
    , miMrtMask(0)
    , mUserData(nullptr) {
  for (int i = 0; i < kmaxitems; i++) {
    _pickvalues[i] = fcolor4(0.0f, 0.0f, 0.0f, 0.0f);
    mUsage[i]      = EPU_FLOAT;
  }
}

/////////////////////////////////////////////////////////////////////////

ork::rtti::ICastable* PixelFetchContext::GetObject(PickBuffer* pb, int ichan) const {
  if (nullptr == pb)
    return nullptr;

  auto pid   = (uint64_t)GetPointer(ichan);
  void* uobj = pb->GetObjectFromPickId(pid);
  if (nullptr != uobj) {
    ork::rtti::ICastable* pObj = reinterpret_cast<ork::rtti::ICastable*>(uobj);
    return pObj;
  }
  return nullptr;
}

/////////////////////////////////////////////////////////////////////////

void* PixelFetchContext::GetPointer(int ichan) const {
  const auto& pickv = _pickvalues[ichan];
  if (auto as_u64 = pickv.TryAs<uint64_t>()) {
    uint64_t uobj = as_u64.value();
    void* pObj    = reinterpret_cast<void*>(uobj);
    return pObj;
  }
  return nullptr;
}

} // namespace ork::lev2
