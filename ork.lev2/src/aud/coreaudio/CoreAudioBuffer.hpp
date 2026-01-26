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
	mGoingDown.store(true);

	// Delete all objects we can get from the pool
	// Don't wait forever - some objects may be in transit queues
	int numdeleted = 0;
	int max_attempts = 100;  // Don't spin forever
	int attempts = 0;

	while( numdeleted < mNumObjectsAllocated && attempts < max_attempts )
	{
		T* data = nullptr;
		if( mObjectPool.try_pop(data) )
		{
			delete data;
			numdeleted++;
			attempts = 0;  // Reset attempts on success
		}
		else
		{
			attempts++;
			usleep(1000);
		}
	}

	// Note: Some objects may be leaked if they're stuck in queues
	// This is acceptable during shutdown
}
///////////////////////////////////////////////////////////////////////////////
template <typename T>
void ObjectPool<T>::signalShutdown()
{
	mGoingDown.store(true);
}
///////////////////////////////////////////////////////////////////////////////
template <typename T>
T* ObjectPool<T>::AllocObject()
{
	// Check shutdown flag - return nullptr if shutting down
	if (mGoingDown.load()) {
		return nullptr;
	}

	T* data = nullptr;
	while( data == nullptr && !mGoingDown.load() )
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

	// If we exited due to shutdown, return nullptr
	if (mGoingDown.load()) {
		if (data) {
			// Return the object we got back to pool
			mObjectPool.push(data);
		}
		return nullptr;
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


