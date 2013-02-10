////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include "../gl.h"
#if defined(_USE_GLSLFX)
#include "glslfxi.h"
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/file/file.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/prop.h>

namespace ork { namespace lev2 {

GlslFxContainer* GenPlat2SolidFx( const AssetPath& pth );
GlslFxContainer* GenPlat2UiFx( const AssetPath& pth );

///////////////////////////////////////////////////////////////////////////////
// FX Interface
///////////////////////////////////////////////////////////////////////////////

void GfxTargetGL::FxInit()
{
	static bool binit = true;

	if( true == binit )
	{
		binit = false;
		FxShader::RegisterLoaders( "shaders/glfx/", "glfx" );
	}
}

///////////////////////////////////////////////////////////////////////////////

void GlslFxInterface::DoBeginFrame()
{
	mLastPass = 0;
}

///////////////////////////////////////////////////////////////////////////////

bool GlslFxPass::HasUniformInstance( GlslFxUniformInstance* puni ) const
{
	GlslFxUniform* pun = puni->mpUniform;
	std::map<std::string,GlslFxUniformInstance*>::const_iterator it=mUniformInstances.find( pun->mName );
	return it!=mUniformInstances.end();
}

///////////////////////////////////////////////////////////////////////////////

const GlslFxUniformInstance* GlslFxPass::GetUniformInstance( GlslFxUniform* puni ) const
{
	std::map<std::string,GlslFxUniformInstance*>::const_iterator it=mUniformInstances.find( puni->mName );
	return (it!=mUniformInstances.end()) ? it->second : nullptr;
}

///////////////////////////////////////////////////////////////////////////////

GlslFxInterface::GlslFxInterface( GfxTargetGL& glctx )
	: mTarget( glctx )
{
}

///////////////////////////////////////////////////////////////////////////////

bool GlslFxInterface::LoadFxShader( const AssetPath& pth, FxShader*pfxshader )
{
	printf( "GLSLFXI LoadShader<%s>\n", pth.c_str() );
	GL_ERRORCHECK();
	bool bok = false;
	pfxshader->SetInternalHandle(0);

	GlslFxContainer* pcontainer = LoadFxFromFile( pth );
	OrkAssert(pcontainer!=nullptr);
	pfxshader->SetInternalHandle( (void*) pcontainer );
	bok = pcontainer->IsValid();

	if( bok )
	{
		BindContainerToAbstract( pcontainer, pfxshader );
	}
	GL_ERRORCHECK();

    return bok;
}

///////////////////////////////////////////////////////////////////////////////

void GlslFxInterface::BindContainerToAbstract(GlslFxContainer* pcont, FxShader* fxh )
{
	for( const auto& ittek : pcont->mTechniqueMap )
	{
		GlslFxTechnique* ptek = ittek.second;
		FxShaderTechnique* ork_tek = new FxShaderTechnique( (void*) ptek );
		ork_tek->mTechniqueName = ittek.first;
		//pabstek->mPasses = ittek->first;
		ork_tek->mbValidated = true;
		fxh->AddTechnique( ork_tek );
	}
	for( const auto& itp : pcont->mUniforms )
	{
		GlslFxUniform* puni = itp.second;
		FxShaderParam* ork_parm = new FxShaderParam;
		ork_parm->mParameterName = itp.first;
		ork_parm->mParameterSemantic = puni->mSemantic;
		ork_parm->mInternalHandle = (void*)puni;
		fxh->AddParameter( ork_parm );
	}

}

///////////////////////////////////////////////////////////////////////////////

void GlslFxContainer::AddConfig( GlslFxConfig* pcfg )
{
	mConfigs[ pcfg->mName ] = pcfg;
}

///////////////////////////////////////////////////////////////////////////////

void GlslFxContainer::AddVertexInterface( GlslFxStreamInterface* pif )
{
	mVertexInterfaces[ pif->mName ] = pif;
}

///////////////////////////////////////////////////////////////////////////////

void GlslFxContainer::AddFragmentInterface( GlslFxStreamInterface* pif )
{
	mFragmentInterfaces[ pif->mName ] = pif;
}

///////////////////////////////////////////////////////////////////////////////

void GlslFxContainer::AddStateBlock( GlslFxStateBlock* psb )
{
	mStateBlocks[ psb->mName ] = psb;
}

///////////////////////////////////////////////////////////////////////////////

void GlslFxContainer::AddTechnique( GlslFxTechnique* ptek )
{
	mTechniqueMap[ ptek->mName ] = ptek;
}

///////////////////////////////////////////////////////////////////////////////

void GlslFxContainer::AddVertexProgram( GlslFxShader* psha )
{
	mVertexPrograms[ psha->mName ] = psha;
}

///////////////////////////////////////////////////////////////////////////////

void GlslFxContainer::AddFragmentProgram( GlslFxShader* psha )
{
	mFragmentPrograms[ psha->mName ] = psha;
}

///////////////////////////////////////////////////////////////////////////////

GlslFxStateBlock* GlslFxContainer::GetStateBlock( const std::string& name ) const
{
	const auto& it = mStateBlocks.find(name);
	return (it==mStateBlocks.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

GlslFxShader* GlslFxContainer::GetVertexProgram( const std::string& name ) const
{
	const auto& it = mVertexPrograms.find(name);
	return (it==mVertexPrograms.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

GlslFxShader* GlslFxContainer::GetFragmentProgram( const std::string& name ) const
{
	const auto& it = mFragmentPrograms.find(name);
	return (it==mFragmentPrograms.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

GlslFxStreamInterface* GlslFxContainer::GetVertexInterface( const std::string& name ) const
{
	const auto& it = mVertexInterfaces.find(name);
	return (it==mVertexInterfaces.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

GlslFxStreamInterface* GlslFxContainer::GetFragmentInterface( const std::string& name ) const
{
	const auto& it = mFragmentInterfaces.find(name);
	return (it==mFragmentInterfaces.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

GlslFxUniform* GlslFxContainer::GetUniform( const std::string& name ) const
{
	const auto& it = mUniforms.find(name);
	return (it==mUniforms.end()) ? nullptr : it->second;
}
//GlslFxAttribute* GlslFxContainer::GetAttribute( const std::string& name ) const
//{
//	std::map<std::string,GlslFxAttribute*>::const_iterator it=mAttributes.find(name);
//	return (it==mAttributes.end()) ? nullptr : it->second;
//}

///////////////////////////////////////////////////////////////////////////////

GlslFxUniform* GlslFxContainer::MergeUniform( const std::string& name )
{
	GlslFxUniform* pret = nullptr;
	const auto& it = mUniforms.find(name);
	if( it==mUniforms.end() )
	{
		pret = new GlslFxUniform( name );
		mUniforms[ name ] = pret;
	}
	else
	{
		pret = it->second;
	}
	printf( "MergedUniform<%s><%p>\n", name.c_str(), pret );
	return pret;
}

///////////////////////////////////////////////////////////////////////////////
/*
GlslFxAttribute* GlslFxContainer::MergeAttribute( const std::string& name )
{
	GlslFxAttribute* pret = nullptr;
	std::map<std::string,GlslFxAttribute*>::const_iterator it=mAttributes.find(name);
	if( it==mAttributes.end() )
	{
		int iloc = int(mAttributes.size());
		pret = new GlslFxAttribute( name );
		mAttributes[ name ] = pret;
		pret->mLocation = iloc;
	}
	else
	{
		pret = it->second;
	}
	printf( "MergedAttribute<%s><%p>\n", name.c_str(), pret );
	return pret;
}
*/
///////////////////////////////////////////////////////////////////////////////

GlslFxContainer::GlslFxContainer(const std::string& nam)
	: mEffectName(nam)
	, mActiveTechnique( nullptr )
	, mActivePass( nullptr )
	, mActiveNumPasses(0)
{
	GlslFxStateBlock* pdefsb = new GlslFxStateBlock;
	pdefsb->mName = "default";
	this->AddStateBlock( pdefsb );
}

///////////////////////////////////////////////////////////////////////////////

void GlslFxContainer::Destroy( void )
{
}

///////////////////////////////////////////////////////////////////////////////

bool GlslFxContainer::IsValid( void )
{
	return true;
}

///////////////////////////////////////////////////////////////////////////////

int GlslFxInterface::BeginBlock( FxShader* hfx, const RenderContextInstData& data )
{
	mTarget.SetRenderContextInstData( & data );
	GlslFxContainer* container = static_cast<GlslFxContainer*>( hfx->GetInternalHandle() );
	mpActiveEffect = container;
	mpActiveFxShader = hfx;
	return container->mActiveTechnique->mPasses.size();
}

///////////////////////////////////////////////////////////////////////////////

void GlslFxInterface::EndBlock( FxShader* hfx )
{
	mpActiveFxShader = 0;
}

///////////////////////////////////////////////////////////////////////////////

void GlslFxInterface::CommitParams( void )
{
	if( mpActiveEffect && mpActiveEffect->mActivePass && mpActiveEffect->mActivePass->mStateBlock )
	{
		const SRasterState& rstate = mpActiveEffect->mActivePass->mStateBlock->mState;
		mTarget.RSI()->BindRasterState(rstate);
	}
	//if( (mpActiveEffect->mActivePass != mLastPass) || (mTarget.GetCurMaterial()!=mpLastFxMaterial) )
	{
		//orkprintf( "CgFxInterface::CommitParams() activepass<%p>\n", mpActiveEffect->mActivePass );
		//cgSetPassState( mpActiveEffect->mActivePass );
		//mpLastFxMaterial = mTarget.GetCurMaterial();
		//mLastPass = mpActiveEffect->mActivePass;
	}
}

///////////////////////////////////////////////////////////////////////////////

const FxShaderTechnique* GlslFxInterface::GetTechnique( FxShader* hfx, const std::string & name )
{
	//orkprintf( "Get cgtek<%s> hfx<%x>\n", name.c_str(), hfx );
	OrkAssert( hfx != 0 );
	GlslFxContainer* container = static_cast<GlslFxContainer*>( hfx->GetInternalHandle() );
	OrkAssert( container != 0 );
	/////////////////////////////////////////////////////////////
	
	const auto& tekmap =  hfx->GetTechniques();
	const auto& it = tekmap.find( name );
	const FxShaderTechnique* htek = (it!=tekmap.end()) ? it->second : 0;

	return htek;
}

///////////////////////////////////////////////////////////////////////////////

bool GlslFxInterface::BindTechnique( FxShader* hfx, const FxShaderTechnique* htek )
{
	GlslFxContainer* container = static_cast<GlslFxContainer*>( hfx->GetInternalHandle() );
	const GlslFxTechnique* ptekcont = static_cast<const GlslFxTechnique*>( htek->GetPlatformHandle() );
	container->mActiveTechnique = ptekcont;
	container->mActivePass = 0;

	//orkprintf( "binding cgtek<%s:%x>\n", ptekcont->mName.c_str(), ptekcont );

	return (ptekcont->mPasses.size()>0);
}

///////////////////////////////////////////////////////////////////////////////

void GlslFxShader::Compile()
{
	mShaderObjectId = glCreateShader( mShaderType );
	
	std::string shadertext = mShaderText;
	shadertext += "void main() { ";
	shadertext += mName;
	shadertext += "(); }\n";
	const char* c_str = shadertext.c_str();

	GL_ERRORCHECK();
	glShaderSource( mShaderObjectId, 1, & c_str, NULL );
	GL_ERRORCHECK();
	glCompileShader( mShaderObjectId );
	GL_ERRORCHECK();

	GLint compiledOk = 0;
	glGetShaderiv( mShaderObjectId, GL_COMPILE_STATUS, & compiledOk );
	if( GL_FALSE==compiledOk )
	{
		char infoLog[ 1 << 16 ];
		glGetShaderInfoLog( mShaderObjectId, sizeof(infoLog), NULL, infoLog );
		printf( "//////////////////////////////////\n" );
		printf( "Effect<%s>\n", mpContainer->mEffectName.c_str() );
		printf( "Shader<%s> InfoLog<%s>\n", mName.c_str(), infoLog );
		printf( "//////////////////////////////////\n" );
		printf( "%s\n", c_str );
		printf( "//////////////////////////////////\n" );
		OrkAssert(false);
		mbError = true;
		return;
	}
	mbCompiled = true;
	GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

bool GlslFxShader::IsCompiled() const
{
	return mbCompiled;
}

///////////////////////////////////////////////////////////////////////////////

bool GlslFxInterface::BindPass( FxShader* hfx, int ipass )
{
	GlslFxContainer* container = static_cast<GlslFxContainer*>( hfx->GetInternalHandle() );
	container->mActivePass = container->mActiveTechnique->mPasses[ipass];
	GlslFxPass* ppass = const_cast<GlslFxPass*>(container->mActivePass);

	GL_ERRORCHECK();
	if( 0 == container->mActivePass->mProgramObjectId )
	{
		GlslFxShader* pvtxshader = container->mActivePass->mVertexProgram;
		GlslFxShader* pfrgshader = container->mActivePass->mFragmentProgram;

		OrkAssert( pvtxshader!=nullptr );
		OrkAssert( pfrgshader!=nullptr );

		if( pvtxshader->IsCompiled() == false )
			pvtxshader->Compile();
		if( pfrgshader->IsCompiled() == false )
			pfrgshader->Compile();
			
		if( pvtxshader->IsCompiled() && pfrgshader->IsCompiled() )
		{
				GL_ERRORCHECK();
			GLuint prgo = glCreateProgram();
			ppass->mProgramObjectId = prgo;
			glAttachShader( prgo, pvtxshader->mShaderObjectId );
				GL_ERRORCHECK();
			glAttachShader( prgo, pfrgshader->mShaderObjectId );
				GL_ERRORCHECK();
			
			GlslFxStreamInterface* vtx_iface = pvtxshader->mpInterface;
			GlslFxStreamInterface* frg_iface = pfrgshader->mpInterface;
			
			printf( "Linking vtxshader<%s> frgshader<%s>\n", pvtxshader->mName.c_str(), pfrgshader->mName.c_str() );
			if( nullptr == vtx_iface )
			{
				printf( "vtxshader<%s> has no interface!\n", pvtxshader->mName.c_str() );
				OrkAssert( false );
			}
			if( nullptr == frg_iface )
			{
				printf( "frgshader<%s> has no interface!\n", pfrgshader->mName.c_str() );
				OrkAssert( false );
			}

			for( const auto& itp : vtx_iface->mAttributes )
			{
				GlslFxAttribute* pattr = itp.second;
				int iloc = pattr->mLocation;
				printf( "vtxattr<%s> loc<%d> dir<%s>\n", pattr->mName.c_str(), iloc, pattr->mDirection.c_str() );
				glBindAttribLocation( prgo, iloc, pattr->mName.c_str() );
				GL_ERRORCHECK();
				ppass->mAttributeById[iloc] = pattr;
			}

			//////////////////////////
			// ensure vtx_iface exports what frg_iface imports
			//////////////////////////

			for( const auto& itp : frg_iface->mAttributes )
			{	const GlslFxAttribute* pfrgattr = itp.second;
				if( pfrgattr->mDirection=="in" )
				{
					int iloc = pfrgattr->mLocation;
					const std::string& name = pfrgattr->mName;
					printf( "frgattr<%s> loc<%d> dir<%s>\n", pfrgattr->mName.c_str(), iloc, pfrgattr->mDirection.c_str() );
					const auto& itf=vtx_iface->mAttributes.find(name);
					const GlslFxAttribute* pvtxattr = (itf!=vtx_iface->mAttributes.end()) ? itf->second : nullptr;
					OrkAssert( pfrgattr != nullptr );
					OrkAssert( pvtxattr != nullptr );
					OrkAssert( pvtxattr->mTypeName == pfrgattr->mTypeName );
				}
			}
			//////////////////////////

			GL_ERRORCHECK();
			glLinkProgram( prgo );
			GL_ERRORCHECK();
			GLint linkstat = 0;
			glGetProgramiv( prgo, GL_LINK_STATUS, & linkstat );
			OrkAssert( linkstat == GL_TRUE );

			//////////////////////////
			// query attr
			//////////////////////////

			GLint numattr = 0;
			glGetProgramiv( prgo, GL_ACTIVE_ATTRIBUTES, & numattr );
			GL_ERRORCHECK();

			for( int i=0; i<numattr; i++ )
			{
				GLchar nambuf[256];
				GLsizei namlen = 0;
				GLint atrsiz = 0;
				GLenum atrtyp = GL_ZERO;
				GL_ERRORCHECK();
				glGetActiveAttrib( prgo, i, sizeof(nambuf), & namlen, & atrsiz, & atrtyp, nambuf );
				OrkAssert( namlen<sizeof(nambuf) );
				GL_ERRORCHECK();
				const auto& it=vtx_iface->mAttributes.find(nambuf);
				OrkAssert( it!=vtx_iface->mAttributes.end() );
				GlslFxAttribute* pattr = it->second;
				printf( "qattr<%d> loc<%d> name<%s>\n", i, pattr->mLocation, nambuf );
				pattr->meType = atrtyp;
				//pattr->mLocation = i;

				ppass->mAttributes[pattr->mSemantic] = pattr;
			}

			//////////////////////////
			// query unis
			//////////////////////////

			GLint numunis = 0;
			GL_ERRORCHECK();
			glGetProgramiv( prgo, GL_ACTIVE_UNIFORMS, & numunis );
			GL_ERRORCHECK();

			for( int i=0; i<numunis; i++ )
			{
				GLchar nambuf[256];
				GLsizei namlen = 0;
				GLint unisiz = 0;
				GLenum unityp = GL_ZERO;
				glGetActiveUniform( prgo, i, sizeof(nambuf), & namlen, & unisiz, & unityp, nambuf );
				OrkAssert( namlen<sizeof(nambuf) );
				GL_ERRORCHECK();
				const auto& it=container->mUniforms.find(nambuf);
				OrkAssert( it!=container->mUniforms.end() );
				GlslFxUniform* puni = it->second;
				puni->meType = unityp;

				GlslFxUniformInstance* pinst = new GlslFxUniformInstance;
				pinst->mpUniform = puni;

				GLint uniloc = glGetUniformLocation( prgo, nambuf );
				pinst->mLocation = uniloc;

				const_cast<GlslFxPass*>(container->mActivePass)->mUniformInstances[puni->mName] = pinst;

			}

		}
	}

	GL_ERRORCHECK();
	glUseProgram( container->mActivePass->mProgramObjectId );
	GL_ERRORCHECK();

	return true;
}

///////////////////////////////////////////////////////////////////////////////

void GlslFxInterface::EndPass( FxShader* hfx )
{
	GlslFxContainer* container = static_cast<GlslFxContainer*>( hfx->GetInternalHandle() );
	GL_ERRORCHECK();
	glUseProgram( 0 );
	GL_ERRORCHECK();
	//cgResetPassState( container->mActivePass );
}

///////////////////////////////////////////////////////////////////////////////

const FxShaderParam* GlslFxInterface::GetParameterH( FxShader* hfx, const std::string & name )
{
	OrkAssert( 0!=hfx );
	const auto& parammap =  hfx->GetParametersByName();
	const auto& it = parammap.find( name );
	const FxShaderParam* hparam = (it!=parammap.end()) ? it->second : 0;
	return hparam;
}

///////////////////////////////////////////////////////////////////////////////

void GlslFxInterface::BindParamBool( FxShader* hfx, const FxShaderParam* hpar, const bool bv )
{

}

///////////////////////////////////////////////////////////////////////////////

void GlslFxInterface::BindParamInt( FxShader* hfx, const FxShaderParam* hpar, const int iv )
{
/*
	if( 0 == hpar ) return; 
	CgFxContainer* container = static_cast<CgFxContainer*>( hfx->GetInternalHandle() );
	CGeffect cgeffect = container->mCgEffect;
	CGparameter cgparam = reinterpret_cast<CGparameter>(hpar->GetPlatformHandle());
   	cgSetParameter1i( cgparam, iv );
	GL_ERRORCHECK();
*/
}

///////////////////////////////////////////////////////////////////////////////

void GlslFxInterface::BindParamVect2( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec )
{
	GlslFxContainer* container = static_cast<GlslFxContainer*>( hfx->GetInternalHandle() );
	GlslFxUniform* puni = static_cast<GlslFxUniform*>( hpar->GetPlatformHandle() );
/*	int iloc = puni->mLocation;
	const char* psem = puni->mSemantic.c_str();
	const char* pnam = puni->mName.c_str();
	GLenum etyp = puni->meType;
	OrkAssert( etyp == 	GL_FLOAT_VEC2 );

	glUniform2fv( iloc, 1, Vec.GetArray() );
	GL_ERRORCHECK();*/
}

void GlslFxInterface::BindParamVect3( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec )
{
	GlslFxContainer* container = static_cast<GlslFxContainer*>( hfx->GetInternalHandle() );
	GlslFxUniform* puni = static_cast<GlslFxUniform*>( hpar->GetPlatformHandle() );
/*	int iloc = puni->mLocation;
	const char* psem = puni->mSemantic.c_str();
	const char* pnam = puni->mName.c_str();
	GLenum etyp = puni->meType;
	OrkAssert( etyp == 	GL_FLOAT_VEC3 );

	glUniform3fv( iloc, 1, Vec.GetArray() );
	GL_ERRORCHECK();*/
}

void GlslFxInterface::BindParamVect4( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec )
{
	GlslFxContainer* container = static_cast<GlslFxContainer*>( hfx->GetInternalHandle() );
	GlslFxUniform* puni = static_cast<GlslFxUniform*>( hpar->GetPlatformHandle() );
	const GlslFxUniformInstance* pinst = container->mActivePass->GetUniformInstance(puni);
	if( pinst )
	{
		int iloc = pinst->mLocation;
		if( iloc>=0 )
		{
			const char* psem = puni->mSemantic.c_str();
			const char* pnam = puni->mName.c_str();
			GLenum etyp = puni->meType;
			OrkAssert( etyp == 	GL_FLOAT_VEC4 );
	
			GL_ERRORCHECK();
			glUniform4fv( iloc, 1, Vec.GetArray() );
			GL_ERRORCHECK();
		}
	}
}

void GlslFxInterface::BindParamVect4Array( FxShader* hfx, const FxShaderParam* hpar, const CVector4 * Vec, const int icount )
{
	/*GlslFxContainer* container = static_cast<GlslFxContainer*>( hfx->GetInternalHandle() );
	GlslFxUniform* puni = static_cast<GlslFxUniform*>( hpar->GetPlatformHandle() );
	int iloc = puni->mLocation;
	if( iloc>=0 )
	{
		const char* psem = puni->mSemantic.c_str();
		const char* pnam = puni->mName.c_str();
		GLenum etyp = puni->meType;
		OrkAssert( etyp == 	GL_FLOAT_VEC4 );

		glUniform4fv( iloc, icount, (float*) Vec );
		GL_ERRORCHECK();
	}*/
}

void GlslFxInterface::BindParamFloat( FxShader* hfx, const FxShaderParam* hpar, float fA )
{
	/*
	GlslFxContainer* container = static_cast<GlslFxContainer*>( hfx->GetInternalHandle() );
	GlslFxUniform* puni = static_cast<GlslFxUniform*>( hpar->GetPlatformHandle() );
	int iloc = puni->mLocation;
	if( iloc>=0 )
	{
		const char* psem = puni->mSemantic.c_str();
		const char* pnam = puni->mName.c_str();
		GLenum etyp = puni->meType;
		OrkAssert( etyp == GL_FLOAT );

		glUniform1f( iloc, fA );
		GL_ERRORCHECK();
	}*/
}
void GlslFxInterface::BindParamFloatArray( FxShader* hfx, const FxShaderParam* hpar, const float *pfa, const int icount )
{
	/*GlslFxContainer* container = static_cast<GlslFxContainer*>( hfx->GetInternalHandle() );
	GlslFxUniform* puni = static_cast<GlslFxUniform*>( hpar->GetPlatformHandle() );
	int iloc = puni->mLocation;
	if( iloc>=0 )
	{
		const char* psem = puni->mSemantic.c_str();
		const char* pnam = puni->mName.c_str();
		GLenum etyp = puni->meType;
		OrkAssert( etyp == GL_FLOAT );

		glUniform1fv( iloc, icount, pfa );
		GL_ERRORCHECK();
	}*/
}

void GlslFxInterface::BindParamFloat2( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB )
{
	/*GlslFxContainer* container = static_cast<GlslFxContainer*>( hfx->GetInternalHandle() );
	GlslFxUniform* puni = static_cast<GlslFxUniform*>( hpar->GetPlatformHandle() );
	int iloc = puni->mLocation;
	if( iloc>=0 )
	{
		const char* psem = puni->mSemantic.c_str();
		const char* pnam = puni->mName.c_str();
		GLenum etyp = puni->meType;
		OrkAssert( etyp == 	GL_FLOAT_VEC2 );

		CVector2 v2( fA, fB );

		glUniform2fv( iloc, 1, v2.GetArray() );
		GL_ERRORCHECK();
	}*/
}

void GlslFxInterface::BindParamFloat3( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC )
{
	/*GlslFxContainer* container = static_cast<GlslFxContainer*>( hfx->GetInternalHandle() );
	GlslFxUniform* puni = static_cast<GlslFxUniform*>( hpar->GetPlatformHandle() );
	int iloc = puni->mLocation;
	if( iloc>=0 )
	{
		const char* psem = puni->mSemantic.c_str();
		const char* pnam = puni->mName.c_str();
		GLenum etyp = puni->meType;
		OrkAssert( etyp == 	GL_FLOAT_VEC3 );

		CVector3 v3( fA, fB, fC );

		glUniform3fv( iloc, 1, v3.GetArray() );
		GL_ERRORCHECK();
	}*/
}

void GlslFxInterface::BindParamFloat4( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC, float fD )
{
	/*GlslFxContainer* container = static_cast<GlslFxContainer*>( hfx->GetInternalHandle() );
	GlslFxUniform* puni = static_cast<GlslFxUniform*>( hpar->GetPlatformHandle() );
	int iloc = puni->mLocation;
	if( iloc>=0 )
	{
		const char* psem = puni->mSemantic.c_str();
		const char* pnam = puni->mName.c_str();
		GLenum etyp = puni->meType;
		OrkAssert( etyp == 	GL_FLOAT_VEC4 );

		CVector4 v4( fA, fB, fC, fD );

		glUniform4fv( iloc, 1, v4.GetArray() );
		GL_ERRORCHECK();
	}*/
}

void GlslFxInterface::BindParamU32( FxShader* hfx, const FxShaderParam* hpar, U32 uval )
{
/*
	CgFxContainer* container = static_cast<CgFxContainer*>( hfx->GetInternalHandle() );
	CGeffect cgeffect = container->mCgEffect;
	CGparameter cgparam = reinterpret_cast<CGparameter>(hpar->GetPlatformHandle());
	GL_ERRORCHECK();
*/
}

void GlslFxInterface::BindParamMatrix( FxShader* hfx, const FxShaderParam* hpar, const CMatrix4 & Mat )
{
	GlslFxContainer* container = static_cast<GlslFxContainer*>( hfx->GetInternalHandle() );
	GlslFxUniform* puni = static_cast<GlslFxUniform*>( hpar->GetPlatformHandle() );
	const GlslFxUniformInstance* pinst = container->mActivePass->GetUniformInstance(puni);
	if( pinst )
	{
		int iloc = pinst->mLocation;
		if( iloc>=0 )
		{
			const char* psem = puni->mSemantic.c_str();
			const char* pnam = puni->mName.c_str();
			GLenum etyp = puni->meType;
			OrkAssert( etyp == GL_FLOAT_MAT4 );

			GL_ERRORCHECK();
			glUniformMatrix4fv( iloc, 1, GL_FALSE, Mat.GetArray() );
			GL_ERRORCHECK();
		}
	}
}

void GlslFxInterface::BindParamMatrix( FxShader* hfx, const FxShaderParam* hpar, const CMatrix3 & Mat )
{
	/*GlslFxContainer* container = static_cast<GlslFxContainer*>( hfx->GetInternalHandle() );
	GlslFxUniform* puni = static_cast<GlslFxUniform*>( hpar->GetPlatformHandle() );
	int iloc = puni->mLocation;
	if( iloc>=0 )
	{
		const char* psem = puni->mSemantic.c_str();
		const char* pnam = puni->mName.c_str();
		GLenum etyp = puni->meType;
		OrkAssert( etyp == GL_FLOAT_MAT3 );

		glUniformMatrix3fv( iloc, 1, GL_FALSE, Mat.GetArray() );
		GL_ERRORCHECK();
	}*/
}

void GlslFxInterface::BindParamMatrixArray( FxShader* hfx, const FxShaderParam* hpar, const CMatrix4 * Mat, int iCount )
{
	/*GlslFxContainer* container = static_cast<GlslFxContainer*>( hfx->GetInternalHandle() );
	GlslFxUniform* puni = static_cast<GlslFxUniform*>( hpar->GetPlatformHandle() );
	int iloc = puni->mLocation;
	if( iloc>=0 )
	{
		const char* psem = puni->mSemantic.c_str();
		const char* pnam = puni->mName.c_str();
		GLenum etyp = puni->meType;
		OrkAssert( etyp == GL_FLOAT_MAT4 );

		glUniformMatrix4fv( iloc, iCount, GL_FALSE, (float*) Mat );
		GL_ERRORCHECK();
	}*/
}

///////////////////////////////////////////////////////////////////////////////

void GlslFxInterface::BindParamCTex( FxShader* hfx, const FxShaderParam* hpar, const Texture *pTex )
{
	GlslFxContainer* container = static_cast<GlslFxContainer*>( hfx->GetInternalHandle() );
	GlslFxUniform* puni = static_cast<GlslFxUniform*>( hpar->GetPlatformHandle() );
	const GlslFxUniformInstance* pinst = container->mActivePass->GetUniformInstance(puni);
	if( pinst )
	{
		int iloc = pinst->mLocation;
		if( iloc>=0 )
		{
			const char* psem = puni->mSemantic.c_str();
			const char* pnam = puni->mName.c_str();
			GLenum etyp = puni->meType;
			//OrkAssert( etyp == GL_FLOAT_MAT4 );
			
			if( pTex!=0 )
			{
				const GLTextureObject* pTEXOBJ = (GLTextureObject*) pTex->GetTexIH();
				GLuint texID = pTEXOBJ ? pTEXOBJ->mObject : 0;
				
				GL_ERRORCHECK();
				glActiveTexture( GL_TEXTURE0 );
				GL_ERRORCHECK();
				glBindTexture( GL_TEXTURE_2D, texID );
				GL_ERRORCHECK();
				//glEnable( GL_TEXTURE_2D );
				//GL_ERRORCHECK();
				glUniform1i( iloc, 0 );
				GL_ERRORCHECK();

			}
		}
	}
/*
	if( 0 == hpar ) return; 
	CgFxContainer* container = static_cast<CgFxContainer*>( hfx->GetInternalHandle() );
	CGeffect cgeffect = container->mCgEffect;
	CGparameter cgparam = reinterpret_cast<CGparameter>(hpar->GetPlatformHandle());
	if( (pTex!=0) && (cgparam!=0) )
	{
		const GLTextureObject* pTEXOBJ = (GLTextureObject*) pTex->GetTexIH();
		//orkprintf( "BINDTEX param<%p:%s> pTEX<%p> pTEXOBJ<%p> obj<%d>\n", hpar, hpar->mParameterName.c_str(), pTex, pTEXOBJ, pTEXOBJ->mObject );
		cgGLSetTextureParameter( cgparam, pTEXOBJ ? pTEXOBJ->mObject : 0 );
	}
	else
	{
		cgGLSetTextureParameter( cgparam, 0 );
	}
	GL_ERRORCHECK();
*/
}

} }
#endif
