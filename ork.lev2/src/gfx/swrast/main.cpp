////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#if 1
#include <ork/pch.h>
#include <ork/application/application.h>
#include <ork/file/filedev.h>
#include <ork/file/fileenv.h>
#include <ork/math/collision_test.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/qtui/qtui.h>
//#include <IL/il.h>
//#include <IL/ilut.h>
////////////////////////////////////////////////////////////

#include "lev3_test.h"
#include "render_graph.h"

#pragma comment( lib, "devil.lib" )
#pragma comment( lib, "ilu.lib" )
#pragma comment( lib, "ilut.lib" )

/*
namespace devil {

void InitDevIL()
{
	static bool binit = true;

	if( binit )
	{
		ilInit();
		int iver = ilGetInteger( IL_VERSION_NUM);
		if(	(iver < IL_VERSION) )
		{	printf("DevIL version is different...exiting!\n");
			OrkAssert(0);
		}
		binit = false;
	}
}

}*/
////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

//static const int kDEFAULTW = 1280+16;
//static const int kDEFAULTH = 720+38;
static const int kDEFAULTW = 640+16;
static const int kDEFAULTH = 360+38;

namespace ork
{ 
	void Init();	

	namespace lev3
	{
		void Init();

		namespace dx11
		{
			void Init();
		}
		namespace dx10
		{
			void Init();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void DemoApp::Render1()
{
#if 0
	D2D1_SIZE_F rtSize = m_pRenderTarget->GetSize();
	int width = static_cast<int>(rtSize.width);
	int height = static_cast<int>(rtSize.height);

	int inumpix = width*height;


	int i = 0;
	ork::fvec3 x0;
	ork::fvec3 x1;
	ork::fvec3 xy;

	mRenderGraph->Compute(mpThreadPool);
	const u8* psrcpixels = (const u8*) mRenderGraph->GetPixels();

	if( miNumAviFrames>0 )
	{
		devil::InitDevIL();
		ILuint image;
		ilGenImages(1, &image);
		ilBindImage(image);

		u8* newbuffer = new u8[ width*height*4 ];

		for( int iy=0; iy<height; iy++ )
		{
			int idsty = (height-1-iy);
			for( int ix=0; ix<width; ix++ )
			{
				int isrcindex = iy*width+ix;
				int idstindex = idsty*width+ix;

				const u8* psrc = psrcpixels+(isrcindex*4);
				u8* pdst = newbuffer+(idstindex*4);

				pdst[0] = psrc[2];
				pdst[1] = psrc[1];
				pdst[2] = psrc[0];
				pdst[3] = psrc[3];
			}
			//memcpy( newbuffer+idst, psrcpixels+isrc, width*4 );
		}

		if (!ilTexImage(width, height, 1, 4, IL_RGBA, IL_UNSIGNED_BYTE, (void*) newbuffer))
		{
			ILenum error = ilGetError();
			printf("Failed to create image (%04X)\n", error);
		}

		delete newbuffer;

		std::string pthstr = ork::CreateFormattedString( "u:\\movieframes\\mframe_%04d.tga", miFrameIndex );
		//ork::file::Path pth;
		//pth.
		ilEnable(IL_FILE_OVERWRITE);
		if (!ilSaveImage(pthstr.c_str()))
		{
			ILenum error = ilGetError();
			printf("Failed to save to %s (%04X)\n", pthstr.c_str(), error);
		}
		ilDeleteImage(image);
		/*int imgsiz2 = (width*height);

		for( int iy=0; iy<height; iy++ )
		{
			for( int ix=0; ix<width; ix++ )
			{
				int isrcindex = (ihm1-iy)*width+ix;
				int idstindex = iy*bmapW+ix;
				const u8* psrc = psrcpixels+(isrcindex*4);

				u8* ppix = ppixbase+(idstindex*3);
				ppix[0] = psrc[0];
				ppix[1] = psrc[1];
				ppix[2] = psrc[2];
			}
		}*/

		miNumAviFrames--;
		miFrameIndex++;
		if( miNumAviFrames==0 )
			EndMovie();

		printf( "FramesLeft<%d>\n", miNumAviFrames );
	}

	D2D1_RECT_U CopyRect;
	CopyRect.top = 0;
	CopyRect.left = 0;
	CopyRect.bottom = height;
	CopyRect.right = width;


	m_pRenderTarget->BeginDraw();
	m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
	m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

	if( psrcpixels)
	{
		mpBackBufferBitmap->CopyFromMemory( &CopyRect, (const void*) psrcpixels, width*4 );
	}

	D2D1_RECT_F rect;
	rect.left = 0;
	rect.top = 0;
	rect.right = rtSize.width;
	rect.bottom = rtSize.height;

	static float fphase = 0.0f;
	fphase += 0.1f;

	float fsc = 0.01f+sinf(fphase*2.23f)*0.01f;
	float fs = sinf(fphase)*fsc;
	float fc = cosf(fphase)*fsc;

	D2D1_MATRIX_3X2_F mtx;
	mtx._11 = 10.f/rtSize.width;
	mtx._12 = 0.0f;
	mtx._21 = 0.0f;
	mtx._22 = 10.f/rtSize.height;
	mtx._31 = 0.0f;
	mtx._32 = 0.0f;

	mpCheckerBrush->SetTransform( & mtx );
	mpCheckerBrush->SetOpacity( 1.0f );

	mpCheckerBrush->SetBitmap( mpBackBufferBitmap );
	//m_pRenderTarget->FillRectangle( & rect, mpCheckerBrush );
	m_pRenderTarget->DrawBitmap(mpBackBufferBitmap,&rect);
	m_pRenderTarget->Flush();

	//mtx._11 = fs;
	//mtx._12 = fc;
	//mtx._21 = fc;
	//mtx._22 = -fs;
	//mtx._31 = 0.0f;
	//mtx._32 = 0.0f;

	rect.top = 30.0f;
	rect.left = 30.0f;
	rect.bottom = rtSize.height-30.0f;
	rect.right = rtSize.width-30.0f;
	mpCheckerBrush->SetTransform( & mtx );
	mpCheckerBrush->SetOpacity( 0.5f );

	//m_pRenderTarget->FillRectangle( & rect, mpCheckerBrush );

	//m_pRenderTarget->DrawLine(
	//	D2D1::Point2F(0.0f, 0.0f),
		//D2D1::Point2F(rtSize.width, rtSize.height),
		//mpDefaultBrush,
		//0.5f
//	);

	HRESULT hr = m_pRenderTarget->EndDraw();

	return hr;
#endif
}

///////////////////////////////////////////////////////////////////////////////

void DemoApp::Render2()
{
#if 0
	D2D1_SIZE_F rtSize = m_pRenderTarget->GetSize();
	int width = static_cast<int>(rtSize.width);
	int height = static_cast<int>(rtSize.height);

	m_pRenderTarget->BeginDraw();

	D2D1::Matrix3x2F mtxS = D2D1::Matrix3x2F::Scale(40.0f,40.0f);
	D2D1::Matrix3x2F mtxT = D2D1::Matrix3x2F::Translation( float(width/2), float(height/2) );
	D2D1::Matrix3x2F mtx = mtxS*mtxT;

	m_pRenderTarget->SetTransform(mtx);
	m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));

