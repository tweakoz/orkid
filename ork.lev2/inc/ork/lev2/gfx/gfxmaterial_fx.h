////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once 

#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/kernel/prop.h>
#include <ork/lev2/gfx/lev2renderer.h>

namespace ork {

namespace lev2 {

class FxShader;
struct FxShaderParam;

struct VertexConfig
{
	std::string Name;
	std::string Type;
	std::string Source;
	std::string Semantic;
};

///////////////////////////////////////////////////////////////////////////////

class GfxMaterialFx;

class GfxMaterialFxParamBase : public ork::Object
{
	RttiDeclareAbstract(GfxMaterialFxParamBase,ork::Object);

public:

	GfxMaterialFxParamBase(GfxMaterialFx *parent = NULL);
	FxParamRec& GetRecord() { return mRecord; }
	const FxParamRec& GetRecord() const { return mRecord; }
	//void SetRecord( FxParamRec* rec ) { mRecord=rec; }

	virtual void Bind( FxShader* fxh, GfxTarget *pTARG ) = 0;
	virtual std::string GetValueString( void ) const = 0;

	bool IsBindable( void ) const { return mbIsBindable; }
	void SetBindable( bool bv ) { mbIsBindable=bv; }

	const std::string& GetInitString() const { return mParameterInitString; }
	void SetInitString( const std::string& initstr ) { mParameterInitString=initstr; }

	GfxMaterialFx* GetParentMaterial() const { return mParentMaterial; }
	orklut<std::string,std::string>& RefAnnotations() { return mAnnotations; }
	const orklut<std::string,std::string>& RefAnnotations() const { return mAnnotations; }
	void AddAnnotation( const char* pkey, const char* pval );

private:

