////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/core/singleton.h>
#include <ork/kernel/prop.h>
#include <ork/lev2/gfx/gfxenv.h>

namespace ork { namespace lev2 {

struct FxShaderParam;

///////////////////////////////////////////////////////////////////////////////

class FxParamRec
{
public:

	enum EBindingScope
	{
		ESCOPE_CONSTANT = 0,
		ESCOPE_PERFRAME,
		ESCOPE_PERMATERIALINST,
		ESCOPE_PEROBJECT,
	};

	FxParamRec();

	std::string				mParameterName;
	std::string				mParameterSemantic;
	EPropType				meParameterType;

	const FxShaderParam*	mParameterHandle;
	EBindingScope			meBindingScope;
	U32						mTargetHash;
};

///////////////////////////////////////////////////////////////////////////////

struct FxShaderPass
{
	std::string				mPassName;
	void*					mInternalHandle;
	bool					mbRestorePass;

	RenderQueueSortingData	mRenderQueueSortingData;

	FxShaderPass( void *ih=0 );
	void* GetPlatformHandle( void ) const { return mInternalHandle; }
};

///////////////////////////////////////////////////////////////////////////////

struct FxShaderTechnique
{
	std::string					mTechniqueName;
	const void*					mInternalHandle;
	orkvector<FxShaderPass*>	mPasses;
	bool						mbValidated;

	FxShaderTechnique( void *ih=0 );

	const void* GetPlatformHandle( void ) const { return mInternalHandle; }
};

///////////////////////////////////////////////////////////////////////////////

struct FxShaderParam
{
	std::string						mParameterName;
	std::string						mParameterSemantic;
	std::string						mParameterType;
	EPropType						meParamType;
	void*							mInternalHandle;
	bool							mBindable;

	FxShaderParam*					mChildParam;

	orklut<std::string,std::string>	mAnnotations;
	FxShaderParam( void *ih=0 );
	void* GetPlatformHandle( void ) const { return mInternalHandle; }
};

///////////////////////////////////////////////////////////////////////////////

class FxShader
{
	void*												mInternalHandle;
	orkmap<std::string,const FxShaderTechnique*>		mTechniques;
	orkmap<std::string,const FxShaderParam*>			mParameterByName;
	//orkmap<std::string,const FxShaderParam*>			mParameterBySemantic;
	bool												mAllowCompileFailure;
	bool												mFailedCompile;
	std::string mName;

public:

	void OnReset();

	static void SetLoaderTarget(GfxTarget*targ);

	FxShader();

	static void RegisterLoaders( const file::Path::NameType & base, const file::Path::NameType & ext );

	void SetInternalHandle( void* ph ) { mInternalHandle=ph; }
	void* GetInternalHandle( void ) { return mInternalHandle; }

	static const char *GetAssetTypeNameStatic( void ) { return "fxshader"; }

	void AddTechnique( const FxShaderTechnique* tek );
	void AddParameter( const FxShaderParam* param );

	const orkmap<std::string,const FxShaderTechnique*>& GetTechniques( void ) const { return mTechniques; }
	const orkmap<std::string,const FxShaderParam*>& 	GetParametersByName( void ) const { return mParameterByName; }
	//const orkmap<std::string,const FxShaderParam*>& 	GetParametersBySemantic( void ) const { return mParameterBySemantic; }

	FxShaderParam* FindParamByName( const std::string& named );
	FxShaderTechnique* FindTechniqueByName( const std::string& named );

	void SetAllowCompileFailure( bool bv ) { mAllowCompileFailure=bv; }
	bool GetAllowCompileFailure() const { return mAllowCompileFailure; }
	void SetFailedCompile( bool bv ) { mFailedCompile=bv; }
	bool GetFailedCompile() const { return mFailedCompile; }

	void SetName(const char *);
	const char *GetName();
};

///////////////////////////////////////////////////////////////////////////////

} }

