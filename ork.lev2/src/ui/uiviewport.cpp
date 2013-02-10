////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>

INSTANTIATE_TRANSPARENT_RTTI( ork::lev2::CUIViewport, "CUIViewport" );

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

void CUIViewport::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

CUIViewport::CUIViewport( const std::string & name, int x, int y, int w, int h, CColor3 color, F32 depth )
	: msName( name )
	, mbClear( true )
	, mcClearColor( color )
	, mfClearDepth( depth )
	, mpTarget( 0 )
	, miX( x )
	, miY( y )
	, miW( w )
	, miH( h )
	, miX2( x+w )
	, miY2( y+h )
	, miLevel(0)
	, mbInit(true)
	, mpDrawEvent(0)
	, mpPickBuffer(nullptr)
{
	mWidgetFlags.Enable();
	mWidgetFlags.SetState( EUI_WIDGET_OFF );
}

void CUIViewport::PushFrameTechnique(FrameTechniqueBase*ptek)
{
	mpActiveFrameTek.push( ptek );
}

void CUIViewport::PopFrameTechnique( )
{
	mpActiveFrameTek.pop();
}

FrameTechniqueBase* CUIViewport::GetFrameTechnique( ) const
{
	return mpActiveFrameTek.size() ? mpActiveFrameTek.top() : 0;
}

/////////////////////////////////////////////////////////////////////////

void CUIViewport::Attach()
{
	mpTarget->FBI()->AttachViewport( this );
}

/////////////////////////////////////////////////////////////////////////

void CUIViewport::Clear()
{
	mpTarget->FBI()->ClearViewport( this );
}

/////////////////////////////////////////////////////////////////////////

void CUIViewport::resize( void )
{
	mWidgetFlags.SetSizeDirty( false );
}

/////////////////////////////////////////////////////////////////////////

bool CUIViewport::IsKeyDepressed( int ch )
{
	if( false==HasKeyboardFocus() )
	{
		return false;
	}

	return CSystem::GetRef().IsKeyDepressed( ch );
}

/////////////////////////////////////////////////////////////////////////

bool CUIViewport::IsHotKeyDepressed( const char* pact )
{
	if( false==HasKeyboardFocus() )
	{
		return false;
	}

	return HotKeyManager::IsDepressed( pact );
}

/////////////////////////////////////////////////////////////////////////

bool CUIViewport::IsHotKeyDepressed( const HotKey& hk )
{
	if( false==HasKeyboardFocus() )
	{
		return false;
	}

	return HotKeyManager::IsDepressed( hk );
}

/////////////////////////////////////////////////////////////////////////

void CUIViewport::BeginFrame( GfxTarget* pTARG )
{
	ork::lev2::CFontMan::GetRef();
	//////////////////////////////////////////////////////////
	//GfxEnv::GetRef().GetGlobalLock().Lock(); // InterThreadLock
	//////////////////////////////////////////////////////////

	pTARG->BeginFrame();

	mbDrawOK = pTARG->IsDeviceAvailable();

	if( mbDrawOK )
	{
		//orkprintf( "BEG CUIViewport::BeginFrame::mbDrawOK\n" );
		CMatrix4 MatOrtho = CMatrix4::Identity;
		MatOrtho.Ortho( 0.0f, (F32)GetW(), 0.0f, (F32)GetH(), 0.0f, 1.0f );

		pTARG->MTXI()->SetOrthoMatrix( MatOrtho );

		SRect SciRect( miX, miY, miX+miW, miY+miH );
		pTARG->FBI()->PushScissor( SciRect );
		pTARG->MTXI()->PushPMatrix( pTARG->MTXI()->GetOrthoMatrix() );
		pTARG->BindMaterial( GfxEnv::GetRef().GetDefaultUIMaterial() );
	}
}

/////////////////////////////////////////////////////////////////////////

void CUIViewport::EndFrame( GfxTarget* pTARG )
{
	if( mbDrawOK )
	{
		//orkprintf( "END CUIViewport::BeginFrame::mbDrawOK\n" );
		pTARG->MTXI()->PopPMatrix();
		pTARG->FBI()->PopScissor();
		pTARG->BindMaterial( 0 );
	}
	pTARG->EndFrame();
	//////////////////////////////////////////////////////////
	//GfxEnv::GetRef().GetGlobalLock().UnLock(); // InterThreadLock
	//////////////////////////////////////////////////////////

	mbDrawOK = false;
}

/////////////////////////////////////////////////////////////////////////

void CUIViewport::Draw(DrawEvent& drwev)
{
	mpDrawEvent = & drwev;
	mpTarget = drwev.GetTarget();

	if( mbInit )
	{
		ork::lev2::CFontMan::GetRef();
		Init(mpTarget);
		mbInit = false;
	}

	DoDraw();
	mpTarget = 0;
	mpDrawEvent = 0;
}

void CUIViewport::ExtDraw( GfxTarget* pTARG )
{
	if( mbInit )
	{
		ork::lev2::CFontMan::GetRef();
		Init(mpTarget);
		mbInit = false;
	}
	mpTarget = pTARG;
	DoDraw();
	mpTarget = 0;
}

/////////////////////////////////////////////////////////////////////////
#if 0
CUICoord CUICoord::Convert( ECOORDSYS etype ) const
{

	f32 fX = 0.0f;
	f32 fY = 0.0f;

	////////////////////////////////////////////////////////////////

	switch( meType ) // first convert to base type (dev:vprel)
	{
		case ECS_DEV_VPREL: // 0..1 relative to viewport origin
			fX = mfX;
			fY = mfY;
			break;
		case ECS_DEV_ABS:   // 0..1 relative to target origin
			OrkAssert( mpViewport );
			fX = mfX;
			fY = mfY;
			break;
		case ECS_PIX_VPREL: // pixels relative to viewport origin
			OrkAssert( mpViewport );
			fX = mfX/mpViewport->GetW();
			fY = mfY/mpViewport->GetH();
			break;
		case ECS_PIX_TGREL: // pixels relative to target origin
			fX = (mfX-mpViewport->GetX())/mpViewport->GetW();
			fY = (mfY-mpViewport->GetY())/mpViewport->GetH();
			break;
	}

	////////////////////////////////////////////////////////////////

	switch( etype ) // now convert to target format
	{
		case ECS_PIX_VPREL:
			OrkAssert( mpViewport );
			fX = fX * mpViewport->GetW();
			fY = fY * mpViewport->GetH();
			break;
		case ECS_PIX_TGREL:
			fX = (fX*mpViewport->GetW())+mpViewport->GetX();
			fY = (fY*mpViewport->GetH())+mpViewport->GetY();
			break;
		case ECS_DEV_VPREL:
			break;
		case ECS_DEV_ABS:
			OrkAssert( 0 );
			fX = fX - mpViewport->GetX();
			fY = fY - mpViewport->GetY();
			break;
	}

	return CUICoord( etype, fX, fY, mpViewport );
}
#endif
/////////////////////////////////////////////////////////////////////////

} }