	D2D1_POINT_2F p0, p1;
	D2D1_COLOR_F c;
	c.r=1.0f;
	c.g=1.0f;
	c.b=1.0f;
	c.a=1.0f;
	D2D1_COLOR_F c2;
	c2.r=1.0f;
	c2.g=0.0f;
	c2.b=1.0f;
	c2.a=1.0f;
	
	mpDefaultBrush->SetColor(c);
	mpDefaultBrush->SetOpacity( 0.5f );
//	mpDefaultBrush->SetTransform( D2D1::Matrix3x2F::Identity() );

	////////////////////////////////////////////////////
	// draw grid
	////////////////////////////////////////////////////
	for( float fx=-10.0f; fx<=10.0f; fx+=1.0f )
	{
		p0.x = fx;
		p0.y = -10.0f;
		p1.x = fx;
		p1.y = 10.0f;
		m_pRenderTarget->DrawLine( p0, p1, mpDefaultBrush, (fx==0.0f)?.15f:0.1f);
	}
	for( float fy=-10.0f; fy<=10.0f; fy+=1.0f )
	{
		p0.x = -10.0f;
		p0.y = fy;
		p1.x = 10.0f;
		p1.y = fy;
		m_pRenderTarget->DrawLine( p0, p1, mpDefaultBrush, (fy==0.0f)?.15f:0.1f );
	}
	for( float fx=-9.5f; fx<=9.5f; fx+=1.0f )
	{
		for( float fy=-9.5f; fy<=9.5f; fy+=1.0f )
		{
			float fd = .05f;
			D2D1_RECT_F r;
			r.left=fx-fd;
			r.right=fx+fd;
			r.top=fy-fd;
			r.bottom=fy+fd;
			m_pRenderTarget->DrawRectangle(&r,mpDefaultBrush, fd*.25f );
		}
	}
	m_pRenderTarget->Flush();
	////////////////////////////////////////////////////
	rend_ivtx vtxA, vtxB, vtxC;
	static float fphase = 0.0f;
	fphase += 0.001f;

