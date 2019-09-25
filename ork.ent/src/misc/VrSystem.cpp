////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.hpp>
#include <pkg/ent/entity.hpp>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include "VrSystem.h"

///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::VrSystemData, "VrSystemData");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void VrSystemData::Describe()
{
    using namespace ork::reflect;

}

///////////////////////////////////////////////////////////////////////////////

VrSystemData::VrSystemData()
{
}

void VrSystemData::defaultSetup(){

}
///////////////////////////////////////////////////////////////////////////////

ork::ent::System* VrSystemData::createSystem(ork::ent::Simulation *pinst) const
{
	return new VrSystem( *this, pinst );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VrSystem::VrSystem( const VrSystemData& data, Simulation *psim )
	: ork::ent::System( &data, psim )
	, _vrSystemData(data)
{
  _vrstate=0;
}

bool VrSystem::enabled() const {
  return true;
}

VrSystem::~VrSystem()
{
}

void VrSystem::DoUpdate(Simulation* psim) {
}


void VrSystem::enqueueDrawables(lev2::DrawableBuffer& buffer) {
  if( _vrstate != 0 ){
    fmtx4 vrmtx = this->_spawnloc->GetEffectiveMatrix(); // copy (updthread->renderthread)
    buffer.setPreRenderCallback(0,[=](lev2::RenderContextFrameData&RCFD){
          RCFD.setUserProperty("vrroot"_crc,vrmtx);
          RCFD.setUserProperty("vrcam"_crc,this->_spawncam);
    });
  }
}

bool VrSystem::DoLink(Simulation* psim) {
  _spawnloc = psim->FindEntity(AddPooledString("spawnloc"));
  _spawncam = psim->cameraData(AddPooledString("spawncam"));
  bool good2go = (_spawnloc!=nullptr) and (_spawncam!=nullptr);
  _vrstate = int(good2go);
  return good2go or (false==enabled());
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
