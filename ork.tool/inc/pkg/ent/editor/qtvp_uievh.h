////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once 

#include <orktool/qtui/uitoolhandler.h>
#include <pkg/ent/editor/qtui_scenevp.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/atomic.h>

namespace ork { namespace ent {
	
class SceneEditorVP;
class SceneEditorVPOverlay;
class TestVP;
class SceneEditorBase;

typedef tool::UIToolHandler<SceneEditorVP> SceneEditorVPToolHandlerBase;

///////////////////////////////////////////////////////////////////////////////

class SceneEditorVPToolHandler : public SceneEditorVPToolHandlerBase
{
protected:
	SceneEditorBase&	mEditor;

	void SetSpawnLoc(const lev2::GetPixelContext& ctx, float fx, float fy);

public:
	SceneEditorVPToolHandler( SceneEditorBase& editor );
	SceneEditorBase& GetEditor() { return mEditor; }

};


///////////////////////////////////////////////////////////////////////////////

struct DeferredPickOperationContext;

typedef std::function<void(DeferredPickOperationContext*)> on_pick_lambda_t;

struct DeferredPickOperationContext
{
	DeferredPickOperationContext();

	int		miX;
	int		miY;
	bool	is_left;
	bool	is_mid;
	bool	is_right;
	bool	is_ctrl;
	bool	is_shift;
	ui::Event mEV;

	ork::rtti::ICastable*		mpCastable;
	SceneEditorVPToolHandler*	mHandler;
	SceneEditorVP*				mViewport;
	on_pick_lambda_t 			mOnPick;
	ork::atomic<int>			mState;
};

struct TestVPDefaultHandler : public SceneEditorVPToolHandler
{
	TestVPDefaultHandler( SceneEditorBase& editor );
private:
	ui::HandlerResult DoOnUiEvent( const ui::Event& EV ) override;
	void DoAttach(SceneEditorVP* ) override;
	void DoDetach(SceneEditorVP* ) override;
	void HandlePickOperation( DeferredPickOperationContext* ppickop );
};

///////////////////////////////////////////////////////////////////////////////

struct ManipHandler : public SceneEditorVPToolHandler
{
	ui::HandlerResult DoOnUiEvent( const ui::Event& EV) override;

protected:
	ManipHandler( SceneEditorBase& editor );
};

///////////////////////////////////////////////////////////////////////////////

struct ManipRotHandler : public ManipHandler
{	
	ManipRotHandler( SceneEditorBase& editor );
private:
	void DoAttach(SceneEditorVP* ) override;
	void DoDetach(SceneEditorVP* ) override;

};

///////////////////////////////////////////////////////////////////////////////

struct ManipTransHandler : public ManipHandler
{	
	ManipTransHandler( SceneEditorBase& editor );
private:
	void DoAttach(SceneEditorVP* ) override;
	void DoDetach(SceneEditorVP* ) override;
};

}
}

