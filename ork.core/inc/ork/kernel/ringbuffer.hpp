///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// MPMC Ring Buffer
//
// Copyright (c) 2010-2011 Dmitry Vyukov. All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
// 
//    1. Redistributions of source code must retain the above copyright notice, this list of
//       conditions and the following disclaimer.
// 
//    2. Redistributions in binary form must reproduce the above copyright notice, this list
//       of conditions and the following disclaimer in the documentation and/or other materials
//       provided with the distribution.
//
// Modified by Michael T. Mayers (michael@tweakoz.com)
//   modified to use memory embedded into the data structure, as opposed to the heap.
//    when combined with placement new, this allows one to construct a lockfree ringbuffer
//    in a shared memory buffer. Lockfree inter-process communications queues can be constructed from this 
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/atomic.h>
#include <unistd.h>

namespace ork {

static const size_t cacheline_size = 64;

template<typename T,size_t max_items> class MpMcRingBuf
{

public:

	typedef T value_type;

	MpMcRingBuf(); // buffer_size must be power of two
	~MpMcRingBuf();

	MpMcRingBuf(const MpMcRingBuf&oth);

	void push(const T& data,int quanta_usec=250);
	void pop(T& data,int quanta_usec=250);
	bool try_push(const T& data);
	bool try_pop(T& data);

private:

	typedef char cacheline_pad_t [cacheline_size];

	struct cell_t
	{
		cell_t()
			: mData()
		{
			mSequence.store(0);
		}

		cell_t( const cell_t& rhs )
			: mData(rhs.mData)
		{
			mSequence.store(rhs.mSequence.load());
		}

		cell_t& operator = ( const cell_t& rhs )
		{
			mData = rhs.mData;
			mSequence.store(rhs.mSequence.load());
			return *this;
		}

		ork::atomic<size_t>   mSequence;
		T                     mData;


	};

	cacheline_pad_t         mPAD0;
	cell_t  		        mCellBuffer[max_items];
	const size_t            kBufferMask;
	cacheline_pad_t         mPAD1;
	ork::atomic<size_t>     mEnqueuePos;
	cacheline_pad_t         mPAD2;
	ork::atomic<size_t>     mDequeuePos;
	cacheline_pad_t         mPAD3;

}; 

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


template<typename T,size_t max_items>
MpMcRingBuf<T,max_items>::MpMcRingBuf()
	: kBufferMask(max_items - 1)
{

	const bool is_size_power_of_two = (max_items >= 2) && ((max_items & (max_items - 1)) == 0);
	static_assert(is_size_power_of_two,"max_items must be a power of two");
	static_assert(sizeof(ork::atomic<size_t>)==sizeof(size_t),"yo");
	//static_assert(1==2,"one must be equal to one");
	for (size_t i=0; i<max_items; i++ )
	{
		cell_t& the_cell = mCellBuffer[i];
		the_cell.mSequence.store(i,MemRelaxed);
	}

	mEnqueuePos.store(0,MemRelaxed);
	mDequeuePos.store(0,MemRelaxed);
}

template<typename T,size_t max_items>
MpMcRingBuf<T,max_items>::MpMcRingBuf(const MpMcRingBuf&oth)
	: kBufferMask(oth.kBufferMask)
{
	size_t bufsize = kBufferMask+1;

	for (size_t i=0; i<bufsize; i++ )
	{
		const cell_t& src_cell = oth.mCellBuffer[i];
		cell_t& dst_cell = mCellBuffer[i];
		dst_cell.mSequence.store(src_cell.mSequence.load());
		dst_cell.mData = src_cell.mData;
	}
	mEnqueuePos.store(oth.mEnqueuePos.load());
	mDequeuePos.store(oth.mDequeuePos.load());
}

///////////////////////////////////////////////////////////////////////////////

template<typename T,size_t max_items>
MpMcRingBuf<T,max_items>::~MpMcRingBuf()
{
}

///////////////////////////////////////////////////////////////////////////////

template<typename T,size_t max_items>
void MpMcRingBuf<T,max_items>::push(const T& item,int quanta)
{
	bool bpushed = try_push(item);
	while(false==bpushed)
	{
		usleep(quanta);
		bpushed = try_push(item);
	}
}

///////////////////////////////////////////////////////////////////////////////

template<typename T,size_t max_items>
void MpMcRingBuf<T,max_items>::pop(T& item,int quanta)
{
	bool popped = try_pop(item);
	while(false==popped)
	{
		usleep(quanta);
		popped = try_pop(item);
	}
}

///////////////////////////////////////////////////////////////////////////////

template<typename T,size_t max_items>
bool MpMcRingBuf<T,max_items>::try_push(const T& data)
{
	cell_t* cell = nullptr;
	size_t pos = mEnqueuePos.load(MemRelaxed);
	for (;;)
	{
		cell = mCellBuffer + (pos & kBufferMask);
		//////////////////////////////////////
		size_t seq = cell->mSequence.load(MemAcquire);
		intptr_t dif = intptr_t(seq) - intptr_t(pos);
		//////////////////////////////////////
		if (dif == 0)
		{
			size_t newval = pos+1;
			bool changed = mEnqueuePos.compare_exchange_weak(pos,newval,MemRelaxed);
			if(changed)
				break;
		}
		//////////////////////////////////////
		else if (dif < 0) // Full ?
			return false;
		//////////////////////////////////////
		else
			pos = mEnqueuePos.load(MemRelaxed);
	}

	cell->mData = data;
	cell->mSequence.store(pos + 1,MemRelease);
	return true;
}

///////////////////////////////////////////////////////////////////////////////

template<typename T,size_t max_items>
bool MpMcRingBuf<T,max_items>::try_pop(T& data)
{
	cell_t* cell;

	size_t pos = mDequeuePos.load(MemRelaxed);

	for (;;)
	{	cell = mCellBuffer + (pos & kBufferMask);
		//////////////////////////////////////
		size_t seq = cell->mSequence.load(MemAcquire);
		intptr_t dif = intptr_t(seq) - intptr_t(pos + 1);
		//////////////////////////////////////
		if (dif == 0)
		{
			size_t newval = pos+1;
			bool changed = mDequeuePos.compare_exchange_weak(pos,newval,MemRelaxed);
			if(changed)
				break;
		}
		//////////////////////////////////////
		else if (dif < 0) // Empty ?
		{
			return false;
		}
		//////////////////////////////////////
		else
			pos = mDequeuePos.load(MemRelaxed);
	}
	//////////////////////
	// Read From Cell 
	//////////////////////
	data = cell->mData;
	cell->mSequence.store(1+pos+kBufferMask,MemRelease);
	return true;

}

} // ork
