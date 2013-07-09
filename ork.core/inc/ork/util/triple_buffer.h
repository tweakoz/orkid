
#pragma once

#include <tbb/atomic.h>
#include <tbb/concurrent_queue.h>
#include <assert.h>
#include <unistd.h>

template <typename T>
class concurrent_triple_buffer
{	
public:
	typedef T value_type;

private:

	static const int kquanta = 1;

	////////////////////////////////
	// StateField (64 possible states)
	////////////////////////////////
	// writ { none, 0, 1, 2 }
	// read { none, 0, 1, 2 }
	// enab { off, starting, on, stopping }
	////////////////////////////////

	struct StateField
	{
		uint32_t mRead;
		uint32_t mWrit;
		uint32_t mNxtR;
		uint32_t mEnab;

		StateField() : mRead(0), mWrit(0), mEnab(0), mNxtR(0) {}
		StateField(uint32_t i)
		{
			UnPack(i);
		}
		uint32_t Pack()
		{
			assert(mRead<4);
			assert(mWrit<4);
			assert(mEnab<4);
			assert(mNxtR<4);
			uint32_t rval = (mRead)|(mWrit<<2)|(mEnab<<4)|(mNxtR<<6);
			return rval;
		}
		void Sanity()
		{
			uint32_t v = Pack();
			assert(v<0x100);
		}
		void UnPack(uint32_t iv)
		{
			assert(iv<0x100);
			mRead=(iv&3);
			mWrit=(iv>>2)&3;
			mEnab=(iv>>4)&3;
			mNxtR=(iv>>6)&3;
		}
		void CommonUpdate()
		{	
			switch( mEnab )
			{	case 0: // off
					assert(mRead==0);
					assert(mWrit==0);
					mNxtR=0;
					break;
				case 1: // starting
					assert(mRead==0);
					assert(mWrit==0);
					mEnab = 2; // started
					mNxtR=0;
					break;
				case 3: // stopping
					if( mRead==mWrit==0 )
						mEnab = 0; // stopped
					mNxtR=0;
					break;
				default:
					break;
			}
			Sanity();	
		}
	};

	/////////////////////////////
	public:
	/////////////////////////////
	concurrent_triple_buffer() 
	{
		mState = 1<<4; // starting
		mValues[0] = new T(0);
		mValues[1] = new T(1);
		mValues[2] = new T(2);
	}
	/////////////////////////////
	~concurrent_triple_buffer() 
	{
		disable();
		delete mValues[2];
		delete mValues[1];
		delete mValues[0];
	}
	/////////////////////////////
	// compute and return new write buffer
	/////////////////////////////
	T* begin_push(void) // get handle to a write buffer
	{	while(1)
		{	uint32_t ost(mState);
			StateField nsf(ost);
			nsf.CommonUpdate();
			uint32_t ord = nsf.mRead;
			uint32_t nwr = nsf.mNxtR;
			switch( nsf.mEnab )
			{	case 2: // on
				{	assert(nwr<4);
					assert(nsf.mRead<4);
					uint32_t Z = 0x100;
					/////////////////////////////////////////////////////////////
					uint32_t tab0[4] = { 1, 2, 3, 1 }; // ord:0
					uint32_t tab1[4] = { 2, 3, 2, 3 }; // ord:1
					uint32_t tab2[4] = { 3, 1, 3, 1 }; // ord:2
					uint32_t tab3[4] = { 1, 2, 1, 2 }; // ord:3
					/////////////////////////////////////////////////////////////
					uint32_t* tabV[4] = { tab0, tab1, tab2, tab3 };
					nwr = (tabV[nsf.mRead])[nwr];
					assert(nwr<4);
					break;
				}
				default:
					nwr = 0;
					break;
			}
			nsf.mWrit=nwr;
			uint32_t nst = nsf.Pack();
			if( ost==mState.compare_and_swap(nst,ost) )
			{
				if(nwr>0)
					assert(ord!=nwr);

				return (nwr>0) ? mValues[nwr-1] : nullptr;
			}
			else usleep(kquanta);
		}
		return nullptr;
	}
	/////////////////////////////
	// publish write buffer
	/////////////////////////////
	void end_push(T* pret)
	{	while(1)
		{	uint32_t ost(mState);
			StateField nsf(ost);
			nsf.mNxtR = nsf.mWrit;
			uint32_t nst = nsf.Pack();
			if( ost==mState.compare_and_swap(nst,ost) )
			{
				return;
			}
			else usleep(kquanta);
		}
	}
	/////////////////////////////
	const T* begin_pull(void) const// get a read buffer
	{
		while(1)
		{	uint32_t ost(mState);
			StateField nsf(ost);
			nsf.CommonUpdate();
			uint32_t nrd = (nsf.mEnab==2) ? nsf.mNxtR : 0;
			nsf.mRead=nrd;
			uint32_t nst = nsf.Pack();
			auto nonc = const_cast<concurrent_triple_buffer*>(this);
			if( ost==nonc->mState.compare_and_swap(nst,ost) )
			{
				return (nrd>0) ? mValues[nrd-1] : nullptr;
			}
			else usleep(kquanta);
		}
		return nullptr;
	}
	/////////////////////////////
	// done reading
	/////////////////////////////
	void end_pull(const T*pret) const
	{
		while(1)
		{	uint32_t ost(mState);
			StateField nsf(ost);
			nsf.CommonUpdate();
			nsf.mRead = 0;
			uint32_t nst = nsf.Pack();
			auto nonc = const_cast<concurrent_triple_buffer*>(this);
			if( ost==nonc->mState.compare_and_swap(nst,ost) )
			{
				return;
			}
			else usleep(kquanta);
		}
	}
	/////////////////////////////	
	void disable()
	{
		bool bc = true;
		while(bc)
		{
			uint32_t ost(mState);
			StateField sf(ost);
			sf.mEnab = 3; // stopping
			sf.mWrit = 0;
			sf.mNxtR = 0;
			uint32_t nst = sf.Pack();
			bool was_set = (ost==mState.compare_and_swap(nst,ost));
			bc = (false==was_set);
			usleep(kquanta);
		}
		bc = true;
		while(bc)
		{
			uint32_t ost(mState);
			StateField sf(ost);
			bc = (sf.mRead!=0);
			usleep(kquanta);
		}
		mState = 0;
	}
	/////////////////////////////	
	void enable()
	{
		mState = 1<<4; // starting
	}
	/////////////////////////////	
	private: // 
	/////////////////////////////
	T* mValues[3]; 						
	tbb::atomic<int> mState;	
};

