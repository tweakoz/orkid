////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <functional>
#include <ork/kernel/concurrent_queue.h>
#include <map>
#include <assert.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::ca {
///////////////////////////////////////////////////////////////////////////////
template <typename T>
ObjectPool<T>::ObjectPool(int max)
	: mMaxObjects(max)
	, mGoingDown(false)
{
	mNumObjectsAllocated = 0;
	mNumObjectsProcessed = 0;
	mNumObjectsOut = 0;

	mUsageCb = [](int){};
}
///////////////////////////////////////////////////////////////////////////////
template <typename T>
ObjectPool<T>::~ObjectPool()
{
	mGoingDown = true;

	int numdeleted = 0;
	while( numdeleted < mNumObjectsAllocated )
	{
		T* data = nullptr;
		while( data == nullptr )
		{
			if( mObjectPool.try_pop(data) )
			{
				delete data;
				numdeleted++;
			}
		}		
	}
}
///////////////////////////////////////////////////////////////////////////////
template <typename T>
T* ObjectPool<T>::AllocObject()
{	
	assert(mGoingDown==false);

	T* data = nullptr;
	while( data == nullptr )
	{
		if( mObjectPool.try_pop(data) )
		{
			// got one
		}
		else if( mNumObjectsAllocated<mMaxObjects )
		{
			data = new T;
			mNumObjectsAllocated++;

		}
		else
			usleep(10);
	}
	int inumout = mNumObjectsOut.fetch_add(1);
	if(inumout>0 && inumout%100==0)
	{
		mUsageCb(inumout);
	}
	return data;
}
///////////////////////////////////////////////////////////////////////////////
template <typename T>
void ObjectPool<T>::ReturnObject(T* data)
{
	mNumObjectsProcessed++;
	int inumout = (mNumObjectsOut.fetch_add(-1)-1);

	if(inumout>0 && inumout%100==0)
	{
		mUsageCb(inumout);
	}

	//if( mNumBuffersProcessed%500 == 0 )
	//	printf( "mNumBuffersProcessed<%d>\n", mNumBuffersProcessed);
	//printf( "ObjectPool::ReturnOutBuffer<%p> mNumObjectsOut<%d> mNumObjectsAllocated<%d> mMaxObjects<%d>\n", data, inumout, int(mNumObjectsAllocated), mMaxObjects );
	mObjectPool.push(data);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ca {


