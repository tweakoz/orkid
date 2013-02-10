////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <CoreFoundation/CFBase.h>
#import <CoreFoundation/CFString.h>
#import "touch.h"
#import <map>
#import <vector>
#import <queue>
#import <set>
#import <dispatch/dispatch.h>

#define DISABLE_MTOUCH

#if defined(DISABLE_MTOUCH)


extern "C" void StartTouchReciever(ITouchReciever*tr)
{
}

#else

typedef void *MTDeviceRef;
typedef int (*MTContactCallbackFunction)(int,MtFinger*,int,double,int);
extern "C" MTDeviceRef MTDeviceCreateDefault();
extern "C" void MTRegisterContactFrameCallback(MTDeviceRef, MTContactCallbackFunction);
extern "C" void MTDeviceStart(MTDeviceRef, int); // thanks comex

static ITouchReciever* gActiveTR = nullptr;

template <typename T>
struct rotQ
{
	T*			mpItems;
	const int	miMAX;
	int			miCOUNT;
	int			miWRI;
	int			miRDI;
	
	rotQ( int imax )
		: miMAX(imax)
		, mpItems(nullptr)
		, miCOUNT(0)
		, miWRI(0)
	{
		mpItems = new T[imax];
	}
	
	~rotQ()
	{
		if( mpItems )
			delete mpItems;
	}
	
	int Count() const { return miCOUNT; }
	void Clear()
	{
		miCOUNT = 0;
	}
	void Push( const T& inp )
	{
		mpItems[miWRI] = inp;
		miWRI = (miWRI+1)%miMAX;
		miCOUNT = std::min(miCOUNT+1,miMAX);
		//printf( "rotQ<%p> PUSH cnt<%d>\n", this, miCOUNT );
	}
	const T& Pull()
	{
		//printf( "rotQ<%p> PULL cnt<%d>\n", this, miCOUNT );
		assert(miCOUNT>0);
		int rdi = (miWRI-miCOUNT);
		if( rdi<0 )
			rdi += miMAX;
		assert(rdi>=0);
		assert(rdi<miMAX);
		miCOUNT--;
		return mpItems[rdi];
	}
};

static dispatch_queue_t GetMTDQ2()
{	
	static dispatch_queue_t gQ=0;
	static dispatch_once_t ginit_once;
	auto once_blk = ^ void (void)
	{
		gQ = dispatch_queue_create( "com.tweakoz.mtouch2", NULL );
	};
	dispatch_once(&ginit_once, once_blk );
	return gQ;
}
struct fingerQ
{
	typedef rotQ<MtFinger> q_t;
	
	fingerQ() 
		: mbEnabled(false)
		, mDQ(0) 
		, mUpdateQ(2)
	{
		// dispatch_get_main_queue()
		mDQ = GetMTDQ2();

	}
	
	////////////////////////
	void TouchBegin(MtFinger f_copy)
	{
		auto Qblock = ^ void()
		{
			mbEnabled = true;
			auto UIblock = ^ void()
			{
				if( gActiveTR )
				{
					gActiveTR->OnTouchBegin(&f_copy);
				}
			};
			dispatch_async( dispatch_get_main_queue(), UIblock );
		};
		dispatch_async( mDQ, Qblock );
	}
	////////////////////////
	void TouchEnd(MtFinger f_copy)
	{
		auto block = ^ void()
		{
			mbEnabled = false;
			mUpdateQ.Clear();

			auto UIblock = ^ void()
			{
				if( gActiveTR )
				{
					gActiveTR->OnTouchEnd(&f_copy);
				}
			};
			dispatch_async( dispatch_get_main_queue(), UIblock );
		};
		dispatch_async( mDQ, block );
	}
	////////////////////////
	void TouchUpdate(MtFinger f_copy)
	{
		auto block = ^ void()
		{
			if( mbEnabled )
			{
				mUpdateQ.Push(f_copy);
			}
		};
		dispatch_async( mDQ, block );
	}
	////////////////////////
	////////////////////////
	
	q_t					mUpdateQ;
	bool				mbEnabled;
	dispatch_queue_t	mDQ;
	
	
};
typedef std::map<int,fingerQ> fingerQmap_t;

fingerQmap_t fingerQmap;

struct FingerBlock
{
	static const int kmaxfingers = 4;
	MtFinger fingers[kmaxfingers];
};

////////////////////////////

