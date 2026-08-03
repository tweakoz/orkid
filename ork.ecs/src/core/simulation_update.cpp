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
#include <ork/ecs/system_stats.h> // perf HUD: per-system timing
#include <ork/ecs/scene.inl>
#include "message_private.h"
#include <ork/util/logger.h>
#include <ork/kernel/profiler.h>

namespace ork::ecs {

static logchannel_ptr_t logchan_simupdate = logger()->configureChannel("ecs-simupdate", fvec3(0.9, 0.9, 0));

///////////////////////////////////////////////////////////////////////////////
float Simulation::_computeDeltaTime() {

  float frame_rate           = desiredFrameRate();
  bool externally_fixed_rate = (frame_rate != 0.0f);

  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  float systime = float(OldSchool::GetRef().GetLoResTime());
  float fdelta  = externally_fixed_rate ? (1.0f / frame_rate) : (systime - mUpTime);

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

      // wall-clock dt is averaged with the previous tick to keep frame jitter from
      // beating through the sim; an externally fixed rate must NOT be averaged —
      // otherwise the first tick spends a half delta and the run stops being one
      // exact step per tick (the whole point of the fixed clock).
      mDeltaTime     = externally_fixed_rate ? fdelta : ((mPrevDeltaTime + fdelta) / 2);
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
  fsm::FsmInstance::update(_updateThreadSMInst);
}

///////////////////////////////////////////////////////////////////////////////

void Simulation::_update_SIMSTATE() {

  OrkProfilerSampleScope(CHANNEL_UPDATE, "Simulation::_update_SIMSTATE");

  _computeDeltaTime();

  if (mDeltaTimeAccum > 1.0f)
    mDeltaTimeAccum = 1.0f;

  switch (_currentSimulationMode) {
    case ork::ecs::ESimulationMode::PAUSE: {
      ork::lev2::InputManager::instance()->poll();
      // the documented pause clock contract (simulation.h): dt reads 0, gameTime holds
      mPrevDeltaTime = 0.0f;
      mDeltaTime     = 0.0f;
      // RENDER-SYNC systems still update — the renderer reads the CAMERA from the
      // published draw buffer, so the scenegraph must keep ENQUEUEING for the view to
      // stay live while everything else is frozen. Gameplay systems hold (trait default).
      _systems.atomicOp([&](const SystemLut& syslut) { _updsyslutcopy = syslut; });
      for (auto sys : _updsyslutcopy)
        if (sys.second->updatesWhilePaused())
          sys.second->_update(this);
      break;
    }
    case ork::ecs::ESimulationMode::ACTIVE: {

      // logchan_simupdate->log( "sim<%p> _update_SIMSTATE::ACTIVE", (void*) this );

      OrkProfilerSampleScope(CHANNEL_UPDATE, "ecs.simulation.update");

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

        _updateSpawnerContexts();
        _updateEntityLifetimes();
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
      for (auto sys : _updsyslutcopy) {
        SystemStatScope _ss('u', sys.first); // perf HUD: per-system update-thread time
        sys.second->_update(this);
      }

      ///////////////////////////////
      break;
    }
    default:
      logchan_simupdate->log("sim<%p> _update_SIMSTATE::???", (void*)this);
      break;
  }
}

///////////////////////////////////////////////////////////////////////////
void Simulation::_sweepResponseCallbacks() {
  auto it = _pendingResponseCallbacks.begin();
  while (it != _pendingResponseCallbacks.end()) {
    auto& response = *it;
    if (response->_ready.load()) {
      if (response->_callback) {
        response->_callback();
      }
      it = _pendingResponseCallbacks.erase(it);
    } else {
      ++it;
    }
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