	vtxA.mSX = std::sinf( fphase )*8.f;
	vtxA.mSY = std::cosf( fphase )*8.f;
	vtxA.mfDepth = 1.0f;
	vtxC.mSX = std::sinf( fphase+(PI*0.8f) )*8.f;
	vtxC.mSY = std::cosf( fphase+(PI*0.8f) )*7.f;
	vtxC.mfDepth = 1.0f;
	vtxB.mSX = std::sinf( fphase+(PI*1.5f)  )*6.f;
	vtxB.mSY = std::cosf( fphase+(PI*1.5f)  )*7.f;
	vtxB.mfDepth = 1.0f;

	if( vtxB.mSY > vtxC.mSY ) std::swap( vtxC, vtxB );
	if( vtxA.mSY > vtxB.mSY ) std::swap( vtxA, vtxB );
	if( vtxB.mSY > vtxC.mSY ) std::swap( vtxC, vtxB );
	if( vtxA.mSY > vtxB.mSY ) std::swap( vtxA, vtxB );

	////////////////////////////////////////////////////
	mpDefaultBrush->SetOpacity( 1.0f );
	////////////////////////////////////////////////////
	p0.x = vtxA.mSX;
	p0.y = vtxA.mSY;
	p1.x = vtxB.mSX;
	p1.y = vtxB.mSY;
	c.r=1.0f;
	c.g=0.0f;
	c.b=0.0f;
	c.a=1.0f;
	mpDefaultBrush->SetColor(c);
	m_pRenderTarget->DrawLine( p0, p1, mpDefaultBrush, 0.06f );
	p1.x = vtxC.mSX;
	p1.y = vtxC.mSY;
	c.r=1.0f;
	c.g=0.5f;
	c.b=0.0f;
	c.a=1.0f;
	mpDefaultBrush->SetColor(c);
	m_pRenderTarget->DrawLine( p0, p1, mpDefaultBrush, 0.06f );
	p0.x = vtxB.mSX;
	p0.y = vtxB.mSY;
	c.r=1.0f;
	c.g=0.0f;
	c.b=0.5f;
	c.a=1.0f;
	mpDefaultBrush->SetColor(c);
	m_pRenderTarget->DrawLine( p0, p1, mpDefaultBrush, 0.06f );
	m_pRenderTarget->Flush();
	////////////////////////////////////////////////////
	////////////////////////////////
	// guarantees
	////////////////////////////////
	// vtxA always guaranteed to be the top
	// vtxB.mSY >= vtxA.mSY
	// vtxC.mSY >= vtxB.mSY
	// vtxC.mSY > vtxA.mSY
	// no guarantees on left/right ordering
	////////////////////////////////
	int iYA = int(std::floor(vtxA.mSY+0.5f)); // iYA, iYC, iy are in pixel coordinate
	int iYC = int(std::floor(vtxC.mSY+0.5f)); // iYA, iYC, iy are in pixel coordinate
	OrkAssert(iYC>=iYA);
	////////////////////////////////
	// gradient set up
	////////////////////////////////
	float dyAB = (vtxB.mSY - vtxA.mSY); // vertical distance between A and B in fp pixel coordinates
	float dyAC = (vtxC.mSY - vtxA.mSY); // vertical distance between A and C in fp pixel coordinates
	float dyBC = (vtxC.mSY - vtxB.mSY); // vertical distance between B and C in fp pixel coordinates
	float dxAB = (vtxB.mSX - vtxA.mSX); // horizontal distance between A and B in fp pixel coordinates
	float dxAC = (vtxC.mSX - vtxA.mSX); // horizontal distance between A and C in fp pixel coordinates
	float dxBC = (vtxC.mSX - vtxB.mSX); // horizontal distance between B and C in fp pixel coordinates
	float dzAB = (vtxB.mfDepth - vtxA.mfDepth); // depth distance between A and B in fp pixel coordinates
	float dzAC = (vtxC.mfDepth - vtxA.mfDepth); // depth distance between A and C in fp pixel coordinates
	float dzBC = (vtxC.mfDepth - vtxB.mfDepth); // depth distance between B and C in fp pixel coordinates
	////////////////////////////////
	bool bABZERO = ((dyAB)==0.0f); // prevent division by zero
	bool bACZERO = ((dyAC)==0.0f); // prevent division by zero
	bool bBCZERO = ((dyBC)==0.0f); // prevent division by zero
	////////////////////////////////
	float dxABdy = bABZERO ? 0.0f : dxAB / dyAB;
	float dxACdy = bACZERO ? 0.0f : dxAC / dyAC;
	float dxBCdy = bBCZERO ? 0.0f : dxBC / dyBC;
	float dzABdy = bABZERO ? 0.0f : dzAB / dyAB;
	float dzACdy = bACZERO ? 0.0f : dzAC / dyAC;
	float dzBCdy = bBCZERO ? 0.0f : dzBC / dyBC;
	////////////////////////////////
	float yprestep = (std::floor(vtxA.mSY)+0.5f)-vtxA.mSY;
	float yprestepB = (std::floor(vtxB.mSY)+0.5f)-vtxB.mSY;
	//////////////////////////////////
	// raster loop
	////////////////////////////////
	for( int iY=iYA; iY<iYC; iY++ )
	{	float pixel_center_Y = float(iY)+0.5f;
		///////////////////////////////////
		// edge selection (AC always active, AB or BC depending upon y)
		///////////////////////////////////
		bool bAB = pixel_center_Y<=vtxB.mSY;
		float dXdyB = bAB ? dxABdy : dxBCdy;
		float yB = bAB ? vtxA.mSY : vtxB.mSY;
		float xB = bAB ? vtxA.mSX : vtxB.mSX;
		float fXA = vtxA.mSX + dxACdy*(pixel_center_Y-vtxA.mSY);
		float fXB = xB + dXdyB*(pixel_center_Y-yB);
		///////////////////////////////////
		c.r=0.0f;
		c.g=1.0f;
		c.b=0.0f;
		c.a=1.0f;
		mpDefaultBrush->SetColor(c);
		mpDefaultBrush->SetOpacity( 0.2f );
		p0.x = fXA;
		p0.y = pixel_center_Y;
		p1.x = fXB;
		p1.y = pixel_center_Y;
		m_pRenderTarget->DrawLine( p0, p1, mpDefaultBrush, 0.06f );
		m_pRenderTarget->Flush();
		///////////////////////////////////
		// calculate left and right boundaries
		float fxLEFT = fXA;
		float fxRIGHT = fXB;
		///////////////////////////////////
		float fzLEFT = 0.0f;
		float fzRIGHT = 0.0f;
		///////////////////////////////////
		// enforce left to right
		///////////////////////////////////
		if( fxLEFT>fxRIGHT ) 
		{	std::swap(fxLEFT,fxRIGHT);
			std::swap(fzLEFT,fzRIGHT);
		}
		int ixLEFT = int(std::floor(fxLEFT+0.5f));
		int ixRIGHT = int(std::floor(fxRIGHT+0.5f));
		///////////////////////////////////
		float fdZdX = (fzRIGHT-fzLEFT)/float(ixRIGHT-ixLEFT);
		float fZ = fzLEFT; 
		///////////////////////////////////
		c.r=1.0f;
		c.g=0.0f;
		c.b=1.0f;
		c.a=1.0f;
		mpDefaultBrush->SetColor(c);
		mpDefaultBrush->SetOpacity( 1.0f );
		p0.x = float(ixLEFT)+0.4f;
		p0.y = float(iY);
		p1.x = float(ixLEFT)+0.4f;
		p1.y = float(iY)+0.5f;
		m_pRenderTarget->DrawLine( p0, p1, mpDefaultBrush, 0.06f );
		m_pRenderTarget->Flush();
		c.r=0.0f;
		c.g=1.0f;
		c.b=1.0f;
		c.a=1.0f;
		mpDefaultBrush->SetColor(c);
		mpDefaultBrush->SetOpacity( 1.0f );
		p0.x = float(ixRIGHT)+0.6f;
		p0.y = float(iY);
		p1.x = float(ixRIGHT)+0.6f;
		p1.y = float(iY)+0.5f;
		m_pRenderTarget->DrawLine( p0, p1, mpDefaultBrush, 0.06f );
		m_pRenderTarget->Flush();
		///////////////////////////////////
		c.r=1.0f;
		c.g=1.0f;
		c.b=1.0f;
		c.a=1.0f;
		mpDefaultBrush->SetColor(c);
		mpDefaultBrush->SetOpacity( 1.0f );
		p0.x = fxLEFT;
		p0.y = float(iY)+0.5f;
		p1.x = fxLEFT;
		p1.y = float(iY)+1.0f;
		m_pRenderTarget->DrawLine( p0, p1, mpDefaultBrush, 0.06f );
		m_pRenderTarget->Flush();
		c.r=1.0f;
		c.g=1.0f;
		c.b=0.0f;
		c.a=1.0f;
		mpDefaultBrush->SetColor(c);
		mpDefaultBrush->SetOpacity( 1.0f );
		p0.x = fxRIGHT;
		p0.y = float(iY)+0.5f;
		p1.x = fxRIGHT;
		p1.y = float(iY)+1.0f;
		m_pRenderTarget->DrawLine( p0, p1, mpDefaultBrush, 0.06f );
		m_pRenderTarget->Flush();
		///////////////////////////////////
		for( int iX=ixLEFT; iX<ixRIGHT; iX++ )
		{	
			c.r=0.0f;
			c.g=0.0f;
			c.b=1.0f;
			c.a=1.0f;
			mpDefaultBrush->SetColor(c);
			mpDefaultBrush->SetOpacity( 0.3f );
			D2D1_RECT_F r;
			r.left=float(iX);
			r.right=float(iX+1);
			r.top=float(iY);
			r.bottom=float(iY+1);
			m_pRenderTarget->DrawRectangle(&r,mpDefaultBrush, 0.25f );
			m_pRenderTarget->Flush();

			fZ += fdZdX;
		}
		///////////////////////////////////
	}	
	//////////////////////////////////////////////////////////
	m_pRenderTarget->Flush();

