#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/kernel/string/deco.inl>
#include "metal.h"
#if defined(__APPLE__)
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::metal {
///////////////////////////////////////////////////////////////////////////////

mtlpp::Device ContextMetal::device(){
  static mtlpp::Device device = mtlpp::Device::CreateSystemDefaultDevice();
  return device;
}

void init(){
  deco::printf(fvec3(1,1,0),"Initializing MetalAPI\n");
  auto device = ContextMetal::device();
  assert(device);
  auto devname = device.GetName().GetCStr();
  deco::printf(fvec3(1,1,0),"mtlpp::Device<%s>\n", devname );
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
#endif
