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

	struct ViewportFlags
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

}}
