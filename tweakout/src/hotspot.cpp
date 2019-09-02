////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2014, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
#include <pkg/ent/ModelArchetype.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <pkg/ent/scene.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/AccessorObjectPropertyType.hpp>
///////////////////////////////////////////////////////////////////////////////
#include "enemy_fighter.h"
#include "enemy_spawner.h"
#include "world.h"
#include <ork/math/basicfilters.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace wiidom {
///////////////////////////////////////////////////////////////////////////////

HotSpot::HotSpot()
	: mWeight( 0.0f )
{
	for( int i=0; i<8; i++ )
	{
		mCardinalDirWeight[i] = 0.0f;
	}
}

///////////////////////////////////////////////////////////////////////////////

/*void HotSpot::AddFighter(FighterControllerInst*fci)
{
	orkvector<FighterControllerInst*>::const_iterator it = std::find(mFighters.begin(), mFighters.end(), fci );
	OrkAssert( it == mFighters.end() );
	mFighters.push_back(fci);
}

///////////////////////////////////////////////////////////////////////////////

void HotSpot::RemoveFighter(FighterControllerInst*fci)
{
	orkvector<FighterControllerInst*>::iterator it = std::find(mFighters.begin(), mFighters.end(), fci );
	OrkAssert( it != mFighters.end() );
	mFighters.erase(it);
}*/

///////////////////////////////////////////////////////////////////////////////
// request a position to equalize density
///////////////////////////////////////////////////////////////////////////////

fvec3 HotSpot::RequestPosition()
{
	fvec3 result = mPosition;
	return result;
}

///////////////////////////////////////////////////////////////////////////////

HotSpotController::HotSpotController()
	: mClosest( 0 )
	, mPrevious( 0 )
	, miSerial( 0 )
	, miSerial2( 0 )
	, mMaxLinkDistance( 100.0f )
	, mWCI( 0 )
{
}

void HotSpotController::ReadSurface( const fvec3& xyz, fvec3& pos, fvec3& normal ) const
{
	/*const terrain::heightfield_ed_component& hec = mHEI->GetHEC();
	const terrain::TerrainSynth& hf = hec.GetTerrainSynth();
	const terrain::hmap_hfield_module& hhm = hf.GetTargetModule();
	hhm.ReadSurface( xyz, pos, normal );*/
}

///////////////////////////////////////////////////////////////////////////////

void HotSpotController::Link(WorldControllerInst*wci)
{
	mWCI = wci;

	mHotSpots.resize( khsdim*khsdim );

	const float kmul = 1000.0f/float(khsdim);

	for( int ix=0; ix<khsdim; ix++ )
	{
		float fx = (float(ix)*kmul)-500.0f;
		
		for( int iz=0; iz<khsdim; iz++ )
		{
			float fz = (float(iz)*kmul)-500.0f;

			fvec3 pos( fx, 0.0f, fz );
			fvec3 hfpos, hfn;
			ReadSurface( pos, hfpos, hfn );


			int iaddr = (iz*khsdim) + ix;

			HotSpot* result = & mHotSpots[ iaddr ];

			result->miX = ix;
			result->miZ = iz;

			result->mPosition = hfpos+fvec3(0.0f,5.0f,0.0f);
		}
	}

	mClosest = GetHotSpot( (khsdim>>1), (khsdim>>1) );

}
///////////////////////////////////////////////////////////////////////////////
// incrementally search for closest node
// this is called every frame for the spawners update
// so we distribute the workload over several frames
// via random distribution checks
///////////////////////////////////////////////////////////////////////////////

