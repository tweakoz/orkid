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
#include "VrSystem.h

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
  , _impl(data._compositingData)
{
  _vrstate=0;
}

bool VrSystem::enabled() const {
  return _impl.IsEnabled();
}

VrSystem::~VrSystem()
{
}

void VrSystem::DoUpdate(Simulation* psim) {

  if( nullptr == _spawnloc ){
    _spawnloc = psim->FindEntity(AddPooledString("spawnloc"));
    _vrstate++;
  }
  if( nullptr == _spawncam ){
    _spawncam = psim->cameraData(AddPooledString("spawncam"));
    _vrstate++;
  }
  if( _vrstate==2 and _prv_vrstate<2 ){
    if( _spawnloc and _spawncam ){
      _impl.setPrerenderCallback(0,[=](lev2::GfxTarget*targ){
            // todo - somehow connect to renderthread spawnloc and spawncam
            fmtx4 vrmtx = _spawnloc->GetEffectiveMatrix();
            auto frame_data = (lev2::RenderContextFrameData*) targ->GetRenderContextFrameData();
            if( frame_data ){
              frame_data->setUserProperty("vrroot"_crc,vrmtx);
              frame_data->setUserProperty("vrcam"_crc,_spawncam);
            }
      });
    }
  }

  _prv_vrstate =   _vrstate;

}

bool VrSystem::DoLink(Simulation* psi) {
  return true;
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
