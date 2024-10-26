////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/opq.h>

#include <ork/lev2/input/inputdevice.h>
#include <ork/lev2/ui/event.h>

#include <ork/ecs/ReferenceArchetype.h>
#include <ork/ecs/entity.h>
#include <ork/ecs/scene.h>
#include <ork/ecs/system.h>
#include <ork/ecs/controller.h>
#include <ork/ecs/scene.inl>
#include <ork/util/logger.h>
#include <ork/profiling.inl>

namespace ork::ecs {

static logchannel_ptr_t logchan_simupdate = logger()->createChannel("ecs-simupdate", fvec3(0.9, 0.9, 0));

///////////////////////////////////////////////////////////////////////////////
float Simulation::_computeDeltaTime() {

  float frame_rate = 0.0f;

  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  float systime = float(OldSchool::GetRef().GetLoResTime());
  float fdelta  = (frame_rate != 0.0f) ? (1.0f / frame_rate) : (systime - mUpTime);

  static float fbasetime = systime;

  if (fdelta == 0.0f)
    return 0.0f;

  mUpTime      = systime;
  mUpDeltaTime = fdelta;

  ////////////////////////////////////////////
  // allowed FPS range is 1000hz to .5 hz
  ////////////////////////////////////////////
  if (fdelta < 0.00001f) {
    // orklogchan_simupdate->log( "FPS is over 10000HZ!!!! you need to reset valid fps range"
    // ); fdelta=0.001f; ork::msleep(1);
    systime      = float(OldSchool::GetRef().GetLoResTime());
    fdelta       = 0.00001f;
    mUpTime      = systime;
    mUpDeltaTime = fdelta;
  } else if (fdelta > 0.1f) {
    // orklogchan_simupdate->log( "FPS is less than 10HZ!!!! you need to reset valid fps
    // range" );
    fdelta = 0.1f;
  }

  ////////////////////////////////////////////

  mUpDeltaTime = fdelta;

  switch (_currentSimulationMode) {
    case ork::ecs::ESimulationMode::NEW:
    case ork::ecs::ESimulationMode::READY:
    case ork::ecs::ESimulationMode::EDIT: {
      mPrevDeltaTime = fdelta;
      mLastGameTime  = mGameTime;
      mGameTime += mDeltaTime;
      break;
    }
    case ork::ecs::ESimulationMode::PAUSE: {
      mDeltaTime = 0.0f;
      //			mDeltaTimeAccum = 1.0f/240.0f;
      break;
    }
    case ork::ecs::ESimulationMode::ACTIVE: {
      ///////////////////////////////
      // update clock
      ///////////////////////////////

      mDeltaTime     = (mPrevDeltaTime + fdelta) / 2;
      mPrevDeltaTime = fdelta;
      mLastGameTime  = mGameTime;
      mGameTime += mDeltaTime;
    }
    default:
      break;
  }

  //	logchan_simupdate->log( "mGameTime<%f>", mGameTime );

  return fdelta;
}

///////////////////////////////////////////////////////////////////////////////

void Simulation::_update() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  _updateThreadSM->update();
}

///////////////////////////////////////////////////////////////////////////////

void Simulation::_update_SIMSTATE() {

  EASY_FUNCTION("Simulation::_update_SIMSTATE");

  _computeDeltaTime();

  if (mDeltaTimeAccum > 1.0f)
    mDeltaTimeAccum = 1.0f;

  switch (_currentSimulationMode) {
    case ork::ecs::ESimulationMode::PAUSE: {
      logchan_simupdate->log("sim<%p> _update_SIMSTATE::PAUSE", (void*)this);
      ork::lev2::InputManager::instance()->poll();
      break;
    }
    case ork::ecs::ESimulationMode::ACTIVE: {

      // logchan_simupdate->log( "sim<%p> _update_SIMSTATE::ACTIVE", (void*) this );

      ork::PerfMarkerPush("ork.simulation.update.begin");

      ///////////////////////////////
      // Update Components
      ///////////////////////////////

      float frame_rate           = desiredFrameRate();
      bool externally_fixed_rate = (frame_rate != 0.0f);

      // float fdelta = 1.0f / 60.0f; // GetDeltaTime();
      float fdelta = deltaTime();

      float step = 0.0f; // ideally should be (1.0f/vsync rate) / some integer

      if (externally_fixed_rate) {
        mDeltaTimeAccum = fdelta;
        step            = fdelta; //(1.0f/120.0f); // ideally should be (1.0f/vsync rate) /
                                  // some integer
      } else {
        mDeltaTimeAccum += fdelta;
        step = 1.0f / 60.0f; //(1.0f/120.0f); // ideally should be (1.0f/vsync
                             // rate) / some integer
      }

      // Nasa - We are doing our own accumulator because there are frame-rate
      // independence bugs in bullet when we are not using a fixed time step
      // around the call to bullet's stepSimulation. Go figure. Nasa - I just
      // verified again that we are still not framerate independent if we take out
      // our own accumulator. 1-30-09.

      mDeltaTimeAccum = fdelta;
      step            = fdelta;

      while (mDeltaTimeAccum >= step) {
        mDeltaTimeAccum -= step;
        ork::lev2::InputManager::instance()->poll();

        mDeltaTime = step;

        _serviceDeactivateQueue();
        _serviceActivateQueue();

        mEntityUpdateCount += mActiveEntities.size();
      }

      mDeltaTime = step;

      ///////////////////////////////
      // todo this atomic will go away when we
      //  complete opq refactor
      ///////////////////////////////

      _systems.atomicOp([&](const SystemLut& syslut) { _updsyslutcopy = syslut; });
      for (auto sys : _updsyslutcopy)
        sys.second->_update(this);

      ///////////////////////////////

      ork::PerfMarkerPush("ork.simulation.update.end");

      ///////////////////////////////
      break;
    }
    default:
      logchan_simupdate->log("sim<%p> _update_SIMSTATE::???", (void*)this);
      break;
  }
}

///////////////////////////////////////////////////////////////////////////
void Simulation::_serviceDeactivateQueue() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  // Copy queue so we can queue more inside Stop
  orkvector<ecs::Entity*> deactivate_queue = mEntityDeactivateQueue;
  mEntityDeactivateQueue.clear();

  for (orkvector<ecs::Entity*>::const_iterator it = deactivate_queue.begin(); it != deactivate_queue.end(); it++) {
    ecs::Entity* pent = (*it);
    OrkAssert(pent);

    registerDeactivatedEntity(pent);
  }
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_serviceActivateQueue() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());

  // Copy queue so we can queue more inside Start (which would modify)
  orkvector<EntityActivationQueueItem> copy_of_activate_queue = mEntityActivateQueue;
  mEntityActivateQueue.clear();

  for (const EntityActivationQueueItem& item : copy_of_activate_queue) {
    ecs::Entity* pent = item._entity;
    OrkAssert(pent);
    if (auto parch = pent->data()->GetArchetype()) {
      parch->stageEntity(this, pent);
      parch->activateEntity(this, pent);
    }
    registerActivatedEntity(pent);
  }
}

} // namespace ork::ecs
