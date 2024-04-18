////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/kernel/Array.hpp>
#include <ork/kernel/opq.h>
#include <ork/kernel/fixedlut.hpp>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/math/collision_test.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork :: lev2 {
///////////////////////////////////////////////////////////////////////////////


LightProbe::LightProbe(){

}

LightProbe::~LightProbe(){

}

void LightProbe::resize(int dim){
  _dim = dim;
  _dirty = true;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork :: lev2 {
///////////////////////////////////////////////////////////////////////////////
