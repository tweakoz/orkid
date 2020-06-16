////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/opq.h>
#include <orktool/qtui/qtui_tool.h>

#include <ork/lev2/gfx/gfxmodel.h>
//
#include <ork/kernel/timer.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/reflect/DirectObjectMapPropertyType.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/reflect/RegisterProperty.h>

#include <orktool/toolcore/dataflow.h>

#include "vpSceneEditor.h"
#include "uiToolsDefault.h"
#include <QtCore/QSettings>
#include <QtGui/qclipboard.h>
#include <pkg/ent/editor/edmainwin.h>
#include <pkg/ent/scene.h>

#include <ork/lev2/gfx/camera/uicam.h>

#include "uiToolHandler.inl"

using namespace ork::lev2;
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

void OuterPickOp(defpickopctx_ptr_t pickctx);

///////////////////////////////////////////////////////////////////////////

DefaultUiHandler::DefaultUiHandler(SceneEditorBase& editor)
    : SceneEditorVPToolHandler(editor) {
  SetBaseIconName("lev2://editor/DefaultToolIcon");
}

///////////////////////////////////////////////////////////////////////////

void DefaultUiHandler::DoAttach(SceneEditorVP* pvp) {
}

///////////////////////////////////////////////////////////////////////////

void DefaultUiHandler::DoDetach(SceneEditorVP* pvp) {
}

///////////////////////////////////////////////////////////////////////////

void DefaultUiHandler::HandlePickOperation(defpickopctx_ptr_t ppickctx) {
  auto process_pick = [=](defpickopctx_ptr_t pickctx) {
    ork::opq::assertOnQueue2(opq::updateSerialQueue());

    SceneEditorVPToolHandler* handler = pickctx->mHandler;

    ork::rtti::ICastable* pcast = pickctx->mpCastable;
    // orkprintf("obj<%p>\n", pcast);
    ork::ent::SceneEditorBase& editor = handler->GetEditor();
    ork::rtti::ICastable* pillegal    = (ork::rtti::ICastable*)0xffffffffffffffff;
    if (pcast && pcast != pillegal) {
      ork::Object* pobj           = rtti::autocast(pcast);
      object::ObjectClass* pclass = rtti::safe_downcast<object::ObjectClass*>(pobj->GetClass());
      // orkprintf("Object<%p> Class<%s>\n", pobj, pclass->Name().c_str());
      auto anno = pclass->Description().classAnnotation("editor.3dpickable");
      // if( anno == "true" )
      {
        editor.ManipManager().AttachObject(pobj);
        if (pickctx->is_ctrl)
          editor.ToggleSelection(pobj);
        else if (pickctx->is_shift)
          editor.AddObjectToSelection(pobj);
        else {
          editor.ClearSelection();
          editor.AddObjectToSelection(pobj);
        }
      }
    } else
      editor.ClearSelection();
  };

  ppickctx->mViewport = GetViewport();
  ppickctx->mOnPick   = process_pick;
  OuterPickOp(ppickctx);
}

///////////////////////////////////////////////////////////////////////////