	FxParamRec						mRecord;
	bool							mbIsBindable;
	std::string						mParameterInitString;
	GfxMaterialFx*					mParentMaterial;
	orklut<std::string,std::string>	mAnnotations;
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> class GfxMaterialFxParam : public GfxMaterialFxParamBase
{
	DECLARE_TRANSPARENT_TEMPLATE_ABSTRACT_RTTI(GfxMaterialFxParam<T>,GfxMaterialFxParamBase);

public:

	GfxMaterialFxParam(GfxMaterialFx *parent = NULL) : GfxMaterialFxParamBase(parent) {}

	virtual const T& GetValue( GfxTarget *pTARG ) const = 0;

	virtual void Bind( FxShader* fxh, GfxTarget *pTARG );

	virtual std::string GetValueString( void ) const;
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> class GfxMaterialFxParamEngine : public GfxMaterialFxParam<T>
{
	DECLARE_TRANSPARENT_TEMPLATE_RTTI(GfxMaterialFxParamEngine<T>,GfxMaterialFxParam<T>);

public:

	typedef const T&(*FunctptrVoid)( GfxTarget *pTARG );
	typedef const T&(*FunctptrParam)( GfxTarget *pTARG, const GfxMaterialFxParamBase*param );

	GfxMaterialFxParamEngine(GfxMaterialFx *parent = NULL)
		: GfxMaterialFxParam<T>(parent)
		, mFuncptrVoid(0)
		, mFuncptrParam( 0 )
	{
	}

	virtual const T& GetValue( GfxTarget *pTARG ) const
	{
		OrkAssert( (mFuncptrVoid!=0) || (mFuncptrParam!=0) );
		return mFuncptrVoid ? mFuncptrVoid(pTARG) : mFuncptrParam(pTARG,this);
	}

	FunctptrVoid mFuncptrVoid;
	FunctptrParam mFuncptrParam;
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> class GfxMaterialFxParamArtist : public GfxMaterialFxParam<T>
{
	DECLARE_TRANSPARENT_TEMPLATE_RTTI(GfxMaterialFxParamArtist<T>,GfxMaterialFxParam<T>);

public:

	GfxMaterialFxParamArtist(GfxMaterialFx *parent = NULL);
	virtual const T& GetValue( GfxTarget *pTARG ) const { return mValue; }

	T mValue;
};

///////////////////////////////////////////////////////////////////////////////

struct GfxMaterialFxEffectInstance
{
	void AddParameter( GfxMaterialFxParamBase* param );
	void ReplaceParameter( GfxMaterialFxParamBase* param );
	std::string GetParamValue( const std::string & pname ) const;

	GfxMaterialFxEffectInstance();
	~GfxMaterialFxEffectInstance();

	FxShader*											mpEffect;
	orklut<std::string,GfxMaterialFxParamBase*>			mParameterInstances;

};

///////////////////////////////////////////////////////////////////////////////

class FxMatrixBlockApplicator;

class GfxMaterialFx : public GfxMaterial
{
	RttiDeclareConcrete(GfxMaterialFx,GfxMaterial);

	friend class FxMatrixBlockApplicator;

	public:

	GfxMaterialFx();
	~GfxMaterialFx() final;

	static bool gEnableLightPreview;

	////////////////////////////////////////////

	void LoadEffect( const AssetPath& EffectAssetName );
	void SetEffect( FxShader* pshader );
	void SetTechnique( const std::string& TechniqueName );
	void AddParameter( GfxMaterialFxParamBase* param );
	std::string GetParamValue( const std::string & pname ) const { return mEffectInstance.GetParamValue( pname ); }

	const orkvector<VertexConfig> & RefVertexConfig( void ) const { return mVertexConfigData; }

	const GfxMaterialFxEffectInstance& GetEffectInstance() const { return mEffectInstance; }

	////////////////////////////////////////////

    void Init( GfxTarget* pTARG ) final;
	bool BeginPass( GfxTarget* pTARG, int iPass=0 ) final;
	void EndPass( GfxTarget* pTARG ) final;
	int BeginBlock( GfxTarget* pTARG, const RenderContextInstData &MatCtx ) final;
	void EndBlock( GfxTarget* pTARG ) final;
	void UpdateMVPMatrix( GfxTarget *pTARG ) final;

	void SetMaterialProperty( const char* prop, const char* val ) final;

	protected:

    void Update( void ) final;
    void BindMaterialInstItem( MaterialInstItem* pitem ) const final;
    void UnBindMaterialInstItem( MaterialInstItem* pitem ) const final;

	///////////////////////////////////////////
	// Lighting

	LightingFxInterface			mLightingInterface;

	fvec3					mScreenZDir;

	///////////////////////////////////////////

	AssetPath										mAssetPath;
	GfxMaterialFxEffectInstance						mEffectInstance;

	const FxShaderTechnique*						mActiveTechnique;
	const FxShaderTechnique*						mActiveShadowTechnique;
	const FxShaderTechnique*						mActiveLightMapTechnique;
	const FxShaderTechnique*						mActiveLightPreviewTechnique;
	const FxShaderTechnique*						mActiveVertexLightTechnique;
	const FxShaderTechnique*						mActiveSkinnedTechnique;
	const FxShaderTechnique*						mActiveSkinnedShadowTechnique;
	const FxShaderTechnique*						mActivePickTechnique;
	const FxShaderTechnique*						mActiveSkinnedPickTechnique;

	const FxShaderParam*							mBonesParam;
	const FxShaderParam*							mWorldMtxParam;
	const FxShaderParam*							mWorldViewMtxParam;
	const FxShaderParam*							mWorldViewProjectionMtxParam;
	const FxShaderParam*							mIsShadowRecieverParam;
	const FxShaderParam*							mIsShadowCasterParam;
	const FxShaderParam*							mIsSkinnedParam;
	const FxShaderParam*							mIsPickParam;
	const FxShaderParam*							mLightMapParam;
	Texture*										mLightMapTexture;

	orkvector<VertexConfig>							mVertexConfigData;

	static PerformanceItem*						gMatFxBeginPassPerfItem;
	static PerformanceItem*						gMatFxBeginBlockPerfItem;

	static const int kMaxEngineParamFloats = RenderContextInstData::kMaxEngineParamFloats;

	float											mEngineParamFloats[kMaxEngineParamFloats];
	std::string	mMainTechniqueName;
	std::string	mActiveTechniqueName;
};

///////////////////////////////////////////////////////////////////////////////

class FxMatrixBlockApplicator : public MaterialInstApplicator
{
	RttiDeclareAbstract(FxMatrixBlockApplicator,MaterialInstApplicator);

public:

	MaterialInstItemMatrixBlock*		mMatrixBlockItem;
	const GfxMaterialFx*				mMaterial;

	///////////////////////////////////////////////////////////////

	FxMatrixBlockApplicator() : mMatrixBlockItem(0), mMaterial(0) {}
	FxMatrixBlockApplicator( MaterialInstItemMatrixBlock* mtxblockitem,	const GfxMaterialFx* pmat );

    ///////////////////////////////////////////////////////////////

private:
	void ApplyToTarget( GfxTarget *pTARG ) final; 


};

///////////////////////////////////////////////////////////////////////////////

} }
