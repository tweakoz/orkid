////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Grpahics Environment (Driver/HAL)
///////////////////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/math/TransformNode.h>
#include <ork/object/Object.h>
#include <ork/rtti/Class.h>
#include <ork/math/cvector4.h>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/lev2renderer.h>
#include <functional>

namespace ork {

struct Frustum;
class CameraData;
class CPlacedGlyph;
class CTextDrawable;

namespace lev2 {

class XgmCluster;
class XgmSubMesh;
class XgmModel;
class XgmMesh;
class XgmModelInst;
class Anim;
class Renderer;
class GfxMaterial;
class Manip;
class RenderContextInstData;
class GfxTarget;
class GfxMaterial;
class ManipManager;
class XgmWorldPose;

///////////////////////////////////////////////////////////////////////////////
/**
 * A Renderable is an object that renders itself in an atomic, Render call. When drawing
 * a scene, the majority of objects drawn are in the form of a Renderable. Renderables
 * are Retained-Mode-style objects that compose the Scene Graph.
 *
 * Use Render() to draw the object immediately. Use Queue() to place the object in a
 * RenderQueue for deferred, sorted rendering based on a sort key.
 *
 * Example Renderables are skyboxes, ground/water planes, and meshes.
 */
///////////////////////////////////////////////////////////////////////////////

class IRenderable : public ork::Object
{
	RttiDeclareAbstract(IRenderable,ork::Object);
public:

	typedef svar16_t var_t;

	IRenderable();

	virtual void Render( const Renderer *renderer ) const = 0;
	virtual bool CanGroup( const IRenderable* oth ) const { return false; }

	/// Renderables implement this function to set the sort key used when all Renderables are sorted together.
	/// The default is 0 for all Renderables. If no Renderable overrides this, then the RenderableQueue is not
	/// sorted and all Renderables are drawn in the order they are queued.
	/// Typically, a Renderable will use the IRenderer::ComposeSortKey() function as a helper when composing
	/// its sort key.
	virtual U32 ComposeSortKey( const Renderer *renderer ) const { return 0; }

	static const int kManipRenderableSortKey = 0x7fffffff;
	static const int kLastRenderableSortKey = 0x7ffffffe;
	static const int kFirstRenderableSortKey = 0;

};

///////////////////////////////////////////////////////////////////////////////

class IRenderableDag : public IRenderable
{
	RttiDeclareAbstract(IRenderableDag,IRenderable);
public:

	static void ClassInit();

	IRenderableDag();

	void SetObject( const ork::Object* o ) { mpObject = o; }
	const ork::Object* GetObject() const { return mpObject; }

	inline const fcolor4& GetModColor() const { return mModColor; }
	inline void SetModColor(const fcolor4& Color) { mModColor = Color; }

	void SetMatrix( const fmtx4& mtx ) { mMatrix=mtx; }
	const fmtx4& GetMatrix() const { return mMatrix; }

	void SetDrawableDataA( const var_t& ap ) { mDrwDataA=ap; }
	const var_t& GetDrawableDataA() const { return mDrwDataA; }
	void SetDrawableDataB( const var_t& ap ) { mDrwDataB=ap; }
	const var_t& GetDrawableDataB() const { return mDrwDataB; }

protected:

	fmtx4						mMatrix;
	const ork::Object*				mpObject;
	fcolor4							mModColor;
	var_t							mDrwDataA;
	var_t							mDrwDataB;

};

///////////////////////////////////////////////////////////////////////////////

class BoxRenderable : public IRenderableDag
{
public: //

	BoxRenderable()
		: IRenderableDag()
		, miPass( -1 )
		, mDefSortKey( 0 )
	{
	}

	int					GetPass( void ) const { return miPass; }

	void				SetPass( int ipass ) { miPass=ipass; }
	void				SetBox( const fvec4 & box ) { mBox=box; }
	void				SetColor( const fcolor4 & clr ) { mColor=clr; }
	void				SetDefaultSortKey( U32 DefSortKey ) { mDefSortKey=DefSortKey; }

