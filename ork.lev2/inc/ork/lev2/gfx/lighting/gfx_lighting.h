////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once
///////////////////////////////////////////////////////////////////////////////

#include <ork/math/plane.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/box.h>
#include <ork/math/sphere.h>
#include <ork/math/frustum.h>
#include <ork/kernel/orklut.h>
#include <ork/kernel/fixedlut.h>
#include <ork/kernel/orkpool.h>
#include <ork/kernel/Array.h>

#include <ork/config/config.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {

class Renderer;
class TextureAsset;
class Texture;
class RenderContextFrameData;

///////////////////////////////////////////////////////////////////////////////

inline int countbits( U32 v )
{
	v = v - ((v >> 1) & 0x55555555);								// reuse input as temporary
	v = (v & 0x33333333) + ((v >> 2) & 0x33333333);					// temp
	int c = (((v + (v >> 4) & 0xF0F0F0F) * 0x1010101) >> 24);	// count
	return c;
}

///////////////////////////////////////////////////////////////////////////////

enum ELightType
{
	ELIGHTTYPE_DIRECTIONAL = 0,
	ELIGHTTYPE_SPOT,
	ELIGHTTYPE_POINT,
	ELIGHTTYPE_AMBIENT,
};

///////////////////////////////////////////////////////////////////////////////

class  LightData : public ork::Object
{
	RttiDeclareAbstract(LightData, ork::Object);

	fvec3				mColor;
	bool					mbShadowCaster;
	float					mShadowSamples;
	float					mShadowBlur;
	float					mShadowBias;
	bool					mbSpecular;

public:

	float GetShadowBias() const { return mShadowBias; }
	float GetShadowSamples() const { return mShadowSamples; }
	float GetShadowBlur() const { return mShadowBlur; }
	bool GetSpecular() const { return mbSpecular; }
	bool IsShadowCaster() const { return mbShadowCaster; }

	const fvec3& GetColor() const { return mColor; }
	void SetColor(const fvec3&clr) { mColor=clr; }

	LightData()
		: mColor( 1.0f, 0.0f, 0.0f )
		, mbSpecular(false)
		, mbShadowCaster(false)
		, mShadowSamples(1.0f)
		, mShadowBlur(0.0f)
		, mShadowBias(0.2f)
	{
	}
};

///////////////////////////////////////////////////////////////////////////////

class  Light : public ork::Object
{
	RttiDeclareAbstract(Light, ork::Object);

	const LightData* mLd;
	const fmtx4& mWorldMatrix;

public:

	float		mPriority;
	int			miInFrustumID;
	bool		mbIsDynamic;

	virtual bool IsInFrustum( const Frustum& frustum ) = 0;
	virtual bool AffectsSphere( const fvec3& center, float radius ) = 0;
	virtual bool AffectsAABox( const AABox& aab ) = 0;
	virtual bool AffectsCircleXZ( const Circle& cir ) = 0;
	virtual ELightType LightType() const = 0;

	const fvec3& GetColor() const { return mLd->GetColor(); }
	const fmtx4& GetMatrix() const { return mWorldMatrix; }
	fvec3 GetWorldPosition() const { return mWorldMatrix.GetTranslation(); }
	fvec3 GetDirection() const { return mWorldMatrix.GetZNormal(); }

	Light( const fmtx4& mtx, const LightData* ld=0 ) : mWorldMatrix(mtx), mLd(ld), mbIsDynamic(false), mPriority(0.0f) {}

};

///////////////////////////////////////////////////////////////////////////////

class  PointLightData : public LightData
{
	RttiDeclareConcrete(PointLightData, LightData);

	float		mRadius;
	float		mFalloff;

public:

	float GetRadius() const { return mRadius; }
	float GetFalloff() const { return mFalloff; }

	PointLightData() : mRadius(1.0f), mFalloff(1.0f) {}
};

///////////////////////////////////////////////////////////////////////////////

class  PointLight : public Light
{
	RttiDeclareAbstract(PointLight, ork::Object);

	const PointLightData* mPld;

public:

	/*virtual*/ bool IsInFrustum( const Frustum& frustum ) override;
	/*virtual*/ bool AffectsSphere( const fvec3& center, float radius ) override;
	/*virtual*/ bool AffectsAABox( const AABox& aab ) override;
	/*virtual*/ bool AffectsCircleXZ( const Circle& cir ) override;
	/*virtual*/ ELightType LightType() const override { return ELIGHTTYPE_POINT; }

	float GetRadius() const { return mPld->GetRadius(); }
	float GetFalloff() const { return mPld->GetFalloff(); }

	PointLight( const fmtx4& mtx, const PointLightData* pld=0 );
};

///////////////////////////////////////////////////////////////////////////////

class  DirectionalLightData : public LightData
{
	RttiDeclareConcrete(DirectionalLightData, LightData);

public:

