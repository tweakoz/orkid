////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ORKTOOL_QTUI_TOOLHANDLER_H
#define _ORKTOOL_QTUI_TOOLHANDLER_H
///////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/texman.h>
///////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool {
///////////////////////////////////////////////////////////////////////////
/// Please use me
///
/// I am the abstract base class for all tool handlers. These are the button icons that fill the top-left edge of the scene editor viewport
/// I also have sub tool icons that can be selected to do various subtasks. See PaintCollisionHandler or BspToolHandler for example
/// implementations.
///
///////////////////////////////////////////////////////////////////////////

template <typename VPTYPE> class UIToolHandler
{

	VPTYPE*								mpViewport;

	virtual void DoAttach( VPTYPE* ) = 0;
	virtual void DoDetach( VPTYPE* ) = 0;

protected:
	
	lev2::Texture*						mpBaseIcon;
	std::string							mBaseIconName;

	orkvector<lev2::Texture*>			mpSubIconVector;
	orkvector<std::string>				mpSubIconNameVector;

	int									mState;

public:

	UIToolHandler();

	void Attach( VPTYPE* );
	void Detach( VPTYPE* );

	virtual ork::lev2::EUIHandled UIEventHandler(ork::lev2::CUIEvent* pEV);
	void LoadToolIcon();
	virtual void DrawToolIcon(ork::lev2::GfxTarget* pTARG, int ix, int iy, bool bhilite);
	virtual void DrawSubToolIcon( ork::lev2::GfxTarget* pTARG, int ix, int iy, bool bhilite );

	VPTYPE* GetViewport() const { return mpViewport; }

	void SetBaseIconName(std::string name);// { mBaseIconName = name; }

	void AddSubIconName(std::string name) { mpSubIconNameVector.push_back(name); }

	void SetState(int state);
	virtual void OnEnter(int state) {}
	virtual void OnExit(int state) {}
};

///////////////////////////////////////////////////////////////////////////
} }
///////////////////////////////////////////////////////////////////////////
#endif
