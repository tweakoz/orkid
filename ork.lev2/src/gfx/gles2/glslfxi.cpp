////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include "gl.h"
#if defined( ORK_CONFIG_OPENGL ) && defined(_USE_GLSLFX)
#include "glslfxi.h"
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/file/file.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/prop.h>

namespace ork { namespace lev2 {

namespace FakeLoadGlslFx {
void FakeLoadUI( FxShader* pfxshader );
void FakeLoadSolid( FxShader* pfxshader );
}


///////////////////////////////////////////////////////////////////////////////
// FX Interface

void GfxTargetGL::FxInit()
{
	static bool binit = true;

	if( true == binit )
	{
		binit = false;
		FxShader::RegisterLoaders( "shaders/glfx/", "glfx" );
	}
}

void GlslFxInterface::DoBeginFrame()
{
	mLastPass = 0;
}

GlslFxInterface::GlslFxInterface( GfxTargetGL& glctx )
	: mTarget( glctx )
{
	///////////////////////////////////////////////////////
	// Post Init

	static bool b1timeonly = true;

	if( b1timeonly )
	{
		GfxTargetGL::GLinit();
	
		b1timeonly = false;

		//mCgContext = cgCreateContext();
		//cgGLRegisterStates( mCgContext );

		//orkprintf( "mCgContext<%x>\n", mCgContext );
		
		//typedef void (*CGerrorCallbackFunc)(void);

		/*struct CGErrorHandler
		{
			static void doit( void )
			{
				CGerror error;
				const char * errstr = cgGetLastErrorString(&error);
				if( error != CG_NO_ERROR )
				{
					const char *errlist = cgGetLastListing( mCgContext );
					orkerrorlog( "ERROR: CGErrorHandler <error %s> <errlist %s>\n", errstr, errlist );
					OrkAssert(false);
				}
			}
		};*/

		//cgSetErrorCallback( CGErrorHandler::doit );

		//cgGLSetOptimalOptions(CG_PROFILE_VP40);
		//cgGLSetOptimalOptions(CG_PROFILE_FP40);
		//cgGLSetOptimalOptions(CG_PROFILE_VP20);
		//cgGLSetOptimalOptions(CG_PROFILE_FP20);
		//cgGLSetManageTextureParameters(mCgContext, CG_TRUE);
		//cgSetAutoCompile( mCgContext, CG_COMPILE_IMMEDIATE );
	}
}

bool GlslFxInterface::LoadFxShader( const AssetPath& pth, FxShader*pfxshader )
{
    orkprintf( "pth<%s>\n", pth.c_str() );
    if( 0 == strcmp( pth.c_str(), "miniorkshader://ui.glfx" ) )
    {
        FakeLoadGlslFx::FakeLoadUI( pfxshader );
        return true;
    }
    else if( 0 == strcmp( pth.c_str(), "miniorkshader://solid.glfx" ) )
    {
        FakeLoadGlslFx::FakeLoadSolid( pfxshader );
        return true;
    }
    else
    {
        OrkAssert(false);
    }
	//CgFxContainer *pcont = new GlslFxContainer;
	//pfxshader->SetInternalHandle( (void*) pcont );

	//bool bok = pcont->Load( mCgContext, pth, pfxshader );

	//if( false == bok )
	//{
	//	pfxshader->SetInternalHandle(0);
	//}

	//return (pcont->IsValid());
    return false;
}

///////////////////////////////////////////////////////////////////////////////

GlslFxPixShader::GlslFxPixShader()
    : mShaderID(0)
{

}

void GlslFxPixShader::InitFromString( const char* pstr )
{
    mShaderCode = pstr;
    mShaderID = glCreateShader(GL_VERTEX_SHADER);

    glShaderSource(mShaderID, 1, & pstr, NULL);
    glCompileShader(mShaderID);
}


///////////////////////////////////////////////////////////////////////////////

GlslFxVtxShader::GlslFxVtxShader()
    : mShaderID(0)
{
}

void GlslFxVtxShader::InitFromString( const char* pstr )
{
    mShaderCode = pstr;
    mShaderID = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(mShaderID, 1, & pstr, NULL);
    glCompileShader(mShaderID);
}

///////////////////////////////////////////////////////////////////////////////

GlslFxPass::GlslFxPass( const char* name, GlslFxVtxShader* pVS, GlslFxPixShader* pFS, GlslFxStateBlock* pSB  )
    : mName(name)
    , mVertexShader(pVS)
    , mPixelShader(pFS)
    , mStateBlock(pSB)
{
}

///////////////////////////////////////////////////////////////////////////////

GlslFxPass* GlslFxTechnique::NewPass( const char* name, GlslFxVtxShader* pVS, GlslFxPixShader* pFS, GlslFxStateBlock* pSB )
{
    GlslFxPass* pass = new GlslFxPass(name,pVS,pFS,pSB);
    mPasses.push_back( pass );
}

///////////////////////////////////////////////////////////////////////////////

GlslFxContainer::GlslFxContainer()
//	: mCgEffect( 0 )
{

}
void GlslFxContainer::Destroy( void )
{
	//if( mCgEffect )
	//{
		//cgDestroyEffect( mCgEffect );
	//}
}

bool GlslFxContainer::IsValid( void )
{
	return true;
}

///////////////////////////////////////////////////////////////////////////////

/*bool GlslFxContainer::Load( CGcontext ctx, const AssetPath& filename, FxShader*pfxshader )
{
	CgFxContainer *pcontainer = static_cast<CgFxContainer*>( pfxshader->GetInternalHandle() );

	std::string path( filename.ToAbsolute().c_str() );

	//OrkAssert( false ); // FIX LINE BELOW
	//path; // = CFileEnv::filespec_to_unix( path );

	std::string incp = path;

	std::string::size_type ilastslash = incp.find_last_of( "/" );

	if( ilastslash != std::string::npos )
	{
		incp = incp.substr( 0, ilastslash );
	}

	std::string incpath = CreateFormattedString( "-I%s", incp.c_str() );

	//orkprintf( "LoadCGShader %s : path %s\n", filename.c_str(), path.c_str() );


	const char* argv[2] = { incpath.c_str(), 0 };

	CGeffect effect = cgCreateEffectFromFile(	ctx,
												path.c_str(),
												argv );
	const char *errstr = 0;

	if( 0 == effect )
	{
		OrkAssert( false );
	}

	mCgEffect = effect;
	mEffectName = std::string(filename.c_str());

    /////////////////////////////////////
	// Dynamic Effect Binding Support

	CGparameter param = cgGetFirstEffectParameter( effect );

	while( param )
	{
		std::string ParamName( cgGetParameterName( param ) );
		std::string ParamSemantic( cgGetParameterSemantic( param ) );

		//printf( "ParamName<%s> ParamSem<%s>\n", ParamName.c_str(), ParamSemantic.c_str() );

		FxShaderParam* ork_parm = new FxShaderParam;
		ork_parm->mParameterName = ParamName;
		ork_parm->mParameterSemantic = ParamSemantic;
		ork_parm->mInternalHandle = (void*)(param);


		CGtype ParamType = cgGetParameterType( param );
		CGparameterclass ParamClass = cgGetParameterClass( param );

		switch( ParamClass )
		{
			case CG_PARAMETERCLASS_SCALAR:
			{
				if( ParamType == CG_FLOAT )
				{
					ork_parm->mParameterType = CPropType<CReal>::GetTypeName();
				}
				break;
			}
			case CG_PARAMETERCLASS_VECTOR:
			{
				if( ParamType == CG_FLOAT )
				{
					int icols = cgGetParameterColumns( param );

					switch( icols )
					{
						case 2:
							ork_parm->mParameterType = CPropType<CVector2>::GetTypeName();
							break;
						case 3:
							ork_parm->mParameterType = CPropType<CVector3>::GetTypeName();
							break;
						case 4:
							ork_parm->mParameterType = CPropType<CVector4>::GetTypeName();
							break;
					}
				}
				break;
			}
		}

		pfxshader->AddParameter( ork_parm );

		param = cgGetNextParameter( param );
	}

	CGtechnique cgtek = cgGetFirstTechnique( effect );

	while( cgtek )
	{
		std::string tekname( cgGetTechniqueName(cgtek) );

		CgFxTechnique *ptekcont = new CgFxTechnique;
		ptekcont->mCgTek = cgtek;
		ptekcont->mName = tekname;
		
		CGbool TekOk = cgValidateTechnique( cgtek );

		const char *errstr = cgGetLastListing( ctx );
		if( CG_FALSE == TekOk )
		{
			orkprintf( "CgFx Error\nTechnique %s\nError %s\n", tekname.c_str(), errstr );
			OrkAssert( false );
		}
		
		CGpass pass = cgGetFirstPass(cgtek);
		while( pass )
		{
			ptekcont->mPasses.push_back(pass);
			pass = cgGetNextPass(pass);
		}

		FxShaderTechnique* ork_tek = new FxShaderTechnique;
		ork_tek->mTechniqueName = tekname;
		ork_tek->mInternalHandle = (void*)(ptekcont);

		pfxshader->AddTechnique( ork_tek );

		cgtek = cgGetNextTechnique( cgtek );
	}

    /////////////////////////////////////

	return (0!=effect);
}*/

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
	OrkAssert( hfx != 0 );
	GlslFxContainer* container = static_cast<GlslFxContainer*>( hfx->GetInternalHandle() );
	OrkAssert( container != 0 );

