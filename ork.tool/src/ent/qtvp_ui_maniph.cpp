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

ManipHandler::ManipHandler( SceneEditorBase& editor )
	: SceneEditorVPToolHandler( editor )
{
}

///////////////////////////////////////////////////////////////////////////

EUIHandled ManipHandler::UIEventHandler( CUIEvent *pEV )
{
	auto& updQ = UpdateSerialOpQ();

	EUIHandled eH = EUI_NOT_HANDLED;

	bool isshift = pEV->mbSHIFT;
	bool isctrl	 = pEV->mbCTRL;
	bool isleft = pEV->mbLeftButton;
	bool isright = pEV->mbRightButton;

	int ix = pEV->miX;
	int iy = pEV->miY;
	float fx = pEV->mfUnitX;
	float fy = pEV->mfUnitY;

	mEditor.ManipManager().SetGridSnap( isshift );

	switch( pEV->miEventCode )
	{
		case UIEV_RELEASE:
		{
			if( false == GetViewport()->HasKeyboardFocus() ) break;
			mEditor.ManipManager().DisableManip();
			mEditor.ManipManager().SetActiveCamera( 0 );
			eH = EUI_HANDLED;
		}
		break;
		case UIEV_DOUBLECLICK:
		{
			if( false == GetViewport()->HasKeyboardFocus() ) break;
			
			if( isleft && false == isright )
			{
				Op([&](){this->mEditor.ClearSelection();}).QueueSync(updQ);
			}

			DeferredPickOperationContext* pickctx = new DeferredPickOperationContext;
			pickctx->miX = ix;
			pickctx->miY = iy;
			pickctx->is_shift = isshift;
			pickctx->is_ctrl = isctrl;
			pickctx->is_left = isleft;
			//pickctx->is_mid = ismid;
			pickctx->is_right = isright;
			pickctx->mHandler = this;
			pickctx->mViewport = GetViewport();

			static auto on_pick = [=](DeferredPickOperationContext*pctx)
			{
				/*ork::rtti::ICastable *pobj = ctx.GetObject(GetViewport()->GetPickBuffer(),0);
				ork::rtti::ICastable *pillegal = (ork::rtti::ICastable *) 0xffffffff;

				orkprintf( "obj<%p>\n", pobj );

				if( isleft )
				{
					if( GetViewport()->GetActiveCamera() )
					{
					//	printf( "SetSpawnLoc fx<%f> fy<%f>\n", fx, fy );
						SetSpawnLoc( ctx, fx, fy );
					}

				}*/
		
			};
			OuterPickOp(pickctx);		
		}
		break;
		case UIEV_PUSH:
		{
			if( false == GetViewport()->HasKeyboardFocus() ) break;

			///////////////////////////////////////////////////////////

			auto the_block = [=](DeferredPickOperationContext*pctx)
			{
				SceneEditorVPToolHandler* handler = pctx->mHandler;
				SceneEditorBase& editor = handler->GetEditor();
				ork::Object* pobj = rtti::autocast(pctx->mpCastable);
				ork::rtti::ICastable *pillegal = (ork::rtti::ICastable *) 0xffffffff;
				//orkprintf( "maniph obj<%p>\n", pobj );

				if( pctx->is_left && false == pctx->is_right )
				{
					editor.ClearSelection();

					if( pobj && pobj!=pillegal)
					{
						//printf( "maniptest<%p>\n", pobj );
						if(CManip *manip = ork::rtti::autocast(pobj))
						{
							//printf( "maniptest2<%p>\n", pobj );
							//mEditor.ManipManager().SetActiveCamera(GetViewport()->GetCamera());
							editor.ManipManager().EnableManip(manip);
							editor.ManipManager().UIEventHandler( &pctx->mEV );
						}
						else if(ork::Object *object = ork::rtti::autocast(pobj))
						{
							editor.AddObjectToSelection(object);
							//printf( "maniptest3<%p>\n", pobj );
						}
					}
					else
					{	editor.ClearSelection();
						//printf( "maniptest4<%p>\n", pobj );
					}
				}
				else if( pctx->is_left && pctx->is_right )
				{
					//if( GetViewport()->GetCamera() )
					{
					//	SetSpawnLoc( ctx, fx, fy );
					}

				}
			};

			///////////////////////////////////////////////////////////

			DeferredPickOperationContext* pickctx = new DeferredPickOperationContext;
			pickctx->mEV = *pEV;
			pickctx->miX = ix;
			pickctx->miY = iy;
			pickctx->is_shift = isshift;
			pickctx->is_ctrl = isctrl;
			pickctx->is_left = isleft;
			pickctx->is_right = isright;
			pickctx->mHandler = this;
			pickctx->mViewport = GetViewport();
			pickctx->mOnPick = the_block;
			OuterPickOp(pickctx);		
			///////////////////////////////////////////////////////////

			mEditor.ManipManager().UIEventHandler( pEV );
			eH = EUI_HANDLED;
		}
		break;

		case UIEV_DRAG:
		{
			if( false == GetViewport()->HasKeyboardFocus() ) break;
			if(mEditor.ManipManager().UIEventHandler( pEV ))
				eH = EUI_HANDLED;
		}
		break;

		case UIEV_MOVE:
		{
			if( false == GetViewport()->HasKeyboardFocus() ) break;
			if(!mEditor.ManipManager().IsVisible())
				return eH;

			///////////////////////////////////////////////////////////

			auto process_pick = [=](DeferredPickOperationContext*pctx)
			{
				SceneEditorVPToolHandler* handler = pctx->mHandler;
				SceneEditorBase& editor = handler->GetEditor();
				ork::Object* pobj = rtti::autocast(pctx->mpCastable);
				ork::rtti::ICastable *pillegal = (ork::rtti::ICastable *) 0xffffffff;
				//orkprintf( "obj<%p>\n", pobj );

				if(pobj && pobj!=pillegal && pobj->GetClass()->IsSubclassOf( CManip::GetClassStatic() ))
					editor.ManipManager().SetHover((CManip*)pobj);
				else
					editor.ManipManager().SetHover(NULL);

				if(pctx->is_shift)
					editor.ManipManager().SetDualAxis(true);
				else
					editor.ManipManager().SetDualAxis(false);
				
			};

			///////////////////////////////////////////////////////////

			DeferredPickOperationContext* pickctx = new DeferredPickOperationContext;
			pickctx->miX = ix;
			pickctx->miY = iy;
			pickctx->is_shift = isshift;
			pickctx->is_ctrl = isctrl;
			pickctx->is_left = isleft;
			pickctx->is_right = isright;
			pickctx->mHandler = this;
			pickctx->mViewport = GetViewport();
			pickctx->mOnPick = process_pick;
			OuterPickOp(pickctx);		
			///////////////////////////////////////////////////////////

			eH = EUI_HANDLED;
		}
		break;
	}

	return eH;
}