	const fvec4 &	GetBox( void ) const { return mBox; }
	const fcolor4 &		GetColor( void ) const { return mColor; }


	void Render( const Renderer *renderer ) const final;
	U32	ComposeSortKey( const Renderer *renderer ) const final;

private:

	int						miPass;

	fvec4				mBox;
	fcolor4					mColor;
	U32						mDefSortKey;

};

///////////////////////////////////////////////////////////////////////////////

class BillboardRenderable : public IRenderableDag
{
public: //

	BillboardRenderable()
		: IRenderableDag()
		, mSize( 1.0f )
		, mpMaterial( 0 )
	{
	}

	void				SetSize( float sz ) { mSize=sz; }
	float				GetSize( void ) const { return mSize; }
	void				SetMaterial( const GfxMaterial *pmat ) { mpMaterial=pmat; }
	const GfxMaterial*	GetMaterial( void ) const { return mpMaterial; }


	void Render( const Renderer *renderer ) const final;
	U32 ComposeSortKey( const Renderer *renderer ) const final;


private:

	const GfxMaterial*		mpMaterial;
	float					mSize;
};

///////////////////////////////////////////////////////////////////////////////

class ModelRenderable : public IRenderableDag
{
	RttiDeclareConcrete(ModelRenderable,IRenderableDag);
public:

	static const int kMaxEngineParamFloats = ork::lev2::RenderContextInstData::kMaxEngineParamFloats;

	ModelRenderable(Renderer *renderer = NULL);

	inline void SetMaterialIndex( int idx ) { mMaterialIndex=idx; }
	inline void SetMaterialPassIndex( int idx ) { mMaterialPassIndex=idx; }
	inline void SetModelInst(const lev2::XgmModelInst* modelInst) { mModelInst = modelInst; }
	inline void SetEdgeColor(int edge_color) { mEdgeColor = edge_color; }
	void SetScale(float scale) { mScale = scale; }
	inline void SetSubMesh( const lev2::XgmSubMesh*cs ) { mSubMesh=cs; }
	inline void SetCluster( const lev2::XgmCluster* c ) { mCluster=c; }
	inline void SetMesh( const lev2::XgmMesh* m ) { mMesh=m; }

	float GetScale() const { return mScale; }
	inline const lev2::XgmModelInst* GetModelInst() const { return mModelInst; }
	inline const fmtx4& GetWorldMatrix() const;
	inline int GetMaterialIndex( void ) const { return mMaterialIndex; }
	inline int GetMaterialPassIndex( void ) const { return mMaterialPassIndex; }
	inline int GetEdgeColor() const { return mEdgeColor; }
	inline const lev2::XgmSubMesh* GetSubMesh( void ) const { return mSubMesh; }
	inline const lev2::XgmCluster* GetCluster( void ) const { return mCluster; }
	inline const lev2::XgmMesh* GetMesh( void ) const { return mMesh; }

	void SetSortKey( U32 skey ) { mSortKey=skey; }

	void AddLight( Light* plight ) { mLightMask.AddLight(plight); }
	void SetLightMask( const lev2::LightMask& lmask ) { mLightMask=lmask; }

	const lev2::LightMask& GetLightMask() const { return mLightMask; }

	void SetRotate( const fvec3& v ) { mRotate=v; }
	void SetOffset( const fvec3& v ) { mOffset=v; }

	const fvec3& GetRotate() const { return mRotate; }
	const fvec3& GetOffset() const { return mOffset; }

	void SetEngineParamFloat(int idx, float fv);
	float GetEngineParamFloat(int idx) const;

	void SetWorldPose( const ork::lev2::XgmWorldPose* wp ) { mWorldPose=wp; }
	const ork::lev2::XgmWorldPose* GetWorldPose() const { return mWorldPose; }

private:

