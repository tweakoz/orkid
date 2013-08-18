#pragma once

#include <ork/lev2/ui/ui.h>
#include <ork/lev2/ui/surface.h>

namespace ork { namespace ui {

struct Viewport : public Surface
{
	RttiDeclareAbstract( Viewport, Surface );

public:

	///////////////////////////////////////

	enum ERefreshState
	{
		EREFRESH_PLAY = 0,		// Refreshes at full framerate
		EREFRESH_UIEVENT,		// Refreshes on any UI input event
		EREFRESH_REQUEST,		// Refreshes on any draw request
	};

	struct SViewportFlags
	{
		ERefreshState	meRefreshState			:2;
		unsigned		mPad					:30;
	};

	///////////////////////////////////////
	
	void BeginFrame( lev2::GfxTarget* pTARG );
	void EndFrame( lev2::GfxTarget* pTARG );

	///////////////////////////////////////

	Viewport( const std::string & name, int x, int y, int w, int h, CColor3 color, F32 depth );
	virtual ~Viewport() {}

	lev2::PickBufferBase* GetPickBuffer() { return mpPickBuffer; }

protected:

	lev2::PickBufferBase*		mpPickBuffer;

	bool				mbDrawOK;

};

/*
class CWidgetFlags
{
	public: //

	unsigned	mbEnabled			:1;		// 1
	unsigned	muState				:3;		// 4
	unsigned	muPushUIShader		:1;		// 5
	unsigned	muGroupDelete		:1;		// 6
	unsigned	muSizeDirty			:1;		// 7
	unsigned	muPad				:25;	// 32

	CWidgetFlags() :
		mbEnabled(1),
		muState(0),
		muPushUIShader(0),
		muGroupDelete(1),
		muSizeDirty(1)
	{
	}

	void Enable( void ) { mbEnabled = true; }
	void Disable( void ) { mbEnabled = false; }
	void SetState( EUIWidgetState eState ) { muState = eState; }
	void SetPushUIShader( bool bval ) { muPushUIShader=bval; }
	void SetSizeDirty( bool bval ) { muSizeDirty=bval; }
	bool GetEnable( void ) const { return mbEnabled; }
	EUIWidgetState GetState( void )	{ return static_cast<EUIWidgetState>(muState); }
	bool GetPushUIShader( void ) const { return muPushUIShader; }
	bool IsSizeDirty( void ) const { return muSizeDirty; }
};

*/
}}
