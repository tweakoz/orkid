////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#if defined(_USE_GLSLFX)

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

struct GlslFxContainer;

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
	GlslFxUniformInstance() : mLocation(-1), mpUniform(nullptr) {}
};

struct GlslFxAttribute
{
	std::string		mName;
	std::string		mTypeName;
	std::string		mDirection;
	GLenum			meType;
	GLint			mLocation;
	std::string		mSemantic;

	GlslFxAttribute(const std::string& nam,const std::string& sem="") 
		: mName(nam)
		, mSemantic(sem)
		, meType(GL_ZERO)
		, mLocation(-1) {}
};

struct GlslFxStreamInterface
{
	typedef std::map<std::string,GlslFxUniform*> UniMap;
	typedef std::map<std::string,GlslFxAttribute*> AttrMap;
	std::string		mName;
	UniMap			mUniforms;
	AttrMap			mAttributes;
	GlslFxAttribute* MergeAttribute( const std::string& name );
};

struct GlslFxStateBlock
{
	std::string		mName;
	SRasterState	mState;
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

	void Compile();
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

struct GlslFxPass
{
	static const int kmaxattrID = 16;
	std::string			mName;
	GlslFxShader*		mVertexProgram;
	GlslFxShader*		mFragmentProgram;
	GlslFxStateBlock*	mStateBlock;
	GLuint				mProgramObjectId;
	std::map<std::string,GlslFxUniformInstance*>	mUniformInstances;
	std::map<std::string,GlslFxAttribute*>			mAttributes;
	GlslFxAttribute*								mAttributeById[kmaxattrID];

	GlslFxPass( const std::string& name, GlslFxShader* pvs=nullptr, GlslFxShader* pfs=nullptr )
		: mName(name)
		, mVertexProgram(pvs)
		, mFragmentProgram(pfs)
		, mProgramObjectId(0)
		, mStateBlock(nullptr)
	{
		for( int i=0; i<kmaxattrID; i++ ) mAttributeById[i] = nullptr;
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
	std::map<std::string,GlslFxStreamInterface*>	mVertexInterfaces;
	std::map<std::string,GlslFxStreamInterface*>	mFragmentInterfaces;
	std::map<std::string,GlslFxStateBlock*>			mStateBlocks;
	std::map<std::string,GlslFxUniform*>			mUniforms;
	//std::map<std::string,GlslFxAttribute*>			mAttributes;
	std::map<std::string,GlslFxShader*>				mVertexPrograms;
	std::map<std::string,GlslFxShader*>				mFragmentPrograms;
	std::map<std::string,GlslFxTechnique*>			mTechniqueMap;
	const GlslFxPass*								mActivePass;
	int												mActiveNumPasses;

	bool Load( const AssetPath& filename, FxShader*pfxshader );
	void Destroy( void );
	bool IsValid( void );

	void AddConfig( GlslFxConfig* pcfg );
	void AddVertexInterface( GlslFxStreamInterface* pif );
	void AddFragmentInterface( GlslFxStreamInterface* pif );
	GlslFxUniform* MergeUniform( const std::string& name );
	//GlslFxAttribute* MergeAttribute( const std::string& name );
	void AddStateBlock( GlslFxStateBlock* pSB );
	void AddTechnique( GlslFxTechnique* ptek );
	void AddVertexProgram( GlslFxShader* psha );
	void AddFragmentProgram( GlslFxShader* psha );

	GlslFxStateBlock* GetStateBlock( const std::string& name ) const;
	GlslFxUniform* GetUniform( const std::string& name ) const;
	//GlslFxAttribute* GetAttribute( const std::string& name ) const;
	GlslFxShader* GetVertexProgram( const std::string& name ) const;
	GlslFxShader* GetFragmentProgram( const std::string& name ) const;
	GlslFxStreamInterface* GetVertexInterface( const std::string& name ) const;
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

#endif
