////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ENT3D_QTVP_UIEVH_H
#define _ENT3D_QTVP_UIEVH_H

#include <orktool/qtui/uitoolhandler.h>
#include <pkg/ent/editor/qtui_scenevp.h>
#include <ork/kernel/opq.h>
#include <tbb/atomic.h>

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
	lev2::CUIEvent mEV;

	ork::rtti::ICastable*		mpCastable;
	SceneEditorVPToolHandler*	mHandler;
	SceneEditorVP*				mViewport;
	on_pick_lambda_t 			mOnPick;
	tbb::atomic<int>			mState;
};

class TestVPDefaultHandler : public SceneEditorVPToolHandler
{
public:
	TestVPDefaultHandler( SceneEditorBase& editor );
private:
	ork::lev2::EUIHandled UIEventHandler( ork::lev2::CUIEvent *pEV ); // virtual
	void DoAttach(SceneEditorVP* ); // virtual
	void DoDetach(SceneEditorVP* ); // virtual

	void HandlePickOperation( DeferredPickOperationContext* ppickop );
};

///////////////////////////////////////////////////////////////////////////////

class ManipHandler : public SceneEditorVPToolHandler
{
public:
	virtual ork::lev2::EUIHandled UIEventHandler( ork::lev2::CUIEvent *pEV );

protected:
	ManipHandler( SceneEditorBase& editor );
};

///////////////////////////////////////////////////////////////////////////////

class ManipRotHandler : public ManipHandler
{	
	virtual void DoAttach(SceneEditorVP* );
	virtual void DoDetach(SceneEditorVP* );
public:
	ManipRotHandler( SceneEditorBase& editor );
};

///////////////////////////////////////////////////////////////////////////////

class ManipTransHandler : public ManipHandler
{	
	virtual void DoAttach(SceneEditorVP* );
	virtual void DoDetach(SceneEditorVP* );
public:
	ManipTransHandler( SceneEditorBase& editor );
};

}
}

#endif
