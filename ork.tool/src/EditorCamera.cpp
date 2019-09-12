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
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/scene.hpp>
#include <pkg/ent/drawable.h>
#include <pkg/ent/event/MeshEvent.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/AccessorObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/gfx/camera.h>
///////////////////////////////////////////////////////////////////////////////
#include "EditorCamera.h"
#include <ork/kernel/string/string.h>

///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::EditorCamArchetype, "EditorCamArchetype" );
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::EditorCamControllerData, "EditorCamControllerData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::EditorCamControllerInst, "EditorCamControllerInst");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

void EditorCamArchetype::DoCompose(ork::ent::ArchComposer& composer)
{
	composer.Register<ork::ent::EditorCamControllerData>();
}

///////////////////////////////////////////////////////////////////////////////

void EditorCamArchetype::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

EditorCamArchetype::EditorCamArchetype()
{
}

///////////////////////////////////////////////////////////////////////////////

void EditorCamControllerData::Describe()
{
	ork::ent::RegisterFamily<EditorCamControllerData>(ork::AddPooledLiteral("camera"));

	ork::reflect::RegisterProperty( "Camera", & EditorCamControllerData::CameraAccessor );

//	ork::reflect::RegisterProperty( "Target", &EditorCamControllerData::mTarget );
//	ork::reflect::RegisterProperty( "Eye", &EditorCamControllerData::mEye );
//	ork::reflect::RegisterProperty( "EyeOffset", &EditorCamControllerData::mEyeOffset );
//	ork::reflect::RegisterProperty( "TargetOffset", &EditorCamControllerData::mTgtOffset );
}

///////////////////////////////////////////////////////////////////////////////

EditorCamControllerData::EditorCamControllerData()
{
	mPerspCam = new lev2::EzUiCam;

	mPerspCam->mfLoc = 1.0f;

	mPerspCam->SetName( "persp" );
}

///////////////////////////////////////////////////////////////////////////////

ent::ComponentInst* EditorCamControllerData::createComponent(ent::Entity* pent) const
{
	mPerspCam->SetName( pent->GetEntData().GetName().c_str() );
	return new EditorCamControllerInst( *this, pent );
}

///////////////////////////////////////////////////////////////////////////////

void EditorCamControllerInst::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

EditorCamControllerInst::EditorCamControllerInst(const EditorCamControllerData& occd, Entity* pent )
	: ComponentInst( & occd, pent )
	, mCD( occd )
{
	const lev2::Camera* pcam = mCD.GetCamera();
	CameraDrawable* pcamd = new CameraDrawable(pent,& pcam->GetCameraData()); // deleted when entity deleted
	//pent->AddDrawable(AddPooledLiteral("Debug"),pcamd);
	pcamd->SetOwner(pent);

//	CameraDrawable* pcamd = new CameraDrawable(pent,&mCameraData); // deleted when entity deleted
//	pent->AddDrawable(pcamd);
//	pcamd->SetOwner(pent);

}

///////////////////////////////////////////////////////////////////////////////

bool EditorCamControllerInst::DoLink(Simulation *psi)
{
	//printf( "LINKING EditorCamControllerInst\n" );
	return true;
}

///////////////////////////////////////////////////////////////////////////////

bool EditorCamControllerInst::DoStart(Simulation *psi, const fmtx4 &world)
{
	const lev2::Camera* pcam = mCD.GetCamera();
	if( GetEntity() )
	{
		const ent::EntData& ED = GetEntity()->GetEntData();
		PoolString name = ED.GetName();
		std::string Name = CreateFormattedString( "%s", name.c_str() );
 		psi->SetCameraData( AddPooledString(Name.c_str()), & pcam->GetCameraData() );
	}
	//printf( "STARTING EditorCamControllerInst\n" );
	return true;
}

///////////////////////////////////////////////////////////////////////////////

void EditorCamControllerInst::DoUpdate( Simulation* psi )
{
}

///////////////////////////////////////////////////////////////////////////////
}}
