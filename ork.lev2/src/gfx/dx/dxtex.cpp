////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/dxt.h>
#if defined( ORK_CONFIG_DIRECT3D )
#include "dx.h"
#include <ork/lev2/gfx/texman.h>

#include <ork/kernel/string/string.h>

#if defined(_XBOX) && defined(PROFILE)
#include <d3d9.h>
#endif

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

DxTextureInterface::DxTextureInterface( GfxTargetDX& target )
	: TextureInterface( )
	, mTargetDX( target )
{
		
}

void DxTextureInterface::TexManInit( void )
{
}

D3DGfxDevice DxTextureInterface::GetD3DDevice()
{
	return mTargetDX.GetD3DDevice();
}
///////////////////////////////////////////////////////////////////////////////

void DxTextureInterface::VRamUpload( Texture *pTex )
{
	D3DXTexturePackage* package = static_cast<D3DXTexturePackage*>( pTex->GetTexIH() );
	OrkAssert( package!=0 );
	package->VRAM_Upload( GetD3DDevice(), pTex );
}

///////////////////////////////////////////////////////////////////////////////

void DxTextureInterface::VRamDeport(Texture *pTex )
{
	D3DXTexturePackage* package = static_cast<D3DXTexturePackage*>(pTex->GetTexIH());
	OrkAssert( package!=0 );
	package->VRAM_Release();
}

///////////////////////////////////////////////////////////////////////////////

bool DxTextureInterface::DestroyTexture( Texture* ptex )
{
	D3DXTexturePackage* package = static_cast<D3DXTexturePackage*>(ptex->GetTexIH());
	if( package )
	{
		OrkAssert( package!=0 );
		package->VRAM_Release();
		delete package;
	}
	ptex->SetTexIH(0);
	return true;
}

///////////////////////////////////////////////////////////////////////////////

D3DXTexturePackage::D3DXTexturePackage()
	: mImageInfo()
	, mpData(0)
	, mD3DxBuffer(0)
	, miDataLen(0)
	, mSurfaceDesc()
	, meType(ETYPE_END)
	, miDataFormat(0)
	, mD3dTexureHandle(0)
	, mTexture( 0 )
{
}

D3DXTexturePackage::~D3DXTexturePackage()
{
	if( mD3dTexureHandle )
	{
		VRAM_Release();
	}

	if( mD3DxBuffer )
	{
		bool bdone = false;

		while( ! bdone )
		{
			ULONG rcount = mD3DxBuffer->Release();
			bdone = (rcount==0);
		}

		mD3DxBuffer = 0;
	}
}

D3DXTexturePackage* D3DXTexturePackage::CreateUserPackage( ETYPE etyp, const Texture* ptex )
{
	D3DXTexturePackage* package = new D3DXTexturePackage;
	package->meType = etyp;
	package->mTexture = ptex;
	package->mD3dTexureHandle = 0;
	return package;

}
D3DXTexturePackage* D3DXTexturePackage::CreateRenderTargetPackage( ETYPE etyp, const Texture* ptex, IDirect3DBaseTexture9* pdxtex )
{
	D3DXTexturePackage* package = new D3DXTexturePackage;
	package->meType = etyp;
	package->mTexture = ptex;
	package->mD3dTexureHandle = pdxtex;
	return package;

}

