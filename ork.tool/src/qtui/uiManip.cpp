////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
#include <ork/kernel/opq.h>

#include <ork/lev2/gfx/gfxmodel.h>
//
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/kernel/timer.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/properties/DirectMapTyped.h>
#include <ork/reflect/properties/DirectMapTyped.hpp>

#include <orktool/toolcore/dataflow.h>

#include "vpSceneEditor.h"
#include "uiToolsDefault.h"
#include <pkg/ent/editor/edmainwin.h>
#include <QtGui/qclipboard.h>
#include <QtCore/QSettings>
#include <pkg/ent/scene.h>
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/ui/context.h>

#include "uiToolHandler.inl"

using namespace ork::lev2;
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

void OuterPickOp(defpickopctx_ptr_t pickctx);

ManipHandler::ManipHandler(SceneEditorBase& editor)
    : SceneEditorVPToolHandler(editor) {
  _updq = opq::updateSerialQueue();
}

///////////////////////////////////////////////////////////////////////////

ui::HandlerResult ManipHandler::DoOnUiEvent(ui::event_constptr_t EV) {

  ui::HandlerResult ret;

  bool isshift = EV->mbSHIFT;
  bool isctrl  = EV->mbCTRL;
  bool isleft  = EV->mbLeftButton;
  bool isright = EV->mbRightButton;

  int ix   = EV->miX;
  int iy   = EV->miY;
  float fx = EV->mfUnitX;
  float fy = EV->mfUnitY;

  mEditor.ManipManager().SetGridSnap(isshift);

  switch (EV->_eventcode) {
    case ui::EventCode::RELEASE: {
      if (false == GetViewport()->_uicontext->hasKeyboardFocus())
        break;
      mEditor.ManipManager().DisableManip();
      mEditor.ManipManager().SetActiveCamera(0);
      ret.setHandled(this);
    } break;
    case ui::EventCode::DOUBLECLICK: {
      if (false == GetViewport()->_uicontext->hasKeyboardFocus())
        break;

      if (isleft && false == isright) {
        opq::Op([&]() { this->mEditor.ClearSelection(); }).QueueSync(_updq);
      }

      auto pickctx         = std::make_shared<DeferredPickOperationContext>(EV);
      pickctx->mHandler    = this;
      pickctx->mViewport   = GetViewport();
      pickctx->_gfxContext = EV->_context;

      static auto on_pick = [=](DeferredPickOperationContext* pctx) {
        /*ork::rtti::ICastable *pobj = ctx.GetObject(GetViewport()->GetPickBuffer(),0);
        ork::rtti::ICastable *pillegal = (ork::rtti::ICastable *) 0xffffffff;

        orkprintf( "obj<%p>\n", pobj );

        if( isleft )
        {
            if( GetViewport()->getActiveCamera() )
            {
            //	printf( "setSpawnLoc fx<%f> fy<%f>\n", fx, fy );
                setSpawnLoc( ctx, fx, fy );
            }

        }*/
      };
      OuterPickOp(pickctx);
    } break;
    case ui::EventCode::PUSH: {
      if (false == GetViewport()->_uicontext->hasKeyboardFocus())
        break;

      ///////////////////////////////////////////////////////////

      auto the_block = [=](defpickopctx_ptr_t pctx) {
        SceneEditorVPToolHandler* handler = pctx->mHandler;
        SceneEditorBase& editor           = handler->GetEditor();
        ork::Object* pobj                 = rtti::autocast(pctx->mpCastable);
        ork::rtti::ICastable* pillegal    = (ork::rtti::ICastable*)0xffffffff;
        // orkprintf( "maniph obj<%p>\n", pobj );

        if (pctx->is_left && false == pctx->is_right) {
          editor.ClearSelection();

          if (pobj && pobj != pillegal) {
            // printf( "maniptest<%p>\n", pobj );
            if (Manip* manip = ork::rtti::autocast(pobj)) {
              // printf( "maniptest2<%p>\n", pobj );
              // mEditor.ManipManager().SetActiveCamera(GetViewport()->GetCamera());
              editor.ManipManager().EnableManip(manip);
              editor.ManipManager().UIEventHandler(pctx->mEV);
            } else if (ork::Object* object = ork::rtti::autocast(pobj)) {
              editor.AddObjectToSelection(object);
              // printf( "maniptest3<%p>\n", pobj );
            }
          } else {
            editor.ClearSelection();
            // printf( "maniptest4<%p>\n", pobj );
          }
        } else if (pctx->is_left && pctx->is_right) {
          // if( GetViewport()->GetCamera() )
          {
            //	setSpawnLoc( ctx, fx, fy );
          }
        }
      };

      ///////////////////////////////////////////////////////////

      auto pickctx         = std::make_shared<DeferredPickOperationContext>(EV);
      pickctx->mHandler    = this;
      pickctx->mViewport   = GetViewport();
      pickctx->_gfxContext = EV->_context;
      pickctx->mOnPick     = the_block;
      OuterPickOp(pickctx);
      ///////////////////////////////////////////////////////////

      mEditor.ManipManager().UIEventHandler(EV);
      ret.setHandled(this);
    } break;

    case ui::EventCode::DRAG: {
      if (false == GetViewport()->_uicontext->hasKeyboardFocus())
        break;
      if (mEditor.ManipManager().UIEventHandler(EV))
        ret.setHandled(this);
    } break;

    case ui::EventCode::MOVE: {
      if (false == GetViewport()->_uicontext->hasKeyboardFocus())
        break;
      if (!mEditor.ManipManager().IsVisible())
        return ret;

      ///////////////////////////////////////////////////////////

      auto process_pick = [=](defpickopctx_ptr_t pctx) {
        SceneEditorVPToolHandler* handler = pctx->mHandler;
        SceneEditorBase& editor           = handler->GetEditor();
        ork::Object* pobj                 = rtti::autocast(pctx->mpCastable);
        ork::rtti::ICastable* pillegal    = (ork::rtti::ICastable*)0xffffffff;
        // orkprintf( "obj<%p>\n", pobj );

        if (pobj && pobj != pillegal && pobj->GetClass()->IsSubclassOf(Manip::GetClassStatic()))
          editor.ManipManager().SetHover((Manip*)pobj);
        else
          editor.ManipManager().SetHover(NULL);

        if (pctx->is_shift)
          editor.ManipManager().SetDualAxis(true);
        else
          editor.ManipManager().SetDualAxis(false);
      };

      ///////////////////////////////////////////////////////////

      auto pickctx       = std::make_shared<DeferredPickOperationContext>(EV);
      pickctx->mHandler  = this;
      pickctx->mViewport = GetViewport();
      pickctx->mOnPick   = process_pick;
      // OuterPickOp(pickctx);
      ///////////////////////////////////////////////////////////

      // ret.setHandled(this);
    } break;
  }

  return ret;
}

