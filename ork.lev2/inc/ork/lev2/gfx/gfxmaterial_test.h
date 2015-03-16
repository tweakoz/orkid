////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once 

#include <ork/lev2/gfx/gfxmaterial.h>

namespace ork { namespace lev2
{
//
///////////////////////////////////////////////////////////////////////////////

class GfxMaterial3DSolid : public GfxMaterial
{
	RttiDeclareConcrete(GfxMaterial3DSolid,GfxMaterial);
public:
	
	static void ClassInit();
	GfxMaterial3DSolid(GfxTarget* pTARG=0);
	GfxMaterial3DSolid(GfxTarget* pTARG, const char* puserfx, const char* pusertek, bool allowcompilefailure=false,bool unmanaged=false );
	
	virtual ~GfxMaterial3DSolid() {};
	virtual void Update( void ) {}
	virtual void Init( GfxTarget *pTarg );

	void SetVolumeTexture( Texture* ptex ) { mVolumeTexture=ptex; }
	void SetTexture( Texture* ptex ) { mCurrentTexture=ptex; }
	void SetTexture2( Texture* ptex ) { mCurrentTexture2=ptex; }
	void SetTexture3( Texture* ptex ) { mCurrentTexture3=ptex; }
	void SetTexture4( Texture* ptex ) { mCurrentTexture4=ptex; }

	void SetUser0( const CVector4& vuser ) { mUser0=vuser; }
	void SetUser1( const CVector4& vuser ) { mUser1=vuser; }
	void SetUser2( const CVector4& vuser ) { mUser2=vuser; }
	void SetUser3( const CVector4& vuser ) { mUser3=vuser; }

	void SetUserFx( const char* puserfx, const char* pusertek )
	{
		mUserFxName = puserfx;
		mUserTekName = pusertek;
	}

	bool IsUserFxOk() const;

	////////////////////////////////////////////

	enum EColorMode
	{
		EMODE_VERTEXMOD_COLOR = 0,
		EMODE_VERTEX_COLOR,
		EMODE_TEX_COLOR,
		EMODE_TEXMOD_COLOR,
		EMODE_TEXTEXMOD_COLOR,
		EMODE_TEXVERTEX_COLOR,
		EMODE_MOD_COLOR,
		EMODE_INTERNAL_COLOR,
		EMODE_USER,
	};

	////////////////////////////////////////////

	EColorMode GetColorMode() const { return meColorMode; }
	void SetColorMode( EColorMode emode ) { meColorMode = emode; }
	void SetColor( const CVector4& color ) { Color=color; }
	const CVector4& GetColor() const { return Color; }
	void SetNoiseAmp( const CVector4& color ) { mNoiseAmp=color; }
	void SetNoiseFreq( const CVector4& color ) { mNoiseFreq=color; }
	void SetNoiseShift( const CVector4& color ) { mNoiseShift=color; }
	
	void SetMaterialProperty( const char* prop, const char* val ); // virtual

	////////////////////////////////////////////

	virtual bool BeginPass( GfxTarget* pTARG, int iPass=0 );
	virtual void EndPass( GfxTarget* pTARG );
	virtual int BeginBlock( GfxTarget* pTARG, const RenderContextInstData &MatCtx );
	virtual void EndBlock( GfxTarget* pTARG );

	void SetAuxMatrix( const CMatrix4& mtx ) { mMatAux=mtx; }

	protected:
		
	EColorMode		meColorMode;
	CVector4		mNoiseAmp;
	CVector4		mNoiseFreq;
	CVector4		mNoiseShift;
	CVector4		Color;
	CVector4		mUser0;
	CVector4		mUser1;
	CVector4		mUser2;
	CVector4		mUser3;
	FxShader*		hModFX;
	Texture*		mVolumeTexture;
	Texture*		mCurrentTexture;
	Texture*		mCurrentTexture2;
	Texture*		mCurrentTexture3;
	Texture*		mCurrentTexture4;
	std::string		mUserFxName;
	std::string		mUserTekName;
	CMatrix4		mMatAux;
	bool 			mUnManaged;
	bool			mAllowCompileFailure;

	const FxShaderTechnique*	hTekUser;
	const FxShaderTechnique*	hTekTexColor;
	const FxShaderTechnique*	hTekTexModColor;
	const FxShaderTechnique*	hTekTexTexModColor;
	const FxShaderTechnique*	hTekTexVertexColor;
	const FxShaderTechnique*	hTekVertexColor;
	const FxShaderTechnique*	hTekVertexModColor;
	const FxShaderTechnique*	hTekModColor;

	const FxShaderParam*		hMatM;
	const FxShaderParam*		hMatV;
	const FxShaderParam*		hMatP;
	const FxShaderParam*		hMatMV;
	const FxShaderParam*		hMatMVP;
	const FxShaderParam*		hMatAux;
	const FxShaderParam*		hVolumeMap;
	const FxShaderParam*		hColorMap;
	const FxShaderParam*		hColorMap2;
	const FxShaderParam*		hColorMap3;
	const FxShaderParam*		hColorMap4;
	const FxShaderParam*		hParamUser0;
	const FxShaderParam*		hParamUser1;
	const FxShaderParam*		hParamUser2;
	const FxShaderParam*		hParamUser3;
	const FxShaderParam*		hParamModColor;
	const FxShaderParam*		hParamTime;
	const FxShaderParam*		hParamNoiseShift;
	const FxShaderParam*		hParamNoiseFreq;
	const FxShaderParam*		hParamNoiseAmp;

};

///////////////////////////////////////////////////////////////////////////////

} }
