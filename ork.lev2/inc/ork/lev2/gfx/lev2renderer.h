////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/renderer_base.h>
#include <ork/gfx/camera.h>
#include <ork/lev2/gfx/gfxenv.h>

namespace ork { class CCameraData; }
namespace ork { namespace lev2 {

class Renderer;
class CCamera;
class Texture;
struct LightingGroup;
class LightManager;
class GfxTarget;
class GfxBuffer;
class RtGroup;
class GfxWindow;
class XgmMaterialStateInst;
class IRenderableDag;

class FrameRenderer;

///////////////////////////////////////////////////////////////////////////////
// Rendering Context Data that can change per draw instance
//  ie, per renderable (or even finer grained than that)
///////////////////////////////////////////////////////////////////////////////

class RenderContextInstData
{
public:

	static const int					kMaxEngineParamFloats = 4;
	static const int					kmaxdirlights = 4;
	static const int					kmaxpntlights = 4;

	static const RenderContextInstData Default;

	RenderContextInstData();

	//////////////////////////////////////
	// renderer interface
	//////////////////////////////////////

	void						SetRenderer( const Renderer* rnd ) { mpActiveRenderer=rnd; }
	void						SetDagRenderable( const IRenderableDag* rnd ) { mpDagRenderable=rnd; }
	const Renderer*				GetRenderer( void ) const { return mpActiveRenderer; }
	const IRenderableDag*		GetDagRenderable( void ) const { return mpDagRenderable; }
	const XgmMaterialStateInst*	GetMaterialInst() const { return mMaterialInst; }

	void SetEngineParamFloat(int idx, float fv);
	float GetEngineParamFloat(int idx) const;

	//////////////////////////////////////
	// material interface
	//////////////////////////////////////

	int		GetMaterialIndex( void ) const { return miMaterialIndex; }
	int		GetMaterialPassIndex( void ) const { return miMaterialPassIndex; }
	void	SetMaterialIndex( int idx ) { miMaterialIndex=idx; }
	void	SetMaterialPassIndex( int idx ) { miMaterialPassIndex=idx; }
	void	SetMaterialInst( const XgmMaterialStateInst* mi ) { mMaterialInst=mi; }
	bool	IsSkinned() const { return mbIsSkinned; }
	void    SetSkinned( bool bv ) { mbIsSkinned=bv; }
	void    SetVertexLit( bool bv ) { mbVertexLit=bv; }
	void	ForceNoZWrite( bool bv ) { mbForzeNoZWrite=bv; }
	bool	IsForceNoZWrite() const { return mbForzeNoZWrite; }

	void SetRenderGroupState( RenderGroupState rgs ) { mRenderGroupState = rgs; }
	RenderGroupState GetRenderGroupState() const { return mRenderGroupState; }

	//////////////////////////////////////
	// environment interface
	//////////////////////////////////////

	void						SetTopEnvMap( Texture*ptex ) { mDPTopEnvMap=ptex; }
	void						SetBotEnvMap( Texture*ptex ) { mDPBotEnvMap=ptex; }
	Texture*					GetTopEnvMap() const { return mDPTopEnvMap; }
	Texture*					GetBotEnvMap() const { return mDPBotEnvMap; }

	//////////////////////////////////////
	// lighting interface
	//////////////////////////////////////

	void					SetLightingGroup( const LightingGroup* lgroup ) { mpLightingGroup=lgroup; }
	const LightingGroup*	GetLightingGroup() const { return mpLightingGroup; }
	void BindLightMap( Texture* ptex ) { mLightMap=ptex; }
	Texture*				GetLightMap() const { return mLightMap; }
	bool					IsLightMapped() const { return (mLightMap!=0); }
	bool					IsVertexLit() const { return mbVertexLit; }

private:

	int									miMaterialIndex;
	int									miMaterialPassIndex;
	const Renderer*						mpActiveRenderer;
	const IRenderableDag*				mpDagRenderable;

	const LightingGroup*				mpLightingGroup;
	const XgmMaterialStateInst*			mMaterialInst;
	Texture*							mDPTopEnvMap;
	Texture*							mDPBotEnvMap;
	Texture*							mLightMap;
	bool								mbIsSkinned;
	bool								mbForzeNoZWrite;
	bool								mbVertexLit;
	float								mEngineParamFloats[kMaxEngineParamFloats];
	RenderGroupState					mRenderGroupState;
};

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

class IRenderTarget
{
public:

	IRenderTarget();

	virtual int GetW() = 0;
	virtual int GetH() = 0;
	virtual void BeginFrame(FrameRenderer&frenderer) = 0;
	virtual void EndFrame(FrameRenderer&frenderer) = 0;
};

class RtGroupRenderTarget : public IRenderTarget
{
public:

	RtGroupRenderTarget( RtGroup* prtgroup );

	private:

	RtGroup* mpRtGroup;

