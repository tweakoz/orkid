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

indexscrambler65k_ptr_t PixelFetchContext::_gscrambler = std::make_shared<IndexScrambler<65536>>(42);
std::atomic<int> PixelFetchContext::_gpickcounter = 0;

/////////////////////////////////////////////////////////////////////////

PixelFetchContext::PixelFetchContext(size_t s)
    : miMrtMask(0)
    , mUserData(nullptr){
    resize(s);
}
void PixelFetchContext::resize(size_t s){
  _pickvalues.resize(s);
  _usage.resize(s);
  miMrtMask = (1<<s)-1;
  for( size_t i=0; i<s; i++ ){
    _usage[i] = EPU_SVARIANT;
    _pickvalues[i] = fvec4(0,0,0,1);
  }
}

/////////////////////////////////////////////////////////////////////////

void PixelFetchContext::beginPickRender(){
  _pickindex = _gpickcounter.fetch_add(4);
  _offset  = uint64_t(_gscrambler->scramble(_pickindex+0))<<0;
  _offset += uint64_t(_gscrambler->scramble(_pickindex+1))<<16;
  _offset += uint64_t(_gscrambler->scramble(_pickindex+2))<<32;
  _offset += uint64_t(_gscrambler->scramble(_pickindex+3))<<48;
  _pickIDlut.clear();
  _pickIDvec.clear();
}
void PixelFetchContext::endPickRender(){
  
}

/////////////////////////////////////////////////////////////////////////

fvec4 PixelFetchContext::encodeVariant(pickvariant_t data){
  fvec4 rval;

  uint64_t hash = data.hash();
  size_t index = 0;
  if(_pickIDlut.find(hash) == _pickIDlut.end()){
    index = _pickIDvec.size();
    _pickIDlut[hash] = index;
    _pickIDvec.push_back(data);
  }
  index += _offset;
  rval.x = float(index & 0xFFFF);
  rval.y = float((index >> 16) & 0xFFFF);
  rval.z = float((index >> 32) & 0xFFFF);
  rval.w = float((index >> 48) & 0xFFFF);
  printf( "enc rval<%g %g %g %g>\n", rval.x, rval.y, rval.z, rval.w );

  if(data.isA<fvec4>()){
    rval = data.get<fvec4>();
  }

  return rval;
}
pickvariant_t PixelFetchContext::decodeVariant(fvec4 rgba){
  pickvariant_t rval;
  uint64_t a             = uint64_t(rgba.x);
  uint64_t b             = uint64_t(rgba.y);
  uint64_t c             = uint64_t(rgba.z);
  uint64_t d             = uint64_t(rgba.w);
  size_t value = (d << 48) | (c << 32) | (b << 16) | a;
  value -= _offset;
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
