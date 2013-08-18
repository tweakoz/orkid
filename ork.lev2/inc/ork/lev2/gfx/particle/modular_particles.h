////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/particle/particle.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/dataflow/dataflow.h>
#include <ork/math/gradient.h>
#include <ork/kernel/any.h>

namespace ork { namespace lev2 { class RenderContextInstData; }}
namespace ork { namespace lev2 { class GfxMaterial3DSolid; }}
namespace ork { namespace lev2 { class TextureAsset; }}
namespace ork { namespace lev2 { class XgmModelAsset; }}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define DeclarePoolOutPlug( name )\
psys_ptclbuf OutDataName(name);\
PtclBufOutPlug	OutPlugName(name);\
ork::Object* OutAccessor##name() { return & OutPlugName(name); }

#define DeclarePoolInpPlug( name )\
PtclBufInpPlug	InpPlugName(name);\
ork::Object* InpAccessor##name() { return & InpPlugName(name); }

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 { namespace particle {
///////////////////////////////////////////////////////////////////////////////

struct psys_ptclbuf
{
	Pool<BasicParticle>*	mPool;
	psys_ptclbuf( Pool<BasicParticle>* pl = 0 ) : mPool(pl) {}
};

typedef ork::dataflow::outplug<psys_ptclbuf> PtclBufOutPlug;
typedef ork::dataflow::inplug<psys_ptclbuf> PtclBufInpPlug;

///////////////////////////////////////////////////////////////////////////////

class Module : public ork::dataflow::dgmodule
{
	RttiDeclareAbstract( Module, ork::dataflow::dgmodule );
	////////////////////////////////////////////////////////////
	virtual void Compute( ork::dataflow::workunit* wu ) {}
	virtual void CombineWork( const ork::dataflow::cluster* c ) {}
	////////////////////////////////////////////////////////////
protected:
	Module();
	virtual void DoLink() {}
	ork::lev2::particle::Context* mpParticleContext;
	Module*	mpTemplateModule;
public:
	void Link(ork::lev2::particle::Context* pctx);
	virtual void Compute( float dt ) = 0; // virtual
	virtual void Reset() {}
	void SetTemplateModule( Module* ptemp ) { mpTemplateModule = ptemp; }
};

///////////////////////////////////////////////////////////////////////////////

class Global : public Module
{
	RttiDeclareConcrete( Global, Module );

private:

	DeclareFloatXfPlug( TimeScale );

	virtual int GetNumInputs() const { return 1; }
	virtual dataflow::inplugbase* GetInput(int idx) { return &mPlugInpTimeScale; } 

	virtual int GetNumOutputs() const { return 9; }
	virtual dataflow::outplugbase* GetOutput(int idx);

	DeclareFloatOutPlug( RelTime );
	DeclareFloatOutPlug( Time );
	DeclareFloatOutPlug( TimeDiv10 );
	DeclareFloatOutPlug( TimeDiv100 );
	DeclareFloatOutPlug( Random );
	DeclareFloatOutPlug( Noise );
	DeclareFloatOutPlug( FastNoise );
	DeclareFloatOutPlug( SlowNoise );
	DeclareVect3OutPlug( RandomNormal );

	virtual void Compute( float dt ); // virtual
	virtual void OnStart();

	float		mfNoiseRat;
	float		mfSlowNoiseRat;
	float		mfFastNoiseRat;
	float		mfNoisePrv;
	float		mfNoiseNew;
	float		mfSlowNoisePrv;
	float		mfFastNoisePrv;
	float		mfNoiseBas;
	float		mfSlowNoiseBas;
	float		mfFastNoiseBas;
	float		mfNoiseTim;
	float		mfSlowNoiseTim;
	float		mfFastNoiseTim;
	float		mfTimeBase;

public:

	Global(); 
};

///////////////////////////////////////////////////////////////////////////////

class Constants : public Module
{
	RttiDeclareConcrete( Constants, Module );

	ork::orklut<ork::PoolString, ork::dataflow::outplug<float>* >		mFloatPlugs;
	ork::orklut<ork::PoolString, ork::dataflow::outplug<CVector3>* >	mVect3Plugs;

	ork::orklut<ork::PoolString,float>									mFloatConsts;
	ork::orklut<ork::PoolString,CVector3>								mVect3Consts;

	bool																mbPlugsDirty;
	/////////////////////////////////////////////////////
	// data currently only flows in from externals
	int GetNumInputs() const { return 0; } // virtual
	dataflow::inplugbase* GetInput(int idx) { return 0; } // virtual
	// data currently only flows in from externals
	/////////////////////////////////////////////////////

	int GetNumOutputs() const; // virtual
	dataflow::outplugbase* GetOutput(int idx); // virtual

	void Compute( float dt ) {} // virtual

	void OnTopologyUpdate(void); // virtual

	bool DoNotify(const ork::event::Event *event); // virtual
	bool PostDeserialize(reflect::IDeserializer &); // virtual

public:

	Constants(); 
};

///////////////////////////////////////////////////////////////////////////////

class ExtConnector : public Module
{
	RttiDeclareConcrete( ExtConnector, Module );

	dataflow::dyn_external*	mpExternalConnector;
	ork::orklut<ork::PoolString, ork::dataflow::outplug<float>* >		mFloatPlugs;
	ork::orklut<ork::PoolString, ork::dataflow::outplug<CVector3>* >	mVect3Plugs;

	/////////////////////////////////////////////////////
	// data currently only flows in from externals
	virtual int GetNumInputs() const { return 0; }
	virtual dataflow::inplugbase* GetInput(int idx) { return 0; }
	// data currently only flows in from externals
	/////////////////////////////////////////////////////

	virtual int GetNumOutputs() const;
	virtual dataflow::outplugbase* GetOutput(int idx);

	virtual void Compute( float dt ) {}


public:

	ExtConnector(); 
	void BindConnector( dataflow::dyn_external* pconnector );
};

///////////////////////////////////////////////////////////////////////////////

class ParticleModule : public Module
{
	RttiDeclareAbstract( ParticleModule, Module );
	bool IsDirty(void) const { return true; } // virtual
protected:
	static psys_ptclbuf		gNoCon;
	ParticleModule() {}
	////////////////////////////////////////////////////////////
	void MarkClean() {}
};

///////////////////////////////////////////////////////////////////////////////

class ParticlePool : public ParticleModule
{
	RttiDeclareConcrete( ParticlePool, ParticleModule );

	DeclareFloatXfPlug( PathInterval );
	DeclareFloatXfPlug( PathProbability );
	DeclarePoolOutPlug( Output );
	DeclareFloatOutPlug( UnitAge );
	Pool<BasicParticle> mPoolOutput;

	int											miPoolSize;
	lev2::particle::EventQueue*					mPathStochasticEventQueue;
	PoolString									mPathStochasticQueueID;
	Char4										mPathStochasticQueueID4;
	lev2::particle::EventQueue*					mPathIntervalEventQueue;
	PoolString									mPathIntervalQueueID;
	Char4										mPathIntervalQueueID4;
	

	/////////////////////////////////////////////////
	int GetNumInputs() const { return 2; }		// virtual
	dataflow::inplugbase* GetInput(int idx);	// virtual
	/////////////////////////////////////////////////
	virtual int GetNumOutputs() const { return 2; }
	virtual ork::dataflow::outplugbase* GetOutput(int idx);
	/////////////////////////////////////////////////
	void Compute( float dt ); // virtual
	void Reset(); // virtual
	void DoLink(); // virtual

public:
	ParticlePool();
	const Pool<BasicParticle>& GetPool() const { return mPoolOutput; }
};

struct ParticlePoolRenderBuffer
{
	BasicParticle*	mpParticles;
	int				miMaxParticles;
	int				miNumParticles;
	ParticlePoolRenderBuffer();
	~ParticlePoolRenderBuffer();
	void Update( const Pool<BasicParticle>& pool );
	void SetCapacity( int inum );
};

///////////////////////////////////////////////////////////////////////////////

class RingEmitter;

class RingDirectedEmitter : public DirectedEmitter 
{
	RingEmitter& mEmitterModule;
	void ComputePosDir( float fi, CVector3& pos, CVector3& dir ); 
public:
	RingDirectedEmitter(RingEmitter& EmitterModule) : mEmitterModule(EmitterModule) {}
	CVector3 mUserDir;
};

class RingEmitter : public ParticleModule
{
	friend class RingDirectedEmitter;

	RttiDeclareConcrete( RingEmitter, ParticleModule );

	float										mfPhase, mfPhase2;
	float										mfLastRadius, mfThisRadius;
	float										mfAccumTime;
	RingDirectedEmitter							mDirectedEmitter;
	EmitterDirection							meDirection;
	EmitterCtx									mEmitterCtx;
	////////////////////////////////////////////////////////////////
	lev2::particle::EventQueue*					mDeathEventQueue;
	PoolString									mDeathQueueID;
	Char4										mDeathQueueID4;
	////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////
	// inputs
	//////////////////////////////////////////////////

	int GetNumInputs() const { return 11; }		// virtual
	dataflow::inplugbase* GetInput(int idx);	// virtual

	DeclarePoolInpPlug( Input )
	DeclareFloatXfPlug( Lifespan );
	DeclareFloatXfPlug( EmissionRadius );
	DeclareFloatXfPlug( EmissionRate );
	DeclareFloatXfPlug( EmissionVelocity );
	DeclareFloatXfPlug( EmitterSpinRate );
	DeclareFloatXfPlug( DispersionAngle );
	DeclareFloatXfPlug( OffsetX );
	DeclareFloatXfPlug( OffsetY );
	DeclareFloatXfPlug( OffsetZ );
	DeclareVect3XfPlug( Direction );

	//////////////////////////////////////////////////
	// outputs
	//////////////////////////////////////////////////

	int GetNumOutputs() const { return 1; }		// virtual
	dataflow::outplugbase* GetOutput(int idx);	// virtual

	DeclarePoolOutPlug( Output )

	//////////////////////////////////////////////////

	void DoLink(); // virtual
	void Compute( float dt );			// virtual

	void Emit( float fdt );
	void Reap( float fdt );
	void Reset(); // virtual

public:
	RingEmitter();
};

///////////////////////////////////////////////////////////////////////////////

class NozzleEmitter;

class NozzleDirectedEmitter : public DirectedEmitter 
{
	NozzleEmitter& mEmitterModule;
	void ComputePosDir( float fi, CVector3& pos, CVector3& dir ); 
public:
	NozzleDirectedEmitter(NozzleEmitter& EmitterModule) : mEmitterModule(EmitterModule) {}
};

class NozzleEmitter : public ParticleModule
{
	friend class NozzleDirectedEmitter;

	RttiDeclareConcrete( NozzleEmitter, ParticleModule );

	float										mfAccumTime;
	Pool<BasicParticle>							mDeadPool;
	EmitterCtx									mEmitterCtx;
	NozzleDirectedEmitter						mDirectedEmitter;
	CVector3									mLastPosition;
	CVector3									mLastDirection;
	CVector3									mDirectionSample;
	CVector3									mOffsetSample;

	//////////////////////////////////////////////////
	// inputs
	//////////////////////////////////////////////////

	int GetNumInputs() const { return 8; }		// virtual
	dataflow::inplugbase* GetInput(int idx);	// virtual

	DeclarePoolInpPlug( Input )
	DeclareFloatXfPlug( Lifespan );
	DeclareFloatXfPlug( EmissionRate );
	DeclareFloatXfPlug( EmissionVelocity );
	DeclareFloatXfPlug( DispersionAngle );
	DeclareVect3XfPlug( Offset );
	DeclareVect3XfPlug( Direction );
	DeclareVect3XfPlug( OffsetVelocity );

	//////////////////////////////////////////////////
	// outputs
	//////////////////////////////////////////////////

	int GetNumOutputs() const { return 2; }		// virtual
	dataflow::outplugbase* GetOutput(int idx);	// virtual

	DeclarePoolOutPlug( Output )
	DeclarePoolOutPlug( TheDead )

	//////////////////////////////////////////////////

	void Compute( float dt );					// virtual

	void Emit( float fdt );
	void Reap( float fdt );
	void Reset(); // virtual

public:
	NozzleEmitter();
};

///////////////////////////////////////////////////////////////////////////////

class ReEmitter;

class ReDirectedEmitter : public DirectedEmitter 
{
	ReEmitter& mEmitterModule;
	void ComputePosDir( float fi, CVector3& pos, CVector3& dir ); 
public:
	ReDirectedEmitter(ReEmitter& EmitterModule) : mEmitterModule(EmitterModule) {}
};

class ReEmitter : public ParticleModule
{
	friend class PoolDirectedEmitter;

	RttiDeclareConcrete( ReEmitter, ParticleModule );

	ReDirectedEmitter									mDirectedEmitter;
	EmitterDirection									meDirection;
	EmitterCtx											mEmitterCtx;
	lev2::particle::EventQueue*							mSpawnEventQueue;
	lev2::particle::EventQueue*							mDeathEventQueue;
	PoolString											mSpawnQueueID;
	PoolString											mDeathQueueID;
	Char4												mSpawnQueueID4;
	Char4												mDeathQueueID4;
	//////////////////////////////////////////////////
	// inputs
	//////////////////////////////////////////////////

	virtual int GetNumInputs() const { return 5; }
	virtual dataflow::inplugbase* GetInput(int idx);
	DeclarePoolInpPlug( Input )
	DeclareFloatXfPlug( SpawnProbability );
	DeclareFloatXfPlug( SpawnMultiplier );
	DeclareFloatXfPlug( Lifespan );
	DeclareFloatXfPlug( EmissionRate );
	DeclareFloatXfPlug( EmissionVelocity );
	DeclareFloatXfPlug( DispersionAngle );

	//////////////////////////////////////////////////
	// outputs
	//////////////////////////////////////////////////

	virtual int GetNumOutputs() const { return 1; }
	virtual dataflow::outplugbase* GetOutput(int idx);

	DeclarePoolOutPlug( Output )

	//////////////////////////////////////////////////

	void DoLink(); // virtual
	void Compute( float dt ); // virtual

	void Emit( float fdt );
	void Reap( float fdt );

public:
	ReEmitter();
};

///////////////////////////////////////////////////////////////////////////////

class WindModule : public ParticleModule
{
	RttiDeclareConcrete( WindModule, ParticleModule );

	//////////////////////////////////////////////////
	// inputs
	//////////////////////////////////////////////////

	virtual int GetNumInputs() const { return 2; }
	virtual dataflow::inplugbase* GetInput(int idx);

	DeclarePoolInpPlug( Input )
	DeclareFloatXfPlug( Force );

	//////////////////////////////////////////////////
	// outputs
	//////////////////////////////////////////////////

	virtual int GetNumOutputs() const { return 1; }
	virtual dataflow::outplugbase* GetOutput(int idx) { return & mPlugOutOutput; }

	DeclarePoolOutPlug( Output )

	//////////////////////////////////////////////////

	void Compute( float dt ); // virtual 

public:

	WindModule();	
};

///////////////////////////////////////////////////////////////////////////////

class GravityModule : public ParticleModule
{
	RttiDeclareConcrete( GravityModule, ParticleModule );

	//////////////////////////////////////////////////
	// inputs
	//////////////////////////////////////////////////

	virtual int GetNumInputs() const { return 6; }
	virtual dataflow::inplugbase* GetInput(int idx);

	DeclarePoolInpPlug( Input )
	DeclareFloatXfPlug( Mass );
	DeclareFloatXfPlug( OthMass );
	DeclareFloatXfPlug( G );
	DeclareFloatXfPlug( MinDistance );
	DeclareVect3XfPlug( Center )


	//////////////////////////////////////////////////
	// outputs
	//////////////////////////////////////////////////

	virtual int GetNumOutputs() const { return 1; }
	virtual dataflow::outplugbase* GetOutput(int idx) { return & mPlugOutOutput; }

	DeclarePoolOutPlug( Output )

	//////////////////////////////////////////////////

	void Compute( float dt ); // virtual 

public:

	GravityModule();	
};

///////////////////////////////////////////////////////////////////////////////

class PlanarColliderModule : public ParticleModule
{
	RttiDeclareConcrete( PlanarColliderModule, ParticleModule );

	int	miDiodeDirection;
	
	//////////////////////////////////////////////////
	// inputs
	//////////////////////////////////////////////////

	virtual int GetNumInputs() const { return 8; }
	virtual dataflow::inplugbase* GetInput(int idx);

	DeclarePoolInpPlug( Input );
	DeclareFloatXfPlug( NormalX );
	DeclareFloatXfPlug( NormalY );
	DeclareFloatXfPlug( NormalZ );
	DeclareFloatXfPlug( OriginX );
	DeclareFloatXfPlug( OriginY );
	DeclareFloatXfPlug( OriginZ );
	DeclareFloatXfPlug( Absorbtion );

	//////////////////////////////////////////////////
	// outputs
	//////////////////////////////////////////////////

	virtual int GetNumOutputs() const { return 1; }
	virtual dataflow::outplugbase* GetOutput(int idx) { return & mPlugOutOutput; }

	DeclarePoolOutPlug( Output )

	//////////////////////////////////////////////////

	void Compute( float dt ); // virtual 

public:

	PlanarColliderModule();	
};

///////////////////////////////////////////////////////////////////////////////

enum EPSYS_FLOATOP
{
	EPSYS_FLOATOP_ADD = 0,
	EPSYS_FLOATOP_SUB,
	EPSYS_FLOATOP_MUL,
};

class FloatOp2Module : public ParticleModule
{
	RttiDeclareConcrete( FloatOp2Module, ParticleModule );

	EPSYS_FLOATOP	meOp;

	//////////////////////////////////////////////////
	// inputs
	//////////////////////////////////////////////////

	virtual int GetNumInputs() const { return 2; }
	virtual dataflow::inplugbase* GetInput(int idx);

	DeclareFloatXfPlug( InputA )
	DeclareFloatXfPlug( InputB );

	//////////////////////////////////////////////////
	// outputs
	//////////////////////////////////////////////////

	virtual int GetNumOutputs() const { return 1; }
	virtual dataflow::outplugbase* GetOutput(int idx) { return & mPlugOutOutput; }

	DeclareFloatOutPlug( Output );

	//////////////////////////////////////////////////

	void Compute( float dt ); // virtual 

public:

	FloatOp2Module();	
};

///////////////////////////////////////////////////////////////////////////////

enum EPSYS_VEC3OP
{
	EPSYS_VEC3OP_ADD = 0,
	EPSYS_VEC3OP_SUB,
	EPSYS_VEC3OP_MUL,
	EPSYS_VEC3OP_DOT,
	EPSYS_VEC3OP_CROSS,
};

class Vec3Op2Module : public ParticleModule
{
	RttiDeclareConcrete( Vec3Op2Module, ParticleModule );

	EPSYS_VEC3OP	meOp;

	//////////////////////////////////////////////////
	// inputs
	//////////////////////////////////////////////////

	virtual int GetNumInputs() const { return 2; }
	virtual dataflow::inplugbase* GetInput(int idx);

	DeclareVect3XfPlug( InputA )
	DeclareVect3XfPlug( InputB );

	//////////////////////////////////////////////////
	// outputs
	//////////////////////////////////////////////////

	virtual int GetNumOutputs() const { return 1; }
	virtual dataflow::outplugbase* GetOutput(int idx)
	{
		return & mPlugOutOutput;
	}

	DeclareVect3OutPlug( Output );

	//////////////////////////////////////////////////

	void Compute( float dt ); // virtual 

public:

	Vec3Op2Module();	
};

///////////////////////////////////////////////////////////////////////////////

class Vec3SplitModule : public ParticleModule
{
	RttiDeclareConcrete( Vec3SplitModule, ParticleModule );

	//////////////////////////////////////////////////
	// inputs
	//////////////////////////////////////////////////

	virtual int GetNumInputs() const { return 1; }
	virtual dataflow::inplugbase* GetInput(int idx);

	DeclareVect3XfPlug( Input );

	//////////////////////////////////////////////////
	// outputs
	//////////////////////////////////////////////////

	virtual int GetNumOutputs() const { return 3; }
	virtual dataflow::outplugbase* GetOutput(int idx)
	{
		switch( idx )
		{
			case 0: return & mPlugOutOutputX;
			case 1: return & mPlugOutOutputY;
			case 2: return & mPlugOutOutputZ;
		}
		return 0;
	}

	DeclareFloatOutPlug( OutputX );
	DeclareFloatOutPlug( OutputY );
	DeclareFloatOutPlug( OutputZ );

	//////////////////////////////////////////////////

	void Compute( float dt ); // virtual 

public:

	Vec3SplitModule();	
};

///////////////////////////////////////////////////////////////////////////////

class DecayModule : public ParticleModule
{
	RttiDeclareConcrete( DecayModule, ParticleModule );

	//////////////////////////////////////////////////
	// inputs
	//////////////////////////////////////////////////

	virtual int GetNumInputs() const { return 2; }
	virtual dataflow::inplugbase* GetInput(int idx);

	DeclarePoolInpPlug( Input )
	DeclareFloatXfPlug( Decay );

	//////////////////////////////////////////////////
	// outputs
	//////////////////////////////////////////////////

	virtual int GetNumOutputs() const { return 1; }
	virtual dataflow::outplugbase* GetOutput(int idx) { return & mPlugOutOutput; }

	DeclarePoolOutPlug( Output )

	//////////////////////////////////////////////////

	void Compute( float dt ); // virtual 

public:

	DecayModule();	
};

///////////////////////////////////////////////////////////////////////////////

class TurbulenceModule : public ParticleModule
{
	RttiDeclareConcrete( TurbulenceModule, ParticleModule );

	//////////////////////////////////////////////////
	// inputs
	//////////////////////////////////////////////////

	virtual int GetNumInputs() const { return 4; }
	virtual dataflow::inplugbase* GetInput(int idx);

	DeclareFloatXfPlug( AmountX );
	DeclareFloatXfPlug( AmountY );
	DeclareFloatXfPlug( AmountZ );
	DeclarePoolInpPlug( Input )

	//////////////////////////////////////////////////
	// outputs
	//////////////////////////////////////////////////

	DeclarePoolOutPlug( Output )

	virtual int GetNumOutputs() const { return 1; }
	virtual dataflow::outplugbase* GetOutput(int idx) { return & mPlugOutOutput; }

	//////////////////////////////////////////////////

	void Compute( float dt ); // virtual 

public:

	TurbulenceModule();	
};

///////////////////////////////////////////////////////////////////////////////

class VortexModule : public ParticleModule
{
	RttiDeclareConcrete( VortexModule, ParticleModule );

	//////////////////////////////////////////////////
	// inputs
	//////////////////////////////////////////////////

	virtual int GetNumInputs() const { return 4; }
	virtual dataflow::inplugbase* GetInput(int idx);

	DeclareFloatXfPlug( Falloff );
	DeclareFloatXfPlug( VortexStrength );
	DeclareFloatXfPlug( OutwardStrength );
	DeclarePoolInpPlug( Input )

	//////////////////////////////////////////////////
	// outputs
	//////////////////////////////////////////////////

	DeclarePoolOutPlug( Output )

	virtual int GetNumOutputs() const { return 1; }
	virtual dataflow::outplugbase* GetOutput(int idx) { return & mPlugOutOutput; }

	//////////////////////////////////////////////////

	void Compute( float dt ); // virtual 

public:

	VortexModule();	
};

///////////////////////////////////////////////////////////////////////////////

class RendererModule : public ParticleModule
{
	RttiDeclareAbstract( RendererModule, ParticleModule );

protected:

	//////////////////////////////////////////////////
	// inputs
	//////////////////////////////////////////////////

	virtual int GetNumInputs() const { return 1; }
	virtual dataflow::inplugbase* GetInput(int idx) { return & mPlugInpInput; }

	DeclarePoolInpPlug( Input )

	//////////////////////////////////////////////////

public:

	RendererModule();

	virtual void Render(const CMatrix4& mtx, ork::lev2::RenderContextInstData& rcid, const ParticlePoolRenderBuffer& buffer, ork::lev2::GfxTarget* targ) = 0;

	const Pool<BasicParticle>* GetPool(); //pool = *mPlugInpInput.GetValue().mPool;
};

///////////////////////////////////////////////////////////////////////////////

class MaterialBase : public ork::Object
{
	RttiDeclareAbstract( MaterialBase, ork::Object );

public:

	virtual lev2::GfxMaterial* Bind( lev2::GfxTarget* pT ) = 0;
	virtual void Update( float ftexframe ) = 0;
	
protected:

	GfxMaterial3DSolid* mpMaterial;
	MaterialBase() : mpMaterial(0) {}
};

class TextureMaterial : public MaterialBase
{
	RttiDeclareConcrete( TextureMaterial, MaterialBase );

public:

	TextureMaterial();

	void SetTextureAccessor( ork::rtti::ICastable* const & tex);
	void GetTextureAccessor( ork::rtti::ICastable* & tex) const;
	ork::lev2::Texture* GetTexture() const;
	virtual void Update( float ftexframe );
	
private:

	ork::lev2::TextureAsset*		mTexture;
	lev2::GfxMaterial* Bind( lev2::GfxTarget* pT ); // virtual	
	
};

class VolTexMaterial : public MaterialBase
{
	RttiDeclareConcrete( VolTexMaterial, MaterialBase );

public:

	VolTexMaterial();

	void SetTextureAccessor( ork::rtti::ICastable* const & tex);
	void GetTextureAccessor( ork::rtti::ICastable* & tex) const;
	ork::lev2::Texture* GetTexture() const;
	virtual void Update( float ftexframe );
	
private:

	ork::lev2::TextureAsset*		mTexture;
	lev2::GfxMaterial* Bind( lev2::GfxTarget* pT ); // virtual	
	
};

///////////////////////////////////////////////////////////////////////////////

class SpriteRenderer : public RendererModule
{
	RttiDeclareConcrete( SpriteRenderer, RendererModule );

	//////////////////////////////////////////////////
	// inputs
	//////////////////////////////////////////////////

	virtual int GetNumInputs() const { return 14; }
	virtual dataflow::inplugbase* GetInput(int idx);

	DeclareFloatXfPlug( Size );
	DeclareFloatXfPlug( Rot );
	DeclareFloatXfPlug( AnimFrame );
	DeclareFloatXfPlug( GradientIntensity );

	DeclareFloatXfPlug( NoiseAmp0 );
	DeclareFloatXfPlug( NoiseAmp1 );
	DeclareFloatXfPlug( NoiseAmp2 );
	DeclareFloatXfPlug( NoiseFreq0 );
	DeclareFloatXfPlug( NoiseFreq1 );
	DeclareFloatXfPlug( NoiseFreq2 );
	DeclareFloatXfPlug( NoiseShift0 );
	DeclareFloatXfPlug( NoiseShift1 );
	DeclareFloatXfPlug( NoiseShift2 );

	//////////////////////////////////////////////////
	// outputs
	//////////////////////////////////////////////////

	virtual int GetNumOutputs() const { return 2; }
	DeclareFloatOutPlug( UnitAge );
	DeclareFloatOutPlug( PtcRandom );
	virtual dataflow::outplugbase* GetOutput(int idx);

	//////////////////////////////////////////////////

	void DrawParticle( const ork::lev2::particle::BasicParticle* ptcl );
	ork::CVector2 uvr0;
	ork::CVector2 uvr1;
	ork::CVector2 uvr2;
	ork::CVector2 uvr3;
	ork::CVector3 NX_NY;
	ork::CVector3 PX_NY;
	ork::CVector3 PX_PY;
	ork::CVector3 NX_PY;
	float mCurFGI;
	int	  miTexCnt;
	float mfTexs;
	int miAnimTexDim;
	ork::lev2::CVtxBuffer<ork::lev2::SVtxV12C4T16>* mpVB;

	//////////////////////////////////////////////////

	orklut<PoolString,ork::Object*>	mGradients;
	orklut<PoolString,ork::Object*> mMaterials;
	PoolString						mActiveGradient;
	PoolString						mActiveMaterial;

	ork::lev2::EBlending			meBlendMode;
	ParticleItemAlignment			meAlignment;
	bool							mbSort;
	int								miImageFrame;

	void Compute( float dt );
	void Render(const CMatrix4& mtx, ork::lev2::RenderContextInstData& rcid, const ParticlePoolRenderBuffer& buffer, ork::lev2::GfxTarget* targ); // virtual
	virtual void DoLink();
	bool DoNotify(const ork::event::Event *event);


public:

	SpriteRenderer();
};

///////////////////////////////////////////////////////////////////////////////

class StreakRenderer : public RendererModule
{
	RttiDeclareConcrete( StreakRenderer, RendererModule );

	//////////////////////////////////////////////////
	// inputs
	//////////////////////////////////////////////////

	virtual int GetNumInputs() const { return 4; }
	virtual dataflow::inplugbase* GetInput(int idx);

	DeclareFloatXfPlug( Length );
	DeclareFloatXfPlug( Width );
	DeclareFloatOutPlug( UnitAge );
	DeclareFloatXfPlug( GradientIntensity );

	//////////////////////////////////////////////////
	// outputs
	//////////////////////////////////////////////////

	virtual int GetNumOutputs() const { return 1; }
	virtual dataflow::outplugbase* GetOutput(int idx);

	//////////////////////////////////////////////////

	ork::Gradient<ork::CVector4>	mGradient;
	ork::lev2::EBlending			meBlendMode;
	GfxMaterial3DSolid*				mpMaterial;
	ork::lev2::TextureAsset*		mTexture;

	ork::Object* GradientAccessor() { return & mGradient; }
	void SetTextureAccessor( ork::rtti::ICastable* const & tex);
	void GetTextureAccessor( ork::rtti::ICastable* & tex) const;

	void Compute( float dt ){} // virtual 
	void Render(const CMatrix4& mtx, ork::lev2::RenderContextInstData& rcid, const ParticlePoolRenderBuffer& buffer, ork::lev2::GfxTarget* targ); // virtual

	ork::lev2::Texture* GetTexture() const;


public:

	StreakRenderer();
};

///////////////////////////////////////////////////////////////////////////////

class ModelRenderer : public RendererModule
{
	RttiDeclareConcrete( ModelRenderer, RendererModule );


	//////////////////////////////////////////////////
	// inputs
	//////////////////////////////////////////////////

	virtual int GetNumInputs() const { return 3; }
	virtual dataflow::inplugbase* GetInput(int idx);

	DeclareFloatXfPlug( AnimScale );
	DeclareFloatXfPlug( AnimRot );

	//////////////////////////////////////////////////
	// outputs
	//////////////////////////////////////////////////

	virtual int GetNumOutputs() const { return 1; }
	virtual dataflow::outplugbase* GetOutput(int idx);

	DeclareFloatOutPlug( UnitAge );

	//////////////////////////////////////////////////

	ork::lev2::XgmModelAsset*		mModel;
	CVector3 						mUpVector;
	CVector4 						mBaseRotAxisAngle;
	CVector3 						mAnimRotAxis;

	void SetModelAccessor( ork::rtti::ICastable* const & tex);
	void GetModelAccessor( ork::rtti::ICastable* & tex) const;

	void Compute( float dt ){} // virtual 
	void Render(const CMatrix4& mtx, ork::lev2::RenderContextInstData& rcid, const ParticlePoolRenderBuffer& buffer, ork::lev2::GfxTarget* targ); // virtual

	ork::lev2::XgmModel* GetModel() const;

public:

	ModelRenderer();
};

///////////////////////////////////////////////////////////////////////////////

class psys_graph_pool;

///////////////////////////////////////////////////////////////////////////////

class psys_graph : public ork::dataflow::graph
{
	friend class psys_graph_pool;

	RttiDeclareAbstract(psys_graph,ork::dataflow::graph);

	bool CanConnect( const ork::dataflow::inplugbase* pin, const ork::dataflow::outplugbase* pout ) const;
	ork::dataflow::dgregisterblock					mdflowregisters;
	ork::dataflow::dgcontext						mdflowctx;
	bool											mbEmitEnable;
	float											mfElapsed;

	bool Query(event::Event *event) const; // virtual

public:

	psys_graph();
	~psys_graph();
	void operator=(const psys_graph&oth);
	void compute();
	void Update( ork::lev2::particle::Context* pctx, float fdt );
	void Reset(ork::lev2::particle::Context* pctx);
	bool GetEmitEnable() const { return mbEmitEnable; }
	void SetEmitEnable( bool bv ) { mbEmitEnable=bv; } 

	void PrepForStart();
};

///////////////////////////////////////////////////////////////////////////////

class psys_graph_pool : public ork::Object
{
	RttiDeclareConcrete(psys_graph_pool,ork::Object);

	const psys_graph*		mNewTemplate; // deprecated
	ork::pool<psys_graph>*	mGraphPool;
	int						miPoolSize;

public:

	psys_graph_pool();

	psys_graph* Allocate();
	void Free(psys_graph*);

	void BindTemplate( const psys_graph& InTemplate );
};

///////////////////////////////////////////////////////////////////////////////

	//int								miAnimTexDim;
	//int								miImgSequenceBegin;
	//int								miImgSequenceEnd;
	//bool							mbImageSequence;
	//bool							mbImageSequenceOK;
	//std::vector<ork::lev2::TextureAsset*>	mSequenceTextures;
	
	//ork::Object* GradientAccessor() { return & mGradient; }
	//void SetTextureAccessor( ork::rtti::ICastable* const & tex);
	//void GetTextureAccessor( ork::rtti::ICastable* & tex) const;
	//void SetVolumeTextureAccessor( ork::rtti::ICastable* const & tex);
	//void GetVolumeTextureAccessor( ork::rtti::ICastable* & tex) const;

}}}
