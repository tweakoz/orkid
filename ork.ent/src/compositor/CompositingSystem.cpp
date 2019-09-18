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
#include <ork/reflect/enum_serializer.h>
#include <pkg/ent/PerfController.h>
#include <pkg/ent/CompositingSystem.h>
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::CompositingSystemData, "CompositingSystemData");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void CompositingSystemData::Describe()
{
    using namespace ork::reflect;
    reflect::RegisterProperty("CompositorData",&CompositingSystemData::_accessor);

}

///////////////////////////////////////////////////////////////////////////////

CompositingSystemData::CompositingSystemData()
{
}

void CompositingSystemData::defaultSetup(){
  _compositingData.defaultSetup();
}

///////////////////////////////////////////////////////////////////////////////

ork::ent::System* CompositingSystemData::createSystem(ork::ent::Simulation *pinst) const
{
	return new CompositingSystem( *this, pinst );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CompositingSystem::CompositingSystem( const CompositingSystemData& data, Simulation *psim )
	: ork::ent::System( &data, psim )
	, _compositingSystemData(data)
  , _impl(data._compositingData)
{
  _vrstate=0;
}

bool CompositingSystem::enabled() const {
  return _impl.IsEnabled();
}

CompositingSystem::~CompositingSystem()
{
}

void CompositingSystem::DoUpdate(Simulation* psim) {

  if( nullptr == _playerspawn ){
    _playerspawn = psim->FindEntity(AddPooledString("playerspawn"));
    _vrstate++;
  }
  if( nullptr == _vrcam ){
    _vrcam = psim->GetCameraData(AddPooledString("vrcam"));
    _vrstate++;
  }
  if( _vrstate==2 and _prv_vrstate<2 ){
    if( _playerspawn and _vrcam ){
      _impl.setPrerenderCallback(0,[=](lev2::GfxTarget*targ){
            fmtx4 playermtx = _playerspawn->GetEffectiveMatrix();
            auto frame_data = (lev2::RenderContextFrameData*) targ->GetRenderContextFrameData();
            if( frame_data ){
              frame_data->setUserProperty("vrroot"_crc,playermtx);
              frame_data->setUserProperty("vrcam"_crc,_vrcam);
            }
      });
    }
  }

  _prv_vrstate =   _vrstate;


}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