void D3DXTexturePackage::VRAM_Upload( D3DGfxDevice device, const Texture* ptex )
{
#if defined(_XBOX) && defined(PROFILE)
	PIXBeginNamedEvent(0, "textureupload");
#endif
	mTexture = ptex;

	Texture* pnonconsttex = const_cast<Texture*>( ptex );

	HRESULT hr;

	switch( pnonconsttex->GetTexClass() )
	{
		default:
			//OrkAssert( false );
			break;
#if ! defined(_XBOX)
		case Texture::ETEXCLASS_PAINTABLE:
		{
			int iw = pnonconsttex->GetWidth();
			int ih = pnonconsttex->GetHeight();

			if( 0 == mD3dTexureHandle )
			{
				DWORD usage = D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_DYNAMIC ;
				D3DFORMAT format = D3DFMT_A8R8G8B8;
				D3DPOOL pool = D3DPOOL_DEFAULT;

				LPDIRECT3DTEXTURE9 dxtex;

				hr = D3DXCreateTexture( device, iw, ih,
										0, usage, format, pool, & dxtex );

				mD3dTexureHandle = dxtex;
				OrkAssert( SUCCEEDED( hr ) );
				ptex->SetDirty(true);
			}

			if( pnonconsttex->IsDirty() )
			{
				LPDIRECT3DTEXTURE9 dxtex = (LPDIRECT3DTEXTURE9) mD3dTexureHandle;

				D3DLOCKED_RECT d3dlr;
				hr = dxtex->LockRect( 0, &d3dlr, 0, 0 );
				OrkAssert( SUCCEEDED( hr ) );
				U8* pDst = (U8*) d3dlr.pBits;
				int DPitch = d3dlr.Pitch;
				int ibpp = ptex->GetBytesPerPixel();

				OrkAssert( (ibpp*iw) == DPitch );
				const void* pdata = ptex->GetTexData();
				memcpy( pDst, pdata, (iw*ih*ibpp) );
				hr = dxtex->UnlockRect (0);
				OrkAssert( SUCCEEDED( hr ) );
				dxtex->SetAutoGenFilterType( D3DTEXF_LINEAR );
				dxtex->GenerateMipSubLevels();
				ptex->SetDirty( false );

			}
			break;
		}
#endif
		case Texture::ETEXCLASS_STATIC:
		{
			OrkAssert( false == IsResident() );

			switch( mImageInfo.ResourceType )
			{
				case D3DRTYPE_VOLUMETEXTURE:
					meType = ETYPE_3D;
					break;
				case D3DRTYPE_CUBETEXTURE:
					meType = ETYPE_CUBEMAP;
					break;
				default:
				case D3DRTYPE_TEXTURE :
					meType = ETYPE_2D;
					break;
			}
					
			switch(meType)
			{
				case ETYPE_CUBEMAP:
				{	LPDIRECT3DCUBETEXTURE9 pD3DTex = 0;
					
					hr = D3DXCreateCubeTextureFromFileInMemory( 
						device, mpData, (int) miDataLen, & pD3DTex );

					OrkAssert( SUCCEEDED( hr ) );
					D3DSURFACE_DESC TexDescriptor;
					IDirect3DCubeTexture9* p3DTex = (IDirect3DCubeTexture9*) pD3DTex;
					hr = p3DTex->GetLevelDesc( 0, & TexDescriptor );
					pnonconsttex->SetWidth( TexDescriptor.Width );
					pnonconsttex->SetHeight( TexDescriptor.Height );
					pnonconsttex->SetDepth( 6 );
					miDataFormat = TexDescriptor.Format;
					//ptex->SetTexIH( (void*) pD3DTex );
					pnonconsttex->SetDirty( false );
					mD3dTexureHandle = pD3DTex;
					break;
				}
				case ETYPE_3D:
				{	LPDIRECT3DVOLUMETEXTURE9 pD3DTex = 0;
					
					HRESULT hr = D3DXCreateVolumeTextureFromFileInMemory( 
						device, mpData, (int) miDataLen, & pD3DTex );
					
					OrkAssert( SUCCEEDED( hr ) );
					D3DVOLUME_DESC VolDescriptor;
					IDirect3DVolumeTexture9* p3DTex = (IDirect3DVolumeTexture9*) pD3DTex;
					hr = p3DTex->GetLevelDesc( 0, & VolDescriptor );
					pnonconsttex->SetWidth( VolDescriptor.Width );
					pnonconsttex->SetHeight( VolDescriptor.Height );
					pnonconsttex->SetDepth( VolDescriptor.Depth );
					miDataFormat = VolDescriptor.Format;
					//ptex->SetTexIH( (void*) pD3DTex );
					pnonconsttex->SetDirty( false );
					mD3dTexureHandle = pD3DTex;
					break;
				}
				case ETYPE_2D:
				{	D3DBaseTex *pD3DTex = 0;
					D3DXIMAGE_INFO info;
					
					HRESULT hr = D3DXGetImageInfoFromFileInMemory(mpData, (int) miDataLen, &info);
					OrkAssert( SUCCEEDED( hr ) );
					
					hr = D3DXCreateTextureFromFileInMemoryEx(
						device, mpData, (int) miDataLen, info.Width, info.Height, info.MipLevels, 
						0, info.Format, D3DPOOL_MANAGED, D3DX_DEFAULT, 
						D3DX_DEFAULT, 0, &info, NULL, (LPDIRECT3DTEXTURE9 *) & pD3DTex);
					
					OrkAssert( SUCCEEDED( hr ) );

					D3DSURFACE_DESC TexDescriptor;
					IDirect3DTexture9* p2DTex = (IDirect3DTexture9*) pD3DTex;
					hr = p2DTex->GetLevelDesc( 0, & TexDescriptor );
					pnonconsttex->SetWidth( info.Width );
					pnonconsttex->SetHeight( info.Height );
					miDataFormat = TexDescriptor.Format;
					//ptex->SetTexIH( (void*) pD3DTex );
					pnonconsttex->SetDirty( false );
					mD3dTexureHandle = pD3DTex;
					break;
				}
				default:
				{
					OrkAssert(false);
					break;
				}
			}
			break;
		}
	}
#if defined(_XBOX) && defined(PROFILE)
	PIXEndNamedEvent();
#endif
}

