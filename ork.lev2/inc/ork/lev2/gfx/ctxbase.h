////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Grpahics Environment (Driver/HAL)
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/core/singleton.h>
#include <ork/kernel/timer.h>

#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/kernel/mutex.h>
#include <ork/lev2/gfx/targetinterfaces.h>
#include <ork/event/Event.h>
#include <ork/object/AutoConnector.h>
#include <ork/lev2/ui/event.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

/// ////////////////////////////////////////////////////////////////////////////
/// Graphics Context Base
/// this abstraction allows us to switch UI toolkits (Qt/Fltk, etc...)
/// ////////////////////////////////////////////////////////////////////////////

#if defined( _WIN32 )
 typedef HWND CTFLXID;
#elif defined( _OSX )
 typedef WindowPtr CTFLXID;
#elif defined (IX)
 typedef void* CTFLXID;
#elif defined (WII)
 typedef unsigned long int CTFLXID;
#endif

 class CTXBASE : public ork::AutoConnector
{
	RttiDeclareAbstract( CTXBASE, ork::AutoConnector );

	DeclarePublicAutoSlot( Repaint );

public:

	enum ERefreshPolicy
	{	
		EREFRESH_FASTEST = 0,	// refresh as fast as the update loop can go
		EREFRESH_WHENDIRTY,		// refresh whenever dirty 
		EREFRESH_FIXEDFPS,		// refresh at a fixed frame rate
	};


	CTFLXID GetThisXID( void ) const { return mxidThis; }
	CTFLXID GetTopXID( void ) const { return mxidTopLevel; }
	void SetThisXID( CTFLXID xid ) { mxidThis=xid; }
	void SetTopXID( CTFLXID xid ) { mxidTopLevel=xid; }
	ERefreshPolicy GetRefreshPolicy() const { return meRefreshPolicy; }

	CTXBASE( GfxWindow* pwin );

	virtual void SlotRepaint( void ) {}
	virtual void SetRefreshRate( int ihz ) {}
	virtual void SetRefreshPolicy( ERefreshPolicy epolicy ) { meRefreshPolicy=epolicy; }

	virtual void Show() {}
	virtual void Hide() {}

	GfxTarget* GetTarget() const { return mpTarget; }
	GfxWindow* GetWindow() const { return mpGfxWindow; }
	void SetTarget(GfxTarget*pt) { mpTarget=pt; }
	void SetWindow(GfxWindow*pw) { mpGfxWindow=pw; }

	virtual CVector2 MapCoordToGlobal( const CVector2& v ) const { return v; }

protected:

	GfxTarget*					mpTarget;
	GfxWindow*					mpGfxWindow;

	ui::Event					mUIEvent;
	CTFLXID						mxidThis;
	CTFLXID						mxidTopLevel;
	bool						mbInitialize;
	ERefreshPolicy				meRefreshPolicy;
};

///////////////////////////////////////////////////////////////////////////////
}} //namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
