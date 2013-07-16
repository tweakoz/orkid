////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#if ! defined( _GFX_GL_GLSHADER_H ) && defined(_USE_CGFX)
#define _GFX_GL_GLSHADER_H

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2
{

struct CgFxPass
{
	CGpass		mPlatformPass;
	CgFxPass() : mPlatformPass(0) {}
};

struct CgFxTechnique
{
	orkvector<CgFxPass*>	mPasses;
	CGtechnique				mCgTek;
	std::string				mName;
};

struct CgFxContainer
{
	std::string								mEffectName;
	CGeffect								mCgEffect;
	const CgFxTechnique*					mActiveTechnique;
	std::map<std::string,CgFxTechnique*>	mTechniqueMap;
	CgFxPass*								mActivePass;
	int										mActiveNumPasses;

	bool Load( CGcontext ctx, const AssetPath& filename, FxShader*pfxshader );
	void Destroy( void );
	bool IsValid( void );

	CgFxContainer();
};

class GfxTargetGL;

class CgFxInterface : public FxInterface
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

	CgFxInterface( GfxTargetGL& glctx );

	CgFxContainer* GetActiveEffect() const { return mpActiveEffect; }

protected:

	static CGcontext					mCgContext;
	CgFxContainer*						mpActiveEffect;
	CgFxPass*							mLastPass;
	FxShaderTechnique*					mhCurrentTek;

	GfxTargetGL&						mTarget;

};

} }

///////////////////////////////////////////////////////////////////////////////

#endif
