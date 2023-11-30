////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
    : miMrtMask(0)
    , mUserData(nullptr) {
  for (int i = 0; i < kmaxitems; i++) {
    _pickvalues[i] = fcolor4(0.0f, 0.0f, 0.0f, 0.0f);
    mUsage[i]      = EPU_FLOAT;
  }
}

/////////////////////////////////////////////////////////////////////////

enum class EPFCEncoding{
  UNKNOWN = 0,
  CONSTOBJPTR = 1,
};

fvec4 PixelFetchContext::encodeVariant(pickvariant_t data){
  fvec4 rval;

  uint64_t hash = data.hash();
  size_t index = 0;
  if(_pickIDlut.find(hash) == _pickIDlut.end()){
    index = _pickIDvec.size();
    _pickIDlut[hash] = index;
    _pickIDvec.push_back(data);
  }
  index += 0xf000f000f000f000;
  rval.x = float(index & 0xFFFF);
  rval.y = float((index >> 16) & 0xFFFF);
  rval.z = float((index >> 32) & 0xFFFF);
  rval.w = float((index >> 48) & 0xFFFF);
  return rval;
}
pickvariant_t PixelFetchContext::decodeVariant(fvec4 rgba){
  pickvariant_t rval;
  uint64_t a             = uint64_t(rgba.x);
  uint64_t b             = uint64_t(rgba.y);
  uint64_t c             = uint64_t(rgba.z);
  uint64_t d             = uint64_t(rgba.w);
  size_t value = (d << 48) | (c << 32) | (b << 16) | a;
  value -= 0xf000f000f000f000;
  if(value < _pickIDvec.size()){
    rval = _pickIDvec[value];
  }
  return rval;
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
  if (auto as_u64 = pickv.tryAs<uint64_t>()) {
    uint64_t uobj = as_u64.value();
    void* pObj    = reinterpret_cast<void*>(uobj);
    return pObj;
  }
  return nullptr;
}

} // namespace ork::lev2