	DirectionalLightData() {}
};

///////////////////////////////////////////////////////////////////////////////

class  DirectionalLight : public Light
{
	RttiDeclareAbstract(DirectionalLight, ork::Object);

	const DirectionalLightData* mDld;

public:

	/*virtual*/ bool IsInFrustum( const Frustum& frustum )override;
	/*virtual*/ bool AffectsSphere( const fvec3& center, float radius ) override { return true; }
	/*virtual*/ bool AffectsCircleXZ( const Circle& cir ) override { return true; }
	/*virtual*/ bool AffectsAABox( const AABox& aab ) override { return true; }
	/*virtual*/ ELightType LightType() const override { return ELIGHTTYPE_DIRECTIONAL; }

	DirectionalLight( const fmtx4& mtx, const DirectionalLightData* dld=0 );
};

///////////////////////////////////////////////////////////////////////////////

class  AmbientLightData : public LightData
{
	RttiDeclareConcrete(AmbientLightData, LightData);

	float	mfAmbientShade;
	fvec3 mvHeadlightDir;

public:

	AmbientLightData() : mfAmbientShade(0.0f), mvHeadlightDir(0.0f, 0.5f, 1.0f) {}
	float GetAmbientShade() const { return mfAmbientShade; }
	void SetAmbientShade( float fv ) { mfAmbientShade=fv; }
	const fvec3& GetHeadlightDir() const { return mvHeadlightDir; }
	void SetHeadlightDir( const fvec3 &dir) { mvHeadlightDir=dir; }
};

///////////////////////////////////////////////////////////////////////////////

class  AmbientLight : public Light
{
	RttiDeclareAbstract(AmbientLight, ork::Object);

	const AmbientLightData* mAld;

public:

	/*virtual*/ bool IsInFrustum( const Frustum& frustum ) override;
	/*virtual*/ bool AffectsSphere( const fvec3& center, float radius ) override { return true; }
	/*virtual*/ bool AffectsCircleXZ( const Circle& cir ) override { return true; }
	/*virtual*/ bool AffectsAABox( const AABox& aab ) override { return true; }
	/*virtual*/ ELightType LightType() const override { return ELIGHTTYPE_AMBIENT; }
	float GetAmbientShade() const { return mAld->GetAmbientShade(); }
	const fvec3& GetHeadlightDir() const { return mAld->GetHeadlightDir(); }

	AmbientLight( const fmtx4& mtx, const AmbientLightData* dld=0 );
};

///////////////////////////////////////////////////////////////////////////////

class  SpotLightData : public LightData
{
	RttiDeclareConcrete(SpotLightData, LightData);

	float				mFovy;
	float				mRange;
	lev2::TextureAsset*	mTexture;

	void SetTextureAccessor( ork::rtti::ICastable* const & tex);
	void GetTextureAccessor( ork::rtti::ICastable* & tex) const;

public:

	float GetFovy() const { return mFovy; }
	float GetRange() const { return mRange; }

	SpotLightData() : mFovy(10.0f), mRange(1.0f), mTexture(0) {}
};

///////////////////////////////////////////////////////////////////////////////

class  SpotLight : public Light
{
	RttiDeclareAbstract(PointLight, ork::Object);

	const SpotLightData* mSld;

public:

	fmtx4		mProjectionMatrix;
	fmtx4		mViewMatrix;
	Frustum			mWorldSpaceLightFrustum;
	//float			mFovy;
	//float			mRange;
	lev2::TextureAsset*	mTexture;


	/*virtual*/ bool IsInFrustum( const Frustum& frustum ) override;
	/*virtual*/ bool AffectsSphere( const fvec3& center, float radius ) override;
	/*virtual*/ bool AffectsAABox( const AABox& aab ) override;
	/*virtual*/ bool AffectsCircleXZ( const Circle& cir ) override;
	/*virtual*/ ELightType LightType() const override { return ELIGHTTYPE_SPOT; }

	void Set( const fvec3& pos, const fvec3& target, const fvec3& up, float fovy );

	void SetTexture( lev2::TextureAsset* ptex ) { mTexture=ptex; }
	lev2::TextureAsset* GetTexture() const { return mTexture; }

	float GetFovy() const { return mSld->GetFovy(); }
	float GetRange() const { return mSld->GetRange(); }

	SpotLight( const fmtx4& mtx, const SpotLightData* sld = 0 );

};

///////////////////////////////////////////////////////////////////////////////

struct  LightContainer
{
	static const int kmaxlights = 8;

	typedef fixedlut<float,Light*,kmaxlights> map_type;

	map_type	mPrioritizedLights;

	void AddLight( Light* plight );
	void RemoveLight( Light* plight );

	LightContainer();
	void Clear();
};

struct  GlobalLightContainer
{
	static const int kmaxlights = 256;

