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
	: _name("")
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

FxShaderParam* FxShaderParamBlock::param(const std::string&name) const {
  auto it = _subparams.find(name);
  return (it!=_subparams.end()) ? it->second : nullptr;
}

/*void FxShaderParamBlockMapping::setMatrix(const FxShaderParam* par, const fmtx4& m) {

}*/
FxShaderParamBufferMapping::FxShaderParamBufferMapping(){}
FxShaderParamBufferMapping::~FxShaderParamBufferMapping(){
	assert(_mappedaddr==nullptr);
}
void FxShaderParamBufferMapping::unmap(){
	_fxi->unmapParamBuffer(this);
}

///////////////////////////////////////////////////////////////////////////////

void FxShader::addTechnique( const FxShaderTechnique * tek )
{
	_techniques[ tek->mTechniqueName ] = tek;
}

void FxShader::addParameter( const FxShaderParam * param )
{
	_parameterByName[ param->_name ] = param;

}
void FxShader::addParameterBlock( const FxShaderParamBlock * block )
{
	_parameterBlockByName[ block->_name ] = block;

}
#if defined(ENABLE_COMPUTE_SHADERS)
void FxShader::addComputeShader( const FxComputeShader* csh )
{
	_computeShaderByName[ csh->_name ] = csh;
}
FxComputeShader *FxShader::findComputeShader(const std::string &named){
	auto it=_computeShaderByName.find(named);
	return const_cast<FxComputeShader*>((it!=_computeShaderByName.end()) ? it->second : nullptr);

}
#endif
#if defined(ENABLE_SHADER_STORAGE)
void FxShader::addStorageBlock(const FxShaderStorageBlock *block) {
	_storageBlockByName[ block->_name ] = block;
}
FxShaderStorageBlock *FxShader::storageBlockByName(const std::string &named){
	auto it=_storageBlockByName.find(named);
	return const_cast<FxShaderStorageBlock*>((it!=_storageBlockByName.end()) ? it->second : nullptr);

}
FxShaderStorageBufferMapping::FxShaderStorageBufferMapping() {}
FxShaderStorageBufferMapping::~FxShaderStorageBufferMapping(){
	assert(_mappedaddr==nullptr);
}
void FxShaderStorageBufferMapping::unmap(){
  _ci->unmapStorageBuffer(this);
}

#endif

///////////////////////////////////////////////////////////////////////////////

static FileDevContext gShaderFileContext1;
static FileDevContext gShaderFileContext2;

void FxShader::RegisterLoaders( const file::Path::NameType & base, const file::Path::NameType & ext )
{
	const FileDevContext& MorkCtx = FileEnv::UrlBaseToContext( "lev2" );
	gShaderFileContext1.SetFilesystemBaseAbs( MorkCtx.GetFilesystemBaseRel()+"/"+base );
	gShaderFileContext1.SetFilesystemBaseEnable( true );
	file::Path::NameType fsbase1 = gShaderFileContext1.GetFilesystemBaseAbs();

	const FileDevContext& DataCtx = FileEnv::UrlBaseToContext( "data" );
	gShaderFileContext2.SetFilesystemBaseAbs( DataCtx.GetFilesystemBaseRel()+"/"+base );
	gShaderFileContext2.SetFilesystemBaseEnable( true );
	file::Path::NameType fsbase2 = gShaderFileContext2.GetFilesystemBaseAbs();

	FileEnv::RegisterUrlBase( "orkshader://", gShaderFileContext1 );
	FileEnv::RegisterUrlBase( "prjshader://", gShaderFileContext2 );
	printf( "FxShader::RegisterLoaders ext<%s> base<miniorkshader:> pth<%s>\n", ext.c_str(), fsbase1.c_str() );
	printf( "FxShader::RegisterLoaders ext<%s> base<gameshader:> pth<%s>\n", ext.c_str(), fsbase2.c_str() );
	gearlyhack = false;
}

///////////////////////////////////////////////////////////////////////////////

void FxShader::OnReset()
{
	GfxTarget* pTARG = GfxEnv::GetRef().GetLoaderTarget();

	for( orkmap<std::string,const FxShaderParam*>::const_iterator it=_parameterByName.begin(); it!=_parameterByName.end(); it++ )
	{
		const FxShaderParam* param = it->second;
		const std::string& type = param->mParameterType;
		if( param->mParameterType == "sampler" || param->mParameterType == "texture" )
		{
			pTARG->FXI()->BindParamCTex( this, param, 0 );
		}

	}
	//_techniques.clear();
	//_parameterByName.clear();
	//mParameterBySemantic.clear();
}

///////////////////////////////////////////////////////////////////////////////

FxShaderParam* FxShader::FindParamByName( const std::string& named )
{
	orkmap<std::string,const FxShaderParam*>::iterator it=_parameterByName.find(named);
	return const_cast<FxShaderParam*>((it!=_parameterByName.end()) ? it->second : 0);
}

///////////////////////////////////////////////////////////////////////////////

FxShaderTechnique* FxShader::FindTechniqueByName( const std::string& named )
{
	orkmap<std::string,const FxShaderTechnique*>::iterator it=_techniques.find(named);
	return const_cast<FxShaderTechnique*>((it!=_techniques.end()) ? it->second : 0);
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

#if defined(ENABLE_COMPUTE_SHADER)
FxComputeShader* FxShader::findComputeShader(const std::string &named) {
    FxComputeShader* rval = nullptr;
    assert(false);
    return rval;
}
#endif

///////////////////////////////////////////////////////////////////////////////

}
}