///////////////////////////////////////////////////////////////////////////

ManipTransHandler::ManipTransHandler( SceneEditorBase& editor )
	: ManipHandler(editor)
{
	SetBaseIconName( "lev2://editor/ManipTrans" );
}
void ManipTransHandler::DoAttach(SceneEditorVP* pvp)
{
	mEditor.ManipManager().SetWorldTrans( false );
	mEditor.ManipManager().SetUIMode( CManipManager::EUIMODE_MANIP_WORLD_TRANSLATE );
	mEditor.ManipManager().SetManipMode( CManipManager::EMANIPMODE_WORLD_TRANS );
}
void ManipTransHandler::DoDetach(SceneEditorVP* pvp)
{
	mEditor.ManipManager().SetUIMode( CManipManager::EUIMODE_STD );
}

///////////////////////////////////////////////////////////////////////////

ManipRotHandler::ManipRotHandler( SceneEditorBase& editor )
	: ManipHandler(editor)
{
	SetBaseIconName( "lev2://editor/ManipRot" );
}

void ManipRotHandler::DoAttach(SceneEditorVP* pvp)
{
	mEditor.ManipManager().SetWorldTrans( true );
	mEditor.ManipManager().SetUIMode( CManipManager::EUIMODE_MANIP_LOCAL_ROTATE );
	mEditor.ManipManager().SetManipMode( CManipManager::EMANIPMODE_LOCAL_ROTATE );
}
void ManipRotHandler::DoDetach(SceneEditorVP* pvp)
{
	mEditor.ManipManager().SetUIMode( CManipManager::EUIMODE_STD );
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork { namespace ent {
