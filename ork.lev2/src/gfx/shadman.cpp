////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/shadman.h>

extern ork::ETocMode getocmod;

namespace ork {
namespace lev2 {

extern bool gearlyhack;

FxShader::FxShader()
	: mInternalHandle( 0 )
	, mAllowCompileFailure(false)
	, mFailedCompile(false)
{
}
///////////////////////////////////////////////////////////////////////////////

FxParamRec::FxParamRec()
	: mParameterName("")
	, mParameterSemantic("")
	, meParameterType(ork::EPROPTYPE_END)
	, mParameterHandle(0)
	, meBindingScope( ESCOPE_PEROBJECT )
	, mTargetHash( 0xffffffff )
{
}

///////////////////////////////////////////////////////////////////////////////

FxShaderPass::FxShaderPass( void *ih )
	: mInternalHandle( ih )
	, mbRestorePass(false)
{

}

FxShaderTechnique::FxShaderTechnique( void *ih )
	: mInternalHandle( ih )
	, mbValidated( false )
{

}

FxShaderParam::FxShaderParam( void *ih )
	: mInternalHandle( ih )
	, mChildParam(0)
	, meParamType( EPROPTYPE_END )
	, mBindable(true)
{

}

///////////////////////////////////////////////////////////////////////////////

void FxShader::AddTechnique( const FxShaderTechnique * tek )
{
	mTechniques[ tek->mTechniqueName ] = tek;
}

void FxShader::AddParameter( const FxShaderParam * param )
{
	mParameterByName[ param->mParameterName ] = param;
	
	/*if( param->mParameterSemantic.length() )
	{
		mParameterBySemantic[ param->mParameterSemantic ] = param;
	}*/
}

///////////////////////////////////////////////////////////////////////////////

static SFileDevContext gShaderFileContext1;
static SFileDevContext gShaderFileContext2;

void FxShader::RegisterLoaders( const file::Path::NameType & base, const file::Path::NameType & ext )
{
	const SFileDevContext& MorkCtx = CFileEnv::UrlBaseToContext( "lev2" );
	gShaderFileContext1.SetFilesystemBaseAbs( MorkCtx.GetFilesystemBaseRel()+"/"+base );
	gShaderFileContext1.SetFilesystemBaseEnable( true );
	file::Path::NameType fsbase1 = gShaderFileContext1.GetFilesystemBaseAbs();
	
	const SFileDevContext& DataCtx = CFileEnv::UrlBaseToContext( "data" );
	gShaderFileContext2.SetFilesystemBaseAbs( DataCtx.GetFilesystemBaseRel()+"/"+base );
	gShaderFileContext2.SetFilesystemBaseEnable( true );
	file::Path::NameType fsbase2 = gShaderFileContext2.GetFilesystemBaseAbs();

	CFileEnv::RegisterUrlBase( "orkshader://", gShaderFileContext1 );
	CFileEnv::RegisterUrlBase( "prjshader://", gShaderFileContext2 );
	printf( "FxShader::RegisterLoaders ext<%s> base<miniorkshader:> pth<%s>\n", ext.c_str(), fsbase1.c_str() );
	printf( "FxShader::RegisterLoaders ext<%s> base<gameshader:> pth<%s>\n", ext.c_str(), fsbase2.c_str() );
	gearlyhack = false;
}

///////////////////////////////////////////////////////////////////////////////

void FxShader::OnReset()
{
	GfxTarget* pTARG = GfxEnv::GetRef().GetLoaderTarget();

	for( orkmap<std::string,const FxShaderParam*>::const_iterator it=mParameterByName.begin(); it!=mParameterByName.end(); it++ )
	{
		const FxShaderParam* param = it->second;
		const std::string& type = param->mParameterType;
		if( param->mParameterType == "sampler" || param->mParameterType == "texture" )
		{
			pTARG->FXI()->BindParamCTex( this, param, 0 );
		}

	}
	//mTechniques.clear();
	//mParameterByName.clear();
	//mParameterBySemantic.clear();
}

///////////////////////////////////////////////////////////////////////////////

FxShaderParam* FxShader::FindParamByName( const std::string& named )
{
	orkmap<std::string,const FxShaderParam*>::iterator it=mParameterByName.find(named);
	return const_cast<FxShaderParam*>((it!=mParameterByName.end()) ? it->second : 0);
}

///////////////////////////////////////////////////////////////////////////////

FxShaderTechnique* FxShader::FindTechniqueByName( const std::string& named )
{
	orkmap<std::string,const FxShaderTechnique*>::iterator it=mTechniques.find(named);
	return const_cast<FxShaderTechnique*>((it!=mTechniques.end()) ? it->second : 0);
}

void FxShader::SetName(const char *name)
{
	mName = name;
}

const char *FxShader::GetName()
{
	return mName.c_str();
}
///////////////////////////////////////////////////////////////////////////////

}
}
