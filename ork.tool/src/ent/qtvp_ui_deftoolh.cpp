////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
#include <ork/kernel/opq.h>

#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/input/input.h>
#include <orktool/qtui/gfxbuffer.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/kernel/timer.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/DirectObjectMapPropertyType.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>

#include <orktool/toolcore/dataflow.h>

#include <pkg/ent/editor/qtui_scenevp.h>
#include <pkg/ent/editor/qtvp_uievh.h>
#include <pkg/ent/editor/edmainwin.h>
#include <QtGui/qclipboard.h>
#include <QtCore/QSettings>
#include <pkg/ent/scene.h>
#include <ork/lev2/gfx/camera/cameraman.h>

#include <orktool/qtui/uitoolhandler.hpp>

using namespace ork::lev2;
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

void OuterPickOp( DeferredPickOperationContext* pickctx );

///////////////////////////////////////////////////////////////////////////

TestVPDefaultHandler::TestVPDefaultHandler( SceneEditorBase& editor )
	: SceneEditorVPToolHandler( editor )
{
	SetBaseIconName( "lev2://editor/DefaultToolIcon" );
}

///////////////////////////////////////////////////////////////////////////

void TestVPDefaultHandler::DoAttach(SceneEditorVP* pvp)
{
}

///////////////////////////////////////////////////////////////////////////

void TestVPDefaultHandler::DoDetach(SceneEditorVP* pvp)
{
}

///////////////////////////////////////////////////////////////////////////

void TestVPDefaultHandler::HandlePickOperation( DeferredPickOperationContext* ppickctx )
{
	auto process_pick = [=](DeferredPickOperationContext*pickctx)
	{
		AssertOnOpQ2( UpdateSerialOpQ() );

		SceneEditorVPToolHandler* handler = pickctx->mHandler;

		ork::rtti::ICastable* pcast = pickctx->mpCastable;
		ork::ent::SceneEditorBase& editor = handler->GetEditor();
		//orkprintf( "obj<%p>\n", pcast );
		ork::rtti::ICastable *pillegal = (ork::rtti::ICastable *) 0xffffffff;
		if( pcast && pcast!=pillegal )
		{	ork::Object *pobj = rtti::autocast(pcast);
			object::ObjectClass* pclass = rtti::safe_downcast<object::ObjectClass*>(pobj->GetClass());
			//rtti::Class* pclass = pobj->GetClass();
			orkprintf( "Object<%08x> Class<%s>\n", pobj, pclass->Name().c_str() );
			any16 anno = pclass->Description().GetClassAnnotation( "editor.3dpickable" );
			//if( anno == "true" )
			{	editor.ManipManager().AttachObject( pobj );
				if( pickctx->is_ctrl )
					editor.ToggleSelection( pobj );
				else if( pickctx->is_shift )
					editor.AddObjectToSelection( pobj );
				else
				{
					editor.ClearSelection();
					editor.AddObjectToSelection( pobj );
				}
			}
		}
		else
			editor.ClearSelection();
	};

	ppickctx->mViewport = GetViewport();
	ppickctx->mOnPick = process_pick;
	OuterPickOp(ppickctx);		
}

///////////////////////////////////////////////////////////////////////////

EUIHandled TestVPDefaultHandler::UIEventHandler( CUIEvent *pEV )
{
	EUIHandled eH = EUI_HANDLED;

	bool isshift = pEV->mbSHIFT;
	bool isctrl	 = pEV->mbCTRL;

	bool isleft = pEV->mbLeftButton;
	bool isright = pEV->mbRightButton;
	bool ismid = pEV->mbMiddleButton;

	int ix = pEV->miX;
	int iy = pEV->miY;

	float fx = float(ix) / float(GetViewport()->GetW());
	float fy = float(iy) / float(GetViewport()->GetH());

	bool AreAnyMoveKeysDown = CSystem::IsKeyDepressed('W') | CSystem::IsKeyDepressed('A') | CSystem::IsKeyDepressed('S') | CSystem::IsKeyDepressed('D');

	switch( pEV->miEventCode )
	{
		case UIEV_SHOW:
		{
			if( GetViewport()->GetTarget() )
			{
				GfxTarget *pTARG = GetViewport()->GetTarget();
				lev2::CTXBASE * CtxBase = pTARG->GetCtxBase();
				CtxBase->SetRefreshRate(0);
			}
			break;
		}
		case UIEV_GOT_KEYFOCUS:
		{
			break;
		}
		case UIEV_KEY:
		{
			switch( pEV->miKeyCode )
			{
				case 'f':
				{
					// TODO: Implement Visitor pattern to collect and grow bounding boxes for selected items
					const orkset<ork::Object*> &selection = mEditor.SelectionManager().GetActiveSelection();
					for(orkset<ork::Object*>::const_iterator it = selection.begin(); it != selection.end(); it++)
					{
						if(const EntData *entdata = rtti::autocast(*it))
						{
						}
					}
					break;
				}
				case 0x01000007: // delete
				{
					orkset<ork::Object*> selection = mEditor.SelectionManager().GetActiveSelection();
					mEditor.ClearSelection();
					for(orkset<ork::Object*>::const_iterator it = selection.begin(); it != selection.end(); it++)
						mEditor.EditorDeleteObject(*it);
					break;
				}
				case 'c':
				{
					if( isctrl )
					{
						//mEditor.EditorDupe();
					}
					else if( isshift )
					{
						//mEditor.EditorUnGroup();
					}
					break;
				}
				case '`':
				case '~':
				{
					break;
				}
				default:
					//printf( "key %d\n", pEV->miKeyCode );
					break;
			}
			break;
		}
		case UIEV_RELEASE:
		{
			break;
		}
		case UIEV_DRAG:
		{
			if( false == GetViewport()->HasKeyboardFocus() ) break;
			if( AreAnyMoveKeysDown ) break;

			break;
		}
		case UIEV_PUSH:
		{
			if( false == GetViewport()->HasKeyboardFocus() ) break;

			//if( isright || ismid ) break;
			if( AreAnyMoveKeysDown ) break;


			if( isleft && false==isright )
			{
				DeferredPickOperationContext* pickctx = new DeferredPickOperationContext;
				pickctx->miX = ix;
				pickctx->miY = iy;
				pickctx->is_shift = isshift;
				pickctx->is_ctrl = isctrl;
				pickctx->is_left = isleft;
				pickctx->is_mid = ismid;
				pickctx->is_right = isright;
				pickctx->mHandler = this;
				pickctx->mViewport = GetViewport();

				HandlePickOperation( pickctx );
			}
			else if( isleft && isright )
			{
				if( GetViewport()->GetActiveCamera() )
				{
					//SetSpawnLoc( ctx, fx, fy );

					if( isshift )
					{						
						//const orkset<ork::Object*> &selection = mEditor.SelectionManager().GetActiveSelection();
						//if(selection.size() == 1)
						{
							//ork::Object *pobj = *selection.begin();
							//if(ent::EntData *entdata = rtti::autocast(pobj))
							{
								//entdata->GetDagNode().GetTransformNode().GetTransform()->SetMatrix(mtx_spawn);
							}
						}
					}

				}

			}
			break;
		}
	}

	return eH;
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork { namespace ent {

