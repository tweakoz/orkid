////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/rtgroup.h>

#if defined( ORK_CONFIG_QT )
#include <ork/lev2/qtui/qtui.h>
#endif
#include <ork/rtti/downcast.h>
#include <ork/kernel/prop.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/ui/viewport.h>

INSTANTIATE_TRANSPARENT_RTTI( ork::lev2::GfxBuffer, "ork::lev2::GfxBuffer" );

namespace ork { namespace lev2 {

/////////////////////////////////////////////////////////////////////////

void GfxBuffer::Describe()
{
}

GfxBuffer::GfxBuffer( GfxBuffer *Parent,
						int iX, int iY, int iW, int iH,
						EBufferFormat efmt,
						ETargetType etgttype,
						const std::string & name )
	: mpContext(nullptr)
	, mParent( Parent )
	, miWidth( iW )
	, miHeight( iH )
	, mbDirty( true )
	, msName( name )
	, meFormat( efmt )
	, meTargetType( etgttype )
	, mParentRtGroup( nullptr )
	, mpTexture( nullptr )
	, mRootWidget( nullptr )
	, mpMaterial( nullptr )
	, mbSizeIsDirty(true)
{
}

GfxBuffer::~GfxBuffer()
{
//	if( mpContext )
//	{
//		delete mpContext;
//	}
}

/////////////////////////////////////////////////////////////////////////

void GfxBuffer::Resize( int ix, int iy, int iw, int ih )
{
	GetContext()->SetSize( ix, iy, iw, ih );

	if( GetRootWidget() )
	{
		GetRootWidget()->SetRect(ix,iy,iw,ih);
		//GetRootWidget()->OnResize();
	}
}

/////////////////////////////////////////////////////////////////////////

GfxWindow::GfxWindow( int iX, int iY, int iW, int iH, const std::string & name, void *pdata )
	: GfxBuffer( 0, iX, iY, iW, iH, EBUFFMT_RGBA32, ETGTTYPE_WINDOW, name )
	, mpCTXBASE( 0 )
{
	gGfxEnv.SetMainWindow( this );
}

GfxWindow::~GfxWindow()
{
	if( mpCTXBASE )
	{
		mpCTXBASE = 0;
	}

}

/////////////////////////////////////////////////////////////////////////

void GfxBuffer::BeginFrame( void )
{
	if( 0 == mpContext )
	{
		GetContext();
	}
	mpContext->BeginFrame();
}

/////////////////////////////////////////////////////////////////////////

void GfxBuffer::EndFrame( void )
{
	if( mpContext )
	{
		mpContext->EndFrame();
	}
}

/////////////////////////////////////////////////////////////////////////

GfxTarget * GfxBuffer::GetContext( void ) const
{
//	if( 0 == mpContext )
	//{
		//CreateContext();
	//}

	return mpContext;
}

/////////////////////////////////////////////////////////////////////////

void GfxBuffer::CreateContext()
{
	mpContext = ork::rtti::safe_downcast<GfxTarget*>(GfxEnv::GetRef().GetTargetClass()->CreateObject());
	mpContext->InitializeContext( this );
}

/////////////////////////////////////////////////////////////////////////

void GfxWindow::CreateContext()
{
	mpContext = ork::rtti::safe_downcast<GfxTarget*>(GfxEnv::GetRef().GetTargetClass()->CreateObject());
	if( mpCTXBASE )
	{
		mpCTXBASE->SetTarget( mpContext );
	}
	mpContext->InitializeContext( this, mpCTXBASE );
}

/////////////////////////////////////////////////////////////////////////

void GfxBuffer::RenderMatOrthoQuad( const SRect& ViewportRect, const SRect& QuadRect, GfxMaterial *pmat, float fu0, float fv0, float fu1, float fv1, float *uv2, const CColor4& clr )
{
	static SRasterState DefaultRasterState;
	auto ctx = GetContext();
	auto mtxi = ctx->MTXI();
	auto fbi = ctx->FBI();

	// align source pixels to target pixels if sizes match
	float fx0 = float(QuadRect.miX);
	float fy0 = float(QuadRect.miY);
	float fx1 = float(QuadRect.miX2);
	float fy1 = float(QuadRect.miY2);
	float fvx0 = float(ViewportRect.miX);
	float fvy0 = float(ViewportRect.miY);
	float fvx1 = float(ViewportRect.miX2);
	float fvy1 = float(ViewportRect.miY2);

	float zeros[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	if (NULL == uv2)
		uv2 = zeros;

	mtxi->PushPMatrix( mtxi->Ortho(fvx0,fvx1,fvy0,fvy1,0.0f,1.0f) );
	mtxi->PushVMatrix( CMatrix4::Identity );
	mtxi->PushMMatrix( CMatrix4::Identity );
	ctx->RSI()->BindRasterState( DefaultRasterState, true );
	fbi->PushViewport( ViewportRect );
	fbi->PushScissor( ViewportRect );
	{	// Draw Full Screen Quad with specified material
		ctx->PushMaterial( pmat );
		ctx->FXI()->InvalidateStateBlock();
		ctx->PushModColor( clr );
		{	
			DynamicVertexBuffer<SVtxV12C4T16> &vb = GfxEnv::GetSharedDynamicVB();

			//U32 uc = clr.GetBGRAU32();
			U32 uc = clr.GetVtxColorAsU32();
			ork::lev2::VtxWriter<SVtxV12C4T16> vw;
			vw.Lock( GetContext(), &vb, 6 );
				vw.AddVertex(SVtxV12C4T16(fx0, fy0, 0.0f, fu0, fv0, uv2[0], uv2[1], uc));
				vw.AddVertex(SVtxV12C4T16(fx1, fy1, 0.0f, fu1, fv1, uv2[4], uv2[5], uc));
				vw.AddVertex(SVtxV12C4T16(fx1, fy0, 0.0f, fu1, fv0, uv2[2], uv2[3], uc));

				vw.AddVertex(SVtxV12C4T16(fx0, fy0, 0.0f, fu0, fv0, uv2[0], uv2[1], uc));
				vw.AddVertex(SVtxV12C4T16(fx0, fy1, 0.0f, fu0, fv1, uv2[6], uv2[7], uc));
				vw.AddVertex(SVtxV12C4T16(fx1, fy1, 0.0f, fu1, fv1, uv2[4], uv2[5], uc));
			vw.UnLock(GetContext());

			ctx->GBI()->DrawPrimitive(vw, EPRIM_TRIANGLES);
		}
		ctx->PopModColor();
		ctx->PopMaterial();
	}
	fbi->PopScissor();
	fbi->PopViewport();
	mtxi->PopPMatrix();
	mtxi->PopVMatrix();
	mtxi->PopMMatrix();
}

OrthoQuad::OrthoQuad()
	: mfrot(0.0f)
	, mfu0a(0.0f)
	, mfu1a(0.0f)
	, mfu0b(0.0f)
	, mfu1b(0.0f)
	, mfv0a(0.0f)
	, mfv1a(0.0f)
	, mfv0b(0.0f)
	, mfv1b(0.0f)
{
}

void GfxBuffer::RenderMatOrthoQuads( const OrthoQuads& oquads )
{
	int inumquads = oquads.miNumQuads;
	const SRect& ViewportRect = oquads.mViewportRect;
	const SRect& OrthoRect = oquads.mOrthoRect;
	GfxMaterial* pmtl = oquads.mpMaterial;
	const OrthoQuad* pquads = oquads.mpQUADS;
	
	if( 0 == inumquads ) return;
	
	static SRasterState DefaultRasterState;
	
	// align source pixels to target pixels if sizes match
	float fvx0 = float(OrthoRect.miX);
	float fvy0 = float(OrthoRect.miY);
	float fvx1 = float(OrthoRect.miX2);
	float fvy1 = float(OrthoRect.miY2);

	GetContext()->MTXI()->PushPMatrix( GetContext()->MTXI()->Ortho(fvx0,fvx1,fvy0,fvy1,0.0f,1.0f) );
	GetContext()->MTXI()->PushVMatrix( CMatrix4::Identity );
	GetContext()->MTXI()->PushMMatrix( CMatrix4::Identity );
	GetContext()->RSI()->BindRasterState( DefaultRasterState, true );
	GetContext()->FBI()->PushViewport( ViewportRect );
	GetContext()->FBI()->PushScissor( ViewportRect );
	{	// Draw Full Screen Quad with specified material
		GetContext()->BindMaterial( pmtl );
		GetContext()->FXI()->InvalidateStateBlock();
		GetContext()->PushModColor( ork::CColor4::White() );
		{	
			ork::lev2::DynamicVertexBuffer<ork::lev2::SVtxV12C4T16> &vb = GfxEnv::GetSharedDynamicVB();

			ork::lev2::VtxWriter<ork::lev2::SVtxV12C4T16> vw;
			vw.Lock( GetContext(), &vb, 6*inumquads );
			{	for( int iq=0; iq<inumquads; iq++ )
				{
					const OrthoQuad& Q = pquads[iq];
					const SRect& QuadRect = Q.mQrect;
					const CColor4& C = Q.mColor;
					U32 uc = C.GetBGRAU32();
					
					bool brot = Q.mfrot!=0.0f;
					
					if( brot )
					{
						float fcx = float(QuadRect.miX+QuadRect.miX2)*0.5f;
						float fcy = float(QuadRect.miY+QuadRect.miY2)*0.5f;
						float fw = float(QuadRect.miX2)-fcx;
						float fh = float(QuadRect.miY2)-fcy;
						
						float fsr = sinf(Q.mfrot);
						float fcr = cosf(Q.mfrot);
						float fsr2 = sinf(Q.mfrot+PI*0.5f);
						float fcr2 = cosf(Q.mfrot+PI*0.5f);
					
						float fx0 = fcx + fcr*fw;
						float fy0 = fcy + fsr*fh;
						float fx1 = fcx + fcr2*fw;
						float fy1 = fcy + fsr2*fh;
						float fx2 = fcx - fcr*fw;
						float fy2 = fcy - fsr*fh;
						float fx3 = fcx - fcr2*fw;
						float fy3 = fcy - fsr2*fh;

						vw.AddVertex(ork::lev2::SVtxV12C4T16(fx0, fy0, 0.0f,  Q.mfu0a,  Q.mfv0a, Q.mfu0b,  Q.mfv0b, uc));
						vw.AddVertex(ork::lev2::SVtxV12C4T16(fx1, fy1, 0.0f,  Q.mfu1a,  Q.mfv0a, Q.mfu1b,  Q.mfv0b, uc));
						vw.AddVertex(ork::lev2::SVtxV12C4T16(fx2, fy2, 0.0f,  Q.mfu1a,  Q.mfv1a, Q.mfu1b,  Q.mfv1b, uc));

						vw.AddVertex(ork::lev2::SVtxV12C4T16(fx0, fy0, 0.0f,  Q.mfu0a,  Q.mfv0a, Q.mfu0b,  Q.mfv0b, uc));
						vw.AddVertex(ork::lev2::SVtxV12C4T16(fx2, fy2, 0.0f,  Q.mfu1a,  Q.mfv1a, Q.mfu1b,  Q.mfv1b, uc));
						vw.AddVertex(ork::lev2::SVtxV12C4T16(fx3, fy3, 0.0f,  Q.mfu0a,  Q.mfv1a, Q.mfu0b,  Q.mfv1b, uc));

					}
					else
					{
						float fx0 = float(QuadRect.miX);
						float fy0 = float(QuadRect.miY);
						float fx1 = float(QuadRect.miX2);
						float fy1 = float(QuadRect.miY2);

						vw.AddVertex(ork::lev2::SVtxV12C4T16(fx0, fy0, 0.0f,  Q.mfu0a,  Q.mfv0a, Q.mfu0b,  Q.mfv0b, uc));
						vw.AddVertex(ork::lev2::SVtxV12C4T16(fx1, fy1, 0.0f,  Q.mfu1a,  Q.mfv1a, Q.mfu1b,  Q.mfv1b, uc));
						vw.AddVertex(ork::lev2::SVtxV12C4T16(fx1, fy0, 0.0f,  Q.mfu1a,  Q.mfv0a, Q.mfu1b,  Q.mfv0b, uc));

						vw.AddVertex(ork::lev2::SVtxV12C4T16(fx0, fy0, 0.0f,  Q.mfu0a,  Q.mfv0a, Q.mfu0b,  Q.mfv0b, uc));
						vw.AddVertex(ork::lev2::SVtxV12C4T16(fx0, fy1, 0.0f,  Q.mfu0a,  Q.mfv1a, Q.mfu0b,  Q.mfv1b, uc));
						vw.AddVertex(ork::lev2::SVtxV12C4T16(fx1, fy1, 0.0f,  Q.mfu1a,  Q.mfv1a, Q.mfu1b,  Q.mfv1b, uc));
					}
				}
			}
			vw.UnLock(GetContext());

			GetContext()->GBI()->DrawPrimitive(vw, ork::lev2::EPRIM_TRIANGLES);
		}
		GetContext()->PopModColor();
	}
	GetContext()->FBI()->PopScissor();
	GetContext()->FBI()->PopViewport();
	GetContext()->MTXI()->PopPMatrix();
	GetContext()->MTXI()->PopVMatrix();
	GetContext()->MTXI()->PopMMatrix();
}

/////////////////////////////////////////////////////////////////////////


} }
