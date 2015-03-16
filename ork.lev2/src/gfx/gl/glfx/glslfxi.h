////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

struct GlslFxContainer;
struct GlslFxScanner;
struct GlslFxScannerView;
struct GlslFxScanViewFilter;

struct GlslFxConfig
{
	std::string		mName;
};
struct GlslFxUniform
{
	std::string		mName;
	std::string		mTypeName;
	GLenum			meType;
	std::string		mSemantic;
	int 			mArraySize;

	GlslFxUniform(const std::string& nam,const std::string& sem="")
		: mName(nam)
		, mSemantic(sem)
		, meType(GL_ZERO)
		, mArraySize(0)
		{}
};
struct GlslFxUniformInstance
{
	GLint			mLocation;
	GlslFxUniform*	mpUniform;
	int 			mSubItemIndex;
	svar16_t		mPrivData;
	
	GlslFxUniformInstance() : mLocation(-1), mpUniform(nullptr), mSubItemIndex(0) {}
};

struct GlslFxAttribute
{
	std::string		mName;
	std::string		mTypeName;
	std::string		mDirection;
	GLenum			meType;
	GLint			mLocation;
	std::string		mSemantic;
	std::string		mComment;
	int             mArraySize;

	GlslFxAttribute(const std::string& nam,const std::string& sem="") 
		: mName(nam)
		, mSemantic(sem)
		, meType(GL_ZERO)
		, mLocation(-1)
		, mArraySize(0) {}
};

struct GlslUniformBlock
{
	typedef std::map<std::string,GlslFxUniform*> UniMap;

	GlslUniformBlock() {}

	std::string		mName;
	UniMap      	mUniforms;
};

struct GlslFxStreamInterface
{
	GlslFxStreamInterface();

	typedef std::map<std::string,GlslFxAttribute*> AttrMap;
	typedef std::vector<std::string> preamble_t;

	std::string mName;
	AttrMap     mAttributes;
	GLenum      mInterfaceType;
	preamble_t  mPreamble;
	int         mGsPrimSize;
	std::set<GlslUniformBlock*> mUniformBlockSet;

	void Inherit( const GlslFxStreamInterface& par );
};

typedef std::function<void(GfxTarget*)> state_applicator_t;

struct GlslFxStateBlock
{
	std::string		mName;
	//SRasterState	mState;
	std::vector<state_applicator_t> mApplicators;

	void AddStateFn(const state_applicator_t& f) { mApplicators.push_back(f); }

};
	
struct GlslFxShader
{
	std::string				mName;
	std::string				mShaderText;
	GlslFxStreamInterface*	mpInterface;
	GlslFxContainer*		mpContainer;
	GLuint					mShaderObjectId;
	GLenum					mShaderType;
	bool					mbCompiled;
	bool					mbError;

	GlslFxShader( const std::string& nam, GLenum etyp ) 
		: mName(nam)
		, mShaderObjectId(0)
		, mShaderType( etyp )
		, mbCompiled(false)
		, mbError(false)
		, mpInterface(nullptr)
		, mpContainer(nullptr)
	{
	}

	bool Compile();
	bool IsCompiled() const;
};
struct GlslFxShaderVtx : GlslFxShader
{
	GlslFxShaderVtx( const std::string& nam="" ) : GlslFxShader(nam,GL_VERTEX_SHADER) {}
};
struct GlslFxShaderFrg : GlslFxShader
{
	GlslFxShaderFrg( const std::string& nam="" ) : GlslFxShader(nam,GL_FRAGMENT_SHADER) {}
};
struct GlslFxShaderGeo : GlslFxShader
{
	GlslFxShaderGeo( const std::string& nam="" ) : GlslFxShader(nam,GL_GEOMETRY_SHADER) {}
};
struct GlslFxShaderTsC : GlslFxShader
{
	GlslFxShaderTsC( const std::string& nam="" ) : GlslFxShader(nam,GL_TESS_CONTROL_SHADER) {}
};
struct GlslFxShaderTsE : GlslFxShader
{
	GlslFxShaderTsE( const std::string& nam="" ) : GlslFxShader(nam,GL_TESS_EVALUATION_SHADER) {}
};

struct GlslFxLibBlock
{
	GlslFxLibBlock( const GlslFxScanner& s );

	std::string				mName;
	GlslFxScanViewFilter*	mFilter;
	GlslFxScannerView* 		mView;

};


