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
#include <ork/lev2/gfx/renderer/drawable.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.h>

#include <orktool/toolcore/dataflow.h>

///////////////////////////////////////////////////////////////////////////////

#include <pkg/ent/CompositingSystem.h>
#include <pkg/ent/editor/edmainwin.h>

#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/kernel/future.hpp>
#include <ork/lev2/lev2_asset.h>

#include <pkg/ent/LightingSystem.h>

#include "vpSceneEditor.h"
#include "uiToolsDefault.h"
#include "vpRenderer.h"

///////////////////////////////////////////////////////////////////////////////

using namespace ork::lev2;

namespace ork { namespace ent {

///////////////////////////////////////////////////////////////////////////////

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

  OpqTest opqtest(&updateSerialQueue());

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
    while (updateSerialQueue().Process())
      ;
    ////////////////////////////////////////////////
    // update scene
    ////////////////////////////////////////////////

    auto simulation = (ent::Simulation*) mpVP->simulation();
    if (simulation)
      simulation->updateThreadTick();

    switch (gUpdateStatus.meStatus) {
      case EUPD_START:
        mpVP->NotInDrawSync();
        gUpdateStatus.SetState(EUPD_RUNNING);
        break;
      case EUPD_RUNNING: {
        break;
      }
      case EUPD_STOP:
        mpVP->NotInDrawSync();
        gUpdateStatus.SetState(EUPD_STOPPED);
        break;
      case EUPD_STOPPED:{
        usleep(100);
        mpVP->NotInDrawSync();
        break;
      }
      default:
        assert(false);
        break;
    }
    ////////////////////////////////////////////////
    ork::msleep(1);
  }
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::ent
