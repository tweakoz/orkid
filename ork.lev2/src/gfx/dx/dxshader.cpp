////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/file/file.h>

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#if defined( ORK_CONFIG_DIRECT3D )
#include "dx.h"
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/prop.h>

#define USE_OLD_SHADERS 1

//#define PASSTHRUPARAMS return;
#define PASSTHRUPARAMS

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

orkset<FxShaderPackage*>	FxShaderPackage::gPackages;

FxShaderPackage::FxShaderPackage() : mpeffect(0)
{
	gPackages.insert(this);
}

void DxFxInterface::DoOnReset()
{
	FxShaderPackage::OnDeviceReset( mTargetDX.GetD3DDevice() );
}

void FxShaderPackage::OnDeviceReset(D3DGfxDevice d3ddev)
{
	for( orkset<FxShaderPackage*>::iterator it=gPackages.begin(); it!=gPackages.end(); it++ )
	{
		FxShaderPackage* package = (*it);
		package->mpOrkShader->OnReset();
		//package->Compile( d3ddev );
	}
}

void FxShaderPackage::Add(FxShaderPackage*package)
{
	gPackages.insert(package);
}

void FxShaderPackage::Compile(D3DGfxDevice d3ddev)
{
    /////////////////////////////////////
    // compilation parameters (prob doesnt take hold with precompiled effect files)

    const D3DXMACRO EffectMacros[] =
    {
        { "DirectX9", "1" },
        { NULL, NULL },
    };

#if defined( DXSDK_FEB2007 )
    DWORD EffectFlags = D3DXSHADER_DEBUG|D3DXSHADER_SKIPOPTIMIZATION|D3DXSHADER_USE_LEGACY_D3DX9_31_DLL; //D3DXSHADER_PACKMATRIX_COLUMNMAJOR; // D3DXSHADER_PARTIALPRECISION D3DXSHADER_PACKMATRIX_COLUMNMAJOR D3DXSHADER_PACKMATRIX_ROWMAJOR D3DXSHADER_SKIPOPTIMIZATION
#else
    DWORD EffectFlags = D3DXSHADER_OPTIMIZATION_LEVEL3; //D3DXSHADER_PACKMATRIX_COLUMNMAJOR; // D3DXSHADER_PARTIALPRECISION D3DXSHADER_PACKMATRIX_COLUMNMAJOR D3DXSHADER_PACKMATRIX_ROWMAJOR D3DXSHADER_SKIPOPTIMIZATION
    //DWORD EffectFlags = D3DXSHADER_AVOID_FLOW_CONTROL; //D3DXSHADER_SKIPOPTIMIZATION; //D3DXSHADER_SKIPOPTIMIZATION; //D3DXSHADER_PACKMATRIX_COLUMNMAJOR; // D3DXSHADER_PARTIALPRECISION D3DXSHADER_PACKMATRIX_COLUMNMAJOR D3DXSHADER_PACKMATRIX_ROWMAJOR D3DXSHADER_SKIPOPTIMIZATION
#endif
    /////////////////////////////////////

	mpeffect = 0;

    LPD3DXBUFFER D3DEffectErrors = 0; // if there are any errors, they will show up here
    //HRESULT hr = D3DXCreateEffect( pDev, pEffectData, iEffectDataLen, EffectMacros, NULL, EffectFlags, NULL, & mpFXEffect, & D3DEffectErrors );


	HRESULT hr = D3DXCreateEffectFromFile( d3ddev, mShaderPath.c_str(), NULL, NULL, EffectFlags, NULL, & mpeffect, & D3DEffectErrors );
    const char *pbuf = 0;

	if( D3DEffectErrors )
    {
        pbuf = reinterpret_cast<const char *>( D3DEffectErrors->GetBufferPointer() );
        //OutputDebugString( pbuf );

		MessageBox( 0, pbuf, "ShaderError", 0 );
	}

    /////////////////////////////////////
	// Dynamic Effect Binding Support

	OrkAssert( D3D_OK == hr );
	D3DXEFFECT_DESC EffectDescription;
	hr = mpeffect->GetDesc( & EffectDescription );
    OrkAssert( D3D_OK == hr );
	UINT inumparams = EffectDescription.Parameters;

	orkvector<FxShaderParam*> Samplers;

	for( UINT ip=0; ip<inumparams; ip++ )
	{
		D3DXHANDLE dxparam = mpeffect->GetParameter( 0, ip );
		D3DXPARAMETER_DESC paramdesc;
		hr = mpeffect->GetParameterDesc( dxparam, & paramdesc );
	    OrkAssert( D3D_OK == hr );

		std::string ParamName( paramdesc.Name ? paramdesc.Name : "" );
		std::string ParamSemantic( paramdesc.Semantic ? paramdesc.Semantic : "" );
		D3DXPARAMETER_CLASS ParamClass = paramdesc.Class;
		D3DXPARAMETER_TYPE ParamType = paramdesc.Type;

		//FxShaderParam* FindParamByName( const std::string& named );

		FxShaderParam* ork_parm = mpOrkShader->FindParamByName( ParamName );

		if( 0 == ork_parm )
		{
			ork_parm = new FxShaderParam;
			ork_parm->mParameterName = ParamName;
			ork_parm->mParameterSemantic = ParamSemantic;
			ork_parm->mParameterType = "";
			mpOrkShader->AddParameter( ork_parm );
		}

		ork_parm->mInternalHandle = (void*)(dxparam);

		switch( ParamClass )
		{
			case D3DXPC_OBJECT:
			{
				if( ParamType == D3DXPT_SAMPLER2D )
				{
					ork_parm->meParamType = EPROPTYPE_SAMPLER;
					ork_parm->mParameterType = "sampler";
					Samplers.push_back( ork_parm );
					//ork_parm->mParameterType = CPropType<CReal>::GetTypeName();
				}
				if( ParamType == D3DXPT_SAMPLER3D )
				{
					ork_parm->mParameterType = "sampler";
					Samplers.push_back( ork_parm );
				}
				if( ParamType == D3DXPT_SAMPLERCUBE )
				{
					ork_parm->mParameterType = "sampler";
					Samplers.push_back( ork_parm );
				}
				if( ParamType == D3DXPT_TEXTURE2D )
				{
					ork_parm->mParameterType = "texture";
					//ork_parm->mParameterType->meParameterType = EPROPTYPE_SAMPLER;
					//ork_parm->mParameterType = CPropType<CReal>::GetTypeName();
				}
				if( ParamType == D3DXPT_TEXTURE3D )
				{
					ork_parm->mParameterType = "texture";
					//ork_parm->mParameterType->meParameterType = EPROPTYPE_SAMPLER;
					//ork_parm->mParameterType = CPropType<CReal>::GetTypeName();
				}
				if( ParamType == D3DXPT_TEXTURECUBE )
				{
					ork_parm->mParameterType = "texture";
					//ork_parm->mParameterType->meParameterType = EPROPTYPE_SAMPLER;
					//ork_parm->mParameterType = CPropType<CReal>::GetTypeName();
				}

				break;
			}
			case D3DXPC_SCALAR:
			{
				if( ParamType == D3DXPT_FLOAT )
				{
					ork_parm->mParameterType = CPropType<CReal>::GetTypeName();
				}
				break;
			}
			case D3DXPC_VECTOR:
			{
				if( ParamType == D3DXPT_FLOAT )
				{
					int icols = paramdesc.Columns;

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
		};

		int inumanno = paramdesc.Annotations;

		for( int ia=0; ia<inumanno; ia++ )
		{
			D3DXHANDLE annoh = mpeffect->GetAnnotation( dxparam, ia );

			if( annoh )
			{
				D3DXPARAMETER_DESC AnnoDesc;
				hr = mpeffect->GetParameterDesc( annoh, & AnnoDesc );
				OrkAssert( D3D_OK == hr );
				std::string AnnoName( AnnoDesc.Name ? AnnoDesc.Name : "" );

				D3DXPARAMETER_CLASS AnnoClass = AnnoDesc.Class;
				D3DXPARAMETER_TYPE AnnoType = AnnoDesc.Type;

				switch( AnnoType )
				{
					case D3DXPT_STRING:
					{
						LPCSTR strval = 0;
						hr = mpeffect->GetString( annoh, & strval );
						std::string AnnoVal( strval ? strval : "" );
						ork_parm->mAnnotations.AddSorted(AnnoName,AnnoVal);
						//orkprintf( "Found String Annotation <name %s> <val %s>\n", AnnoName.c_str(), AnnoVal.c_str() );
						break;
					}
					default:
						break;
				}

			}
		}
	}

	size_t inumsamplers = Samplers.size();

	for( size_t isampler=0; isampler<inumsamplers; isampler++ )
	{
		FxShaderParam* psampler = Samplers[isampler];

		psampler->mBindable = false;

		std::string ProcName = psampler->mParameterName;
		std::string::size_type inamesampler = ProcName.find( "Sampler" );
		if( inamesampler != std::string::npos )
		{
			size_t ilen = ProcName.length();
			ProcName = ProcName.substr( 0, inamesampler );
			FxShaderParam* pchild = mpOrkShader->FindParamByName( ProcName );

			if( pchild )
			{
				psampler->mChildParam = pchild;
			}
		}
	}

	UINT inumtek = EffectDescription.Techniques;
	for( UINT it=0; it<inumtek; it++ )
	{
		D3DXHANDLE dxtek = mpeffect->GetTechnique( it );
		D3DXTECHNIQUE_DESC TekDesc;
		mpeffect->GetTechniqueDesc( dxtek, & TekDesc );
		std::string tekname( TekDesc.Name ? TekDesc.Name : "" );

		//FxShaderTechnique* FindTechniqueByName( const std::string& named );

		FxShaderTechnique* ork_tek = mpOrkShader->FindTechniqueByName( tekname );

		if( 0 == ork_tek )
		{
			ork_tek = new FxShaderTechnique;
			ork_tek->mTechniqueName = tekname;

			mpOrkShader->AddTechnique( ork_tek );

			int inumpasses = int( TekDesc.Passes );

			for( int ipass=0; ipass<inumpasses; ipass++ )
			{
				D3DXHANDLE hpass = mpeffect->GetPass( dxtek, ipass );

				D3DXPASS_DESC PassDesc;
				hr = mpeffect->GetPassDesc( hpass, & PassDesc );

				

				FxShaderPass* FxPass = new FxShaderPass;
				FxPass->mRenderQueueSortingData.miSortingPass = 4; // default standard
				FxPass->mInternalHandle = (void*) hpass;
				FxPass->mPassName = PassDesc.Name ? std::string(PassDesc.Name) : "noname";

				///////////////////////////
				if( FxPass->mPassName == "Restore" )
				{
					FxPass->mbRestorePass = true;
				}

				///////////////////////////
				D3DXHANDLE annoh = mpeffect->GetAnnotationByName( hpass, "miniork_rq_transparency" );

				if( annoh )
				{
					LPCSTR StrVal = 0;
					hr = mpeffect->GetString( annoh, & StrVal );
					OrkAssert( D3D_OK == hr );

					if( 0 == strcmp( "true", StrVal ) )
					{
						FxPass->mRenderQueueSortingData.mbTransparency = true;
					}
					if( 0 == strcmp( "false", StrVal ) )
					{
						FxPass->mRenderQueueSortingData.mbTransparency = false;
					}

				}
				///////////////////////////
				annoh = mpeffect->GetAnnotationByName( hpass, "miniork_rq_sortpass" );
				if( annoh )
				{
					LPCSTR StrVal = 0;
					hr = mpeffect->GetString( annoh, & StrVal );
					OrkAssert( D3D_OK == hr );

					if( 0 == strcmp( "skybox", StrVal ) )
					{
						FxPass->mRenderQueueSortingData.miSortingPass = 2;
					}
					if( 0 == strcmp( "standard", StrVal ) )
					{
						FxPass->mRenderQueueSortingData.miSortingPass = 4;
					}
					if( 0 == strcmp( "post", StrVal ) )
					{
						FxPass->mRenderQueueSortingData.miSortingPass = 6;
					}

				}
				///////////////////////////
				annoh = mpeffect->GetAnnotationByName( hpass, "miniork_rq_sortoffset" );
				if( annoh )
				{
					LPCSTR StrVal = 0;
					hr = mpeffect->GetString( annoh, & StrVal );
					OrkAssert( D3D_OK == hr );
					int val = atoi( StrVal );
					FxPass->mRenderQueueSortingData.miSortingOffset = val;
				}
				///////////////////////////

				ork_tek->mPasses.push_back( FxPass );
			}
		}

		ork_tek->mInternalHandle = (void*)(dxtek);

	}

    /////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// FX Interface

int DxFxInterface::BeginBlock( FxShader* hfx, const RenderContextInstData& data )
{
	mTargetDX.SetRenderContextInstData( & data );
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	ID3DXEffect*pFX = package->mpeffect;
	UINT ipasses = 0;
	HRESULT hr = pFX->Begin( & ipasses, 0 ); //D3DXFX_DONOTSAVESTATE );
	OrkAssert( D3D_OK == hr );
	mpActiveFxShader = hfx;
	return ipasses;

}
void DxFxInterface::EndBlock( FxShader* hfx )
{
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	ID3DXEffect*pFX = package->mpeffect;
	pFX->End();
	mpActiveFxShader = 0;
}
bool DxFxInterface::BindPass( FxShader* hfx, int ipass )
{
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	ID3DXEffect*pFX = package->mpeffect;
	HRESULT hr = pFX->BeginPass( ipass );
	OrkAssert( false == FAILED( hr ) );
	return false;
}
void DxFxInterface::CommitParams()
{
	if( mpActiveFxShader )
	{
		FxShaderPackage* package = static_cast<FxShaderPackage*>( mpActiveFxShader->GetInternalHandle() );
		ID3DXEffect*pFX = package->mpeffect;
		HRESULT hr = pFX->CommitChanges();
		OrkAssert( false == FAILED( hr ) );
		mpLastFxMaterial = mTargetDX.GetCurMaterial();
	}
}
bool DxFxInterface::BindTechnique( FxShader* hfx, const FxShaderTechnique* htek )
{
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	ID3DXEffect*pFX = package->mpeffect;
	D3DXHANDLE dtek = reinterpret_cast<D3DXHANDLE>( htek->GetPlatformHandle() );
    HRESULT hr = pFX->SetTechnique( dtek );
	return (D3D_OK==hr);
}

void DxFxInterface::EndPass( FxShader* hfx )
{
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	ID3DXEffect*pFX = package->mpeffect;
	pFX->EndPass();
}

const FxShaderTechnique* DxFxInterface::GetTechnique( FxShader* hfx, const std::string & name )
{
	const orkmap<std::string,const FxShaderTechnique*>& tekmap =  hfx->GetTechniques();
	const orkmap<std::string,const FxShaderTechnique*>::const_iterator it = tekmap.find( name );
	const FxShaderTechnique* htek = (it!=tekmap.end()) ? it->second : 0;

	if( htek && false == htek->mbValidated )
	{
		FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
		ID3DXEffect*pFX = package->mpeffect;
		D3DXHANDLE hTECH = pFX->GetTechniqueByName( name.c_str() );
		//HRESULT hr = pFX->ValidateTechnique( hTECH );
		//OrkAssert(D3D_OK==hr);
		const_cast<FxShaderTechnique*>(htek)->mbValidated = true;
	}
	return htek;
}

const FxShaderParam* DxFxInterface::GetParameterH( FxShader* hfx, const std::string & name )
{
	OrkAssert( 0!=hfx );
	const orkmap<std::string,const FxShaderParam*>& parammap =  hfx->GetParametersByName();
	const orkmap<std::string,const FxShaderParam*>::const_iterator it = parammap.find( name );
	const FxShaderParam* hparam = (it!=parammap.end()) ? it->second : 0;
	return hparam;
}

///////////////////////////////////////////////////////////////////////////////

void DxFxInterface::BindParamBool( FxShader* hfx, const FxShaderParam* hpar, const bool bv )
{
	PASSTHRUPARAMS;
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	ID3DXEffect*pFX = package->mpeffect;
	D3DXHANDLE dh = reinterpret_cast<D3DXHANDLE>( hpar->GetPlatformHandle() );
	HRESULT hr = pFX->SetBool( dh, BOOL(bv) );
	OrkAssert( D3D_OK == hr );
}

void DxFxInterface::BindParamInt( FxShader* hfx, const FxShaderParam* hpar, const int ival )
{
	PASSTHRUPARAMS;
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	ID3DXEffect*pFX = package->mpeffect;
	D3DXHANDLE dh = reinterpret_cast<D3DXHANDLE>( hpar->GetPlatformHandle() );
	HRESULT hr = pFX->SetInt( dh, ival );
	OrkAssert( D3D_OK == hr );
}

void DxFxInterface::BindParamVect2( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec )
{
	PASSTHRUPARAMS;
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	ID3DXEffect*pFX = package->mpeffect;
	D3DXHANDLE dh = reinterpret_cast<D3DXHANDLE>( hpar->GetPlatformHandle() );
	HRESULT hr = pFX->SetFloatArray( dh, (float*) Vec.GetArray(), 2 );
	OrkAssert( D3D_OK == hr );
}

void DxFxInterface::BindParamVect3( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec )
{
	PASSTHRUPARAMS;
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	ID3DXEffect*pFX = package->mpeffect;
	D3DXHANDLE dh = reinterpret_cast<D3DXHANDLE>( hpar->GetPlatformHandle() );
	HRESULT hr = pFX->SetFloatArray( dh, (float*) Vec.GetArray(), 3 );
	OrkAssert( D3D_OK == hr );
}

void DxFxInterface::BindParamVect4( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec )
{
	//PASSTHRUPARAMS;
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	ID3DXEffect*pFX = package->mpeffect;
	D3DXHANDLE dh = reinterpret_cast<D3DXHANDLE>( hpar->GetPlatformHandle() );
	HRESULT hr = pFX->SetFloatArray( dh, (float*) Vec.GetArray(), 4 );
	OrkAssert( D3D_OK == hr );
}

void DxFxInterface::BindParamVect4Array( FxShader* hfx, const FxShaderParam* hpar, const CVector4 * Vec, const int icount )
{
	PASSTHRUPARAMS;
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	ID3DXEffect*pFX = package->mpeffect;
	D3DXHANDLE dh = reinterpret_cast<D3DXHANDLE>( hpar->GetPlatformHandle() );
	HRESULT hr = pFX->SetVectorArray( dh, (const D3DXVECTOR4 *) Vec, icount );
	OrkAssert( D3D_OK == hr );
}

void DxFxInterface::BindParamFloat( FxShader* hfx, const FxShaderParam* hpar, float fA )
{
	PASSTHRUPARAMS;
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	ID3DXEffect*pFX = package->mpeffect;
	D3DXHANDLE dh = reinterpret_cast<D3DXHANDLE>( hpar->GetPlatformHandle() );
	HRESULT hr = pFX->SetFloat( dh, fA );
	OrkAssert( D3D_OK == hr );
}

void DxFxInterface::BindParamFloat2( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB )
{
	PASSTHRUPARAMS;
	f32 fary[2] = { fA, fB };
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	ID3DXEffect*pFX = package->mpeffect;
	D3DXHANDLE dh = reinterpret_cast<D3DXHANDLE>( hpar->GetPlatformHandle() );
	HRESULT hr = pFX->SetFloatArray( dh, fary, 2 );
	OrkAssert( D3D_OK == hr );
}

void DxFxInterface::BindParamFloat3( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC )
{
	PASSTHRUPARAMS;
	f32 fary[3] = { fA, fB, fC };
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	ID3DXEffect*pFX = package->mpeffect;
	D3DXHANDLE dh = reinterpret_cast<D3DXHANDLE>( hpar->GetPlatformHandle() );
	HRESULT hr = pFX->SetFloatArray( dh, fary, 3 );
	if( D3D_OK != hr )
	{
		GfxMaterial* curmtl = mTargetDX.GetCurMaterial();
		const char* pmtlname = curmtl->GetName().c_str();

		orkprintf( "WARNING someone forgot to bind a texture to texture slot<%s> in shader<%s> material<%s>\n", hpar->mParameterName.c_str(), hfx->GetName(), pmtlname );
	}
}

void DxFxInterface::BindParamFloat4( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC, float fD )
{
	PASSTHRUPARAMS;
	f32 fary[4] = { fA, fB, fC, fD };
	D3DXHANDLE dh = reinterpret_cast<D3DXHANDLE>( hpar->GetPlatformHandle() );
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	ID3DXEffect*pFX = package->mpeffect;
	HRESULT hr = pFX->SetFloatArray( dh, fary, 4 );
	OrkAssert( D3D_OK == hr );
}

void DxFxInterface::BindParamFloatArray( FxShader* hfx, const FxShaderParam* hpar, const float *pfa, const int icount )
{
	PASSTHRUPARAMS;
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	ID3DXEffect*pFX = package->mpeffect;
	D3DXHANDLE dh = reinterpret_cast<D3DXHANDLE>( hpar->GetPlatformHandle() );
	HRESULT hr = pFX->SetFloatArray( dh, pfa, icount );
	OrkAssert( D3D_OK == hr );
}

void DxFxInterface::BindParamU32( FxShader* hfx, const FxShaderParam* hpar, U32 uval )
{
	PASSTHRUPARAMS;
	f32 fary[4];
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	ID3DXEffect*pFX = package->mpeffect;
	D3DXHANDLE dh = reinterpret_cast<D3DXHANDLE>( hpar->GetPlatformHandle() );
	U32 uX = (uval&0x000000ff);
	U32 uY = (uval&0x0000ff00)>>8;
	U32 uZ = (uval&0x00ff0000)>>16;
	U32 uW = (uval&0xff000000)>>24;
	fary[0] = (F32) uX / 255.0f;
	fary[1] = (F32) uY / 255.0f;
	fary[2] = (F32) uZ / 255.0f;
	fary[3] = (F32) uW / 255.0f;
	HRESULT hr = pFX->SetFloatArray( dh, fary, 4 );
}

void DxFxInterface::BindParamMatrix( FxShader* hfx, const FxShaderParam* hpar, const CMatrix4 & Mat )
{
	//PASSTHRUPARAMS;
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	ID3DXEffect*pFX = package->mpeffect;
	D3DXHANDLE dh = reinterpret_cast<D3DXHANDLE>( hpar->GetPlatformHandle() );
	const D3DXMATRIX *pDXMAT = reinterpret_cast< const D3DXMATRIX*>( Mat.GetArray() );
	HRESULT hr = pFX->SetMatrix( dh, pDXMAT );
	OrkAssert( D3D_OK == hr );
}

void DxFxInterface::BindParamMatrix( FxShader* hfx, const FxShaderParam* hpar, const CMatrix3 & Mat )
{
	CMatrix4 mtx4;
	mtx4.SetRow(0,CVector4(Mat.GetRow(0),0.0f));
	mtx4.SetRow(1,CVector4(Mat.GetRow(1),0.0f));
	mtx4.SetRow(2,CVector4(Mat.GetRow(2),0.0f));
	//PASSTHRUPARAMS;
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	ID3DXEffect*pFX = package->mpeffect;
	D3DXHANDLE dh = reinterpret_cast<D3DXHANDLE>( hpar->GetPlatformHandle() );
	const D3DXMATRIX *pDXMAT = reinterpret_cast< const D3DXMATRIX*>( mtx4.GetArray() );
	HRESULT hr = pFX->SetMatrix( dh, pDXMAT );
	OrkAssert( D3D_OK == hr );
}

void DxFxInterface::BindParamMatrixArray( FxShader* hfx, const FxShaderParam* hpar, const CMatrix4 * Mat, int iCount )
{
	PASSTHRUPARAMS;
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	ID3DXEffect*pFX = package->mpeffect;
	D3DXHANDLE dh = reinterpret_cast<D3DXHANDLE>( hpar->GetPlatformHandle() );
	const D3DXMATRIX *pDXMAT = reinterpret_cast< const D3DXMATRIX*>( Mat[0].GetArray() );
	HRESULT hr = pFX->SetMatrixArray( dh, pDXMAT, iCount );
	OrkAssert( D3D_OK == hr );
}

void DxFxInterface::BindParamCTex( FxShader* hfx, const FxShaderParam* hpar, const Texture *pTex )
{
	PASSTHRUPARAMS;
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	ID3DXEffect*pFX = package->mpeffect;

	const FxShaderParam* setparam = 0;

	D3DBaseTex *pDXTexture = 0;

	if( pTex )
	{
		Texture *pnonconst = const_cast<Texture *>( pTex );

		D3DXTexturePackage* package = static_cast<D3DXTexturePackage*>( pTex->GetTexIH() );
		if( 0 == package )
		{
			if( pTex->GetTexClass() == lev2::Texture::ETEXCLASS_PAINTABLE )
			{
				package = D3DXTexturePackage::CreateUserPackage( D3DXTexturePackage::ETYPE_2D, pTex );
				pTex->SetTexIH( (void*) package );
			}
		}
		if( package )
		{
			pDXTexture = static_cast<D3DBaseTex*>( package->mD3dTexureHandle );
			if( 0 == pDXTexture )
			{
				mTargetDX.TXI()->VRamUpload( pnonconst );
				pDXTexture = static_cast<D3DBaseTex*>( package->mD3dTexureHandle );
			}
			if( pDXTexture )
			{
				if( pTex->IsDirty() )
				{
					mTargetDX.TXI()->VRamUpload( pnonconst );
					pDXTexture =  static_cast<D3DBaseTex*>( package->mD3dTexureHandle );
				}
				setparam = hpar;
			}
		}
	}
	else
	{
		setparam = hpar;
	}

	if( setparam )
	{
		if( EPROPTYPE_SAMPLER == setparam->meParamType )
		{
			setparam = setparam->mChildParam;
		}
		if( setparam && setparam->mBindable )
		{
			HRESULT hr = pFX->SetTexture( reinterpret_cast<D3DXHANDLE>(setparam->GetPlatformHandle()), pDXTexture );
			OrkAssert( D3D_OK == hr );
		}
	}

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool DxFxInterface::LoadFxShader( const AssetPath& pth, FxShader* pfxshader ) //
{
	AssetPath assetname = pth;
	assetname.SetExtension( "fx" );
	FxShaderPackage *peffect = LoadFxShader( pfxshader, pth );
	pfxshader->SetInternalHandle( (void*) peffect );
	return (peffect!=0);
}

///////////////////////////////////////////////////////////////////////////////

FxShaderPackage* DxFxInterface::LoadFxShader( FxShader* pfxshader, const AssetPath& fname ) //
{
	file::Path abspath = fname.ToAbsolute();

	FxShaderPackage* package = new FxShaderPackage;
	package->mpOrkShader = pfxshader;
	package->mShaderPath = fname.ToAbsolute();
	package->Compile( GfxTargetDX::GetD3DDevice() );
	FxShaderPackage::Add( package );

	return package;
}

DxFxInterface::DxFxInterface( GfxTargetDX& targ )
	: mTargetDX(targ)
{
}
///////////////////////////////////////////////////////////////////////////////

} }

#endif