struct GlslFxPass
{
	typedef std::map<std::string,GlslFxUniformInstance*> uni_map_t;
	typedef std::map<std::string,GlslFxAttribute*> attr_map_t;

	static const int    kmaxattrID = 16;
	std::string         mName;
	GlslFxShaderVtx*    mVertexProgram;
	GlslFxShaderTsC*    mTessCtrlProgram;
	GlslFxShaderTsE*    mTessEvalProgram;
	GlslFxShaderGeo*    mGeometryProgram;
	GlslFxShaderFrg*    mFragmentProgram;
	GlslFxStateBlock*   mStateBlock;
	GLuint              mProgramObjectId;
	uni_map_t           mUniformInstances;
	attr_map_t          mVtxAttributesBySemantic;
	GlslFxAttribute*    mVtxAttributeById[kmaxattrID];
	int                 mSamplerCount;

	GlslFxPass(  const std::string& name )
		: mName(name)
		, mVertexProgram(nullptr)
		, mTessCtrlProgram(nullptr)
		, mTessEvalProgram(nullptr)
		, mGeometryProgram(nullptr)
		, mFragmentProgram(nullptr)
		, mProgramObjectId(0)
		, mStateBlock(nullptr)
		, mSamplerCount(0)
	{
		for( int i=0; i<kmaxattrID; i++ ) mVtxAttributeById[i] = nullptr;
	}
	bool HasUniformInstance( GlslFxUniformInstance* puni ) const;
	const GlslFxUniformInstance* GetUniformInstance( GlslFxUniform* puni ) const;
};

struct GlslFxTechnique
{
	orkvector<GlslFxPass*>	mPasses;
	std::string             mName;

	GlslFxTechnique( const std::string& nam ) { mName=nam; }

	void AddPass( GlslFxPass* pps ) { mPasses.push_back( pps ); }
};

struct GlslFxContainer
{
	std::string										mEffectName;
	const GlslFxTechnique*							mActiveTechnique;
	std::map<std::string,GlslFxConfig*>				mConfigs;
	std::map<std::string,GlslUniformBlock*>			mUniformBlocks;
	std::map<std::string,GlslFxStreamInterface*>	mVertexInterfaces;
	std::map<std::string,GlslFxStreamInterface*>	mTessCtrlInterfaces;
	std::map<std::string,GlslFxStreamInterface*>	mTessEvalInterfaces;
	std::map<std::string,GlslFxStreamInterface*>	mGeometryInterfaces;
	std::map<std::string,GlslFxStreamInterface*>	mFragmentInterfaces;
	std::map<std::string,GlslFxStateBlock*>			mStateBlocks;
	std::map<std::string,GlslFxUniform*>			mUniforms;
	std::map<std::string,GlslFxShaderVtx*>			mVertexPrograms;
	std::map<std::string,GlslFxShaderTsC*>			mTessCtrlPrograms;
	std::map<std::string,GlslFxShaderTsE*>			mTessEvalPrograms;
	std::map<std::string,GlslFxShaderGeo*>			mGeometryPrograms;
	std::map<std::string,GlslFxShaderFrg*>			mFragmentPrograms;
	std::map<std::string,GlslFxTechnique*>			mTechniqueMap;
	std::map<std::string,GlslFxLibBlock*>			mLibBlocks;
	const GlslFxPass*								mActivePass;
	int												mActiveNumPasses;
	const FxShader* 								mFxShader;
	bool 											mShaderCompileFailed;

	//bool Load( const AssetPath& filename, FxShader*pfxshader );
	void Destroy( void );
	bool IsValid( void );

	void AddConfig( GlslFxConfig* pcfg );
	void AddUniformBlock( GlslUniformBlock* pif );
	void AddVertexInterface( GlslFxStreamInterface* pif );
	void AddTessCtrlInterface( GlslFxStreamInterface* pif );
	void AddTessEvalInterface( GlslFxStreamInterface* pif );
	void AddGeometryInterface( GlslFxStreamInterface* pif );
	void AddFragmentInterface( GlslFxStreamInterface* pif );
	GlslFxUniform* MergeUniform( const std::string& name );
	void AddStateBlock( GlslFxStateBlock* pSB );
	void AddTechnique( GlslFxTechnique* ptek );
	void AddVertexProgram( GlslFxShaderVtx* psha );
	void AddTessCtrlProgram( GlslFxShaderTsC* psha );
	void AddTessEvalProgram( GlslFxShaderTsE* psha );
	void AddGeometryProgram( GlslFxShaderGeo* psha );
	void AddFragmentProgram( GlslFxShaderFrg* psha );
	void AddLibBlock( GlslFxLibBlock* plb );

