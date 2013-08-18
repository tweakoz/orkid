#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxprimitives.h>

namespace ork { namespace ui {

/////////////////////////////////////////////////////////////////////////

Group::Group( const std::string & name, int x, int y, int w, int h )
	: Widget(name,x,y,w,h)
{

}

/////////////////////////////////////////////////////////////////////////

void Group::AddChild( Widget* pch )
{
	assert(pch->GetParent()==nullptr);
	mChildren.push_back( pch );
	pch->mParent = this;
}

/////////////////////////////////////////////////////////////////////////

void Group::DrawChildren(ui::DrawEvent& drwev)
{
	for( auto& it : mChildren )
	{
		it->Draw(drwev);
	}
}

/////////////////////////////////////////////////////////////////////////

void Group::OnResize()
{
	//printf( "Group<%s>::OnResize x<%d> y<%d> w<%d> h<%d>\n", msName.c_str(), miX, miY, miW, miH );

	for( auto& it : mChildren )
	{
		if( it->mSizeDirty )
			it->OnResize();
	}
}

/////////////////////////////////////////////////////////////////////////

void Group::DoLayout()
{
	//printf( "Group<%s>::DoLayout x<%d> y<%d> w<%d> h<%d>\n", msName.c_str(), miX, miY, miW, miH );
	for( auto& it : mChildren )
	{
		it->ReLayout();
	}
}

/////////////////////////////////////////////////////////////////////////

HandlerResult Group::DoRouteUiEvent( const Event& Ev )
{
	HandlerResult res;
	for( auto& child : mChildren )
	{
		if( res.mHandler == nullptr )
		{
			bool binside = child->IsEventInside(Ev);
			//printf( "Group::RouteUiEvent ev<%d,%d> child<%p> inside<%d>\n", Ev.miX, Ev.miY, child, int(binside) );
			if( binside )
			{	
				auto child_res = child->RouteUiEvent(Ev);
				if( child_res.WasHandled() )
				{	res = child_res;
				}
			}
		}
	}	
	if( res.mHandler == nullptr )
	{
		res = OnUiEvent(Ev);
	}
	return res;
}

/////////////////////////////////////////////////////////////////////////

}} // namespace ork { namespace ui {

