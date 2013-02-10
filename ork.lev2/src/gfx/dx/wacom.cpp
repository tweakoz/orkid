////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#if defined( ORK_CONFIG_DIRECT3D )
#if defined( _USE_WACOM )

#include <orkmath.h>
#include <gfx/dx/dx.h>
#include <ui/ui.h>
#include <input/input.h>

/////////////////////////////////////////////////////////////////////////
// wacom tablet 

#define PACKETDATA	PK_X | PK_Y | PK_BUTTONS | PK_NORMAL_PRESSURE
#define PACKETMODE	0

#include <wintab/wintab.h>
#include <wintab/pktdef.h>

#pragma comment( lib, "wintab32" )

/////////////////////////////////////////////////////////////////////////////

LOGCONTEXT lc;
HCTX hCtx;

/////////////////////////////////////////////////////////////////////////////

void WacomInit( HWND hwnd )
{
	////////////////////////////////////////////////////
	// initalize tablet

	UINT WinTabInfo = WTInfo(0, 0, NULL);

	// Get default context information
	WTInfo( WTI_DEFCONTEXT, 0, &lc );

	// Open the context
	lc.lcPktData = PACKETDATA;
	lc.lcPktMode = PACKETMODE;
	lc.lcOptions = CXO_MESSAGES;
	//hCtx = WTOpen( m_hWnd, &lc, TRUE );

	hCtx = WTOpen( hwnd, &lc, TRUE );

	WTEnable( hCtx, TRUE );

	////////////////////////////////////////////////////

}

/////////////////////////////////////////////////////////////////////////////

void GfxTargetDX::OnWTPacket( HWND hwnd, UINT msg, WPARAM wSerial, LPARAM hCtx )
{
	static CUIEvent UIEvent;

	// Read the packet
	PACKET pkt;
	WTPacket( (HCTX)hCtx, wSerial, &pkt );

	int ipres = OrkSTXClampToRange( (int) pkt.pkNormalPressure-500, 0, 500 );
	UIEvent.mfPressure = (f32) ipres * 0.002f;

	static bool bLastRight = false;
	static bool bLastMid = false;

	bool bRIGHT = pkt.pkButtons&2;
	bool bMID = (0!=(pkt.pkButtons&4));

	
	f32 fX = (f32) pkt.pkX / (f32) lc.lcInExtX;
	f32 fY = (f32) pkt.pkY / (f32) lc.lcInExtY;
	f32 fW = (f32) GetW();
	f32 fH = (f32) GetH();

	static f32 fLX = fX;
	f32 fDX = fX - fLX;

	UIEvent.mbLeftButton = false;
	UIEvent.mbMiddleButton = false;

	if( (fX>=0.0f) && (fX<=1.0f) && (fY>=0.0f) && (fY<=1.0f) )
	{
		if( bRIGHT )
		{
			extern int giCurMouseX;
			extern int giCurMouseY;

			UIEvent.miLastX = UIEvent.miX;
			UIEvent.miLastY = UIEvent.miY;
			UIEvent.miX = (int) (fX*fW);
			UIEvent.miY = (int) ((1.0f-fY)*fH);
			giCurMouseX = UIEvent.miX;
			giCurMouseY = UIEvent.miY;

			if( false==bLastRight ) // right on
			{
				UIEvent.miEventCode = UIEV_PUSH;
				UIEvent.mbRightButton = true;
				UIEventHandler( & UIEvent );

				if( sqrtf( (fX-0.5f)*(fX-0.5f) + (fX-0.5f)*(fX-0.5f) ) < 0.2f )
				{
					CInputManager::SetTrigger( ETRIG_RAW_KEY_PAGE_UP, 127 );
				}
				else if( fY > 0.5f )
				{
					if( fX > 0.5f )
					{
						CInputManager::SetTrigger( ETRIG_RAW_KEY_LALT, 127 );
					}
					else
					{
						if( fDX > 0.0f )
                            CInputManager::SetTrigger( ETRIG_RAW_KEY_HOME, 127 );
						else if( fDX < 0.0f )
                            CInputManager::SetTrigger( ETRIG_RAW_KEY_END, 127 );

					}
				}

				fLX = fX;

			}
			else
			{
				UIEvent.miEventCode = UIEV_DRAG;
				UIEvent.mbRightButton = true;
				UIEventHandler( & UIEvent );
			}
		}
		else
		{
			CInputManager::SetTrigger( ETRIG_RAW_KEY_PAGE_UP, 0 );
			CInputManager::SetTrigger( ETRIG_RAW_KEY_LALT, 0 );
			CInputManager::SetTrigger( ETRIG_RAW_KEY_HOME, 0 );
			CInputManager::SetTrigger( ETRIG_RAW_KEY_END, 0 );

			if( bLastRight ) // right off
			{
				UIEvent.mfX = fX*fW;
				UIEvent.mfY = (1.0f-fY)*fH;
				UIEvent.miEventCode = UIEV_RELEASE;
				UIEvent.mbRightButton = false;
				UIEventHandler( & UIEvent );
			}
			else // 
			{
				UIEvent.miEventCode = UIEV_TABLET_BRUSH;
				UIEvent.mfX = fX;
				UIEvent.mfY = 1.0f-fY;
				UIEventHandler( & UIEvent );
			}
		}
		//orkprintf( "buttons %f %f %f\n", fX, fY, UIEvent.mfPressure );
	}

	bLastRight = bRIGHT;

	//orkprintf( "pkt.pkButtons %08x %d L %d R %d\n", pkt.pkButtons, pkt.pkNormalPressure, UIEvent.mbLeftButton, UIEvent.mbRightButton  );
}

#endif
#endif