	GlslFxStateBlock* GetStateBlock( const std::string& name ) const;
	GlslFxUniform* GetUniform( const std::string& name ) const;
	GlslFxShaderVtx* GetVertexProgram( const std::string& name ) const;
	GlslFxShaderTsC* GetTessCtrlProgram( const std::string& name ) const;
	GlslFxShaderTsE* GetTessEvalProgram( const std::string& name ) const;
	GlslFxShaderGeo* GetGeometryProgram( const std::string& name ) const;
	GlslFxShaderFrg* GetFragmentProgram( const std::string& name ) const;
	GlslUniformBlock* GetUniformBlock( const std::string& name ) const;
	GlslFxStreamInterface* GetVertexInterface( const std::string& name ) const;
	GlslFxStreamInterface* GetTessCtrlInterface( const std::string& name ) const;
	GlslFxStreamInterface* GetTessEvalInterface( const std::string& name ) const;
	GlslFxStreamInterface* GetGeometryInterface( const std::string& name ) const;
	GlslFxStreamInterface* GetFragmentInterface( const std::string& name ) const;
	
	GlslFxContainer(const std::string& nam);
};

class GfxTargetGL;

class GlslFxInterface : public FxInterface
{
public:

	virtual void DoBeginFrame();

	virtual int BeginBlock( FxShader* hfx, const RenderContextInstData& data );
	virtual bool BindPass( FxShader* hfx, int ipass );
	virtual bool BindTechnique( FxShader* hfx, const FxShaderTechnique* htek );
	virtual void EndPass( FxShader* hfx );
	virtual void EndBlock( FxShader* hfx );
	virtual void CommitParams( void );

	virtual const FxShaderTechnique* GetTechnique( FxShader* hfx, const std::string & name );
	virtual const FxShaderParam* GetParameterH( FxShader* hfx, const std::string & name );

	virtual void BindParamBool( FxShader* hfx, const FxShaderParam* hpar, const bool bval );
	virtual void BindParamInt( FxShader* hfx, const FxShaderParam* hpar, const int ival );
	virtual void BindParamVect2( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec );
	virtual void BindParamVect3( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec );
	virtual void BindParamVect4( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec );
	virtual void BindParamVect4Array( FxShader* hfx, const FxShaderParam* hpar, const CVector4 * Vec, const int icount );
	virtual void BindParamFloatArray( FxShader* hfx, const FxShaderParam* hpar, const float * pfA, const int icnt );
	virtual void BindParamFloat( FxShader* hfx, const FxShaderParam* hpar, float fA );
	virtual void BindParamFloat2( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB );
	virtual void BindParamFloat3( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC );
	virtual void BindParamFloat4( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC, float fD );
	virtual void BindParamMatrix( FxShader* hfx, const FxShaderParam* hpar, const CMatrix4 & Mat );
	virtual void BindParamMatrix( FxShader* hfx, const FxShaderParam* hpar, const CMatrix3 & Mat );
	virtual void BindParamMatrixArray( FxShader* hfx, const FxShaderParam* hpar, const CMatrix4 * MatArray, int iCount );
	virtual void BindParamU32( FxShader* hfx, const FxShaderParam* hpar, U32 uval );
	virtual void BindParamCTex( FxShader* hfx, const FxShaderParam* hpar, const Texture *pTex );
	virtual bool LoadFxShader( const AssetPath& pth, FxShader *ptex );

	GlslFxInterface( GfxTargetGL& glctx );

	void BindContainerToAbstract(GlslFxContainer* pcont, FxShader* fxh );

	GlslFxContainer* GetActiveEffect() const { return mpActiveEffect; }

protected:

	//static CGcontext					mCgContext;
	GlslFxContainer*					mpActiveEffect;
	const GlslFxPass*					mLastPass;
	FxShaderTechnique*					mhCurrentTek;

	GfxTargetGL&						mTarget;

};

GlslFxContainer* LoadFxFromFile( const AssetPath& pth );

} }

///////////////////////////////////////////////////////////////////////////////