	HRESULT hr = m_pRenderTarget->EndDraw();

	return hr;

#endif
}

///////////////////////////////////////////////////////////////////////////////

#if 0
HRESULT DemoApp::OnRender()
{
	HRESULT hr = S_OK;

	hr = CreateDeviceResources();

	if (SUCCEEDED(hr))
	{
		hr = Render1();	

	}
	if (hr == D2DERR_RECREATE_TARGET)
	{
		hr = S_OK;
		DiscardDeviceResources();
	}
	return hr;
}
#endif

///////////////////////////////////////////////////////////////////////////////

int lev3_test_main( int argc, char** argv )
{
	////////////////////////////////////////////////////
	ork::Application app;
    ApplicationStack::Push(&app);
	//ork::lev2::CQNoMocBase::MocInitAll();
	ork::rtti::Class::InitializeClasses();
	ork::lev2::GfxTargetCreationParams CreationParams;
	CreationParams.miNumSharedVerts = 512<<10;
	ork::lev2::GfxEnv::GetRef().PushCreationParams(CreationParams);
	////////////////////////////////////////////////////
    QApplication qapp(argc,argv);
    DemoApp dapp(640,480);


    while(1)
    {
        qapp.exec();
    }
	{	//ork::Init();

		//cm.RegisterClassWithParent( ork::dataflow::inplug<rend_primbuffer>::GetRttiStatic() );
		//cm.RegisterClassWithParent( ork::dataflow::inplug<rend_pixelbuffer>::GetRttiStatic() );
		//cm.RegisterClassWithParent( ork::dataflow::inplug<RenderData>::GetRttiStatic() );
		//cm.RegisterClassWithParent( ork::dataflow::inplug<GeoPrimTable>::GetRttiStatic() );
		//cm.RegisterClassWithParent( ork::dataflow::inplug<SlicerData>::GetRttiStatic() );
		//cm.RegisterClassWithParent( ork::dataflow::outplug<rend_primbuffer>::GetRttiStatic() );
		//cm.RegisterClassWithParent( ork::dataflow::outplug<rend_pixelbuffer>::GetRttiStatic() );
		//cm.RegisterClassWithParent( ork::dataflow::outplug<RenderData>::GetRttiStatic() );
		//cm.RegisterClassWithParent( ork::dataflow::outplug<GeoPrimTable>::GetRttiStatic() );
		//cm.RegisterClassWithParent( ork::dataflow::outplug<SlicerData>::GetRttiStatic() );

	//ork::lev3::Init();
//		cm.Init();

//		if( SUCCEEDED(CoInitialize(NULL)) )
//		{
//			DemoApp app( kDEFAULTW, kDEFAULTH );
//
//			if (SUCCEEDED(app.Initialize()))
//			{
//				app.RunMessageLoop();
//			}
//			CoUninitialize();
//		}
		
	}
	////////////////////////////////////////////////////
	//ork::gstack<ork::ClassManager>::pop();
	////////////////////////////////////////////////////
	return 0;
}
#endif
