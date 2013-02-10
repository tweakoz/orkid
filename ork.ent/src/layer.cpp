////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/drawable.h>
#include <pkg/ent/entity.h>

///////////////////////////////////////////////////////////////////////////////
// Queue and Render Scene into active target
//  this may go to the onscreen or pickbuffer targets
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace ent {

SceneDrawLayerData::SceneDrawLayerData()
{
}

///////////////////////////////////////////////////////////////////////////////

const CCameraData* SceneDrawLayerData::GetCameraData( int icam ) const
{
	int inumscenecameras = mCameraLut.size();
	//printf( "NumSceneCameras<%d>\n", inumscenecameras );
	if( inumscenecameras && icam>=0)
	{
		icam = icam%inumscenecameras;
		const ork::ent::CameraLut::value_type& itCAM = mCameraLut.GetItemAtIndex(icam);
		const CCameraData* pdata = & itCAM.second;
		const lev2::CCamera* pcam = pdata->GetLev2Camera();
		//printf( "icam<%d> pdata<%p> pcam<%p>\n", icam, pdata, pcam );
		return pdata;
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////

const CCameraData* SceneDrawLayerData::GetCameraData( const PoolString& named ) const
{
	int inumscenecameras = mCameraLut.size();
	const ork::ent::CameraLut::const_iterator itCAM = mCameraLut.find(named);
	if( itCAM != mCameraLut.end() )
	{
		const CCameraData* pdata = & itCAM->second;
		return pdata;
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////

void SceneDrawLayerData::Queue( ork::ent::SceneInst* psi,  ork::ent::DrawableBuffer* dbuf )
{
	if( 0 == psi ) return;
	
	///////////////////////////////////////////////////////////////////////////
	// queue drawable buffers
	///////////////////////////////////////////////////////////////////////////

	dbuf->Reset();
	////////////////////////////////////////
	// Queue drawables (also setup game cameras)
	////////////////////////////////////////
	psi->QueueAllDrawablesToBuffer(*dbuf);
	////////////////////////////////////////
	// get camera data from dbuffer
	////////////////////////////////////////
	mCameraLut.clear();
	int inumscenecameras = dbuf->mCameraDataLUT.size();
	for( int icam=0; icam<inumscenecameras; icam++ )
	{
		const ork::ent::CameraLut::value_type& itCAM = dbuf->mCameraDataLUT.GetItemAtIndex(icam);
		const CCameraData& data = itCAM.second;
		const PoolString& name = itCAM.first;
		//printf( "CopyCam<%s>\n", name.c_str() );
		mCameraLut.AddSorted( name, data );
	}

}
//

}}
