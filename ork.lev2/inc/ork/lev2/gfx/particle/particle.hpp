////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef ORK_LEV2_GFX_PARTICLE_PARTICLE_HPP
#define ORK_LEV2_GFX_PARTICLE_PARTICLE_HPP

#include <ork/lev2/gfx/particle/particle.h>

namespace ork { namespace lev2 { namespace particle {

///////////////////////////////////////////////////////////////////////////////

template <typename ptype>
Pool<ptype>::Pool( )
	: miMaxParticles(0)
{

}

///////////////////////////////////////////////////////////////////////////////

template <typename ptype>
void Pool<ptype>::Init( int imax )
{
	miMaxParticles = imax;
	mParticleBlock.resize(imax);
	mActiveParticles.reserve(imax);
	mInactiveParticles.reserve(imax);
	mInactiveParticles.clear();
	mActiveParticles.clear();

	for( int i=0; i<miMaxParticles; i++ )
	{
		mInactiveParticles.push_back( & mParticleBlock[i] );
	}

}

template <typename ptype>
void Pool<ptype>::Copy( const Pool<ptype>& oth )
{
	miMaxParticles = oth.miMaxParticles;
	mActiveParticles.reserve(miMaxParticles);
	mInactiveParticles.reserve(miMaxParticles);
	mActiveParticles.clear();
	mInactiveParticles.clear();
	mParticleBlock = oth.mParticleBlock;
	
	const ptype* pfirst = & oth.mParticleBlock[0];
	size_t inumactive = oth.mActiveParticles.size();
	for( size_t i=0; i<inumactive; i++ )
	{	ptype* ptc = oth.mActiveParticles[i];
		size_t index = (ptc-pfirst);
		mActiveParticles.push_back(& mParticleBlock[index] );
	}
	size_t inuminactive = oth.mInactiveParticles.size();
	for( size_t i=0; i<inuminactive; i++ )
	{	ptype* ptc = oth.mInactiveParticles[i];
		size_t index = (ptc-pfirst);
		mInactiveParticles.push_back(& mParticleBlock[index] );
	}
}

///////////////////////////////////////////////////////////////////////////////

template <typename ptype>
void BasicEmitter<ptype>::Reap( Pool<ptype>& pool, float dt )
{
	//////////////////////////
	// kill particles
	
	for( int i=0; i<pool.GetNumAlive(); i++ )
	{
		ptype *ptc = pool.mActiveParticles[ i ];

		if( ptc->IsDead() ) // kill particle
		{
			pool.mInactiveParticles.push_back(ptc);

			int ilast_alive = pool.GetNumAlive()-1;
			ptype *ptclast = pool.mActiveParticles[ ilast_alive ];

			pool.mActiveParticles[ i ] = ptclast;

			pool.mActiveParticles.erase( pool.mActiveParticles.begin()+ilast_alive );
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

template <typename ptype>
void SpiralEmitter<ptype>::Emit( Pool<ptype>& pool, float dt )
{
	// emit new particles

	float scaler = mSed.GetEmitScale();

	float fdeltap = (mSed.GetEmissionRate()*dt);
	float femitout = mSed.GetEmissionOut();
	mfEmitterMark += fdeltap;

	while( mfEmitterMark>=1.0f )
	{
		int inumdead = pool.GetNumDead();

		if( inumdead>0 )
		{
			ptype *ptc = pool.mInactiveParticles[ inumdead-1 ];
			pool.mInactiveParticles.erase( pool.mInactiveParticles.begin()+inumdead-1 );
			pool.mActiveParticles.push_back(ptc);

			float fsx = sinf(mfPhase);
			float fsz = cosf(mfPhase);

			ptc->mfAge = 0.0f;
			ptc->mfLifeSpan = mSed.GetLifespan();
			ptc->mPosition.SetX( fsx*scaler  );
			ptc->mPosition.SetY( 0.0f );
			ptc->mPosition.SetZ( fsz*scaler );
			ptc->mVelocity.SetX( fsx*femitout );
			ptc->mVelocity.SetY( mSed.GetEmissionUp() );
			ptc->mVelocity.SetZ( fsz*femitout );

			mfPhase += mSed.GetSpinRate();

		}
		mfEmitterMark-=1.0f;
	}
}

///////////////////////////////////////////////////////////////////////////////

template <typename ptype>
void FixedSystem<ptype>::update( float dt )
{
	if( mbEmitEnable )
	{
		mEmitter->Emit( mPool, dt );
	}	
	
	mController->Update( mPool, dt );
	mEmitter->Reap( mPool, dt );

	mElapsed += dt;
}

///////////////////////////////////////////////////////////////////////////////

template <typename ptype>
FixedSystem<ptype>::FixedSystem(
		Emitter<ptype>*		Emitter,
		Controller<ptype>*	Controller
)
	: mEmitter(Emitter)
	, mController(Controller)
	, mElapsed( 0.0f )
{
}

template <typename ptype>
FixedSystem<ptype>::~FixedSystem()
{
}

///////////////////////////////////////////////////////////////////////////////

template <typename ptype>
void FixedSystem<ptype>::Reset()
{
	mElapsed = 0.0f;
	mPool.Reset();
	if( mEmitter ) mEmitter->Reset();
	if( mController ) mController->Reset();
}

///////////////////////////////////////////////////////////////////////////////

} } }

#endif // ORK_LEV2_GFX_PARTICLE_PARTICLE_HPP
