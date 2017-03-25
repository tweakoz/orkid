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
class CCameraData;
class CPlacedGlyph;
class CTextDrawable;

namespace lev2 {

class XgmCluster;
class XgmSubMesh;
class XgmModel;
class XgmMesh;
class XgmModelInst;
class CAnim;
class Renderer;
class GfxMaterial;
class CManip;
class RenderContextInstData;
class GfxTarget;
class GfxMaterial;
class CManipManager;
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
		
	inline const CColor4& GetModColor() const { return mModColor; }
	inline void SetModColor(const CColor4& Color) { mModColor = Color; }

	void SetMatrix( const CMatrix4& mtx ) { mMatrix=mtx; }
	const CMatrix4& GetMatrix() const { return mMatrix; }

	void SetDrawableDataA( const anyp& ap ) { mDrwDataA=ap; }
	const anyp& GetDrawableDataA() const { return mDrwDataA; }
	void SetDrawableDataB( const anyp& ap ) { mDrwDataB=ap; }
	const anyp& GetDrawableDataB() const { return mDrwDataB; }

protected:

	CMatrix4						mMatrix;
	const ork::Object*				mpObject;
	CColor4							mModColor;
	anyp							mDrwDataA;
	anyp							mDrwDataB;

};

///////////////////////////////////////////////////////////////////////////////

class CBoxRenderable : public IRenderableDag
{
public: //

	CBoxRenderable()
		: IRenderableDag()
		, miPass( -1 )
		, mDefSortKey( 0 )
	{
	}

	int					GetPass( void ) const { return miPass; }
	
	void				SetPass( int ipass ) { miPass=ipass; }
	void				SetBox( const CVector4 & box ) { mBox=box; }
	void				SetColor( const CColor4 & clr ) { mColor=clr; }
	void				SetDefaultSortKey( U32 DefSortKey ) { mDefSortKey=DefSortKey; }

	const CVector4 &	GetBox( void ) const { return mBox; }
	const CColor4 &		GetColor( void ) const { return mColor; }


	void Render( const Renderer *renderer ) const final;
	U32	ComposeSortKey( const Renderer *renderer ) const final;

private:

	int						miPass;
	
	CVector4				mBox;
	CColor4					mColor;
	U32						mDefSortKey;

};

///////////////////////////////////////////////////////////////////////////////

class CBillboardRenderable : public IRenderableDag
{
public: //

	CBillboardRenderable()
		: IRenderableDag()
		, mSize( 1.0f )
		, mpMaterial( 0 )
	{
	}
	
	void				SetSize( CReal sz ) { mSize=sz; }
	CReal				GetSize( void ) const { return mSize; }
	void				SetMaterial( const GfxMaterial *pmat ) { mpMaterial=pmat; } 
	const GfxMaterial*	GetMaterial( void ) const { return mpMaterial; } 


	void Render( const Renderer *renderer ) const final;
	U32 ComposeSortKey( const Renderer *renderer ) const final;


private:

	const GfxMaterial*		mpMaterial;
	CReal					mSize;
};

///////////////////////////////////////////////////////////////////////////////

class CModelRenderable : public IRenderableDag
{
	RttiDeclareConcrete(CModelRenderable,IRenderableDag);
public:

	static const int kMaxEngineParamFloats = ork::lev2::RenderContextInstData::kMaxEngineParamFloats;

	CModelRenderable(Renderer *renderer = NULL);
	
	inline void SetMaterialIndex( int idx ) { mMaterialIndex=idx; }
	inline void SetMaterialPassIndex( int idx ) { mMaterialPassIndex=idx; }
	inline void SetModelInst(const lev2::XgmModelInst* modelInst) { mModelInst = modelInst; }
	inline void SetEdgeColor(int edge_color) { mEdgeColor = edge_color; }
	void SetScale(CReal scale) { mScale = scale; }
	inline void SetSubMesh( const lev2::XgmSubMesh*cs ) { mSubMesh=cs; }
	inline void SetCluster( const lev2::XgmCluster* c ) { mCluster=c; }
	inline void SetMesh( const lev2::XgmMesh* m ) { mMesh=m; }

	CReal GetScale() const { return mScale; }
	inline const lev2::XgmModelInst* GetModelInst() const { return mModelInst; }
	inline const CMatrix4& GetWorldMatrix() const;
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

	void SetRotate( const CVector3& v ) { mRotate=v; }
	void SetOffset( const CVector3& v ) { mOffset=v; }

	const CVector3& GetRotate() const { return mRotate; }
	const CVector3& GetOffset() const { return mOffset; }

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
	CReal						mScale;
	CVector3					mOffset;
	CVector3					mRotate;
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

	void SetUserData0( anyp pdata ) { mUserData0=pdata; }
	const anyp& GetUserData0() const { return mUserData0; }
	void SetUserData1( anyp pdata ) { mUserData1=pdata; }
	const anyp& GetUserData1() const { return mUserData1; }

	void SetRenderCallback( cbtype_t cb ) { mRenderCallback=cb; }
	cbtype_t GetRenderCallback() const { return mRenderCallback; }

private:

    void Render( const Renderer *renderer ) const final;
    U32 ComposeSortKey( const Renderer *renderer ) const final { return mSortKey; }   

	U32								mSortKey;
	int								mMaterialIndex;
	int								mMaterialPassIndex;
	anyp							mUserData0;	
	anyp							mUserData1;	
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
	const CColor4&		GetColor() const { return mColor; }

	void				SetFrustum( const Frustum& frus ) { mFrustum = frus; }
	void				SetColor( const CColor4& clr ) { mColor = clr; }


	bool				IsObjSpace() const { return mObjSpace; }
	void				SetObjSpace( bool bv ) { mObjSpace=bv; }

private:

    U32     ComposeSortKey( const Renderer *renderer ) const final { return kLastRenderableSortKey; }
    void    Render( const Renderer *renderer ) const final;

	Frustum			mFrustum;
	CColor4			mColor;
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

	void				SetColor( const CColor4& clr ) { mColor = clr; }
	void				SetPosition( const CVector3& pos ) { mPosition=pos; }
	void				SetRadius( float rad ) { mRadius=rad; }

	const CColor4&		GetColor() const { return mColor; }
	const CVector3 &	GetPosition() const { return mPosition; }
	float				GetRadius() const { return mRadius; }

private:

    void        Render( const Renderer *renderer ) const final;
    U32         ComposeSortKey( const Renderer *renderer ) const final { return 0; }

	CColor4					mColor;
	CVector3				mPosition;
	float					mRadius;
};

///////////////////////////////////////////////////////////////////////////////

}}