	/////////////////////////////////////////////////////////////
	
	const orkmap<std::string,const FxShaderTechnique*>& tekmap =  hfx->GetTechniques();
	const orkmap<std::string,const FxShaderTechnique*>::const_iterator it = tekmap.find( name );
	const FxShaderTechnique* htek = (it!=tekmap.end()) ? it->second : 0;

	//orkprintf( "Get cgtek<%s:%x>\n", name.c_str(), htek );

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
//    return false;
}

///////////////////////////////////////////////////////////////////////////////

bool GlslFxInterface::BindPass( FxShader* hfx, int ipass )
{
	GlslFxContainer* container = static_cast<GlslFxContainer*>( hfx->GetInternalHandle() );
	container->mActivePass = container->mActiveTechnique->mPasses[ipass];
	return true;
}

///////////////////////////////////////////////////////////////////////////////

void GlslFxInterface::EndPass( FxShader* hfx )
{
	GlslFxContainer* container = static_cast<GlslFxContainer*>( hfx->GetInternalHandle() );
	//cgResetPassState( container->mActivePass );
}

///////////////////////////////////////////////////////////////////////////////

const FxShaderParam* GlslFxInterface::GetParameterH( FxShader* hfx, const std::string & name )
{
/*
	OrkAssert( 0!=hfx );
	const orkmap<std::string,const FxShaderParam*>& parammap =  hfx->GetParametersByName();
	const orkmap<std::string,const FxShaderParam*>::const_iterator it = parammap.find( name );
	const FxShaderParam* hparam = (it!=parammap.end()) ? it->second : 0;
	return hparam;
*/
    return nullptr;
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
/*
	if( 0 == hpar ) return; 
	CgFxContainer* container = static_cast<CgFxContainer*>( hfx->GetInternalHandle() );
	CGeffect cgeffect = container->mCgEffect;
	CGparameter cgparam = reinterpret_cast<CGparameter>(hpar->GetPlatformHandle());
   	cgSetParameter2fv( cgparam, (const float *) Vec.GetArray() );
	GL_ERRORCHECK();
*/
}

void GlslFxInterface::BindParamVect3( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec )
{
/*
	if( 0 == hpar ) return; 
	CgFxContainer* container = static_cast<CgFxContainer*>( hfx->GetInternalHandle() );
	CGeffect cgeffect = container->mCgEffect;
	CGparameter cgparam = reinterpret_cast<CGparameter>(hpar->GetPlatformHandle());
	cgSetParameter3fv( cgparam, (const float *) Vec.GetArray() );
	GL_ERRORCHECK();
*/
}

void GlslFxInterface::BindParamVect4( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec )
{
/*
	CgFxContainer* container = static_cast<CgFxContainer*>( hfx->GetInternalHandle() );
	CGeffect cgeffect = container->mCgEffect;
	CGparameter cgparam = reinterpret_cast<CGparameter>(hpar->GetPlatformHandle());

	//orkprintf( "CgFxInterface::BindParamVect4() cgparam<%p> vect<%f %f %f %f>\n", cgparam, Vec.GetX(), Vec.GetY(), Vec.GetZ(), Vec.GetW() );
	cgSetParameter4fv( cgparam, (const float *) Vec.GetArray() );
	GL_ERRORCHECK();
*/
}

void GlslFxInterface::BindParamVect4Array( FxShader* hfx, const FxShaderParam* hpar, const CVector4 * Vec, const int icount )
{
/*
	if( 0 == hpar ) return; 
	CgFxContainer* container = static_cast<CgFxContainer*>( hfx->GetInternalHandle() );
	CGeffect cgeffect = container->mCgEffect;
	CGparameter cgparam = reinterpret_cast<CGparameter>(hpar->GetPlatformHandle());
	cgGLSetParameterArray4f( cgparam, 0, icount, (const float *) Vec );
	GL_ERRORCHECK();
*/
}

void GlslFxInterface::BindParamFloat( FxShader* hfx, const FxShaderParam* hpar, float fA )
{
/*
	if( 0 == hpar ) return; 
	CgFxContainer* container = static_cast<CgFxContainer*>( hfx->GetInternalHandle() );
	CGeffect cgeffect = container->mCgEffect;
	CGparameter cgparam = reinterpret_cast<CGparameter>(hpar->GetPlatformHandle());
	cgSetParameter1f( cgparam, fA );
	GL_ERRORCHECK();
*/
}
void GlslFxInterface::BindParamFloatArray( FxShader* hfx, const FxShaderParam* hpar, const float *pfa, const int icount )
{
/*
	if( 0 == hpar ) return; 
	CgFxContainer* container = static_cast<CgFxContainer*>( hfx->GetInternalHandle() );
	CGeffect cgeffect = container->mCgEffect;
	CGparameter cgparam = reinterpret_cast<CGparameter>(hpar->GetPlatformHandle());
	GL_ERRORCHECK();
*/
}

void GlslFxInterface::BindParamFloat2( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB )
{
/*
	if( 0 == hpar ) return; 
	CgFxContainer* container = static_cast<CgFxContainer*>( hfx->GetInternalHandle() );
	CGeffect cgeffect = container->mCgEffect;
	CGparameter cgparam = reinterpret_cast<CGparameter>(hpar->GetPlatformHandle());
	cgSetParameter2f( cgparam, fA, fB );
	GL_ERRORCHECK();
*/
}

void GlslFxInterface::BindParamFloat3( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC )
{
/*
	if( 0 == hpar ) return; 
	CgFxContainer* container = static_cast<CgFxContainer*>( hfx->GetInternalHandle() );
	CGeffect cgeffect = container->mCgEffect;
	CGparameter cgparam = reinterpret_cast<CGparameter>(hpar->GetPlatformHandle());
	cgSetParameter3f( cgparam, fA, fB, fC );
	GL_ERRORCHECK();
*/
}

void GlslFxInterface::BindParamFloat4( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC, float fD )
{
/*
	CgFxContainer* container = static_cast<CgFxContainer*>( hfx->GetInternalHandle() );
	CGeffect cgeffect = container->mCgEffect;
	CGparameter cgparam = reinterpret_cast<CGparameter>(hpar->GetPlatformHandle());
	cgSetParameter4f( cgparam, fA, fB, fC, fD );
	GL_ERRORCHECK();
*/
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
/*
	CgFxContainer* container = static_cast<CgFxContainer*>( hfx->GetInternalHandle() );
	CGeffect cgeffect = container->mCgEffect;
	CGparameter cgparam = reinterpret_cast<CGparameter>(hpar->GetPlatformHandle());
	cgSetMatrixParameterfr( cgparam, (const float *) Mat.GetArray() );
	GL_ERRORCHECK();
*/
}

void GlslFxInterface::BindParamMatrix( FxShader* hfx, const FxShaderParam* hpar, const CMatrix3 & Mat )
{
/*
	if( 0 == hpar ) return; 
	CgFxContainer* container = static_cast<CgFxContainer*>( hfx->GetInternalHandle() );
	CMatrix4 mtx4;
	mtx4.SetRow(0,CVector4(Mat.GetRow(0),0.0f));
	mtx4.SetRow(1,CVector4(Mat.GetRow(1),0.0f));
	mtx4.SetRow(2,CVector4(Mat.GetRow(2),0.0f));
	CGeffect cgeffect = container->mCgEffect;
	CGparameter cgparam = reinterpret_cast<CGparameter>(hpar->GetPlatformHandle());
	cgSetMatrixParameterfr( cgparam, (const float *) mtx4.GetArray() );
	GL_ERRORCHECK();
*/
}

void GlslFxInterface::BindParamMatrixArray( FxShader* hfx, const FxShaderParam* hpar, const CMatrix4 * Mat, int iCount )
{
/*
	if( 0 == hpar ) return; 
	CgFxContainer* container = static_cast<CgFxContainer*>( hfx->GetInternalHandle() );
	CGeffect cgeffect = container->mCgEffect;
	CGparameter cgparam = reinterpret_cast<CGparameter>(hpar->GetPlatformHandle());
	GL_ERRORCHECK();
*/
}

///////////////////////////////////////////////////////////////////////////////

void GlslFxInterface::BindParamCTex( FxShader* hfx, const FxShaderParam* hpar, const Texture *pTex )
{
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