	int GetW();
	int GetH();
	void BeginFrame(FrameRenderer&frenderer);
	void EndFrame(FrameRenderer&frenderer);
};

class UiViewportRenderTarget : public IRenderTarget
{
public:

	UiViewportRenderTarget( ui::Viewport* pVP );

	private:

	ui::Viewport* mpViewport;

	int GetW();
	int GetH();
	void BeginFrame(FrameRenderer&frenderer);
	void EndFrame(FrameRenderer&frenderer);
};

class UiSurfaceRenderTarget : public IRenderTarget
{
public:

	UiSurfaceRenderTarget( ui::Surface* pVP );

	private:

	ui::Surface* mSurface;

	int GetW();
	int GetH();
	void BeginFrame(FrameRenderer&frenderer);
	void EndFrame(FrameRenderer&frenderer);
};

///////////////////////////////////////////////////////////////////////////////
// Rendering Context Data that can change per frame
//  render modes, target, etc....
///////////////////////////////////////////////////////////////////////////////

class RenderContextFrameData
{
public:
	enum ERenderingMode
	{
		ERENDMODE_NONE = 0,
		ERENDMODE_PRERENDER,
		ERENDMODE_STANDARD,
		ERENDMODE_SHADOWED,
		ERENDMODE_SHADOWMAP,
		ERENDMODE_LIGHTPREPASS,
		ERENDMODE_HDRJOIN,
		ERENDMODE_HUD,
	};

	RenderContextFrameData();

	GfxTarget*			GetTarget( void ) const { return mpTarget; }
	const CCameraData*	GetCameraData() const { return mCameraData; }
	const CCameraData*	GetPickCameraData() const { return mPickCameraData; }
	LightManager*		GetLightManager() const { return mLightManager; }

	ERenderingMode		GetRenderingMode( void ) const { return meMode; }
	GfxBuffer*			GetShadowBuffer( void ) const { return mpShadowBuffer; }
	const SRect&		GetDstRect( ) const { return mDstRect; }
	const SRect&		GetMrtRect( ) const { return mMrtRect; }

	void SetRenderingMode( ERenderingMode emode ) { meMode=emode; }
	void SetShadowBuffer( GfxBuffer* ShadowBuffer ) { mpShadowBuffer=ShadowBuffer; }
	void SetCameraData( const CCameraData* data ) { mCameraData=data; }
	void SetPickCameraData( const CCameraData* data ) { mPickCameraData=data; }
	void SetLightManager( LightManager* lmgr ) { mLightManager=lmgr; }
	void SetTarget( GfxTarget* ptarg );
	void SetDstRect( const SRect& rect ) { mDstRect=rect; }
	void SetMrtRect( const SRect& rect ) { mMrtRect=rect; }

	bool IsPickMode() const; // { return mpTarget ? mpTarget->FBI()->IsPickMode() : false; }

	CameraCalcContext& GetCameraCalcCtx() { return mCameraCalcCtx; }
	const CameraCalcContext& GetCameraCalcCtx() const { return mCameraCalcCtx; }

	void ClearLayers();
	void AddLayer( const PoolString& layername );
	bool HasLayer( const PoolString& layername ) const;

	void PushRenderTarget( IRenderTarget* ptarg );
	IRenderTarget* GetRenderTarget();
	void PopRenderTarget();

	const orklut<PoolString,anyp>& UserProperties() const { return mUserProperties; }
	orklut<PoolString,anyp>& UserProperties() { return mUserProperties; }

	void SetUserProperty( const char* prop, anyp data );
	anyp GetUserProperty( const char* prop );

private:

	orkstack<IRenderTarget*>	mRenderTargetStack;
	orklut<PoolString,anyp>		mUserProperties;
	LightManager*		mLightManager;
	ERenderingMode		meMode;
	GfxBuffer*			mpShadowBuffer;
	GfxTarget*			mpTarget;
	const CCameraData*	mCameraData;
	const CCameraData*	mPickCameraData;
	CameraCalcContext	mCameraCalcCtx;
	SRect				mDstRect;
	SRect				mMrtRect;
	orkset<PoolString>	mLayers;
};

///////////////////////////////////////////////////////////////////////////////

class FrameRenderer
{
	RenderContextFrameData	mFrameData;

public:
	FrameRenderer() {}
	virtual void Render() = 0;
	RenderContextFrameData& GetFrameData() { return mFrameData; }
};

///////////////////////////////////////////////////////////////////////////////

class FrameTechniqueBase
{
public:

	FrameTechniqueBase( int iW, int iH );
	virtual ~FrameTechniqueBase(){}
	
	virtual void Render( FrameRenderer& ContextData ) = 0;
	virtual RtGroup* GetFinalRenderTarget() const { return mpMrtFinal; }
	void Init(GfxTarget* targ);

protected:

	int				miW;
	int				miH;
	RtGroup*		mpMrtFinal;

private:

	virtual void DoInit(GfxTarget* targ) {}

};

///////////////////////////////////////////////////////////////////////////////

}}
