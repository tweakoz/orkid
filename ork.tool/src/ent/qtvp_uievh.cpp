////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
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
#include <ork/reflect/RegisterProperty.h>

#include <orktool/toolcore/dataflow.h>

#include <QtCore/QSettings>
#include <QtGui/qclipboard.h>
#include <ork/lev2/gfx/camera/cameraman.h>
#include <pkg/ent/editor/edmainwin.h>
#include "qtui_scenevp.h"
#include "qtvp_uievh.h"
#include <pkg/ent/scene.h>

///////////////////////////////////////////////////////////////////////////////

#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include "uitoolhandler.inl"
#include <pkg/ent/entity.hpp>
#include <pkg/ent/scene.hpp>

///////////////////////////////////////////////////////////////////////////////

bool gtoggle_hud = true;

using namespace ork::lev2;

namespace ork { namespace ent {

SceneEditorVPToolHandler::SceneEditorVPToolHandler(SceneEditorBase& editor) : mEditor(editor) {}

void OuterPickOp(DeferredPickOperationContext* pickctx);

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::BindToolHandler(SceneEditorVPToolHandler* handler) {
  if (mpCurrentHandler) {
    mpCurrentHandler->Detach(this);
  }
  mpCurrentHandler = handler;
  mpCurrentHandler->Attach(this);
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::BindToolHandler(const std::string& toolname) {
  OrkAssert(mToolHandlers.find(toolname) != mToolHandlers.end());
  BindToolHandler((*mToolHandlers.find(toolname)).second);
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::RegisterToolHandler(const std::string& toolname, SceneEditorVPToolHandler* handler) {
  OrkAssert(mToolHandlers.find(toolname) == mToolHandlers.end());
  mToolHandlers[toolname] = handler;
  handler->SetToolName(toolname);
}

///////////////////////////////////////////////////////////////////////////

ui::HandlerResult SceneEditorVP::DoOnUiEvent(const ui::Event& EV) {
  ui::HandlerResult ret;
  // OrkAssert( 0 != mpCurrentHandler );

  lev2::CTXBASE* CtxBase = GetTarget() ? GetTarget()->GetCtxBase() : 0;

  ork::ent::SceneInst* psceneinst = mEditor.GetActiveSceneInst();

  ////////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////////

  bool isshift = EV.mbSHIFT;
  bool isalt = EV.mbALT;
  bool isctrl = EV.mbCTRL;
  bool ismeta = EV.mbMETA;
  bool ismulti = false;

  switch (EV.miEventCode) {
    case ui::UIEV_GOT_KEYFOCUS: {
      // bool brunning = psceneinst ? psceneinst->GetSceneInstMode()==ork::ent::ESCENEMODE_RUN : false;
      // CtxBase->SetRefreshPolicy( brunning ? lev2::CTXBASE::EREFRESH_FASTEST : lev2::CTXBASE::EREFRESH_WHENDIRTY );
      break;
    }
    case ui::UIEV_LOST_KEYFOCUS: {
      // CtxBase->SetRefreshPolicy( lev2::CTXBASE::EREFRESH_WHENDIRTY);
      break;
    }
    case ui::UIEV_MULTITOUCH:
      ismulti = true;
      break;
  }
  bool bcamhandled = false;

  if (_editorCamera) {
    bcamhandled = _editorCamera->UIEventHandler(EV);
    if (bcamhandled) {
      ret.SetHandled(this);
      return ret;
    }
  }

  if (0 == EV.miEventCode)
    return ret;

  ////////////////////////////////////////////////////////////////

  switch (EV.miEventCode) {
    case ui::UIEV_KEYUP: {
      break;
    }
    case ui::UIEV_KEY: {
      int icode = EV.miKeyCode;

      switch (icode) {
        case 't': {
          if (isctrl)
            mEditor.EditorPlaceEntity();
          break;
        }
        case 'k': {
          if (isshift) {
            // move editor camera to selected locator
            CMatrix4 matrix;
            if (mEditor.EditorGetEntityLocation(matrix) && _editorCamera) {
              _editorCamera->SetFromWorldSpaceMatrix(matrix);
            }
          }
          break;
        }
        case 'l': {
          if (isshift) {
            // move selected locator to editor camera
            // CMatrix4 matrix = mpActiveCamera->GetVMatrix();
            // matrix.Inverse();
            // mEditor.EditorLocateEntity(matrix);
          }
          break;
        }
        case 'e': {
          if (isctrl)
            mEditor.EditorNewEntity();
          break;
        }
        case 'r': {
          if (isctrl)
            mEditor.EditorReplicateEntity();
          break;
        }
        case 'q': {
          if (isctrl) {
            tool::GetGlobalDataFlowScheduler()->GraphSet().LockForWrite().clear();
            mMainWindow.SlotUpdateAll();
            tool::GetGlobalDataFlowScheduler()->GraphSet().UnLock();
          }
          break;
        }
        case Qt::Key_Pause: {
          /*switch( mEditor.mpSceneInst->GetSceneInstMode() )
          {
              case ent::ESCENEMODE_RUN:
                  mEditor.mpSceneInst->SetSceneInstMode( ent::ESCENEMODE_EDIT );
                  break;
              default:
                  mEditor.mpSceneInst->SetSceneInstMode( ent::ESCENEMODE_RUN );
                  break;
          }*/
          break;
        }
        case 'g': {
          if (isctrl)
            mGridMode = (mGridMode + 1) % 3;
          break;
        }
        case '~': {
          if (miCullCameraIndex >= 0)
            miCullCameraIndex = -1;
          else
            miCullCameraIndex = miCameraIndex;
          printf("CULLCAMERAINDEX<%d>\n", miCullCameraIndex);
          ret.SetHandled(this);
          break;
        }
        case '`': {
          miCameraIndex++;
          printf("CAMERAINDEX<%d>\n", miCameraIndex);
          ret.SetHandled(this);
          break;
        }
        case '1': {
          mCompositorSceneIndex++;
          ret.SetHandled(this);
          break;
        }
        case '2': {
          mCompositorSceneItemIndex++;
          ret.SetHandled(this);
          break;
        }
        case '!': {
          mCompositorSceneIndex = -1;
          ret.SetHandled(this);
          break;
        }
        case '@': {
          mCompositorSceneItemIndex = -1;
          ret.SetHandled(this);
          break;
        }
        case '/': {
          gtoggle_hud = !gtoggle_hud;
          break;
        }
        case ' ': {
          const auto& CDATA = GetCMCI()->systemData();
          CDATA.Toggle();
          break;
        }
        case 'f': // focus on selected entity
        {
          auto& selmgr = mEditor.SelectionManager();
          auto selset = selmgr.GetActiveSelection();

          if (selset.size() == 1) {
            EntData* as_ent = rtti::autocast(*selset.begin());

            if (as_ent) {
              auto& dn = as_ent->GetDagNode();
              CMatrix4 mtx;
              dn.GetMatrix(mtx);

              auto pos = mtx.GetTranslation();

              if (_editorCamera) {
                CCamera_persp* as_persp = rtti::autocast(_editorCamera);

                if (as_persp) {
                  as_persp->mvCenter = pos;
                }
              }
            }
          }
        }
        default:
          break;
      }
      break;
    }
    case ui::UIEV_PUSH: {
      printf("scenevp::uiev_push\n");
      int ix = EV.miX;
      int iy = EV.miY;

      int ity = 4;
      int itx = 4;
      for (orkmap<std::string, SceneEditorVPToolHandler*>::const_iterator it = mToolHandlers.begin(); it != mToolHandlers.end();
           it++) {
        int iX1 = itx;
        int iY1 = ity;
        int iX2 = itx + 32;
        int iY2 = ity + 32;

        bool bInWidget = ((ix >= iX1) && (ix < iX2) && (iy >= iY1) && (iy < iY2));

        if (bInWidget) {
          auto th = (*it).second;

          printf("bInWidget<%d>, th<%p>\n", int(bInWidget), th);
          BindToolHandler(th);
          ret.SetHandled(th);
        }
        ity += 36;
      }
      break;
    }
  }

  if (mpCurrentHandler) {
    if (!ret.WasHandled()) {
      // printf( "CurrentHandler<%s>\n", mpCurrentHandler->GetToolName().c_str() );
      ret = mpCurrentHandler->OnUiEvent(EV);
    }
  }

  return ret;
}

///////////////////////////////////////////////////////////////////////////

static CMatrix4 mtx_spawn;
void SceneEditorVPToolHandler::SetSpawnLoc(const lev2::GetPixelContext& ctx, float fx, float fy) {

  if (GetViewport()->GetActiveCamera()) {

    fvec4 normal_d = ctx.mPickColors[1];

    auto cam = GetViewport()->GetActiveCamera();
    auto& camdat = cam->mCameraData;
    fvec3 vdir, vori;
    camdat.ProjectDepthRay( fvec2( fx, fy ),
                            vdir,
                            vori );
    float l = normal_d.z;
    float il = 1.0-l;

    //float fd = (normal_d.GetZ()-camdat.GetNear() / camdat.GetFar());
    float fd = (il*camdat.GetNear())+(l*camdat.GetFar());
    fvec3 ynormal( normal_d.x, normal_d.y, normal_d.z );
    ynormal.Normalize();
    fvec3 spawnloc = normal_d.xyz(); //vori+(vdir*fd);
    cam->CamFocus = spawnloc;
    orkprintf( "vdir <%f,%f,%f>\n", vdir.x, vdir.y, vdir.z );
    orkprintf( "spawncursor <%f,%f,%f> fd<%f>\n", spawnloc.x, spawnloc.y, spawnloc.z, fd );
    fvec3 xnormal = ynormal.Cross( vdir.Normal() ).Normal();
    fvec3 znormal = xnormal.Cross( ynormal ).Normal();
    mtx_spawn.NormalVectorsIn( xnormal, ynormal, znormal );
    mtx_spawn.SetTranslation( spawnloc );
    mEditor.SetSpawnMatrix( mtx_spawn );
    cam->CamFocusYNormal = ynormal;
  }

}

///////////////////////////////////////////////////////////////////////////

}} // namespace ork::ent
