////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>

///////////////////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/gfxprimitives.h>
#include <orktool/toolcore/selection.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/texman.h>
#include "vpRenderer.h"
#include "vpSceneEditor.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool {
///////////////////////////////////////////////////////////////////////////////

struct NormalToSphereCoordMapper
{
	fmtx4 NormalToSphMapMtx;

	NormalToSphereCoordMapper()
	{
		NormalToSphMapMtx.SetElemYX( 0,0,  0.5f );
		NormalToSphMapMtx.SetElemYX( 1,1, -0.5f );
		NormalToSphMapMtx.SetElemYX( 2,2,  0.0f );

		NormalToSphMapMtx.SetElemYX( 0,3,  0.5f );
		NormalToSphMapMtx.SetElemYX( 1,3, -0.5f );
	}

	fvec2 MapBottom( const fvec3& vin )
	{
		fvec3 vdir = vin;
		vdir.Normalize();
		fvec2 rval;

		rval.SetX( vdir.GetX()/(1.0-vdir.GetY()) );
		rval.SetY( vdir.GetZ()/(1.0-vdir.GetY()) );

		return rval;
	}
	fvec2 MapTop( const fvec3& vin )
	{
		fvec3 vdir = vin;
		vdir.Normalize();
		fvec2 rval;

		rval.SetX( -vdir.GetX()/(1.0+vdir.GetY()) );
		rval.SetY( -vdir.GetZ()/(1.0+vdir.GetY()) );

		return rval;
	}

	U32 ToColor( const fvec3& vin )
	{
		fvec3 vdir = vin;
		vdir.Normalize();
		vdir = (vdir*0.5f)+fvec3(0.5f,0.5f,0.5f);

		fvec4 v4( vdir, 1.0f );

		return v4.GetARGBU32();
	}

};

///////////////////////////////////////////////////////////////////////////////