    U32 ComposeSortKey( const Renderer *renderer ) const final { return mSortKey; }
    void Render( const Renderer *renderer ) const final;
    bool CanGroup( const IRenderable* oth ) const final;

	float mEngineParamFloats[kMaxEngineParamFloats];

	const lev2::XgmModelInst*	mModelInst;
	U32							mSortKey;
	int							mSubMeshIndex;
	int							mMaterialIndex;
	int							mMaterialPassIndex;
	int							mEdgeColor;
	float						mScale;
	fvec3					mOffset;
	fvec3					mRotate;
	const lev2::XgmWorldPose*	mWorldPose;
	const lev2::XgmSubMesh*		mSubMesh;
	const lev2::XgmCluster*		mCluster;
	const lev2::XgmMesh*		mMesh;
	lev2::LightMask				mLightMask;
};

///////////////////////////////////////////////////////////////////////////////

class CallbackRenderable : public IRenderableDag
{
public:

	typedef std::function< void( lev2::RenderContextInstData& rcid, lev2::GfxTarget* targ, const CallbackRenderable* pren ) > cbtype_t;

	CallbackRenderable(Renderer *renderer = NULL);

	void SetSortKey( U32 skey ) { mSortKey=skey; }

	void SetUserData0( var_t pdata ) { mUserData0=pdata; }
	const var_t& GetUserData0() const { return mUserData0; }
	void SetUserData1( var_t pdata ) { mUserData1=pdata; }
	const var_t& GetUserData1() const { return mUserData1; }

	void SetRenderCallback( cbtype_t cb ) { mRenderCallback=cb; }
	cbtype_t GetRenderCallback() const { return mRenderCallback; }

private:

    void Render( const Renderer *renderer ) const final;
    U32 ComposeSortKey( const Renderer *renderer ) const final { return mSortKey; }

	U32								mSortKey;
	int								mMaterialIndex;
	int								mMaterialPassIndex;
	var_t							mUserData0;
	var_t							mUserData1;
	cbtype_t						mRenderCallback;
};

///////////////////////////////////////////////////////////////////////////////

class FrustumRenderable : public IRenderableDag
{
public:

	static void ClassInit() {}

	FrustumRenderable()
		: IRenderableDag()
		, mObjSpace( false )
	{
	}

	const Frustum&		GetFrustum() const { return mFrustum; }
	const fcolor4&		GetColor() const { return mColor; }

	void				SetFrustum( const Frustum& frus ) { mFrustum = frus; }
	void				SetColor( const fcolor4& clr ) { mColor = clr; }


	bool				IsObjSpace() const { return mObjSpace; }
	void				SetObjSpace( bool bv ) { mObjSpace=bv; }

private:

    U32     ComposeSortKey( const Renderer *renderer ) const final { return kLastRenderableSortKey; }
    void    Render( const Renderer *renderer ) const final;

	Frustum			mFrustum;
	fcolor4			mColor;
	bool			mObjSpace;

};

///////////////////////////////////////////////////////////////////////////////

class SphereRenderable : public IRenderableDag
{
public: //

	SphereRenderable()
		: IRenderableDag()
		, mRadius( 0.0f )
	{
	}

	void				SetColor( const fcolor4& clr ) { mColor = clr; }
	void				SetPosition( const fvec3& pos ) { mPosition=pos; }
	void				SetRadius( float rad ) { mRadius=rad; }

	const fcolor4&		GetColor() const { return mColor; }
	const fvec3 &	GetPosition() const { return mPosition; }
	float				GetRadius() const { return mRadius; }

private:

    void        Render( const Renderer *renderer ) const final;
    U32         ComposeSortKey( const Renderer *renderer ) const final { return 0; }

	fcolor4					mColor;
	fvec3				mPosition;
	float					mRadius;
};

///////////////////////////////////////////////////////////////////////////////

}}
