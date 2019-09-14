////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/frametek.h>

namespace ork { namespace lev2 {

enum EFrameEffect
{
	EFRAMEFX_NONE = 0,
	EFRAMEFX_STANDARD,
	EFRAMEFX_COMIC,
	EFRAMEFX_GLOW,
	EFRAMEFX_GHOSTLY,
	EFRAMEFX_AFTERLIFE,
};

///////////////////////////////////////////////////////////////////////////////

class TexBuffer : public GfxBuffer
{
	public:

	TexBuffer(	GfxBuffer *parent,
				EBufferFormat efmt,
				int iW, int iH );

};

///////////////////////////////////////////////////////////////////////////

class BasicFrameTechnique : public FrameTechniqueBase
{
public:

	BasicFrameTechnique(  );

	virtual void Render( ork::lev2::FrameRenderer & ContextData );

	bool mbDoBeginEndFrame;
};

class PickFrameTechnique : public FrameTechniqueBase
{
public:

	PickFrameTechnique( );

	virtual void Render( ork::lev2::FrameRenderer & ContextData );
};

///////////////////////////////////////////////////////////////////////////////

class BuiltinFrameEffectMaterial : public GfxMaterial
{
	public:

	BuiltinFrameEffectMaterial();
	void Init( GfxTarget* pTarg );
	void PostInit( GfxTarget* pTarg , const char *FxFile, const char* TekName );
	void SetAuxMaps( Texture *AuxMap0, Texture* AuxMap1 );
	~BuiltinFrameEffectMaterial() {}
	virtual void Update( void );
	virtual bool BeginPass( GfxTarget* pTarg,int iPass=0 );
	virtual void EndPass( GfxTarget* pTarg );
	virtual int BeginBlock( GfxTarget* pTarg,const RenderContextInstData &MatCtx );
	virtual void EndBlock( GfxTarget* pTarg );
	void SetEffectAmount( float famount ) { mfEffectAmount=famount; }

	void BindRtGroups( RtGroup* cur, RtGroup* prev ) { mpCurrentRtGroup=cur; mpPreviousRtGroup=prev; }

	//////////////////////////////////////////////////////////////////////////////

	private:

	RtGroup*					mpCurrentRtGroup;
	RtGroup*					mpPreviousRtGroup;

	file::Path					mFxFile;
	file::Path::NameType		mTekName;
	FxShader*					hFX;
	const FxShaderTechnique*	hTek;
	const FxShaderParam*		hMVP;
	const FxShaderParam*		hModColor;
	const FxShaderParam*		hMrtMap0;
	const FxShaderParam*		hMrtMap1;
	const FxShaderParam*		hMrtMap2;
	const FxShaderParam*		hMrtMap3;
	const FxShaderParam*		hNoiseMap;
	const FxShaderParam*		hAuxMap0;
	const FxShaderParam*		hAuxMap1;
	const FxShaderParam*		hTime;
	const FxShaderParam*		hBlurFactor;
	const FxShaderParam*		hEffectAmount;
	const FxShaderParam*		hBlurFactorI;
	const FxShaderParam*		hViewportDim;
	Texture*					mpNoiseMap;
	Texture*					mpAuxMap0;
	Texture*					mpAuxMap1;
	float						mfEffectAmount;
};

///////////////////////////////////////////////////////////////////////////////

class BuiltinFrameTechniques : public FrameTechniqueBase
{
	static const int knumpingpongbufs = 4;

	bool							mbPostFxFb;

	RtGroup*						mpHDRRtGroup[knumpingpongbufs];

	RtGroup*						mpMrtAux0;
	RtGroup*						mpMrtAux1;
	RtGroup*						mpMrtFinalHD;
	RtGroup*						mpReadRtGroup;
	Texture*						mpRadialMap;
    Texture*                        mpFbUvMap;

	std::string						mEffectName;
	TexBuffer*						mpAuxBuffer0;
	TexBuffer*						mpAuxBuffer1;
	BuiltinFrameEffectMaterial		mFrameEffectNZPAC; //[n3 z1] [p3 amb1] [c3]
	BuiltinFrameEffectMaterial		mFrameEffectRadialBlur;
	BuiltinFrameEffectMaterial		mFrameEffectBlurX;
	BuiltinFrameEffectMaterial		mFrameEffectBlurY;
	BuiltinFrameEffectMaterial		mFrameEffectStandard;
	BuiltinFrameEffectMaterial		mFrameEffectComic;
	BuiltinFrameEffectMaterial		mFrameEffectGlowJoin;
	BuiltinFrameEffectMaterial		mFrameEffectGhostJoin;
	BuiltinFrameEffectMaterial		mFrameEffectDofJoin;
	BuiltinFrameEffectMaterial		mFrameEffectAfterLifeJoin;
	BuiltinFrameEffectMaterial		mFrameEffectPainterly;
	BuiltinFrameEffectMaterial		mFrameEffectDbgNormals;
	BuiltinFrameEffectMaterial		mFrameEffectDbgDepth;
	ork::lev2::GfxMaterial3DSolid	mUtilMaterial;

	ork::lev2::GfxMaterial3DSolid	mFBinMaterial;
	BuiltinFrameEffectMaterial		mFBoutMaterial;

	void PreProcess( RenderContextFrameData& FrameData );
	void PostProcess( RenderContextFrameData& FrameData );

public:

	RtGroup* GetNextWriteRtGroup() const;
	void SetReadRtGroup( RtGroup* pgrp ) { mpReadRtGroup=pgrp; }
	RtGroup* GetReadRtGroup() const { return mpReadRtGroup; }

	void SetPostFxFb( bool bena ) { mbPostFxFb=bena; }

	int GetW() const { return miWidth; }
	int GetH() const { return miHeight; }

	void ResizeFinalBuffer( int iw, int ih );
	void ResizeFxBuffer( int iw, int ih );

	float	mfAmount;
	float	mfFeedbackAmount;
	int							miWidth;
	int							miHeight;
	int							miFinalW;
	int							miFinalH;
	int							miFxW;
	int							miFxH;
	float						mfSourceAmplitude;
	mutable int					miRtGroupIndex;

	RtGroup*					mOutputRt;

	void DoInit( GfxTarget* ptgt );
	BuiltinFrameTechniques( int iW, int iH );
	~BuiltinFrameTechniques();
	virtual void Render( FrameRenderer & ContextData );
	void SetEffect( const char *EffectName, float famount=0.0f, float feedbackamt=0.0f );
    virtual RtGroup* GetFinalRenderTarget() const { return mOutputRt; }
    void SetFbUvMap( Texture* ptex ) { mpFbUvMap=ptex; }
};

///////////////////////////////////////////////////////////////////////////

class ShadowFrameTechnique : public FrameTechniqueBase
{
	RtGroup* _pRtGroup;
	TexBuffer* _pShadowBuffer;

public:

	ShadowFrameTechnique( GfxWindow* Parent, ui::Viewport* pvp, int iW, int iH );
	void Render( FrameRenderer & ContextData ) final;
};

}} // namespace ork::lev2
