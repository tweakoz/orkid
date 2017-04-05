////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
#include <ork/kernel/prop.h>
#if defined(_DARWIN)
#include <dispatch/dispatch.h>
#include <unistd.h>
extern FILE* g_orig_stdout;
#endif
#include <fcntl.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <cassert>
///////////////////////////////////////////////////////////////////////////////
#include <orktool/qtui/qtconsole.h>
///////////////////////////////////////////////////////////////////////////////
#include <QtWidgets/QScrollBar>
#include <QtCore/qtextstream.h>
#include <QtCore/qsocketnotifier.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/qtui/qtui.hpp>
#include <ork/lev2/gfx/dbgfontman.h>
///////////////////////////////////////////////////////////////////////////////

namespace ork {
namespace tool {
///////////////////////////////////////////////////////////////////////////////
vp_cons::vp_cons( const std::string & name )
	: ui::Viewport( name, 0, 0, 0, 0, CColor3::Red(), 0.0f )
	, mCTQT(nullptr)
{
}
///////////////////////////////////////////////////////////////////////////////
void ork::tool::vp_cons::BindCTQT(ork::lev2::CTQT* pctqt)
{
	mCTQT = pctqt;
	mBaseMaterial.Init( mCTQT->GetTarget() );
	//Register();
	
	mCTQT->SetRefreshPolicy( ork::lev2::CTXBASE::EREFRESH_FIXEDFPS );
	mCTQT->SetRefreshRate(2);
}
///////////////////////////////////////////////////////////////////////////////
ui::HandlerResult vp_cons::DoOnUiEvent( ui::Event *pEV )
{
	bool bisshift = pEV->mbSHIFT;

	switch( pEV->miEventCode )
	{	
		case ui::UIEV_KEY:
		{
			break;
		}
	}
	return ui::HandlerResult(this);
}
///////////////////////////////////////////////////////////////////////////////
void vp_cons::DoDraw(ui::DrawEvent& drwev)
{
	typedef lev2::SVtxV12C4T16 basevtx_t;
	
	if( (nullptr == mCTQT) || (nullptr == mCTQT->GetTarget()) ) return;

#if defined(_DARWIN)
	//if( 0 == g_orig_stdout ) return;
#endif

	lev2::GfxTarget* pTARG = mCTQT->GetTarget();
	
	int IW = pTARG->GetW();
	int IH = pTARG->GetH();
	
	pTARG->FBI()->SetAutoClear(true);
	pTARG->FBI()->SetClearColor(CColor4(1.0f,0.0f,0.1f,0.0f));
	BeginFrame(pTARG);
	SRect VPRect( 0, 0, IW, IH );
	pTARG->FBI()->PushViewport( VPRect );
	pTARG->MTXI()->PushUIMatrix();
	{
		/////////////////////////
		// GRADIENT BG
		/////////////////////////
		ork::lev2::DynamicVertexBuffer<ork::lev2::SVtxV12C4T16>& vbuf = lev2::GfxEnv::GetSharedDynamicVB();

		ork::lev2::SVtxV12C4T16 v0,v1,v2,v3;
		CVector2 uv0(0.0f,0.0f);
		CVector2 uv1(1.0f,0.0f);
		CVector2 uv2(1.0f,1.0f);
		CVector2 uv3(0.0f,1.0f);
		float faspect = float(pTARG->GetW())/float(pTARG->GetH());
		pTARG->BindMaterial( & mBaseMaterial );
		mBaseMaterial.SetColorMode( lev2::GfxMaterial3DSolid::EMODE_VERTEX_COLOR );

		u32 ucolor1 = 0xff000000;
		u32 ucolor2 = 0xff600060;
		
		v0 = lev2::SVtxV12C4T16( CVector3(0.0f,0.0f,0.0f), uv0, ucolor1 );
		v1 = lev2::SVtxV12C4T16( CVector3(float(IW),0.0f,0.0f), uv1, ucolor1 );
		v2 = lev2::SVtxV12C4T16( CVector3(float(IW),float(IH),0.0f), uv2, ucolor2 );
		v3 = lev2::SVtxV12C4T16( CVector3(0.0f,float(IH),0.0f), uv3, ucolor2 );

		lev2::VtxWriter<lev2::SVtxV12C4T16> vw;
		vw.Lock( pTARG, &vbuf, 6 );
		{
			vw.AddVertex( v0 );
			vw.AddVertex( v1 );
			vw.AddVertex( v2 );
			
			vw.AddVertex( v0 );
			vw.AddVertex( v2 );
			vw.AddVertex( v3 );
		}
		vw.UnLock(pTARG);

		pTARG->GBI()->DrawPrimitive( vw, ork::lev2::EPRIM_TRIANGLES, 6 );

		/////////////////////////
		// TEXT
		/////////////////////////
	
		const int inumlines = mLines.size();
		int inumlines_max_visible = IH/16;
		/////////////////////////
		int inumchars = 0;
		/////////////////////////
		std::vector<std::string> display_lines;
		display_lines.reserve(inumlines);
		int ilctr = 0;
		for( int iline=0; iline<inumlines; iline++ )
		{
			int irline = (inumlines-1)-iline;
			const std::string& line = mLines[irline];
			if( line.length() )
			{
				bool bskipline = (line.length()==1)&&line.c_str()[0]=='\n';
				if( false == bskipline && (ilctr<inumlines_max_visible) )
				{	inumchars += line.length()+1;
					//fprintf( g_orig_stdout, "iline<%d> <%s>\n", iline, line.c_str() );
					display_lines.push_back(line);
					ilctr++;
				}
			}
		}
		/////////////////////////
		int inumactuallines = display_lines.size();
		//fprintf( g_orig_stdout, "inumlines<%d>\n", inumlines );
		//fprintf( g_orig_stdout, "inumlines_max_visible<%d>\n", inumlines_max_visible );
		//fprintf( g_orig_stdout, "inumactuallines<%d>\n", inumactuallines );
		/////////////////////////
		float fx = 24;
		float fy = float(IH-16); //-float(inumlines_viz)*16.0f;
		/////////////////////////
		static int ibase = 0;
		pTARG->PushModColor( ork::CColor4::Green() );
		ork::lev2::CFontMan::PushFont("i16");
		ork::lev2::CFontMan::BeginTextBlock( pTARG, inumchars+inumactuallines );
		{
			for( int ili=0; ili<inumactuallines; ili++ )
			{
				const std::string& line = display_lines[ili];
				ork::lev2::CFontMan::DrawText( pTARG, fx,  fy, line.c_str() );
				fy -= 16.0f;
			}
		}
		ork::lev2::CFontMan::EndTextBlock(pTARG);
		pTARG->PopModColor();
		ibase++;
	/////////////////////////

	}
	pTARG->MTXI()->PopUIMatrix();
	pTARG->FBI()->PopViewport();
	EndFrame(pTARG);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} } // namespace ork::tool