#if 0
static void RenderFace( lev2::Context* pTARG, const fcolor4 & clr, lev2::Texture* ptex,
						int iface, float fstep,
						float fumin, float fumax,
						float fvmin, float fvmax,
						const fmtx4& uvmatrix,
						bool btop )
{

	static NormalToSphereCoordMapper Mapper;


	static lev2::GfxMaterial3DSolid MyMaterial( pTARG );

	static ork::lev2::CVtxBuffer<ork::lev2::SVtxV12C4T16> WarpVtxBuffer( 32768, 0, lev2::EPrimitiveType::TRIANGLES );

	MyMaterial.SetTexture( ptex );
	MyMaterial.SetColorMode( lev2::GfxMaterial3DSolid::EMODE_TEX_COLOR );

	static bool binit = true;

	if( binit )
	{
		pTARG->VtxBuf_Init( WarpVtxBuffer );
		binit = false;
	}

	pTARG->GBI()->LockVB( WarpVtxBuffer );

	for( float fu=fumin; fu<fumax; fu+=fstep )
	{
		float fu2 = fu+fstep;

		for( float fv=fvmin; fv<fvmax; fv+=fstep )
		{
			float fv2 = fv+fstep;

			fvec3 vface00;
			fvec3 vface10;
			fvec3 vface01;
			fvec3 vface11;

			switch( iface )
			{
				//////////////////////////////////
				// +/- Y
				//////////////////////////////////
				case 0:
					vface00 = fvec3( fu, 1.0f, fv );
					vface10 = fvec3( fu2, 1.0f, fv );
					vface01 = fvec3( fu, 1.0f, fv2 );
					vface11 = fvec3( fu2, 1.0f, fv2 );
					break;
				case 1:
					vface00 = fvec3( fu, -1.0f, fv );
					vface10 = fvec3( fu2, -1.0f, fv );
					vface01 = fvec3( fu, -1.0f, fv2 );
					vface11 = fvec3( fu2, -1.0f, fv2 );
					break;

				//////////////////////////////////
				// +/- Z
				//////////////////////////////////
				case 2:
					vface00 = fvec3( fu, fv, -1.0f );
					vface10 = fvec3( fu2, fv, -1.0f );
					vface01 = fvec3( fu, fv2, -1.0f );
					vface11 = fvec3( fu2, fv2, -1.0f );
					break;
				case 3:
					vface00 = fvec3( fu, fv, 1.0f );
					vface10 = fvec3( fu2, fv, 1.0f );
					vface01 = fvec3( fu, fv2, 1.0f );
					vface11 = fvec3( fu2, fv2, 1.0f );
					break;

				//////////////////////////////////
				// +/- X
				//////////////////////////////////
				case 4: // top
					vface00 = fvec3( 1.0f, fu, fv );
					vface10 = fvec3( 1.0f, fu2, fv );
					vface01 = fvec3( 1.0f, fu, fv2 );
					vface11 = fvec3( 1.0f, fu2, fv2 );
					break;
				case 5:
					vface00 = fvec3( -1.0f, fu, fv );
					vface10 = fvec3( -1.0f, fu2, fv );
					vface01 = fvec3( -1.0f, fu, fv2 );
					vface11 = fvec3( -1.0f, fu2, fv2 );
					break;

			}

			fvec2 vface00_mapped = btop ? Mapper.MapTop( vface00 ) : Mapper.MapBottom( vface00 );
			fvec2 vface10_mapped = btop ? Mapper.MapTop( vface10 ) : Mapper.MapBottom( vface10 );
			fvec2 vface01_mapped = btop ? Mapper.MapTop( vface01 ) : Mapper.MapBottom( vface01 );
			fvec2 vface11_mapped = btop ? Mapper.MapTop( vface11 ) : Mapper.MapBottom( vface11 );

			lev2::SVtxV12C4T16 vtx00, vtx10, vtx01, vtx11;

			fvec4 vUV00 = fvec4( fu, fv, 0.0f, 0.0f ).Transform( uvmatrix );
			fvec4 vUV10 = fvec4( fu2, fv, 0.0f, 0.0f ).Transform( uvmatrix );
			fvec4 vUV01 = fvec4( fu, fv2, 0.0f, 0.0f ).Transform( uvmatrix );
			fvec4 vUV11 = fvec4( fu2, fv2, 0.0f, 0.0f ).Transform( uvmatrix );


			vtx00.miX = vface00_mapped.GetX();
			vtx00.miY = vface00_mapped.GetY();
			vtx00.miZ = 0.0f;
			vtx00.mfU = vUV00.GetX();
			vtx00.mfV = vUV00.GetY();
			vtx00.muColor = Mapper.ToColor(vface00);

			vtx01.miX = vface01_mapped.GetX();
			vtx01.miY = vface01_mapped.GetY();
			vtx01.miZ = 0.0f;
			vtx01.mfU = vUV01.GetX();
			vtx01.mfV = vUV01.GetY();
			vtx01.muColor = Mapper.ToColor(vface01);

			vtx10.miX = vface10_mapped.GetX();
			vtx10.miY = vface10_mapped.GetY();
			vtx10.miZ = 0.0f;
			vtx10.mfU = vUV10.GetX();
			vtx10.mfV = vUV10.GetY();
			vtx10.muColor = Mapper.ToColor(vface10);

			vtx11.miX = vface11_mapped.GetX();
			vtx11.miY = vface11_mapped.GetY();
			vtx11.miZ = 0.0f;
			vtx11.mfU = vUV11.GetX();
			vtx11.mfV = vUV11.GetY();
			vtx11.muColor = Mapper.ToColor(vface11);

			WarpVtxBuffer.AddVertex( vtx00 );
			WarpVtxBuffer.AddVertex( vtx10 );
			WarpVtxBuffer.AddVertex( vtx11 );

			WarpVtxBuffer.AddVertex( vtx00 );
			WarpVtxBuffer.AddVertex( vtx11 );
			WarpVtxBuffer.AddVertex( vtx01 );
		}
	}
	pTARG->GBI()->UnLockVB( WarpVtxBuffer );
	/////////////////////////////////////////////
	pTARG->PushModColor( clr );
	pTARG->BindMaterial( & MyMaterial );
	{
		pTARG->GBI()->DrawPrimitive( WarpVtxBuffer, lev2::EPrimitiveType::TRIANGLES );
	}
	pTARG->PopModColor();
	/////////////////////////////////////////////

}
#endif

///////////////////////////////////////////////////////////////////////////////

