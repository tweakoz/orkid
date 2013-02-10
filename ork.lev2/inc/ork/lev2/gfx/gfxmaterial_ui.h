////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#if ! defined( _GFX_GFXMATERIAL_UI_H )
#define	_GFX_GFXMATERIAL_UI_H

#include <ork/lev2/gfx/gfxmaterial.h>

namespace ork { namespace lev2
{

///////////////////////////////////////////////////////////////////////////////

class GfxMaterialUI : public GfxMaterial
{
	RttiDeclareAbstract(GfxMaterialUI,GfxMaterial);
	//////////////////////////////////////////////////////////////////////////////

	public:

	GfxMaterialUI(GfxTarget *pTarg=0);
	virtual ~GfxMaterialUI();

	virtual void Update( void ) {}

	virtual void Init( GfxTarget *pTarg );

	virtual bool BeginPass( GfxTarget* pTARG, int iPass=0 );
	virtual void EndPass( GfxTarget* pTARG );
	virtual int BeginBlock( GfxTarget* pTARG, const RenderContextInstData &MatCtx );
	virtual void EndBlock( GfxTarget* pTARG );

	void SetUIColorMode( EUIColorMode emod ) { meUIColorMode = emod; }
	EUIColorMode GetUIColorMode( void ) { return meUIColorMode; }

	enum EType
	{
		ETYPE_STANDARD = 0,
		ETYPE_CIRCLE
	};

	void SetType( EType etyp ) { meType=etyp; }

	//////////////////////////////////////////////////////////////////////////////

	protected:

	CVector4 PosScale;
	CVector4 PosBias;
	CVector4 Color;
	FxShader* hModFX;
	EUIColorMode meUIColorMode;
	const FxShaderTechnique* hTekMod;
	const FxShaderTechnique* hTekVtx;
	const FxShaderTechnique* hTekModVtx;
	const FxShaderTechnique* hTekCircle;
	const FxShaderParam* hVPW;
	const FxShaderParam* hBias;
	const FxShaderParam* hScale;
	const FxShaderParam* hTransform;
	const FxShaderParam* hModColor;
	const FxShaderParam* hColorMap;
	const FxShaderParam* hCircleInnerRadius;
	const FxShaderParam* hCircleOuterRadius;
	EType		 meType;

};

///////////////////////////////////////////////////////////////////////////////

class GfxMaterialUIText : public GfxMaterial 
{
	//////////////////////////////////////////////////////////////////////////////

	public:
	static void ClassInit() {}

	GfxMaterialUIText(GfxTarget *pTarg=0);
	
	virtual void Update( void ) {}
	virtual void Init( GfxTarget *pTarg );

	virtual bool BeginPass( GfxTarget* pTARG, int iPass=0 );
	virtual void EndPass( GfxTarget* pTARG );
	virtual int BeginBlock( GfxTarget* pTARG, const RenderContextInstData &MatCtx );
	virtual void EndBlock( GfxTarget* pTARG );

	//////////////////////////////////////////////////////////////////////////////

	protected:

	CVector4 PosScale;
	CVector4 PosBias;
	CVector4 TexScale;
	CVector4 TexColor;

	FxShader* hModFX;
	const FxShaderTechnique* hTek;
	const FxShaderParam* hVPW;
	const FxShaderParam* hBias;
	const FxShaderParam* hScale;
	const FxShaderParam* hUVScale;
	const FxShaderParam* hTransform;
	const FxShaderParam* hModColor;
	const FxShaderParam* hColorMap;
};

///////////////////////////////////////////////////////////////////////////////

class GfxMaterialUITextured : public GfxMaterial 
{	public:
	
	static void ClassInit();
	GfxMaterialUITextured( GfxTarget *pTarg = 0, const std::string & Technique="uitextured" );
	virtual void Init( GfxTarget *pTarg, const std::string & Technique );
	virtual void Init( GfxTarget *pTarg );
	virtual void Update( void ) {}
	virtual bool BeginPass( GfxTarget* pTARG, int iPass=0 );
	virtual void EndPass( GfxTarget* pTARG );
	virtual int BeginBlock( GfxTarget* pTARG, const RenderContextInstData &MatCtx );
	virtual void EndBlock( GfxTarget* pTARG );
	
	void EffectInit( void );

	protected:
	
	std::string mTechniqueName;

	CVector4 Color;
	FxShader* hModFX;

	const FxShaderTechnique* hTek;
	const FxShaderParam* hVPW;
	const FxShaderParam* hBias;
	const FxShaderParam* hScale;
	const FxShaderParam* hTransform;
	const FxShaderParam* hModColor;
	const FxShaderParam* hColorMap;
};

///////////////////////////////////////////////////////////////////////////////

} }

#endif