	typedef fixedlut<float,Light*,kmaxlights> map_type;

	map_type	mPrioritizedLights;

	void AddLight( Light* plight );
	void RemoveLight( Light* plight );

	GlobalLightContainer();
	void Clear();
};

///////////////////////////////////////////////////////////////////////////////

struct  LightMask
{
	U32	mMask;

	LightMask() : mMask(0) {}

	void SetMask( U32 mask ) { mMask=mask; }
	void AddLight( const Light* plight );
	int GetNumLights() const { return countbits(mMask); }
};

///////////////////////////////////////////////////////////////////////////////

class LightManager;

struct  LightingGroup
{
	static const int kmaxinst = 32;

	LightMask							mLightMask;
	ork::fixedvector<fmtx4,kmaxinst>	mInstances;
	LightManager*						mLightManager;
	Texture*							mLightMap;
	Texture*							mDPEnvMap;

	size_t GetNumLights() const;
	size_t GetNumMatrices() const;
	const fmtx4* GetMatrices() const;
	int GetLightId( int idx ) const;

	LightingGroup();
};

///////////////////////////////////////////////////////////////////////////////

class  LightManagerData : public ork::Object
{
	RttiDeclareConcrete(LightManagerData, ork::Object);

public:

};

///////////////////////////////////////////////////////////////////////////////

class  LightCollector
{
public:

	static const int	kmaxonscreengroups = 32;
	static const int	kmaxflagwords = kmaxonscreengroups>>5;

private:

	//typedef fixedmap<U32,LightingGroup*,kmaxonscreengroups>	ActiveMapType;
	//typedef orklut< U32,LightingGroup*, allocator_fixedpool< std::pair<U32,LightingGroup*>,kmaxonscreengroups > >	ActiveMapType;
	typedef ork::fixedlut< U32,LightingGroup*,kmaxonscreengroups >	ActiveMapType;

	fixed_pool<LightingGroup,kmaxonscreengroups>		mGroups;
	ActiveMapType										mActiveMap;

	LightManager*		mManager;

public:

	//const LightingGroup& GetActiveGroup( int idx ) const;
	size_t GetNumGroups() const;
	void SetManager( LightManager*mgr );
	void Clear();
	LightCollector();
	~LightCollector();
	void QueueInstance( const LightMask& lmask, const fmtx4& mtx );
};

///////////////////////////////////////////////////////////////////////////////

class  LightManager
{
	const LightManagerData& mLmd;

	LightCollector mcollector;

public:

	LightManager( const LightManagerData& lmd ) : mLmd(lmd) {}

	GlobalLightContainer	mGlobalStationaryLights;	// non-moving, potentially animating color or texture (and => not lightmappable)
	LightContainer			mGlobalMovingLights;		// moving lights

	//virtual void GetStationaryLights( const Frustum& frustum, LightContainer& container ) = 0;
	//virtual void GetMovingLights( const Frustum& frustum, LightContainer& container ) = 0;

	void EnumerateInFrustum( const Frustum& frustum );

	static const int kmaxinfrustum = 32;
	fixedvector<Light*,kmaxinfrustum> mLightsInFrustum;

	//int	miNumLightsInFrustum;

	void QueueInstance( const LightMask& lgid, const fmtx4& mtx );

	size_t GetNumLightGroups() const;
	void Clear();

	//const LightingGroup& GetGroup( int igroupindex ) const;
};

struct HeadLightManager
{
	ork::fmtx4		mHeadLightMatrix;
	LightingGroup		mHeadLightGroup;
	AmbientLightData	mHeadLightData;
	AmbientLight		mHeadLight;
	LightManagerData	mHeadLightManagerData;
	LightManager		mHeadLightManager;

	HeadLightManager( RenderContextFrameData & FrameData );
};

class FxShader;
class FxShaderParam;
class GfxTarget;

struct LightingFxInterface
{
	FxShader*					mpShader;
	const FxShaderParam*		hAmbientLight;
	const FxShaderParam*		hNumDirectionalLights;
	const FxShaderParam*		hDirectionalLightColors;
	const FxShaderParam*		hDirectionalLightDirs;
	const FxShaderParam*		hDirectionalLightPos;

	const FxShaderParam*		hDirectionalAttenA;
	const FxShaderParam*		hDirectionalAttenK;
	const FxShaderParam*		hLightMode;

	const LightingGroup*		mCurrentLightingGroup;

	void ApplyLighting( GfxTarget *pTarg, int iPass );

	bool						mbHasLightingInterface;

	LightingFxInterface( );

	void Init( FxShader* pshader );
};
/*
///////////////////////
// usage scenario
///////////////////////

Drawables will be preattached to any statically bound lights,
however some lights are dynamically created, destroyed and moved



///////////////////////
///////////////////////
*/


}}