#if 0
void SceneEditorVP::SaveCubeMap()
{
	const lev2::EzUiCam* persp = mPerspCam;
	PickBuffer<SceneEditorVP>* pb = _pickbuffer;
	lev2::Context* pTEXTARG = _pickbuffer->context();

	const fvec3 Locator = mPerspCam->CamFocus.xyz();


	float ffar = 1000.0f;
	float fnear = 1.0f;
	float fang = 90.0f;

	///////////////////////////////////////
	// setup 6 cardinal facing cameras that cover the whole sphere

	CameraData CardinalCameras[6]; // top bottom front back left right

	ork::fmtx4 ProjMat = pTEXTARG->MTXI()->Persp( fang, 1.0f, fnear, ffar );

	CardinalCameras[0].mMatView = pTEXTARG->MTXI()->LookAt(	Locator, Locator+fvec3(0.0f,1.0f,0.0f), fvec3(0.0f,0.0f,1.0f) );
	CardinalCameras[1].mMatView = pTEXTARG->MTXI()->LookAt(	Locator, Locator+fvec3(0.0f,-1.0f,0.0f), fvec3(0.0f,0.0f,1.0f) );

	CardinalCameras[2].mMatView = pTEXTARG->MTXI()->LookAt(	Locator, Locator+fvec3(0.0f,0.0f,1.0f), fvec3(0.0f,1.0f,0.0f) );
	CardinalCameras[3].mMatView = pTEXTARG->MTXI()->LookAt(	Locator, Locator+fvec3(0.0f,0.0f,-1.0f), fvec3(0.0f,1.0f,0.0f) );

	CardinalCameras[4].mMatView = pTEXTARG->MTXI()->LookAt(	Locator, Locator+fvec3(1.0f,0.0f,0.0f), fvec3(0.0f,1.0f,0.0f) );
	CardinalCameras[5].mMatView = pTEXTARG->MTXI()->LookAt(	Locator, Locator+fvec3(-1.0f,0.0f,0.0f), fvec3(0.0f,1.0f,0.0f) );

	for( int icam=0; icam<6; icam++ )
	{
		CardinalCameras[icam].mMatProj = ProjMat;
		CardinalCameras[icam].mAper = fang;
		CardinalCameras[icam].mNear = fnear;
		CardinalCameras[icam].mFar = ffar;

		CardinalCameras[icam].computeMatrices();
	}

	lev2::Texture* FaceTextures[6];

	///////////////////////////////////////
	// render offscreen using each camera, save each to a tga file
	for( int icam=0; icam<6; icam++ )
	///////////////////////////////////////
	{
		pTEXTARG->BeginFrame();
		{
			ent::SceneData *pscene = mEditor.mpScene;

			///////////////////////////////////////////////////////////////////////////
			mRenderer->setContext( pTEXTARG );
			lev2::RenderContextFrameData ContextData( *mRenderer, & CardinalCameras[icam] );
			ContextData.SetRenderingMode( lev2::RenderContextFrameData::ERENDMODE_STANDARD );
			///////////////////////////////////////////////////////////////////////////
			if( pscene )
			{
				mRenderer->SetActiveDisplayLayer( 0 );
				mRenderer->setCameraData( & CardinalCameras[icam] );
				mSceneView.Render( ContextData, mEditor.mpSimulation );
			}

			pTEXTARG->MTXI()->PushPMatrix( CardinalCameras[icam].mMatProj );
			pTEXTARG->MTXI()->PushVMatrix( CardinalCameras[icam].mMatView );
			pTEXTARG->MTXI()->PushMMatrix( fmtx4::Identity );
			{
				pTEXTARG->BindMaterial( lev2::GfxEnv::GetDefault3DMaterial() );
				pTEXTARG->PushModColor( fcolor4::White() );
				{
					mRenderer->drawEnqueuedRenderables();
					//ContextData.GetCamera()->AttachViewport( this );
					//mPerspCam->draw( pTEXTARG );
				}
				pTEXTARG->PopModColor();

			}
			pTEXTARG->MTXI()->PopMMatrix();
			pTEXTARG->MTXI()->PopVMatrix();
			pTEXTARG->MTXI()->PopPMatrix();

		}
		pTEXTARG->EndFrame();

		//////////////////////////////////////////////////////////////
		// write it to disk

		std::string FileName = CreateFormattedString( "yo%d.tga", icam );

		file::Path TexPath( FileName.c_str() );

		pTEXTARG->Capture( TexPath );

		FaceTextures[icam] = lev2::Texture::LoadUnManaged( TexPath );

	}

	///////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////

	file::Path TexPathBot( "yo_dualparamap_bot.tga" );
	file::Path TexPathTop( "yo_dualparamap_top.tga" );

	///////////////////////////////////////////////////////////
	// render offscreen creating a spheremap from the 6 faces
	///////////////////////////////////////////////////////////

	const float fstep = 1.0f / 30.0f;

	////////////////////////////////////////
	fmtx4 MatInvY, MatInvX, MatRotZN90;
	fmtx4 MatBiasY, MatBiasX;
	MatInvX.Scale( -1.0f, 1.0f, 1.0f );
	MatInvY.Scale( 1.0f, -1.0f, 1.0f );
	MatRotZN90.RotateZ( -90.0f * DTOR );
	MatBiasY.SetTranslation( 0.0f, 1.0f, 0.0f );
	MatBiasX.SetTranslation( 1.0f, 0.0f, 0.0f );
	////////////////////////////////////////

	pTEXTARG->BeginFrame();
	{
		lev2::GfxMaterial3DSolid tmat(pTEXTARG);
		tmat._rasterstate.SetAlphaTest( lev2::EALPHATEST_OFF, 0.0f );
		tmat._rasterstate.SetDepthTest( lev2::EDEPTHTEST_OFF );

		mRenderer->setContext( pTEXTARG );

		pTEXTARG->MTXI()->PushPMatrix( pTEXTARG->Ortho( -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f ) );
		pTEXTARG->MTXI()->PushVMatrix( fmtx4::Identity );
		pTEXTARG->MTXI()->PushMMatrix( fmtx4::Identity );
		pTEXTARG->pushViewport( SRect(0,0,1024,1024) );
		{

			fmtx4 TexMatrix[6];

			TexMatrix[2] = (MatInvY*MatInvX)*(MatBiasY*MatBiasX);
			TexMatrix[3] = MatInvY*MatBiasY;

			TexMatrix[4] = MatRotZN90*MatInvX*MatBiasX;
			TexMatrix[5] = MatRotZN90;

			pTEXTARG->BindMaterial( & tmat );
			{
				RenderFace( pTEXTARG, fcolor4::Green(),	FaceTextures[0], 0, fstep, -1.0f, 1.0f, -1.0f, 1.0f, TexMatrix[0], true );

				RenderFace( pTEXTARG, fcolor4::Blue(),	FaceTextures[2], 2, fstep, -1.0f, 1.0f, 0.0f, 1.0f, TexMatrix[2], true );
				RenderFace( pTEXTARG, fcolor4::Blue(),	FaceTextures[3], 3, fstep, -1.0f, 1.0f, 0.0f, 1.0f, TexMatrix[3], true );

				RenderFace( pTEXTARG, fcolor4::Red(),	FaceTextures[4], 4, fstep, 0.0f, 1.0f, -1.0f, 1.0f, TexMatrix[4], true );
				RenderFace( pTEXTARG, fcolor4::Red(),	FaceTextures[5], 5, fstep, 0.0f, 1.0f, -1.0f, 1.0f, TexMatrix[5], true );
			}

		}
		pTEXTARG->popViewport();
		pTEXTARG->MTXI()->PopMMatrix();
		pTEXTARG->MTXI()->PopVMatrix();
		pTEXTARG->MTXI()->PopPMatrix();

	}
	pTEXTARG->EndFrame();

	pTEXTARG->Capture( TexPathTop );

	///////////////////////////////////////////////////////////
	// render offscreen creating a spheremap from the 6 faces
	///////////////////////////////////////////////////////////

	pTEXTARG->BeginFrame();
	{
		lev2::GfxMaterial3DSolid tmat(pTEXTARG);
		tmat._rasterstate.SetAlphaTest( lev2::EALPHATEST_OFF, 0.0f );
		tmat._rasterstate.SetDepthTest( lev2::EDEPTHTEST_OFF );

		///////////////////////////////////////////////////////////////////////////
		mRenderer->setContext( pTEXTARG );

		pTEXTARG->MTXI()->PushPMatrix( pTEXTARG->Ortho( -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f ) );
		pTEXTARG->MTXI()->PushVMatrix( fmtx4::Identity );
		pTEXTARG->MTXI()->PushMMatrix( fmtx4::Identity );
		pTEXTARG->pushViewport( SRect(0,0,1024,1024) );
		{
			fmtx4 TexMatrix[6];

			TexMatrix[1] = (MatInvY*MatInvX)*(MatBiasY*MatBiasX);

			TexMatrix[2] = (MatInvY)*(MatBiasY);
			TexMatrix[3] = (MatInvY*MatInvX)*(MatBiasY*MatBiasX);

			TexMatrix[4] = MatRotZN90;
			TexMatrix[5] = MatRotZN90*MatInvX*MatBiasX;

			pTEXTARG->BindMaterial( & tmat );
			{
				RenderFace( pTEXTARG, fcolor4::Green(),	FaceTextures[1], 1, fstep, -1.0f, 1.0f, -1.0f, 1.0f, TexMatrix[0], false );

				RenderFace( pTEXTARG, fcolor4::Blue(),	FaceTextures[2], 2, fstep, -1.0f, 1.0f, -1.0f, 0.0f, TexMatrix[2], false );
				RenderFace( pTEXTARG, fcolor4::Blue(),	FaceTextures[3], 3, fstep, -1.0f, 1.0f, -1.0f, 0.0f, TexMatrix[3], false );

				RenderFace( pTEXTARG, fcolor4::Red(),	FaceTextures[5], 4, fstep, -1.0f, 0.0f, -1.0f, 1.0f, TexMatrix[4], false );
				RenderFace( pTEXTARG, fcolor4::Red(),	FaceTextures[4], 5, fstep, -1.0f, 0.0f, -1.0f, 1.0f, TexMatrix[5], false );
			}

		}
		pTEXTARG->popViewport();
		pTEXTARG->MTXI()->PopMMatrix();
		pTEXTARG->MTXI()->PopVMatrix();
		pTEXTARG->MTXI()->PopPMatrix();

	}
	pTEXTARG->EndFrame();

	pTEXTARG->Capture( TexPathBot );

	//////////////////////////////////////////////////////////////
	// invalidate pickbuffer, now that we trashed it
	//////////////////////////////////////////////////////////////

	pb->SetDirty(true);
}
#endif

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
