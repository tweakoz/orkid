////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#if ! defined( _GFX_GFXMATERIAL_BASIC_H )
#define	_GFX_GFXMATERIAL_BASIC_H

#include <ork/lev2/gfx/gfxmaterial.h>

namespace ork { namespace lev2 {

class GfxMaterialWiiBasic;

///////////////////////////////////////////////////////////////////////////////

class WiiMatrixBlockApplicator : public MaterialInstApplicator
{
	RttiDeclareAbstract(WiiMatrixBlockApplicator,MaterialInstApplicator);

public:

	MaterialInstItemMatrixBlock*		mMatrixBlockItem;
	const GfxMaterialWiiBasic*			mMaterial;

	///////////////////////////////////////////////////////////////

	WiiMatrixBlockApplicator() : mMatrixBlockItem(0), mMaterial(0) {}
	WiiMatrixBlockApplicator( MaterialInstItemMatrixBlock* mtxblockitem,	const GfxMaterialWiiBasic* pmat );
	void ApplyToTarget( GfxTarget *pTARG ); // virtual

	///////////////////////////////////////////////////////////////

};

///////////////////////////////////////////////////////////////////////////////

class WiiMatrixApplicator : public MaterialInstApplicator
{
	RttiDeclareAbstract(WiiMatrixApplicator,MaterialInstApplicator);

public:

	MaterialInstItemMatrix*			mMatrixItem;
	const GfxMaterialWiiBasic*			mMaterial;

	///////////////////////////////////////////////////////////////

	WiiMatrixApplicator() : mMatrixItem(0), mMaterial(0) {}
	WiiMatrixApplicator( MaterialInstItemMatrix* mtxitem, const GfxMaterialWiiBasic* pmat );
	void ApplyToTarget( GfxTarget *pTARG ); // virtual

	///////////////////////////////////////////////////////////////

};

///////////////////////////////////////////////////////////////////////////////

class GfxMaterialWiiBasic : public GfxMaterial //TRttiBase<GfxMaterialWiiBasic,GfxMaterial>
{
	friend class WiiMatrixBlockApplicator;
	friend class WiiMatrixApplicator;

	RttiDeclareConcrete(GfxMaterialWiiBasic,GfxMaterial);
	public:

	static void StaticInit();

	GfxMaterialWiiBasic( const char* pbastek = "/modvtx" );
	virtual ~GfxMaterialWiiBasic(){};

	virtual void Init( GfxTarget *pTarg );										//virtual 

	int  BeginBlock( GfxTarget* pTARG, const RenderContextInstData &MatCtx );	//virtual 
	void EndBlock( GfxTarget* pTARG );											//virtual 
	void Update( void ) {}														//virtual 
	bool BeginPass( GfxTarget* pTARG, int iPass=0 );							//virtual 
	void EndPass( GfxTarget* pTARG );											//virtual 

	float			mSpecularPower;
	CVector4		mEmissiveColor;

	const std::string & GetBasicTechName( void ) const { return mBasicTechName; }

	void BindMaterialInstItem( MaterialInstItem* pitem ) const;					//virtual 
	void UnBindMaterialInstItem( MaterialInstItem* pitem ) const;				//virtual 
	void UpdateMVPMatrix( GfxTarget *pTARG );									//virtual 
																				
protected:																		

	const std::string	mBasicTechName;

	FxShader*					hModFX;
	const FxShaderTechnique*	hTekModVtxTex;
	const FxShaderTechnique*	hTekMod;

	const FxShaderParam*	hMatMV;
	const FxShaderParam*	hMatP;
	const FxShaderParam*	hWVPMatrix;
	const FxShaderParam*	hVPMatrix;
	const FxShaderParam*	hWMatrix;

	const FxShaderParam*	hIWMatrix;

	const FxShaderParam*	hVMatrix;
	const FxShaderParam*	hWRotMatrix;
	const FxShaderParam*	hBoneMatrices;
	const FxShaderParam*	hDiffuseMapMatrix;
	const FxShaderParam*	hSpecularMapMatrix;
	const FxShaderParam*	hNormalMapMatrix;
	const FxShaderParam*	hAmbientMapMatrix;
	const FxShaderParam*	hDiffuseTEX;
	const FxShaderParam*	hSpecularTEX;
	const FxShaderParam*	hSpecularPower;
	const FxShaderParam*	hAmbientTEX;
	const FxShaderParam*	hNormalTEX;
	const FxShaderParam*	hMODCOLOR;
	const FxShaderParam*	hTIME;
	const FxShaderParam*	hWCamLoc;

	const FxShaderParam*	hTopEnvTEX;
	const FxShaderParam*	hBotEnvTEX;

	///////////////////////////////////////////
	// Lighting

	LightingFxInterface			mLightingInterface;

	///////////////////////////////////////////
	// Material

	const FxShaderParam*	hEmissiveColor;

	///////////////////////////////////////////


	CVector4		mColor;
	CVector3		mScreenZDir;

private:

	static const orkmap<std::string,std::string> mBasicTekMap;
	static const orkmap<std::string,std::string> mPickTekMap;

};


///////////////////////////////////////////////////////////////////////////////

} }

#endif
