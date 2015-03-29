////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/object/Object.h>
#include <ork/kernel/string/ArrayString.h>
#include <ork/dataflow/dataflow.h>
#include <ork/math/cvector4.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/kernel/fixedlut.h>

namespace ork { namespace lev2 { namespace particle {

///////////////////////////////////////////////////////////////////////////////

template <typename ptype> class Pool
{
public:

	orkvector<ptype*>	mActiveParticles;
	orkvector<ptype*>	mInactiveParticles;
	orkvector<ptype>	mParticleBlock;
	
	int					miMaxParticles;

	Pool( );
	void Init( int imax );
	void Copy( const Pool& oth );

	inline int GetMax() const { return miMaxParticles; }
	inline int GetNumAlive() const { return int(mActiveParticles.size()); }
	inline int GetNumDead() const { return int(mInactiveParticles.size()); }

	inline const ptype* GetActiveParticle(int idx) const
	{
		return mActiveParticles[ idx ];
	}
	inline ptype* GetActiveParticle(int idx)
	{
		return mActiveParticles[ idx ];
	}
	inline void Reset()
	{
		mActiveParticles.clear();
		mInactiveParticles.clear();
		for( int i=0; i<miMaxParticles; i++ )
		{
			mInactiveParticles.push_back( & mParticleBlock[i] );
		}
	}

	inline ptype* FastAlloc()
	{
		ptype* rval = 0;
		int inumdead = GetNumDead();
		if( inumdead>0 )
		{
			ptype *ptc = mInactiveParticles[ inumdead-1 ];
			mInactiveParticles.erase( mInactiveParticles.begin()+inumdead-1 );
			mActiveParticles.push_back(ptc);
			rval = ptc;
		}
		return rval;	
	}

	inline void FastFree( int iactiveindex )
	{	ptype* ptc = mActiveParticles[ iactiveindex ];
		mInactiveParticles.push_back(ptc);
		int ilast_alive = GetNumAlive()-1;
		ptype *ptclast = mActiveParticles[ ilast_alive ];
		mActiveParticles[ iactiveindex ] = ptclast;
		mActiveParticles.erase( mActiveParticles.begin()+ilast_alive );
	}
};

//////////////////////////////////////////////////////////////////////////////

template <typename ptype> class Emitter
{
public:
	virtual void Emit( Pool<ptype>& pool, float dt ) = 0;
	virtual void Reap( Pool<ptype>& pool, float dt ) = 0;
	virtual void Reset() {}
};

//////////////////////////////////////////////////////////////////////////////

template <typename ptype> 
class BasicEmitter : public Emitter<ptype>
{
public:
	virtual void Reap( Pool<ptype>& pool, float dt );
};

//////////////////////////////////////////////////////////////////////////////

class SpiralEmitterData : public ork::Object
{
	RttiDeclareConcrete(SpiralEmitterData,ork::Object);

public:

	SpiralEmitterData();

	float GetLifespan() const { return mfLifespan; }
	float GetEmitScale() const { return mfEmitScale; }
	float GetEmissionUp() const { return mfEmissionUp; }
	float GetEmissionOut() const { return mfEmissionOut; }
	float GetEmissionRate() const { return mfEmissionRate; }
	float GetSpinRate() const { return mfSpinRate; }

private:

	float						mfLifespan;
	float						mfEmitScale;
	float						mfEmissionUp;
	float						mfEmissionOut;
	float						mfEmissionRate;
	float						mfSpinRate;

};

//////////////////////////////////////////////////////////////////////////////

template <typename ptype> 
class SpiralEmitter : public BasicEmitter<ptype>
{
public:
	virtual void Emit( Pool<ptype>& pool, float dt );

	SpiralEmitter(const SpiralEmitterData& sed) 
		: mSed(sed)
		, mfEmitterMark(0.0f)
		, miEmitterMark(0)
		, mfPhase( 0.0f )
	{}

private:

	const SpiralEmitterData&	mSed;
	float						mfEmitterMark;
	int							miEmitterMark;
	float						mfPhase;

};

//////////////////////////////////////////////////////////////////////////////

template <typename ptype> class Controller 
{
public:
	virtual void Update( Pool<ptype>& pool, float dt ) = 0;
	virtual void Reset() {}
};

//////////////////////////////////////////////////////////////////////////////

template <typename ptype>
class FixedSystem
{
	Pool<ptype>			mPool;
	Emitter<ptype>*		mEmitter;
	Controller<ptype>*	mController;
	float				mElapsed;
	bool				mbEmitEnable;
	//float				mDuration;

public: //
	
	void Reset();

	//////////////////////////

	void SetMaxParticles( int imax ) { mPool.Init(imax); }
	int GetNumAlive(void) const { return mPool.GetNumAlive(); }
	const ptype* GetActiveParticle(int idx) const { return mPool.GetActiveParticle(idx); }
	//void SetDuration(float fv) { mDuration=fv; }
	void SetElapsed( float fv ) { mElapsed=fv; }
	inline void update( float dt );
	void SetEmitEnable( bool bv ) { mbEmitEnable=bv; }

	//////////////////////////

	FixedSystem( Emitter<ptype>* Emitter = 0, Controller<ptype>* Controller = 0 );
	virtual ~FixedSystem();
};


///////////////////////////////////////////////////////////////////////////////

template <typename ptype>
class ForceField 
{
public: //

	virtual void Apply( Pool<ptype>& pool, float dt ) {}

	ForceField() {}

};

///////////////////////////////////////////////////////////////////////////////

struct BasicParticle
{
	ork::CVector3	mPosition;
	ork::CVector3	mLastPosition;
	ork::CVector3	mVelocity;
	ork::CVector3	mLastVelocity;
	float			mfRandom;
	float			mfAge;
	float			mfLifeSpan;
	void*			mKey;