///////////////////////////////////////////////////////////////////////////

ManipTransHandler::ManipTransHandler(SceneEditorBase& editor)
    : ManipHandler(editor) {
  SetBaseIconName("lev2://editor/ManipTrans");
}
void ManipTransHandler::DoAttach(SceneEditorVP* pvp) {
  mEditor.ManipManager().SetWorldTrans(false);
  mEditor.ManipManager().SetUIMode(ManipManager::EUIMODE_MANIP_WORLD_TRANSLATE);
  mEditor.ManipManager().SetManipMode(ManipManager::EMANIPMODE_WORLD_TRANS);
}
void ManipTransHandler::DoDetach(SceneEditorVP* pvp) {
  mEditor.ManipManager().SetUIMode(ManipManager::EUIMODE_STD);
}

///////////////////////////////////////////////////////////////////////////

ManipRotHandler::ManipRotHandler(SceneEditorBase& editor)
    : ManipHandler(editor) {
  SetBaseIconName("lev2://editor/ManipRot");
}

void ManipRotHandler::DoAttach(SceneEditorVP* pvp) {
  mEditor.ManipManager().SetWorldTrans(true);
  mEditor.ManipManager().SetUIMode(ManipManager::EUIMODE_MANIP_LOCAL_ROTATE);
  mEditor.ManipManager().SetManipMode(ManipManager::EMANIPMODE_LOCAL_ROTATE);
}
void ManipRotHandler::DoDetach(SceneEditorVP* pvp) {
  mEditor.ManipManager().SetUIMode(ManipManager::EUIMODE_STD);
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
