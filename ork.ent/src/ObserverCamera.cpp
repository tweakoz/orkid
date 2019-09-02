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
#include <pkg/ent/drawable.h>
#include <pkg/ent/event/MeshEvent.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/AccessorObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/gfx/camera.h>
///////////////////////////////////////////////////////////////////////////////
#include "ObserverCamera.h"
#include <ork/kernel/string/string.h>
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::ObserverCamArchetype, "ObserverCamArchetype" );
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ObserverCamControllerData, "ObserverCamControllerData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ObserverCamControllerInst, "ObserverCamControllerInst");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

void ObserverCamArchetype::DoCompose(ork::ent::ArchComposer& composer)
{
	composer.Register<ork::ent::ObserverCamControllerData>();
}

///////////////////////////////////////////////////////////////////////////////

void ObserverCamArchetype::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

ObserverCamArchetype::ObserverCamArchetype()
{
}

///////////////////////////////////////////////////////////////////////////////

void ObserverCamControllerData::Describe()
{
	ork::reflect::RegisterProperty( "Target", &ObserverCamControllerData::mTarget );
	ork::reflect::RegisterProperty( "Eye", &ObserverCamControllerData::mEye );
	ork::reflect::RegisterProperty( "EyeUp", &ObserverCamControllerData::mEyeUp );
	ork::reflect::RegisterProperty( "EyeOffset", &ObserverCamControllerData::mEyeOffset );
	ork::reflect::RegisterProperty( "TargetOffset", &ObserverCamControllerData::mTgtOffset );
	ork::reflect::RegisterProperty( "Aperature", &ObserverCamControllerData::mfAperature );
	ork::reflect::RegisterProperty( "Near", &ObserverCamControllerData::mfNear );
	ork::reflect::RegisterProperty( "Far", &ObserverCamControllerData::mfFar );

	reflect::AnnotatePropertyForEditor< ObserverCamControllerData >("Aperature", "editor.range.min", "1.0" );
	reflect::AnnotatePropertyForEditor< ObserverCamControllerData >("Aperature", "editor.range.max", "150.0" );

	reflect::AnnotatePropertyForEditor< ObserverCamControllerData >("Near", "editor.range.min", "0.1" );
	reflect::AnnotatePropertyForEditor< ObserverCamControllerData >("Near", "editor.range.max", "10000.0" );

	reflect::AnnotatePropertyForEditor< ObserverCamControllerData >("Far", "editor.range.min", "1.0" );
	reflect::AnnotatePropertyForEditor< ObserverCamControllerData >("Far", "editor.range.max", "10000.0" );
}

///////////////////////////////////////////////////////////////////////////////

ObserverCamControllerData::ObserverCamControllerData()
	: mfAperature(45.0f)
	, mfNear(1.0f)
	, mfFar(500.0f)
{
}

///////////////////////////////////////////////////////////////////////////////

ent::ComponentInst* ObserverCamControllerData::createComponent(ent::Entity* pent) const
{
	return new ObserverCamControllerInst( *this, pent );
}

///////////////////////////////////////////////////////////////////////////////

void ObserverCamControllerInst::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

ObserverCamControllerInst::ObserverCamControllerInst(const ObserverCamControllerData& occd, Entity* pent )
	: ComponentInst( & occd, pent )
	, mCD( occd )
	, mpTarget(0)
	, mpEye(0)
{
	CameraDrawable* pcamd = new CameraDrawable(pent,&mCameraData); // deleted when entity deleted
	//pent->AddDrawable(AddPooledLiteral("Default"),pcamd);
	pcamd->SetOwner(pent);

	mCameraData.Persp( 0.1f, 1.0f, 45.0f );
	mCameraData.Lookat( CVector3(0.0f,0.0f,0.0f), CVector3(0.0f,0.0f,1.0f), CVector3(0.0f,1.0f,0.0f) );

	printf( "OCCI<%p> camdat<%p> l2cam<%p>\n", this, & mCameraData, mCameraData.getEditorCamera() );
}

///////////////////////////////////////////////////////////////////////////////

bool ObserverCamControllerInst::DoLink(SceneInst *psi)
{
	mpTarget = psi->FindEntity(mCD.GetTarget());
	mpEye = psi->FindEntity(mCD.GetEye());
	return true;
}

bool ObserverCamControllerInst::DoStart(SceneInst *psi, const CMatrix4 &world)
{
	if( GetEntity() )
	{
		const ent::EntData& ED = GetEntity()->GetEntData();
		PoolString name = ED.GetName();
		std::string Name = CreateFormattedString( "%s", name.c_str() );
 		psi->SetCameraData( AddPooledString(Name.c_str()), & mCameraData );


	}
	return true;
}
///////////////////////////////////////////////////////////////////////////////

void ObserverCamControllerInst::DoUpdate( SceneInst* psi )
{
	CVector3 cam_UP = CVector3(0.0f,1.0f,0.0f);
	CVector3 cam_EYE = CVector3(0.0f,0.0f,0.0f);

	CVector3 eye_up = mCD.GetEyeUp();

	if( mpEye )
	{
		DagNode& dnodeEYE = mpEye->GetDagNode();
		const TransformNode& t3dEYE = dnodeEYE.GetTransformNode();
		CMatrix4 mtxEYE = t3dEYE.GetTransform().GetMatrix();
		cam_EYE = CVector4(mCD.GetEyeOffset()).Transform(mtxEYE).xyz();
		cam_UP = mtxEYE.GetYNormal();

		if( eye_up.Mag() )
			cam_UP = eye_up.Normal();
	}
	else
	{
		DagNode& dnodeEYE = GetEntity()->GetDagNode();
		const TransformNode& t3dEYE = dnodeEYE.GetTransformNode();
		CMatrix4 mtxEYE = t3dEYE.GetTransform().GetMatrix();
		cam_EYE = CVector4(mCD.GetEyeOffset()).Transform(mtxEYE).xyz();
	}

	if( mpTarget )
	{
		DagNode& dnodeTGT = mpTarget->GetDagNode();
		const TransformNode& t3dTGT = dnodeTGT.GetTransformNode();
		CMatrix4 mtxTGT = t3dTGT.GetTransform().GetMatrix();
		CVector3 cam_TGT = CVector4(mCD.GetTgtOffset()).Transform(mtxTGT).xyz();

		float fnear = mCD.GetNear();
		float ffar = mCD.GetFar();
		float faper = mCD.GetAperature();

		CVector3 N = (cam_TGT-cam_EYE).Normal();

		mCameraData.Persp( fnear, ffar, faper );
		mCameraData.Lookat( cam_EYE, cam_TGT, cam_UP );

		//orkprintf( "ocam eye<%f %f %f>\n", cam_EYE.GetX(), cam_EYE.GetY(), cam_EYE.GetZ() );
		//orkprintf( "ocam tgt<%f %f %f>\n", cam_TGT.GetX(), cam_TGT.GetY(), cam_TGT.GetZ() );
		//orkprintf( "ocam dir<%f %f %f>\n", N.GetX(), N.GetY(), N.GetZ() );

		//psi->SetCameraData( AddPooledLiteral("game1"), & mCameraData );
	}
}

///////////////////////////////////////////////////////////////////////////////
}}