void mtQ_ui_consumer() // on main Q
{
	///////////////////////////
	// pull from the mtQ
	///////////////////////////

	for( fingerQmap_t::iterator 
			it=fingerQmap.begin();
			it!=fingerQmap.end();
			it++ )
	{
		int ID = it->first;
		fingerQ& FQ = it->second;
		if( FQ.mbEnabled )
		{
			fingerQ::q_t& FQQ = FQ.mUpdateQ;
			while( FQQ.Count() )
			{
				__block MtFinger finger = FQQ.Pull();

				auto UIblock = ^ void()
				{
					if( gActiveTR )
						gActiveTR->OnTouchUpdate(&finger);
				};
				dispatch_async( dispatch_get_main_queue(), UIblock );
			}
			//int isize = FQQ.size();
			//printf( "Q<%d> Size<%d>\n", ID, isize );
		}
	}




	///////////////////////////
	// schedule next appointment
	// TODO : switch to using a timer based dispatch_source
	///////////////////////////

	auto repeat_blk = ^ void (void)
	{
		mtQ_ui_consumer();
	};
	
	dispatch_time_t at = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC/30 );
	dispatch_after( at, GetMTDQ2(), repeat_blk );

}

////////////////////////////

int callback(int device, MtFinger *data, int nFingers, double timestamp, int frame) {

	static std::map<int,MtFinger*> gTouchMap;
	static dispatch_queue_t DQ = dispatch_queue_create( "com.tweakoz.mtouch", NULL );
	static FingerBlock fblock;
	
	static int gPREVNUMFINGERS = 0;
	__block float favgx = 0.0f;
	__block float favgy = 0.0f;

	////////////////////////////
	// kickstart the UI consumer
	////////////////////////////

	static dispatch_once_t ginit_once;
	auto once_blk = ^ void (void)
	{
		mtQ_ui_consumer();
	};
	dispatch_once(&ginit_once, once_blk );

	////////////////////////////
	// copy the fingers out

	if( nFingers>FingerBlock::kmaxfingers ) nFingers=FingerBlock::kmaxfingers;

	for (int i=0; i<nFingers; i++)
	{
		fblock.fingers[i]=data[i];
	}	
	////////////////////////////
	//FingerBlock* pblock = & fblock;
	auto block = ^ void()
	{
		for (int i=0; i<nFingers; i++)
		{
			const MtFinger *f = & fblock.fingers[i];
			float fx = f->normalized.pos.x;
			float fy = f->normalized.pos.y;
			favgx += fx;
			favgy += fy;
			
			fingerQ& FQ = fingerQmap[f->identifier];

			if( gTouchMap.find(f->identifier) != gTouchMap.end() )
			{	
				FQ.TouchUpdate(*f);
				gTouchMap.erase(f->identifier);  
			}
			else {
				FQ.TouchBegin(*f);
			}
		}
		// the remaining ids are touch leave,up events
		for( std::map<int,MtFinger*>::const_iterator iter =
			gTouchMap.begin();
			iter != gTouchMap.end();
		   ++iter )
		{
			//leave
			const MtFinger* f = iter->second;
			fingerQ& FQ = fingerQmap[f->identifier];
			FQ.TouchEnd(*f);
		}
		// update _lastTouches
		gTouchMap.clear();
		for( int i=0; i<nFingers; i++ ) {
			MtFinger* finger = &data[i];
			gTouchMap[finger->identifier] = finger;
		}
	};
	dispatch_async(DQ,block);
	/*
	static float fStartX, fStartY = 0.0f;
	switch( nFingers )
	{
		case 1:
		{
			break;
		}
		case 2:
			favgx *= 0.5f;
			favgy *= 0.5f;
			if( gPREVNUMFINGERS==2 )
			{
				float dx = favgx-fStartX;
				float dy = favgy-fStartY;
				float fudy = fabs(dy);
				bool  y_is_pos = dy>0.0f;

				float fthresh = 0.035f;
				int irept = 1;
				
				if( favgx<0.5f )
					irept = 10;
				
				if( fudy > fthresh )
				{	
					fStartY = favgy;
					
					//if( gActiveTozSynView && gActiveTozSynView->mEditor.mpSynth )
					//{
					//	if( !y_is_pos )
					//		gActiveTozSynView->mEditor.mpSynth->incProgram(irept);
					//	else
					//		gActiveTozSynView->mEditor.mpSynth->decProgram(irept);
					//}
				}
				//printf( "dx<%f> dy<%f>\n", dx, dy );
			}
			else
			{
				fStartX = favgx;
				fStartY = favgy;
			}
			break;
		default:
			break;
	}
	gPREVNUMFINGERS = nFingers;*/
	return 0;
}

extern "C" void StartTouchReciever(ITouchReciever*tr)
{
	gActiveTR = tr;
	MTDeviceRef dev = MTDeviceCreateDefault();
	MTRegisterContactFrameCallback(dev, callback);
	MTDeviceStart(dev, 0);
}

#endif
