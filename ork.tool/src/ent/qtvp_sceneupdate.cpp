////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/thread.h>
#include <ork/kernel/timer.h>
#include <orktool/qtui/qtui_tool.h>

///////////////////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
//
#include <ork/kernel/timer.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/texman.h>

#include <ork/reflect/RegisterProperty.h>
#include <orktool/qtui/qtvp_edrenderer.h>
#include <pkg/ent/drawable.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.h>

#include <orktool/toolcore/dataflow.h>

///////////////////////////////////////////////////////////////////////////////

#include "qtui_scenevp.h"
#include "qtvp_uievh.h"
#include <pkg/ent/Compositor.h>
#include <pkg/ent/editor/edmainwin.h>

#include <ork/gfx/camera.h>
#include <ork/kernel/future.hpp>
#include <ork/lev2/lev2_asset.h>

#include <pkg/ent/Lighting.h>

///////////////////////////////////////////////////////////////////////////////

using namespace ork::lev2;

namespace ork { namespace ent {

///////////////////////////////////////////////////////////////////////////////

#if defined(_THREADED_RENDERER)

UpdateThread::UpdateThread(SceneEditorVP* pVP) : mpVP(pVP), mbEXITING(false) {}

///////////////////////////////////////////////////////////////////////////////

UpdateThread::~UpdateThread() { mbEXITING = true; }

///////////////////////////////////////////////////////////////////////////////

void UpdateThread::run() // virtual
{
  SetCurrentThreadName("UpdateRunLoop");

  ork::Timer timr;
  timr.Start();
  int icounter = 0;

  OpqTest opqtest(&UpdateSerialOpQ());

  while (false == mbEXITING) {
    icounter++;
    float fsecs = timr.SecsSinceStart();
    if (fsecs > 10.0f) {
      printf("ups<%f>\n", float(icounter) / fsecs);
      timr.Start();
      icounter = 0;
    }
    ////////////////////////////////////////////////
    // process serial update opQ
    ////////////////////////////////////////////////
    while (UpdateSerialOpQ().Process())
      ;
    ////////////////////////////////////////////////
    // update scene
    ////////////////////////////////////////////////
    switch (gUpdateStatus.meStatus) {
      case EUPD_START:
        mpVP->NotInDrawSync();
        gUpdateStatus.SetState(EUPD_RUNNING);
        break;
      case EUPD_RUNNING: {
        // ork::PerfMarkerPush( "ork.begin_update" );
        ent::DrawableBuffer* dbuf = ork::ent::DrawableBuffer::LockWriteBuffer(7);
        {
          OrkAssert(dbuf);

          auto psi = (ent::Simulation*)mpVP->simulation();
          if (psi) {
            auto cmci = psi->compositingSystem();
            float frame_rate = cmci ? cmci->currentFrameRate() : 0.0f;
            bool externally_fixed_rate = (frame_rate != 0.0f);

            if (externally_fixed_rate) {
              RenderSyncToken syntok;
              if (DrawableBuffer::mOfflineUpdateSynchro.try_pop(syntok)) {
                syntok.mFrameIndex++;
                psi->Update();
                DrawableBuffer::mOfflineRenderSynchro.push(syntok);
              }
            } else
              psi->Update();
          }
          mpVP->enqueueSimulationDrawables(dbuf);
        }
        ork::ent::DrawableBuffer::UnLockWriteBuffer(dbuf);
        // ork::PerfMarkerPush( "ork.end_update" );
        break;
      }
      case EUPD_STOP:
        mpVP->NotInDrawSync();
        gUpdateStatus.SetState(EUPD_STOPPED);
        break;
      case EUPD_STOPPED:
        mpVP->NotInDrawSync();
        break;
      default:
        assert(false);
        break;
    }
    ////////////////////////////////////////////////
    ork::msleep(1);
  }
}
#endif

///////////////////////////////////////////////////////////////////////////////

bool SceneEditorVP::DoNotify(const ork::event::Event* pev) {
  const ork::ent::SimulationEvent* sei = ork::rtti::autocast(pev);

  if (sei) {
    switch (sei->GetEvent()) {
      case ork::ent::SimulationEvent::ESIEV_DISABLE_UPDATE: {
        auto lamb = [=]() { gUpdateStatus.SetState(EUPD_STOP); };
        Op(lamb).QueueASync(UpdateSerialOpQ());
        break;
      }
      case ork::ent::SimulationEvent::ESIEV_ENABLE_UPDATE: {
        auto lamb = [=]() { gUpdateStatus.SetState(EUPD_START); };
        Op(lamb).QueueASync(UpdateSerialOpQ());
        break;
      }
      case ork::ent::SimulationEvent::ESIEV_DISABLE_VIEW: {
        auto lamb = [=]() {
          this->DisableSceneDisplay();
          //#disable path that would lead to gfx globallock
          //# maybe show a "loading" screen or something
        };
        Op(lamb).QueueASync(MainThreadOpQ());
        // mDbLock.ReleaseCurrent();
        break;
      }
      case ork::ent::SimulationEvent::ESIEV_ENABLE_VIEW: {
        auto lamb = [=]() {
          this->EnableSceneDisplay();
          //#disable path that would lead to gfx globallock
          //# maybe show a "loading" screen or something
        };
        Op(lamb).QueueASync(MainThreadOpQ());
        // mDbLock.ReleaseCurrent();
        break;
      }
      case ork::ent::SimulationEvent::ESIEV_BIND:
        // mDbLock.ReleaseCurrent();
        break;
      case ork::ent::SimulationEvent::ESIEV_START:
        break;
      case ork::ent::SimulationEvent::ESIEV_STOP:
        break;
      case ork::ent::SimulationEvent::ESIEV_USER:
        break;
    }
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::ent