ui::HandlerResult DefaultUiHandler::DoOnUiEvent(ui::event_constptr_t EV) {
  // printf( "DefaultUiHandler::DoOnUiEvent\n");
  ui::HandlerResult ret(this);

  bool isshift = EV->mbSHIFT;
  bool isctrl  = EV->mbCTRL;

  bool isleft  = EV->mbLeftButton;
  bool isright = EV->mbRightButton;
  bool ismid   = EV->mbMiddleButton;

  int ix = EV->miX;
  int iy = EV->miY;

  float fx = float(ix) / float(GetViewport()->width());
  float fy = float(iy) / float(GetViewport()->height());

  bool AreAnyMoveKeysDown = OldSchool::IsKeyDepressed('W') | OldSchool::IsKeyDepressed('A') | OldSchool::IsKeyDepressed('S') |
                            OldSchool::IsKeyDepressed('D');

  switch (EV->_eventcode) {
    case ui::EventCode::SHOW: {
      if (GetViewport()->GetTarget()) {
        Context* pTARG         = GetViewport()->GetTarget();
        lev2::CTXBASE* CtxBase = pTARG->GetCtxBase();
        // CtxBase->SetRefreshRate(0);
      }
      break;
    }
    case ui::EventCode::GOT_KEYFOCUS: {
      break;
    }
    case ui::EventCode::KEY: {
      switch (EV->miKeyCode) {
        case 0x01000007: // delete
        {
          orkset<ork::Object*> selection = mEditor.selectionManager().getActiveSelection();
          auto l                         = [=]() {
            mEditor.ClearSelection();
            for (orkset<ork::Object*>::const_iterator it = selection.begin(); it != selection.end(); it++)
              mEditor.EditorDeleteObject(*it);
          };
          opq::Op(l).QueueASync(opq::updateSerialQueue());
          break;
        }
        case 'c': {
          if (isctrl) {
            // mEditor.EditorDupe();
          } else if (isshift) {
            // mEditor.EditorUnGroup();
          }
          break;
        }
        case '`':
        case '~': {
          break;
        }
        default:
          // printf( "key %d\n", pEV->miKeyCode );
          break;
      }
      break;
    }
    case ui::EventCode::RELEASE: {
      ret.mHoldFocus = false;
      break;
    }
    case ui::EventCode::DRAG: {
      if (false == GetViewport()->HasKeyboardFocus())
        break;
      if (AreAnyMoveKeysDown)
        break;

      ret.mHoldFocus = true;
      break;
    }
    /////////////////////////////////////////
    // set cursor
    /////////////////////////////////////////
    case ui::EventCode::DOUBLECLICK: {

      if (false == GetViewport()->HasKeyboardFocus())
        break;
      if (AreAnyMoveKeysDown)
        break;
      if (GetViewport()->getActiveCamera()) {
        auto pickctx         = std::make_shared<DeferredPickOperationContext>(EV);
        pickctx->miX         = ix;
        pickctx->miY         = iy;
        pickctx->is_shift    = isshift;
        pickctx->is_ctrl     = isctrl;
        pickctx->is_left     = isleft;
        pickctx->is_mid      = ismid;
        pickctx->is_right    = isright;
        pickctx->mHandler    = this;
        pickctx->mViewport   = GetViewport();
        pickctx->_gfxContext = EV->_context;
        OrkAssert(EV->_context);

        auto process_pick = [=](defpickopctx_ptr_t pickctx) {
          ork::opq::assertOnQueue2(opq::updateSerialQueue());

          SceneEditorVPToolHandler* handler = pickctx->mHandler;
          auto& pixctx                      = pickctx->_pixelctx;
          this->setSpawnLoc(pixctx, fx, fy);
        };

        pickctx->mViewport = GetViewport();
        pickctx->mOnPick   = process_pick;
        OuterPickOp(pickctx);
      }
      break;
    }
    /////////////////////////////////////////
    case ui::EventCode::PUSH: {
      if (false == GetViewport()->HasKeyboardFocus())
        break;

      if (AreAnyMoveKeysDown)
        break;

      ret.mHoldFocus = true;

      if (isleft && false == isright) {
        auto pickctx         = std::make_shared<DeferredPickOperationContext>(EV);
        pickctx->miX         = ix;
        pickctx->miY         = iy;
        pickctx->is_shift    = isshift;
        pickctx->is_ctrl     = isctrl;
        pickctx->is_left     = isleft;
        pickctx->is_mid      = ismid;
        pickctx->is_right    = isright;
        pickctx->mHandler    = this;
        pickctx->mViewport   = GetViewport();
        pickctx->_gfxContext = EV->_context;
        OrkAssert(EV->_context);
        HandlePickOperation(pickctx);
      }
      break;
    }
  }

  return ret;
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
