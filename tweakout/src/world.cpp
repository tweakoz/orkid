////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2014, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/drawable.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
#include <pkg/ent/ModelArchetype.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/scene.hpp>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/math/collision_test.h>
#include <ork/kernel/orklut.hpp>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/input/input.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/AccessorObjectPropertyType.hpp>
///////////////////////////////////////////////////////////////////////////////
#include "world.h"
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI( ork::wiidom::WorldArchetype, "Tweakout/WorldArchetype" );
INSTANTIATE_TRANSPARENT_RTTI( ork::wiidom::WorldControllerData, "Tweakout/WorldControllerData" );
//INSTANTIATE_TRANSPARENT_RTTI( ork::wiidom::WorldControllerInst, "WorldControllerInst" );
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace wiidom {
///////////////////////////////////////////////////////////////////////////////
void WorldArchetype::Describe()
{
}

WorldArchetype::WorldArchetype()
{
}

void WorldArchetype::DoCompose(ork::ent::ArchComposer& composer)
{
	composer.Register<ent::EditorPropMapData>();
	composer.Register<ork::wiidom::WorldControllerData>();
}


///////////////////////////////////////////////////////////////////////////////
void WiimoteView( ork::lev2::GfxTarget* pTARG );
void DrawWorld( lev2::RenderContextInstData& rcid, lev2::GfxTarget* targ, const lev2::CallbackRenderable* pren )
{
	auto user_data0 = pren->GetUserData0();
	const ent::Entity* pent = user_data0.Get<const ent::Entity*>();
	//const ent::Entity* pent = (const ent::Entity*) pren->GetUserData();
	//WiimoteView( targ );
	
}
///////////////////////////////////////////////////////////////////////////////

void WorldArchetype::DoLinkEntity( ent::SceneInst* psi, ent::Entity *pent ) const
{
	WorldControllerInst* wci = pent->GetTypedComponent<WorldControllerInst>();
	//auto hei = psi->FindTypedEntityComponent<ent::heightfield_rt_inst>("/ent/terrain");
	//wci->SetHEI( hei );

	//////////////////////////////////////////
	#if 0 //DRAWTHREADS
	ent::CallbackDrawable* pdrw = OrkNew ent::CallbackDrawable( pent );
	pent->AddDrawable( AddPooledLiteral("Default"),pdrw );
	pdrw->SetCallback( DrawWorld );
	pdrw->SetOwner( & pent->GetEntData() );
	pdrw->SetData( (const ent::Entity*) pent );
	#endif
	//////////////////////////////////////////

}

void WorldArchetype::DoStartEntity( ork::ent::SceneInst* psi, const ork::CMatrix4& mtx, ork::ent::Entity* pent ) const
{

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void WorldControllerData::Describe()
{
	ork::ent::RegisterFamily<WorldControllerData>(ork::AddPooledLiteral("control"));
}

WorldControllerData::WorldControllerData()
{
}

ent::ComponentInst* WorldControllerData::DoCreateComponent(ent::Entity* pent) const
{
	return OrkNew WorldControllerInst( *this, pent );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

WorldControllerInst::WorldControllerInst( const WorldControllerData& pcd, ent::Entity *entity )
	: ork::ent::ComponentInst( & pcd, entity )
	, mPcd(pcd)
	//, mHEI( 0 )
	//, mHMAP( 0 )
{
}

void WorldControllerInst::DoUpdate(ent::SceneInst* sinst)
{
	//ork::lev2::CInputManager::GetRef().Poll();
}
/*
void WorldControllerInst::SetHEI( const ent::heightfield_rt_inst* hei )
{
	mHEI = hei;
	if( hei )
	{
		mHMAP = & hei->HeightMapData();
	}
}*/

///////////////////////////////////////////////////////////////////////////////
} }
///////////////////////////////////////////////////////////////////////////////