HotSpot* HotSpotController::UpdateHotSpot( const fvec3& TargetPos, const fvec3& TargetVel, const fvec3& TargetAcc )
{
	////////////////////////////////
	// measure target motion
	////////////////////////////////
	
	static fvec3 TargetLPos = TargetPos+fvec3(0.0f,1.0f,0.0f);

	//////////////////////////////////

	fvec3 TargetDir = ( (TargetVel.Normal()*0.6f) + (TargetAcc.Normal()*0.4f) ).Normal();

	static avg_filter<16> filtx;
	static avg_filter<16> filty;
	static avg_filter<16> filtz;

	float fx = filtx.compute( TargetDir.GetX() );
	float fy = filty.compute( TargetDir.GetY() );
	float fz = filtz.compute( TargetDir.GetZ() );

	TargetDir = fvec3( fx, fy, fz );

	//////////////////////////////////

	float fmag = TargetVel.Mag()*1.1f;

	//////////////////////////////////

	int irand = rand()%10;
	float frand = 1.0f; //(float(irand)/10.0f)-0.2f;
	int jrand = rand()%10;
	float fjrand = 0.0f; //0.3f*((float(jrand)/10.0f)-0.5f);

	fvec3 JDir = TargetDir.Cross( fvec3(0.0f,1.0f,0.0f) ).Normal();

	fvec3 Dir = ((TargetDir*frand)+(JDir*fjrand)).Normal();
	fvec3 AnticTargetPos = TargetPos+(Dir*fmag);

	//////////////////////////////////
	// find projected spot
	//////////////////////////////////
	//fvec3 Home = fvec3( fx, fy, fz );
	fvec3 hfnextp, hfnextn;
	//mWCI->ReadSurface( AnticTargetPos, hfnextp, hfnextn );
	AnticTargetPos = hfnextp+fvec3( 0.0f, 5.0f, 0.0f );
	//////////////////////////////////

	static float koffs = 0.5f / float(khsdim);

	float fX = (AnticTargetPos.GetX() / 1000.0f)+0.5f;
	float fZ = (AnticTargetPos.GetZ() / 1000.0f)+0.5f;

	int iX = int((fX+koffs)*khsdim);
	int iZ = int((fZ+koffs)*khsdim);

	if( (iX>=0) && (iZ>=0) && (iX<khsdim) && (iZ<khsdim) )
	{
		mClosest = GetHotSpot( iX, iZ );
		
		/////////////////////////////////

		if( mPrevious != mClosest )
		{
			mClosest->mWeight += 1.0f;

			if( mPrevious )
			{
				fvec3 vdir = (mClosest->mPosition-mPrevious->mPosition).Normal();
				fvec2 vdirXZ = fvec2( vdir.GetX(), vdir.GetZ() ).Normal();
				int icard = GetCardinalDir( vdirXZ );
				mPrevious->mCardinalDirWeight[ icard ] += 1.0f;
			}
		}

		mPrevious = mClosest;

		/////////////////////////////////
	}

	return mClosest;
}

///////////////////////////////////////////////////////////////////////////////

HotSpot* HotSpotController::GetHotSpot( int iX, int iZ )
{
	OrkAssert( iX < khsdim );
	OrkAssert( iZ < khsdim );
	OrkAssert( iX >= 0 );
	OrkAssert( iZ >= 0 );
	int iaddr = (iZ*khsdim) + iX;
	return & mHotSpots[ iaddr ];
}

///////////////////////////////////////////////////////////////////////////////

int HotSpotController::GetCardinalDir(const ork::fvec2 &vxz)
{
	float fat2 = atan2( vxz.GetY(), vxz.GetX() );
	int io = int( fat2 * (8.0f/PI2) );
	return io;
}

///////////////////////////////////////////////////////////////////////////////

HotSpot* HotSpotController::GetConnected( const HotSpot* psrc, int icard )
{
	int ix = psrc->miX;
	int iz = psrc->miZ;

	float fang = float(icard) * (PI2/8.0f);
	float fx = sinf( fang );
	float fz = cosf( fang );

	int ixo = (fx<0.0f) ? -1 : (fx>0.0f) ? 1 : 0;
	int izo = (fz<0.0f) ? -1 : (fz>0.0f) ? 1 : 0;

	ix += ixo;
	iz += izo;

	if( ix < 0 ) return 0;
	if( ix >= khsdim ) return 0;
	if( iz < 0 ) return 0;
	if( iz >= khsdim ) return 0;

	return GetHotSpot( ix, iz );
}

///////////////////////////////////////////////////////////////////////////////

void HotSpotController::SortConnections( const HotSpot* phs, int(&SortedCardinals)[9] )
{
	////////////////////////////////
	// init to not found
	////////////////////////////////
	for( int i=0; i<9; i++ )
	{
		SortedCardinals[i] = -1;
	}

	////////////////////////////////
	// sort by weight
	////////////////////////////////

	float fmax = CFloat::TypeMax();
	for( int ic=0; ic<3; ic++ )
	{	float fmx = fmax;
		float fmin = -CFloat::TypeMax();
		for( int iw=0; iw<9; iw++ )
		{	float fw = phs->mCardinalDirWeight[ iw ];
			if( (fw<fmx) && (fw>fmin) )
			{	SortedCardinals[ic] = iw;
				fmax = fw;
				fmin = fw;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

HotSpot* HotSpotController::GetHotSpot()
{
	HotSpot* pnode = mClosest;
	HotSpot* presult = pnode;

	if( 0 ) //for( int ihop=0; ihop<3; ihop++ )
	{
		if( pnode )
		{
			int SortedCardinals[9];
			SortConnections( pnode, SortedCardinals );
		
			int icount = 0;
			for( int i=0; i<9; i++ )
			{	if( SortedCardinals[i]>=0 ) icount++;
			}

			int index = -1;

			if( icount>1 )
			{
				index = rand()%icount;
			}
			else if( icount == 1 )
			{
				index = 0;
			}

			if( index>= 0 )
			{
				pnode = GetConnected( pnode, index );
				if( pnode )
				{
					presult = pnode;
				}
			}
			else
			{
				pnode = 0;
			}
		}
	}
	OrkAssert( presult );
	return presult;
}

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

void HotSpotController::UpdateHotSpotLinks()
{
	
}

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