void D3DXTexturePackage::VRAM_Release()
{
	//OrkAssert( IsResident() == true );
	if( mD3dTexureHandle )
	{
		mD3dTexureHandle->Release();
		mD3dTexureHandle = 0;
	}
}

///////////////////////////////////////////////////////////////////////////////

bool DxTextureInterface::LoadTexture( const AssetPath& infname, Texture *ptex )
{
	HRESULT hr;
	AssetPath Filename = infname;
	bool bIsAbsPath = Filename.IsAbsolute();
	bool bHasExt = Filename.HasExtension();

	float ftime1 = ork::CSystem::GetRef().GetLoResRelTime();

	///////////////////////////////////////////////
	int texextidx = 0;
	EFileErrCode eFileErr = EFEC_FILE_DOES_NOT_EXIST;
	struct LoadTex
	{
		static CFile *DoIt( const AssetPath& filename, EFileErrCode &errcode )
		{
			CFile *pTexFile = new CFile( filename, EFM_READ );
			if( false == pTexFile->IsOpen() )
			{
				delete pTexFile;
				pTexFile = 0;
			}
			return pTexFile;
		}
	};

	CFile *pTexFile = 0;
	std::string subdir = bHasExt ? "" : "textures/";
	///////////////////////////////////////////////
	bool bISDDS = false;
	if( 0 == pTexFile )
	{
		AssetPath DDSFilename = infname; 
		DDSFilename.SetExtension( ".dds" );
		if( ork::CFileEnv::GetRef().DoesFileExist( DDSFilename ) )
		{
			pTexFile = LoadTex::DoIt( DDSFilename, eFileErr );
			bISDDS = true;
		}
	}
	if( 0 == pTexFile )
	{
		AssetPath TGAFilename = infname; 
		TGAFilename.SetExtension( ".tga" );
		if( ork::CFileEnv::GetRef().DoesFileExist( TGAFilename ) )
		{
			pTexFile = LoadTex::DoIt( TGAFilename, eFileErr );
		}
	}
	if( 0 == pTexFile )
	{
		AssetPath PNGFilename = infname; 
		PNGFilename.SetExtension( ".png" );
		if( ork::CFileEnv::GetRef().DoesFileExist( PNGFilename ) )
		{
			pTexFile = LoadTex::DoIt( PNGFilename, eFileErr );
		}
	}
	///////////////////////////////////////////////

	if( pTexFile )
	{
		file::Path fname = pTexFile->GetFileName();
		D3DXTexturePackage* package = new D3DXTexturePackage;
		//ptex->SetTexData( (void*) package );
		ptex->SetTexIH( (void*) package );
		
		eFileErr = pTexFile->GetLength( package->miDataLen );
		hr = D3DXCreateBuffer( (int) package->miDataLen, & package->mD3DxBuffer );
		eFileErr = pTexFile->Read( package->mD3DxBuffer->GetBufferPointer(), package->miDataLen );
		delete pTexFile;

///////////////////////////////////
// read header info
///////////////////////////////////

		if( bISDDS )
		{
			dxt::DDS_HEADER MyHeader;
			memcpy( & MyHeader, package->mD3DxBuffer->GetBufferPointer(), sizeof(MyHeader) );

#if defined( _XBOX )
			MyHeader.FixEndian();
#endif

			package->mImageInfo.Width = MyHeader.dwWidth;
			package->mImageInfo.Height = MyHeader.dwHeight;
			package->mImageInfo.Depth = MyHeader.dwDepth;
			package->mImageInfo.MipLevels = MyHeader.dwMipMapCount;


			////////////////////////////////////

			bool bISCUBEMAP = (MyHeader.dwCaps2 & dxt::DDS_CUBEMAP);
			bool bISVOLUMEMAP = (MyHeader.dwCaps2 & dxt::DDS_VOLUME);

			if( bISVOLUMEMAP )
			{
				package->mImageInfo.ResourceType = D3DRTYPE_VOLUMETEXTURE;
			}
			else if( bISCUBEMAP )
			{
				package->mImageInfo.ResourceType = D3DRTYPE_CUBETEXTURE;
			}
			else
			{
				package->mImageInfo.ResourceType = D3DRTYPE_TEXTURE;
			
			}

			////////////////////////////////////

			if( IsDXT1(MyHeader.ddspf) )
			{
				package->mImageInfo.Format = D3DFMT_DXT1;
			}
			else if( IsDXT3(MyHeader.ddspf) )
			{
				package->mImageInfo.Format = D3DFMT_DXT3;
			}
			else if( IsDXT5(MyHeader.ddspf) )
			{
				package->mImageInfo.Format = D3DFMT_DXT5;
			}
			else if( IsABGR8(MyHeader.ddspf) )
			{
				package->mImageInfo.Format = D3DFMT_A8B8G8R8;
			}
			else if( IsRGB8(MyHeader.ddspf) )
			{
				package->mImageInfo.Format = D3DFMT_X8R8G8B8;
			}
			else if( IsXBGR8(MyHeader.ddspf) )
			{
				package->mImageInfo.Format = D3DFMT_X8B8G8R8;
			}
			else
			{
				OrkAssert(false);
			}
		}
		else
		{
			hr = D3DXGetImageInfoFromFile( fname.ToAbsolute().c_str(), & package->mImageInfo );
		}

///////////////////////////////////

		package->mpData = (void*) package->mD3DxBuffer->GetBufferPointer();
		ptex->SetWidth( package->mImageInfo.Width );
		ptex->SetHeight( package->mImageInfo.Height );
		ptex->SetDepth( package->mImageInfo.Depth );
		ptex->SetNumMipMaps( package->mImageInfo.MipLevels );

		bool IsData = (ptex->GetTexClass() == lev2::Texture::ETEXCLASS_DATA);

		switch( package->mImageInfo.Format )
		{
			case D3DFMT_R32F:
				ptex->SetBytesPerPixel( 4 );
				if( IsData )
				{
					int iw = ptex->GetWidth();
					int ih = ptex->GetHeight();
					float* pfloat = new float[ iw*ih ];

					IDirect3DSurface9 *preadsurf = 0;

					D3DSURFACE_DESC RdDesc;
					RdDesc.Width = iw;
					RdDesc.Height = ih;
					RdDesc.Format = D3DFMT_R32F;
					RdDesc.Usage = 0;


#if ! defined(_XBOX)
					hr = GetD3DDevice()->CreateOffscreenPlainSurface( RdDesc.Width,
																	RdDesc.Height,
																	RdDesc.Format,
																	D3DPOOL_SYSTEMMEM,
																	& preadsurf,
																	NULL );
					OrkAssert( SUCCEEDED( hr ) );
#endif

					hr = D3DXLoadSurfaceFromFileInMemory( preadsurf,
						0,
						0,
						package->mD3DxBuffer->GetBufferPointer(),
						(int) package->miDataLen,
						0,
						D3DX_FILTER_NONE,
						0x00000000,
						0);

					OrkAssert( SUCCEEDED( hr ) );

					D3DLOCKED_RECT lrect;
					hr = preadsurf->LockRect( & lrect, 0, D3DLOCK_READONLY );
					OrkAssert( SUCCEEDED( hr ) );
					hr = preadsurf->GetDesc( & RdDesc );
					OrkAssert( SUCCEEDED( hr ) );

					int ipitch = lrect.Pitch;
					const void* pdata = lrect.pBits;

					OrkAssert( (ipitch*ih) == (iw*ih*int(sizeof(float))) );
					memcpy( pfloat, pdata, ipitch*ih );
					ptex->SetImageData( (void*)pfloat );
				}
				break;
			case D3DFMT_A1R5G5B5:
				ptex->SetBytesPerPixel( 2 );
				break;
			case D3DFMT_X8R8G8B8:
				ptex->SetBytesPerPixel( 3 );
				break;
#if ! defined(_XBOX)
			case D3DFMT_R8G8B8:
				ptex->SetBytesPerPixel( 3 );
				break;
#endif
			case D3DFMT_DXT1:
			//case D3DFMT_DXT2:
			//case D3DFMT_DXT4:
			case D3DFMT_DXT3:
			case D3DFMT_DXT5:
				ptex->SetBytesPerPixel( 3 );
				OrkAssert( IsData==false );
				break;
			default:
				ptex->SetBytesPerPixel( 0 );
				break;
		}
		ptex->SetTexClass( Texture::ETEXCLASS_STATIC );
	}
	ptex->SetDirty(true);
	float ftime2 = ork::CSystem::GetRef().GetLoResRelTime();

	static float ftotaltime = 0.0f;
	static int iltotaltime = 0;

	ftotaltime += (ftime2-ftime1);

	int itotaltime = int(ftotaltime);

	//if( itotaltime > iltotaltime )
	{
		std::string outstr = ork::CreateFormattedString(
		"DDS AccumTime<%f>\n", ftotaltime );
		////OutputDebugString( outstr.c_str() );
		iltotaltime = itotaltime;
	}

	return true;	
}