	bool IsDead( void )
	{
		return(mfAge>=mfLifeSpan);
	}

	BasicParticle()
		: mfAge(0.0f)
		, mfLifeSpan(0.0f)
		, mPosition(0.0f,0.0f,0.0f)
		, mLastPosition(0.0f,0.0f,0.0f)
		, mVelocity(0.0f,0.0f,0.0f)
		, mLastVelocity(0.0f,0.0f,0.0f)
		, mKey(0)
	{
		// todo - this prob should still be deterministic,
		//  have the emitter assign the random val
		
		int irandom	=	(rand()&255)
					|	(rand()&255)<<8;
		mfRandom = float(irandom)/65536.0f;

	}
};

///////////////////////////////////////////////////////////////////////////////

struct Event
{
	Char4 mEventType;
	Char4 mEventId;			// 8
	CVector3 mPosition;		// 20
	CVector3 mVelocity;		// 32
	CVector3 mLastPosition;	// 44
	
	Event( const Event& oth )
		: mEventType(oth.mEventType)
		, mEventId(oth.mEventId)
		, mPosition(oth.mPosition)
		, mVelocity(oth.mVelocity)
		, mLastPosition(oth.mLastPosition)
	{
	}
	Event()
		: mEventType("NULL")
		, mEventId("NULL")
		, mPosition(0.0f,0.0f,0.0f)
		, mLastPosition(0.0f,0.0f,0.0f)
		, mVelocity(0.0f,0.0f,0.0f)
	{
	}
};

struct EventQueue
{
	static const int kmaxevents = 65536;
	fixedvector<Event,kmaxevents>	mEvents;
	int								miSize;
	int								mInIndex;
	int								mOutIndex;
	
	int GetNumEvents() const { return miSize; }
	EventQueue();
	void QueueEvent( const Event& ev );
	Event DequeueEvent();
	void Clear();
};

typedef orklut<Char4,ork::lev2::particle::EventQueue*> EventQueueLut;
struct Context
{
	Context() : mfCurrentTime(0.0f) {}

	EventQueueLut	mEventQueueLut;
	float mfCurrentTime;
	
	EventQueue* MergeQueue(Char4 qname);
	EventQueue* FindQueue(Char4 qname);
	
	void BeginFrame(float currenttime);
	float CurrentTime() const { return mfCurrentTime; }
};

inline Char4 PoolString2Char4( const PoolString& ps )
{
	Char4 rval;
	
	int ilen = ps.c_str() ? strlen(ps.c_str()) : 0;
	
	if( ilen>0 && ilen<=4 )
		rval = Char4(ps.c_str());
	else
		rval.SetU32(0);

	return rval;
}
///////////////////////////////////////////////////////////////////////////////

enum EmitterDirection
{
	EMITDIR_CONSTANT = 0,
	EMITDIR_VEL,
	EMITDIR_CROSS_X,
	EMITDIR_CROSS_Y,
	EMITDIR_CROSS_Z,
	EMITDIR_TO_POINT,
	EMITDIR_USER,
};

struct EmitterCtx
{
	Pool<BasicParticle>*	mPool;
	CVector3				mPosition;
	CVector3				mLastPosition;
	CVector3				mOffsetVelocity;
	EventQueue*				mSpawnQueue;
	EventQueue*				mDeathQueue;
	float					mfSpawnProbability;
	float					mfSpawnMultiplier;
	float					mDispersion;
	float					mfDeltaTime;
	float					mfEmitterMark;
	float					mfEmissionRate;
	float					mfEmissionVelocity;
	float					mfLifespan;
	int						miEmitterMark;
	void*					mKey;

	EmitterCtx();
};

class DirectedEmitter 
{
public:
	void Emit( EmitterCtx& ctx );
	void EmitCB( EmitterCtx& ctx );
	void EmitSQ( EmitterCtx& ctx );
	void Reap( EmitterCtx& ctx );
	EmitterDirection	meDirection;
	float				mDispersionAngle;

	virtual void ComputePosDir( float fi, CVector3& pos, CVector3& dir ) = 0; 
};

enum ParticleItemAlignment
{
	EPIA_BILLBOARD = 0,
	EPIA_XZ,
	EPIA_XY,
	EPIA_YZ,
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class ParticleItemBase;

class ParticleSystemBase : public ork::Object
{
	RttiDeclareAbstract(ParticleSystemBase, ork::Object);

public:

	ParticleSystemBase(const ParticleItemBase& itemb);
	~ParticleSystemBase();

	void Reset();
	void Update(float fdt);
	float GetElapsed() const { return mElapsed; }	
	void SetElapsed( float fv );

protected:

	float mElapsed;
	const ParticleItemBase& mItem;

	virtual void DoUpdate( float fdt ) = 0;
	virtual void DoReset() {}
	virtual void DoSetElapsed( float fv ) {}
	virtual void SetEmitterEnable( bool bv ) = 0;

};

///////////////////////////////////////////////////////////////////////////////

class ParticleItemBase : public ork::Object
{
	RttiDeclareAbstract(ParticleItemBase, ork::Object);
	
public:

	float GetPreCharge() const { return mfPreCharge; }
	float GetStartTime() const { return mfStartTime; }
	float GetDuration() const { return mfDuration; }
	float GetSortValue() const { return mfSortValue; }
	bool IsWorldSpace() const { return mbWorldSpace; }

	~ParticleItemBase();

protected:

	ParticleItemBase();

	float											mfPreCharge;
	float											mfStartTime;
	float											mfDuration;
	float											mfSortValue;
	bool											mbWorldSpace;

};

///////////////////////////////////////////////////////////////////////////////

} } }

///////////////////////////////////////////////////////////////////////////////
