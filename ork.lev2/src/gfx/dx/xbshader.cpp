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

//#define PASSTHRUPARAMS return;
#define PASSTHRUPARAMS

namespace ork { 
	
void SplitString( const FixedString<256>& instr, orkvector<FixedString<64>> &splitvect, const char *pdelim );
	
namespace lev2 {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static FXLHANDLE gCurrentPass = 0;
orkset<FxShaderPackage*>	FxShaderPackage::gPackages;

FxShaderPackage::FxShaderPackage() : mpeffect(0)
{
	gPackages.insert(this);
}

void DxFxInterface::DoOnReset()
{
	gCurrentPass = 0;
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

HRESULT SaveUPDBFile(LPD3DXSHADER_COMPILE_PARAMETERSA pParameters)
{
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HRESULT hr = E_FAIL;
    LPD3DXBUFFER pUPDBBuffer = NULL;

    // Check arg.
    if(pParameters == NULL) {
        goto Exit;
    }

    // Check to see if UPDB data was generated.
    pUPDBBuffer = pParameters->pUPDBBuffer;
    
  
    if(pUPDBBuffer == NULL)
    {
        //OutputDebugStringA("ERROR: No UPDB data was generated. UPDB file not saved.\n");
        goto Exit;
    }

    if(pUPDBBuffer->GetBufferSize() == 0) {
        //OutputDebugStringA("ERROR: UPDB data was 0 bytes. UPDB file not saved.\n");
        goto Exit;
    }

    // Create the UPDB file.
    hFile = CreateFile(pParameters->UPDBPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, NULL);


    if(INVALID_HANDLE_VALUE == hFile)
    {
        //OutputDebugStringA("ERROR: Couldn't create UPDB file.\n");
        goto Exit;
    }
  

    DWORD bytesWritten = 0;
    DWORD bytesToWrite = pUPDBBuffer->GetBufferSize();

    if(!WriteFile(hFile, pUPDBBuffer->GetBufferPointer(), bytesToWrite,
            &bytesWritten, NULL) || bytesWritten != bytesToWrite)
    {
        //OutputDebugStringA("ERROR: Couldn't write UPDB file.\n");
        goto Exit;
    }

    // If we made it this far, we need to return S_OK.
    hr = S_OK;

Exit:
    if(hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hFile);
    }

    if(pUPDBBuffer != NULL)
    {
        pUPDBBuffer->Release();
        pParameters->pUPDBBuffer = NULL;
    }

    return hr;
}


void FxShaderPackage::Compile(D3DGfxDevice d3ddev)
{
    /////////////////////////////////////
    // compilation parameters (prob doesnt take hold with precompiled effect files)

    const D3DXMACRO EffectMacros[] =
    {
        { "DirectX9", "1" },
        { "XBox", "1" },
        { NULL, NULL },
    };

    DWORD EffectFlags = D3DXSHADER_AVOID_FLOW_CONTROL | D3DXSHADER_FXLANNOTATE_SEMANTIC_NAMES|D3DXSHADER_FXLPARAMETERS_AS_VARIABLE_NAMES; 
#if defined(RETAIL)
	EffectFlags |= D3DXSHADER_MICROCODE_BACKEND_NEW;
#endif
	//D3DXSHADER_SKIPOPTIMIZATION; 
	//D3DXSHADER_SKIPOPTIMIZATION;
	//D3DXSHADER_PACKMATRIX_COLUMNMAJOR;
	// D3DXSHADER_PARTIALPRECISION D3DXSHADER_PACKMATRIX_COLUMNMAJOR D3DXSHADER_PACKMATRIX_ROWMAJOR D3DXSHADER_SKIPOPTIMIZATION

	/////////////////////////////////////

	mpeffect = 0;

    LPD3DXBUFFER D3DEffectErrors = 0; // if there are any errors, they will show up here
    //HRESULT hr = D3DXCreateEffect( pDev, pEffectData, iEffectDataLen, EffectMacros, NULL, EffectFlags, NULL, & mpFXEffect, & D3DEffectErrors );

	ork::CFile shfile(mShaderPath, ork::EFM_READ);
	int ilen = 0;
	ork::EFileErrCode ec = shfile.GetLength(ilen);
	char* ptext = new char[ ilen ];
	shfile.Read( ptext, ilen );
	ec = shfile.Close();

	LPD3DXBUFFER pEffectData;
	LPD3DXBUFFER pErrorList;

	struct MyIncluder : public ID3DXInclude
	{
		HRESULT Open(D3DXINCLUDE_TYPE itype,LPCSTR pfilename,LPCVOID parentdata,LPCVOID* pdataout,UINT* pnumbytesout,LPSTR pfullpath,DWORD cbfullpath)
		{
			ork::file::Path fpath( "miniorkshader://" );
			fpath.SetFile( pfilename );
			ork::CFile shfile(fpath, ork::EFM_READ);
			int ilen = 0;
			ork::EFileErrCode ec = shfile.GetLength( ilen );
			char* ptext = new char[ ilen ];
			shfile.Read( ptext, ilen );
			ec = shfile.Close();
			*pdataout = (LPCVOID) ptext;
			*pnumbytesout = (UINT) ilen;
			return S_OK;
		}
		HRESULT Close(LPCVOID)
		{
			return S_OK;
		}
	};
	MyIncluder minc;


	ork::file::Path pth = mShaderPath;
	pth.SetFolder("");
	pth.SetExtension("");
	
	std::string pths = "xe:\\updbs\\"+std::string(pth.GetName().c_str())+std::string(".updb");

	orkprintf( "shaderout<%s>\n", pths.c_str() );

	D3DXSHADER_COMPILE_PARAMETERS  compileParams;
	ZeroMemory( &compileParams, sizeof(compileParams) );

#if 0
	compileParams.Flags = D3DXSHADEREX_GENERATE_UPDB;
	compileParams.UPDBPath = pths.c_str();
#endif

	if( FAILED( FXLCompileEffectEx( ptext, ilen, EffectMacros, & minc, EffectFlags, &pEffectData, &pErrorList, 0 ))) //& compileParams ) ) )
	{
		pErrorList->Release();
		//OutputDebugString( "Couldn't compile effect\n" );
	}

#if 0
	HRESULT hr2 = SaveUPDBFile(&compileParams);
#endif

	// Create effect
	if( FAILED( FXLCreateEffect( d3ddev, pEffectData->GetBufferPointer(),NULL, &mpeffect ) ) )
	{
		//OutputDebugString( "Couldn't create effect\n" );
	}

	bool btest = true; //(strstr(mShaderPath.c_str(),"novaglass")!=0);

	//if( btest )
	//{
	//	orkprintf( "yo\n" );
	//}

	///////////////////////////////////////////////////////
	/////////////////////////////////////
	// Dynamic Effect Binding Support

	FXLEFFECT_DESC EffectDescription;
	mpeffect->GetEffectDesc( & EffectDescription );
	UINT inumparams = EffectDescription.Parameters;

	orkmap<FxSmlString,FxShaderParam*> Samplers;

	for( UINT ip=0; ip<inumparams; ip++ )
	{
		FXLHANDLE dxparam = mpeffect->GetParameterHandleFromIndex( ip );
		FXLPARAMETER_DESC paramdesc;
		mpeffect->GetParameterDesc( dxparam, & paramdesc );

		const char* ParamName( paramdesc.pName ? paramdesc.pName : "" );
		
		if( btest )
		{
			orkprintf( "ParamIdx<%d> ParamName<%s>\n", ip, ParamName );
		}

		FXLDATA_CLASS ParamClass = paramdesc.Class;
		FXLDATA_TYPE ParamType = paramdesc.Type;

		//FxShaderParam* FindParamByName( const std::string& named );

		FxShaderParam* ork_parm = mpOrkShader->FindParamByName( ParamName );

		if( 0 == ork_parm )
		{
			ork_parm = new FxShaderParam;
			ork_parm->mParameterName = ParamName;
			ork_parm->mParameterType = "";
			mpOrkShader->AddParameter( ork_parm );
		}

		FxShaderParamPackage* parampkg = new FxShaderParamPackage;
		parampkg->mShaderParamHandle = dxparam;
		ork_parm->mInternalHandle = (void*)parampkg;
		
		switch( ParamClass )
		{
			case FXLDCLASS_SAMPLER2D:
			case FXLDCLASS_SAMPLER3D:
			case FXLDCLASS_SAMPLERCUBE:
			{
				ork_parm->meParamType = EPROPTYPE_SAMPLER;
				ork_parm->mParameterType = "sampler";
				Samplers[ParamName] = ork_parm;
				if( btest )
				{
					FxShaderParam* ppp = Samplers[ParamName];
					orkprintf( "Samplers<%s:%p>\n", ParamName, ppp );
				}

				//ork_parm->mParameterType = CPropType<CReal>::GetTypeName();
				break;
			}
			/*if( ParamType == D3DXPT_TEXTURE2D )
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
			}*/

			case FXLDCLASS_SCALAR:
			{
				if( ParamType == FXLDTYPE_FLOAT )
				{
					ork_parm->mParameterType = CPropType<CReal>::GetTypeName();
				}
				if( ParamType == FXLDTYPE_STRING )
				{
					ork_parm->mParameterType = "string";
					ork_parm->mBindable = false;
				}
				break;
			}
			case FXLDCLASS_VECTOR:
			{
				if( ParamType == FXLDTYPE_FLOAT )
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
			FXLHANDLE annoh = mpeffect->GetAnnotationHandleFromIndex( dxparam, ia );

			if( annoh )
			{
				FXLANNOTATION_DESC AnnoDesc;
				mpeffect->GetAnnotationDesc( annoh, & AnnoDesc );
				std::string AnnoName( AnnoDesc.pName ? AnnoDesc.pName : "" );

				FXLDATA_CLASS AnnoClass = AnnoDesc.Class;
				FXLDATA_TYPE AnnoType = AnnoDesc.Type;

				switch( AnnoType )
				{
					case FXLDTYPE_STRING:
					{
						LPCSTR strval = 0;
						//mpeffect->GetString( annoh, & strval );
						//std::string AnnoVal( strval ? strval : "" );
						//ork_parm->mAnnotations.AddSorted(AnnoName,AnnoVal);
						//orkprintf( "Found String Annotation <name %s> <val %s>\n", AnnoName.c_str(), AnnoVal.c_str() );
						break;
					}
					default:
						break;
				}

			}
		}
	}
	///////////////////////////////////////////////////////
	///////////////////////////////////////////////////////
	///////////////////////////////////////////////////////
	//fxlitesux_samplermap<ColorMap> ColorMapSampler TextMapSampler TextStretchMapSampler
	bool bdone = false;
	const char* pmkr = ptext;
	int il = strlen(ptext);
	std::vector<FxBigString> samplermaps;
	while( false == bdone )
	{
		const char* pfxlsuxstr = strstr( pmkr, "//fxlitesux_samplermap" );
		if( pfxlsuxstr != 0 )
		{
			const char* pret = strstr( pfxlsuxstr, "\n" );
			std::string mstr( pfxlsuxstr, (pret-1) );
			samplermaps.push_back( mstr.c_str() );
			if( btest )
			{
				orkprintf( "FxLiteSuxAdd<%s>\n", mstr.c_str() );
			}
			pmkr = pret;
			if( pmkr>=(ptext+il) ) bdone=true;

		}
		else bdone = true;
	}
	int inumsms = samplermaps.size();
	for( int is=0; is<inumsms; is++ )
	{
		const FxBigString& sm = samplermaps[is];
		std::string::size_type srchA = sm.find( "<" );
		std::string::size_type srchB = sm.find( ">" );
		FxSmlString TexName = sm.substr( srchA+1, (srchB-srchA)-1 ).c_str();
		FxBigString TheRest = sm.substr( srchB+1, sm.length()-(srchB+1) );

		mtex2samplermap.insert(std::make_pair<FxSmlString,FxLightTextureToSamplerMap>(TexName,FxLightTextureToSamplerMap()));
		FxLightTextureToSamplerMap& the_map = mtex2samplermap.find(TexName)->second;
		SplitString( TheRest, the_map.mSamplers, " " );

		FxShaderParam* ork_parm = mpOrkShader->FindParamByName( TexName.c_str() );

		if( 0 == ork_parm )
		{
			ork_parm = new FxShaderParam;
			ork_parm->mParameterName = TexName.c_str();
			ork_parm->mParameterType = "texture";
			mpOrkShader->AddParameter( ork_parm );
		}

		FxShaderParamPackage* parampkg = new FxShaderParamPackage;
		parampkg->mShaderParamHandle = 0;
		ork_parm->mInternalHandle = (void*)parampkg;

		int inums=the_map.mSamplers.size();
		for( int isamp=0; isamp<inums; isamp++ )
		{
			const FxSmlString& sampler_name = the_map.mSamplers[isamp];
			const char* pname = sampler_name.c_str();

			orkmap<FxSmlString,FxShaderParam*>::const_iterator it=Samplers.find(sampler_name);

			if( it != Samplers.end() )
			{
				FxShaderParam* subparam = it->second;

				FxShaderParamPackage* subpkg = (FxShaderParamPackage*) it->second->GetPlatformHandle();

				if( subpkg )
				{
					FXLHANDLE dxparam = subpkg->mShaderParamHandle;
					FXLPARAMETER_DESC paramdesc;
					mpeffect->GetParameterDesc( dxparam, & paramdesc );

					const char* ParamName( paramdesc.pName ? paramdesc.pName : "" );
					
					FXLDATA_CLASS ParamClass = paramdesc.Class;
					FXLDATA_TYPE ParamType = paramdesc.Type;

					bool bOK = (ParamClass==FXLDCLASS_SAMPLER2D) | (ParamClass==FXLDCLASS_SAMPLER3D) | (ParamClass==FXLDCLASS_SAMPLERCUBE);

					OrkAssert( bOK );

					if( btest )
					{
						orkprintf( "TexName<%s> FxLiteSuxAdd2<%s:%s>\n", TexName.c_str(), pname, ParamName );
					}

					parampkg->AddSampler(dxparam);
				}
			}
		}
		//ork_parm->mInternalHandle = (void*)parampkg;

	}
	///////////////////////////////////////////////////////
	///////////////////////////////////////////////////////
	///////////////////////////////////////////////////////
	UINT inumtek = EffectDescription.Techniques;
	for( UINT it=0; it<inumtek; it++ )
	{
		FXLHANDLE dxtek = mpeffect->GetTechniqueHandleFromIndex( it );
		FXLTECHNIQUE_DESC  TekDesc;
		mpeffect->GetTechniqueDesc( dxtek, & TekDesc );
		std::string tekname( TekDesc.pName ? TekDesc.pName : "" );

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
				FXLHANDLE hpass = mpeffect->GetPassHandleFromIndex( dxtek, ipass );

				FXLPASS_DESC  PassDesc;
				mpeffect->GetPassDesc( hpass, & PassDesc );

				FxShaderPass* FxPass = new FxShaderPass;
				FxPass->mRenderQueueSortingData.miSortingPass = 4; // default standard
				FxPass->mInternalHandle = (void*) hpass;
				FxPass->mPassName = PassDesc.pName ? std::string(PassDesc.pName) : "noname";

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
static FXLHANDLE gpBoundTechnique = 0;

///////////////////////////////////////////////////////////////////////////////
// FX Interface

int DxFxInterface::BeginBlock( FxShader* hfx, const RenderContextInstData& data )
{
	mTargetDX.SetRenderContextInstData( & data );
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	FXLEffect*pFX = package->mpeffect;
	FXLTECHNIQUE_DESC tdesc;
	pFX->GetTechniqueDesc(gpBoundTechnique,&tdesc);
	int ipasses = tdesc.Passes;
	pFX->BeginTechnique( gpBoundTechnique, 0 );
	mpActiveFxShader = hfx;
	return ipasses;

}
void DxFxInterface::EndBlock( FxShader* hfx )
{
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	FXLEffect*pFX = package->mpeffect;
	pFX->EndTechnique();
	mpActiveFxShader = 0;
}
bool DxFxInterface::BindPass( FxShader* hfx, int ipass )
{
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	FXLEffect*pFX = package->mpeffect;
	gCurrentPass = pFX->GetPassHandleFromIndex( gpBoundTechnique,ipass );
	pFX->BeginPass(gCurrentPass);
	return true;
}
void DxFxInterface::CommitParams()
{
	//if( mpActiveFxShader )
	{
		FxShaderPackage* package = static_cast<FxShaderPackage*>( mpActiveFxShader->GetInternalHandle() );
		FXLEffect*pFX = package->mpeffect;
		pFX->Commit();
		mpLastFxMaterial = mTargetDX.GetCurMaterial();
	}
}
bool DxFxInterface::BindTechnique( FxShader* hfx, const FxShaderTechnique* htek )
{
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	FXLEffect*pFX = package->mpeffect;
	gpBoundTechnique = reinterpret_cast<FXLHANDLE>( htek->GetPlatformHandle() );
	return true;
}

void DxFxInterface::EndPass( FxShader* hfx )
{
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	FXLEffect*pFX = package->mpeffect;
	pFX->EndPass();
}

const FxShaderTechnique* DxFxInterface::GetTechnique( FxShader* hfx, const std::string & name )
{
	const orkmap<std::string,const FxShaderTechnique*>& tekmap =  hfx->GetTechniques();
	const orkmap<std::string,const FxShaderTechnique*>::const_iterator it = tekmap.find( name );
	const FxShaderTechnique* htek = (it!=tekmap.end()) ? it->second : 0;
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
	FXLEffect*pFX = package->mpeffect;
	FxShaderParamPackage* dh = static_cast<FxShaderParamPackage*>( hpar->GetPlatformHandle() );
	pFX->SetBool( dh->mShaderParamHandle, BOOL(bv) );
}

void DxFxInterface::BindParamInt( FxShader* hfx, const FxShaderParam* hpar, const int ival )
{
	PASSTHRUPARAMS;
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	FXLEffect*pFX = package->mpeffect;
	FxShaderParamPackage* dh = static_cast<FxShaderParamPackage*>( hpar->GetPlatformHandle() );
	pFX->SetInt( dh->mShaderParamHandle, ival );
}

void DxFxInterface::BindParamVect2( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec )
{
	PASSTHRUPARAMS;
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	FXLEffect*pFX = package->mpeffect;
	FxShaderParamPackage* dh = static_cast<FxShaderParamPackage*>( hpar->GetPlatformHandle() );
	pFX->SetVectorF( dh->mShaderParamHandle, (float*) Vec.GetArray() );
}

void DxFxInterface::BindParamVect3( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec )
{
	PASSTHRUPARAMS;
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	FXLEffect*pFX = package->mpeffect;
	FxShaderParamPackage* dh = static_cast<FxShaderParamPackage*>( hpar->GetPlatformHandle() );
	pFX->SetVectorF( dh->mShaderParamHandle, (float*) Vec.GetArray() );
}

void DxFxInterface::BindParamVect4( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec )
{
	//PASSTHRUPARAMS;
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	FXLEffect*pFX = package->mpeffect;
	FxShaderParamPackage* dh = static_cast<FxShaderParamPackage*>( hpar->GetPlatformHandle() );
	pFX->SetVectorF( dh->mShaderParamHandle, (float*) Vec.GetArray() );
}

void DxFxInterface::BindParamVect4Array( FxShader* hfx, const FxShaderParam* hpar, const CVector4 * Vec, const int icount )
{
	PASSTHRUPARAMS;
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	FXLEffect*pFX = package->mpeffect;
	FxShaderParamPackage* dh = static_cast<FxShaderParamPackage*>( hpar->GetPlatformHandle() );
	pFX->SetVectorArrayF( dh->mShaderParamHandle, (const FLOAT *) Vec, icount );
}

void DxFxInterface::BindParamFloat( FxShader* hfx, const FxShaderParam* hpar, float fA )
{
	PASSTHRUPARAMS;
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	FXLEffect*pFX = package->mpeffect;
	FxShaderParamPackage* dh = static_cast<FxShaderParamPackage*>( hpar->GetPlatformHandle() );
	pFX->SetFloat( dh->mShaderParamHandle, fA );
}

void DxFxInterface::BindParamFloat2( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB )
{
	PASSTHRUPARAMS;
	f32 fary[2] = { fA, fB };
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	FXLEffect*pFX = package->mpeffect;
	FxShaderParamPackage* dh = static_cast<FxShaderParamPackage*>( hpar->GetPlatformHandle() );
	pFX->SetScalarArrayF( dh->mShaderParamHandle, fary, 2 );
}

void DxFxInterface::BindParamFloat3( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC )
{
	PASSTHRUPARAMS;
	f32 fary[3] = { fA, fB, fC };
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	FXLEffect*pFX = package->mpeffect;
	FxShaderParamPackage* dh = static_cast<FxShaderParamPackage*>( hpar->GetPlatformHandle() );
	pFX->SetScalarArrayF( dh->mShaderParamHandle, fary, 3 );
}

void DxFxInterface::BindParamFloat4( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC, float fD )
{
	PASSTHRUPARAMS;
	f32 fary[4] = { fA, fB, fC, fD };
	FxShaderParamPackage* dh = static_cast<FxShaderParamPackage*>( hpar->GetPlatformHandle() );
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	FXLEffect*pFX = package->mpeffect;
	pFX->SetScalarArrayF( dh->mShaderParamHandle, fary, 4 );
}

void DxFxInterface::BindParamFloatArray( FxShader* hfx, const FxShaderParam* hpar, const float *pfa, const int icount )
{
	PASSTHRUPARAMS;
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	FXLEffect*pFX = package->mpeffect;
	FxShaderParamPackage* dh = static_cast<FxShaderParamPackage*>( hpar->GetPlatformHandle() );
	pFX->SetScalarArrayF( dh->mShaderParamHandle, pfa, icount );
}

void DxFxInterface::BindParamU32( FxShader* hfx, const FxShaderParam* hpar, U32 uval )
{
	PASSTHRUPARAMS;
	f32 fary[4];
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	FXLEffect*pFX = package->mpeffect;
	FxShaderParamPackage* dh = static_cast<FxShaderParamPackage*>( hpar->GetPlatformHandle() );
	U32 uX = (uval&0x000000ff);
	U32 uY = (uval&0x0000ff00)>>8;
	U32 uZ = (uval&0x00ff0000)>>16;
	U32 uW = (uval&0xff000000)>>24;
	fary[0] = (F32) uX / 255.0f;
	fary[1] = (F32) uY / 255.0f;
	fary[2] = (F32) uZ / 255.0f;
	fary[3] = (F32) uW / 255.0f;
	pFX->SetScalarArrayF( dh->mShaderParamHandle, fary, 4 );
}

void DxFxInterface::BindParamMatrix( FxShader* hfx, const FxShaderParam* hpar, const CMatrix4 & Mat )
{
	//PASSTHRUPARAMS;
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	FXLEffect*pFX = package->mpeffect;
	FxShaderParamPackage* dh = static_cast<FxShaderParamPackage*>( hpar->GetPlatformHandle() );
	const float* pDXMAT = static_cast<const float*>( Mat.GetArray() );
	pFX->SetMatrixF4x4( dh->mShaderParamHandle, pDXMAT );
}

void DxFxInterface::BindParamMatrix( FxShader* hfx, const FxShaderParam* hpar, const CMatrix3 & Mat )
{
	//PASSTHRUPARAMS;
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	FXLEffect*pFX = package->mpeffect;
	FxShaderParamPackage* dh = static_cast<FxShaderParamPackage*>( hpar->GetPlatformHandle() );
	const float* pDXMAT = static_cast<const float*>( Mat.GetArray() );
	pFX->SetMatrixF( dh->mShaderParamHandle, pDXMAT );
}

void DxFxInterface::BindParamMatrixArray( FxShader* hfx, const FxShaderParam* hpar, const CMatrix4 * Mat, int iCount )
{
	PASSTHRUPARAMS;
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	FXLEffect*pFX = package->mpeffect;
	FxShaderParamPackage* dh = static_cast<FxShaderParamPackage*>( hpar->GetPlatformHandle() );
	FLOAT* pDXMAT = reinterpret_cast<FLOAT*>( Mat[0].GetArray() );
	pFX->SetMatrixArrayF4x4( dh->mShaderParamHandle, pDXMAT, iCount );
}

void DxFxInterface::BindParamCTex( FxShader* hfx, const FxShaderParam* hpar, const Texture *pTex )
{
	PASSTHRUPARAMS;
	FxShaderPackage* package = static_cast<FxShaderPackage*>( hfx->GetInternalHandle() );
	FXLEffect*pFX = package->mpeffect;

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

	bool btest = false; //(strstr(hfx->GetName(),"lambert")!=0);
	if( btest )
	{
		const char* paramname = hpar->mParameterName.c_str();
		//orkprintf( "shader<%s> param<%s>\n", hfx->GetName(), paramname );
	}
	if( setparam )
	{
		if( EPROPTYPE_ASSET == setparam->meParamType )
		{
			//setparam = setparam->mChildParam;
		}
		if( setparam && setparam->mBindable )
		{
			FxShaderParamPackage* dh = static_cast<FxShaderParamPackage*>( hpar->GetPlatformHandle() );
			int inums = dh->miNumSamplers;
			for( int is=0; is<inums; is++ )
			{
				FXLHANDLE sampler_h = dh->mSamplerParams[is];

				if( sampler_h && gCurrentPass )
				{
					FXLPARAMETER_CONTEXT ctx = pFX->GetParameterContext( gCurrentPass, sampler_h );

					if( 0 != ctx )
					{
						OrkAssert( ctx == FXLPCONTEXT_PIXELSHADERSAMPLER );
					}
					if( ctx == FXLPCONTEXT_PIXELSHADERSAMPLER )
					{
						if( btest )
						{
							const char* paramname = hpar->mParameterName.c_str();
							const char* pshname = hfx->GetName();
							FXLHANDLE sampler_h2 = sampler_h;
							//orkprintf( "shader<%s> param<%s> sampler<%08x> tex<%p>\n", hfx->GetName(), paramname, sampler_h, pDXTexture );
						}
						pFX->SetSampler( sampler_h, pDXTexture );
					}
				}
			}
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
	package->mShaderPath = fname;
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
