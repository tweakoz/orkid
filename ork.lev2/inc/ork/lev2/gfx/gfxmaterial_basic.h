////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

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

private:
	void ApplyToTarget( GfxTarget *pTARG ) final;

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

	///////////////////////////////////////////////////////////////
private:
    void ApplyToTarget( GfxTarget *pTARG ) final;

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
	~GfxMaterialWiiBasic() final {};

	void Init( GfxTarget *pTarg ) final;
	int  BeginBlock( GfxTarget* pTARG, const RenderContextInstData &MatCtx ) final;
	void EndBlock( GfxTarget* pTARG ) final;
	void Update( void ) final {}
	bool BeginPass( GfxTarget* pTARG, int iPass=0 ) final;
	void EndPass( GfxTarget* pTARG ) final;
	void BindMaterialInstItem( MaterialInstItem* pitem ) const final;
	void UnBindMaterialInstItem( MaterialInstItem* pitem ) const final;
	void UpdateMVPMatrix( GfxTarget *pTARG ) final ;

    const std::string & GetBasicTechName( void ) const { return mBasicTechName; }

    float           mSpecularPower;
    fvec4        mEmissiveColor;

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


	fvec4		mColor;
	fvec3		mScreenZDir;

private:

	static const orkmap<std::string,std::string> mBasicTekMap;
	static const orkmap<std::string,std::string> mPickTekMap;

};


///////////////////////////////////////////////////////////////////////////////

} }