///////////////////////////////////////////////////////////////////////////////
//bool GfxTargetDX::LoadTexture( Texture *ptex )
//{
//	AssetPath Filename = ptex->GetName();
//	return LoadTexture( Filename, ptex );
//}
///////////////////////////////////////////////////////////////////////////////

bool DxFrameBufferInterface::CaptureToTexture( const CaptureBuffer& capbuf, Texture& tex )
{
#if defined(_XBOX)
return false;
#else
	int iw = capbuf.GetWidth();
	int ih = capbuf.GetHeight();
	EBufferFormat efmt = capbuf.GetFormat();

	D3DXTexturePackage* package = (D3DXTexturePackage*) tex.GetTexIH();

	bool breset = false;

	if( package == 0 )
	{
		package = new D3DXTexturePackage;
		tex.SetTexIH( (void*) package );
		breset = true;
	}

	if( tex.GetWidth() != iw )
	{
		breset = true;
	}

	tex.SetWidth( iw );
	tex.SetHeight( ih );
	tex.SetDepth( 1 );
	tex.SetNumMipMaps( package->mImageInfo.MipLevels );

	HRESULT hr = D3D_OK;

	const void* pData = capbuf.GetData();

	IDirect3DTexture9* pdxtex = 0;
	int ipixsize = 0;

	if( 0 != package->mD3dTexureHandle )
	{	pdxtex = (IDirect3DTexture9*) package->mD3dTexureHandle;
	}
	else 
	{	DWORD dwUsage = D3DUSAGE_AUTOGENMIPMAP; // rendered tex's set here
		int imips = 0;
		switch( efmt )
		{	case lev2::EBUFFMT_RGBA32:
			{	hr = GetD3DDevice()->CreateTexture( iw, ih, imips, dwUsage, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &pdxtex, NULL );
				OrkAssert( SUCCEEDED( hr ) );
				package->mD3dTexureHandle = pdxtex;
				ipixsize = 4;
				break;
			}
			case lev2::EBUFFMT_RGBA128:
			{	hr = GetD3DDevice()->CreateTexture( iw, ih, imips, dwUsage, D3DFMT_A32B32G32R32F, D3DPOOL_MANAGED, &pdxtex, NULL );
				OrkAssert( SUCCEEDED( hr ) );
				package->mD3dTexureHandle = pdxtex;
				ipixsize = 16;
				break;
			}
			default:
				break;
		}
	}
	if( pdxtex )
	{
		D3DLOCKED_RECT d3dlr;
		hr = pdxtex->LockRect( 0, &d3dlr, 0, 0 );
		OrkAssert( SUCCEEDED( hr ) );
		U8* pDst = (U8*) d3dlr.pBits;
		int DPitch = d3dlr.Pitch;
		memcpy( pDst, pData, (iw*ih*ipixsize) );
		hr = pdxtex->UnlockRect (0);
		OrkAssert( SUCCEEDED( hr ) );
		tex.SetDirty( false );
	}

	return true;
#endif
}

///////////////////////////////////////////////////////////////////////////////

void DxTextureInterface::SaveTexture( const AssetPath& Filename, Texture* ptex )
{
	D3DXTexturePackage* package = (D3DXTexturePackage*) ptex->GetTexIH();

	OrkAssert( package );
	
	//LPDIRECT3DTEXTURE9 dxtex = ;
	
	HRESULT hr = D3DXSaveTextureToFile( Filename.ToAbsolute().c_str(), D3DXIFF_TGA, package->mD3dTexureHandle, 0 ); // D3DXIFF_HDR
	OrkAssert( SUCCEEDED( hr ) );

}

///////////////////////////////////////////////////////////////////////////////

} } //namespace ork::lev2

#endif



