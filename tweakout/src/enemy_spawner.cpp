////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2014, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/scene.h>
#include <pkg/ent/scene.hpp>
#include <pkg/ent/drawable.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
#include <pkg/ent/ModelArchetype.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/ModelComponent.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/asset/AssetManager.h>
#include <ork/application/application.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/AccessorObjectPropertyType.hpp>
///////////////////////////////////////////////////////////////////////////////
#include "enemy_fighter.h"
#include "enemy_spawner.h"
#include "world.h"
#include "ship.h"

///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI( ork::wiidom::EnemySpawnerArchetype, "EnemySpawnerArchetype" );
INSTANTIATE_TRANSPARENT_RTTI( ork::wiidom::EnemySpawnerControllerData, "EnemySpawnerControllerData" );
//INSTANTIATE_TRANSPARENT_RTTI( ork::wiidom::EnemySpawnerControllerInst, "EnemySpawnerControllerInst" );
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace wiidom {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void EnemySpawnerControllerData::Describe()
{
	ork::ent::RegisterFamily<EnemySpawnerControllerData>(ork::AddPooledLiteral("control"));

	reflect::RegisterProperty( "Debug", & EnemySpawnerControllerData::mDebug );
	reflect::RegisterProperty( "HeatAdd", & EnemySpawnerControllerData::mHeatAdd );
	reflect::RegisterProperty( "HeatSpread", & EnemySpawnerControllerData::mHeatSpread );
	reflect::RegisterProperty( "HeatDecay", & EnemySpawnerControllerData::mHeatDecay );
	reflect::RegisterProperty( "MaxLinkDist", & EnemySpawnerControllerData::mMaxLinkDist );

	reflect::AnnotatePropertyForEditor<EnemySpawnerControllerData>( "MaxLinkDist", "editor.range.min", "1.0" );
	reflect::AnnotatePropertyForEditor<EnemySpawnerControllerData>( "MaxLinkDist", "editor.range.max", "200.0" );
}

EnemySpawnerControllerData::EnemySpawnerControllerData()
	: mDebug( 0 )
	, mHeatAdd( 0.0f )
	, mHeatSpread( 0.0f )
	, mHeatDecay( 0.0f )
	, mMaxLinkDist( 100.0f )
{
}

ent::ComponentInst* EnemySpawnerControllerData::CreateComponent(ent::Entity* pent) const
{
	return OrkNew EnemySpawnerControllerInst( *this, pent );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

//void EnemySpawnerControllerInst::Describe()
//{
//}

EnemySpawnerControllerInst::EnemySpawnerControllerInst( const EnemySpawnerControllerData& pcd, ork::ent::Entity* pent )
	: ork::ent::ComponentInst( & pcd, pent )
	, mCD(pcd)
	, mTarget( 0 )
	, mNumEnemies( 0 )
	, mfTimer( 0.0f )
	, mLastHotSpot( 0 )
	, mbInit( true )
{
	//////////////////////////////////////////////////////////////
	// init the entities drawable to this callback
	//////////////////////////////////////////////////////////////
	struct yo
	{
		static void doit( lev2::RenderContextInstData& rcid, lev2::GfxTarget* targ, const lev2::CallbackRenderable* pren )
		{
			///////////////////////
			// Draw RigidBody Object
			///////////////////////
			auto user_data0 = pren->GetUserData0();
			const ent::Entity* pent = user_data0.Get<const ent::Entity*>();
			const EnemySpawnerControllerInst* sci = pent->GetTypedComponent<EnemySpawnerControllerInst>();
			OrkAssert( sci );
			EnemySpawnerControllerInst* scinc = const_cast<EnemySpawnerControllerInst*>(sci);
			CMatrix4 MatS;
			static lev2::GfxMaterial3DSolid matsolid(targ);

			if( scinc->GetData().GetDebug() )
			{
				///////////////////////////////////////////////////////
				// Basic Draw
				///////////////////////////////////////////////////////

				targ->BindMaterial( & matsolid );

				const orkvector<HotSpot>& hotspots = sci->HotSpots().HotSpots();

				int inumhs = int(hotspots.size());

				for( int ih=0; ih<inumhs; ih++ )
				{
					const HotSpot* hs = & hotspots[ih]; //sci->HotSpots().mClosest;

					///////////////////////////////////////////
					// Draw Edges
					///////////////////////////////////////////
					
					/*int inumcon = hs->mConnections.size();
					CVector4 a = hs->mPosition;
					bool bcon2 = false;
					for( orkmap<U32,HotSpotLink*>::const_iterator it = hs->mConnections.begin(); it!=hs->mConnections.end(); it++ )
					{
						HotSpotLink* lnk = (*it).second;

						HotSpot* ohs = (lnk->mHotSpotA==hs) ? lnk->mHotSpotB : lnk->mHotSpotA;

						if( ohs == sci->HotSpots().mClosest )
						{
							bcon2 = true;
						}
						CVector4 b = ohs->mPosition;
						targ->ImmDrawLine( a, b );
					}*/

					///////////////////////////////////////////
					// Draw Nodes
					///////////////////////////////////////////

					if( hs->mWeight > 0.0f )
					{
						MatS.SetScale( 2.0f, 2.0f, 2.0f );
						MatS.SetTranslation( hs->mPosition );

						//float ftem = hs->mTemperature*0.3f;
						float ftem = (hs == sci->HotSpots().mClosest) ? 1.0f : 0.0f;

						targ->PushModColor( CVector3::Yellow()*ftem );
						targ->MTXI()->PushMMatrix( MatS );
						{
							matsolid.SetColorMode( lev2::GfxMaterial3DSolid::EMODE_MOD_COLOR );
							lev2::CGfxPrimitives::GetRef().RenderDiamond( targ );

						}
						targ->PopModColor();
						targ->MTXI()->PopMMatrix();
					}
				}
			
			}
		}
	};

	#if 0 //DRAWTHREADS
	ent::CallbackDrawable* pdrw = OrkNew ent::CallbackDrawable( pent );
	pent->AddDrawable( AddPooledLiteral("Default"),pdrw );
	pdrw->SetCallback( yo::doit );
	pdrw->SetOwner(  & pent->GetEntData() );
	pdrw->SetData( (const void*) pent );
	#endif
}

void EnemySpawnerControllerInst::Link( ShipControllerInst*sci, WorldControllerInst*wci )
{
	mWCI = wci;
	mSCI = sci;
	//mHotSpots.Link( wci, hei );
}

///////////////////////////////////////////////////////////////////////////////

void EnemySpawnerControllerInst::ReAssignFighter( FighterControllerInst* fci )
{
	//const CVector3 TargetPos = mTarget->GetDagNode().GetTransformNode().GetTransform()->GetPosition();
	//CVector3 TargetDir = ((mTargetAcc.Normal()*0.3f)+(mTargetVel.Normal()*0.7f)).Normal();
	if( mHotSpots.mClosest )
	{	
		HotSpot* asshp = mHotSpots.mClosest;

		CVector3 ClosPos = asshp->mPosition;
		
		fci->SetHotSpot( asshp );
	}
}

///////////////////////////////////////////////////////////////////////////////

void EnemySpawnerControllerInst::DoUpdate(ork::ent::SceneInst *sinst)
{
	float dt = sinst->GetDeltaTime();

	if( mbInit )
	{
		for( int i=0; i<80; i++ )
		{
			int ix = rand()%100;
			int iz = rand()%100;

			float fx = -0.5f+(float(ix)/100.0f);
			float fz = -0.5f+(float(iz)/100.0f);

			fx *= 1000.0f;
			fz *= 1000.0f;

			CVector3 pos( fx, 0.0f, fz );
			CVector3 hfpos, hfn;
			//mWCI->ReadSurface( pos, hfpos, hfn );

			//HotSpot* ClosestHotSpot = mHotSpots.UpdateHotSpot( hfpos+CVector3( 0.0f, 5.0f, 0.0f ) );

		}

		mHotSpots.mClosest = mHotSpots.GetHotSpot(HotSpotController::khsdim>>1,HotSpotController::khsdim>>1);

		mbInit = false;
	}

	////////////////////////////////

	mHotSpots.mHeatAdd = mCD.GetHeatAdd();
	mHotSpots.mHeatSpread = mCD.GetHeatSpread();
	mHotSpots.mHeatDecay = mCD.GetHeatDecay();
	mHotSpots.mMaxLinkDistance = mCD.GetMaxLinkDistance();

	////////////////////////////////
	// measure target motion
	////////////////////////////////
	
	const CVector3 TargetPos = mTarget->GetDagNode().GetTransformNode().GetTransform().GetPosition();
	static CVector3 TargetLPos = TargetPos+CVector3(0.0f,1.0f,0.0f);
	mTargetVel = (TargetPos-TargetLPos)*(1.0f/dt);
	static CVector3 TargetLVel = mTargetVel;

	mTargetAcc = (mTargetVel-TargetLVel)*(1.0f/dt);;

	TargetLPos = TargetPos;
	TargetLVel = mTargetVel;

	OrkAssert( mTarget );
	const ent::DagNode& targdn = mTarget->GetDagNode();

	//////////////////////////////////

	HotSpot* ClosestHotSpot = mHotSpots.UpdateHotSpot( TargetPos, mTargetVel, mTargetAcc );

	if( mfTimer <= 0.0f )
	{
		mfTimer = 0.5f;
		if( mNumEnemies < 10 )
		{
			HotSpot* hspot = mHotSpots.GetHotSpot();

			///////////////////////////////////

			static FighterControllerData fcd;
			static const ent::EntData fed;
			static ork::ent::ModelComponentData mcd;
			static auto model = asset::AssetManager<lev2::XgmModelAsset>::Load( "data://actors/enemy/enemy" );

			mcd.SetModel( model );
			mcd.SetScale( 1.0f );

			///////////////////////////////
			// create entity, init it's position to players position
			///////////////////////////////
			ent::Entity* pent = OrkNew ent::Entity( fed, sinst );
			pent->GetDagNode().CopyTransformMatrixFrom( targdn );
			///////////////////////////////
			// create entity components and connect them
			///////////////////////////////
			FighterControllerInst* fci = (FighterControllerInst*) fcd.CreateComponent(pent);
			fci->SetWCI( mWCI );
			fci->SetTarget( mTarget );
			fci->SetSpawner( this );
			fci->SetHotSpot( hspot );
			mFighters.push_back( fci );

			ork::ent::ModelComponentInst* mci = ork::rtti::autocast(mcd.CreateComponent(pent));
	
			///////////////////////////////
			// activate entity in sceneinst
			///////////////////////////////
			pent->GetComponents().AddComponent( fci );
			pent->GetComponents().AddComponent( mci );
			//sinst->QueueActivateEntity( pent );
			assert(false);
			///////////////////////////////
				
			mNumEnemies++;
			
		}
	}

	mfTimer -= dt;

	mHotSpots.UpdateHotSpotLinks();

}

void EnemySpawnerControllerInst::Despawning( FighterControllerInst* fsi )
{
	OrkAssert(mNumEnemies>0);
	mNumEnemies--;

	orkvector<FighterControllerInst*>::iterator it = std::find( mFighters.begin(), mFighters.end(), fsi );

	OrkAssert( it != mFighters.end() );
	
	mFighters.erase(it);

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void EnemySpawnerArchetype::Describe()
{
	//reflect::AnnotateClassForEditor<WorldArchetype>( "editor.instantiable", "false" );
}

///////////////////////////////////////////////////////////////////////////////

EnemySpawnerArchetype::EnemySpawnerArchetype()
{
}

///////////////////////////////////////////////////////////////////////////////

void EnemySpawnerArchetype::DoCompose()
{
	EnemySpawnerControllerData* acd = GetTypedComponent<EnemySpawnerControllerData>();

	if( 0 == acd )
	{
		acd = OrkNew EnemySpawnerControllerData;
		mComponentDataTable.AddComponent( acd );
	}
}

///////////////////////////////////////////////////////////////////////////////

void EnemySpawnerArchetype::DoLinkEntity( const ent::SceneInst* psi, ent::Entity *pent ) const
{
	EnemySpawnerControllerInst* sci = pent->GetTypedComponent<EnemySpawnerControllerInst>();
	OrkAssert( sci );

	WorldControllerInst* wci = psi->FindTypedEntityComponent<WorldControllerInst>("/ent/world");
	ent::Entity * target = psi->FindEntity( FindPooledString( "/ent/ship" ) );
	ShipControllerInst* shci = target->GetTypedComponent<ShipControllerInst>();

	//auto hei = psi->FindTypedEntityComponent<ent::heightfield_rt_inst>("/ent/terrain");

	OrkAssert(shci);

	sci->SetTarget( target );
	sci->SetEntity( pent );
	sci->Link( shci, wci );


}

///////////////////////////////////////////////////////////////////////////////
} }
///////////////////////////////////////////////////////////////////////////////
