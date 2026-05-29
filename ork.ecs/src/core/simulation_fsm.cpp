////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/opq.h>
#include <ork/kernel/string/deco.inl>

#include <ork/lev2/input/inputdevice.h>
#include <ork/lev2/ui/event.h>

#include <ork/ecs/ReferenceArchetype.h>
#include <ork/ecs/entity.h>
#include <ork/ecs/scene.h>
#include <ork/ecs/scene_import_data.h>
#include <ork/ecs/system.h>
#include <ork/ecs/controller.h>
#include <ork/ecs/scene.inl>
#include <ork/ecs/datatable.h>
#include <ork/util/logger.h>
#include <ork/kernel/profiler.h>

#include "message_private.h"
#include <random>

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan_simfsm = logger()->configureChannel("ecs.simfsm", fvec3(1.0, 0.9, 0), false);

struct RootState : public fsm::State {
  RootState(fsm::FsmData* data)
      : State(data) {
  }
  void onEnter(fsm::fsminstance_ptr_t inst) override {
    // logchan_simfsm->log("ROOT.enter");
  }
  void onExit(fsm::fsminstance_ptr_t inst) override {
    // logchan_simfsm->log("ROOT.exit");
  }
  void onUpdate(fsm::fsminstance_ptr_t inst) override {
    // logchan_simfsm->log("ROOT.update");
  }
};

///////////////////////////////////////////////////////////////////////////////

void Simulation::_buildStateMachine() {
  ///////////////////////////////////////////////////////////
  // set up update thread state machine
  ///////////////////////////////////////////////////////////
  _updateThreadSMData       = std::make_shared<fsm::FsmData>();
  auto upd_root             = _updateThreadSMData->newState<RootState>();
  _updateReadySimState      = _updateThreadSMData->newState<fsm::LambdaState>(upd_root);
  _updateEditSimState       = _updateThreadSMData->newState<fsm::LambdaState>(upd_root);
  _updateActiveSimState     = _updateThreadSMData->newState<fsm::LambdaState>(upd_root);
  _updatePausedSimState     = _updateThreadSMData->newState<fsm::LambdaState>(upd_root);
  _updateTerminatedSimState = _updateThreadSMData->newState<fsm::LambdaState>(upd_root);

  // Create instance
  _updateThreadSMInst = fsm::FsmInstance::create(_updateThreadSMData);

  ////////////////////////////////////////////////////////
  // READY STATE
  ////////////////////////////////////////////////////////
  _updateReadySimState->_onenter = [this](fsm::fsminstance_ptr_t inst) {
    logchan_simfsm->log("entering readymode");
    if (inst->currentState() == nullptr) {
      // Phase-locked forward init: compose + link on update thread, then a
      // single rendezvous runs _onGpuInit + _onGpuLink back-to-back on the
      // render thread before we return. All systems are fully composed AND
      // linked by the time their GPU hooks fire — no more partial-lut snapshot
      // races.
      _initialize();
      _compose();
      _link();
      _runGpuPhaseOnRenderThread([this](lev2::Context* ctx) {
        SystemLut gpu_systems;
        _systems.atomicOp([&](const SystemLut& syslut) { gpu_systems = syslut; });
        for (auto sys : gpu_systems) {
          sys.second->_onGpuInit(this, ctx);
        }
        if (_controller) for (auto& cb : _controller->_onGpuPostInit) cb(this, ctx);
        for (auto sys : gpu_systems) {
          sys.second->_onGpuLink(this, ctx);
        }
        if (_controller) for (auto& cb : _controller->_onGpuPostLink) cb(this, ctx);
        // GPU FSM is now ready to run the per-frame _gpuUpdate loop.
        _gpuUpdateSMInst->changeState(_gpuReadyState);
      });
    } else if (inst->currentState() == _updateEditSimState) {
      _runGpuPhaseOnRenderThread([this](lev2::Context* ctx) {
        SystemLut gpu_systems;
        _systems.atomicOp([&](const SystemLut& syslut) { gpu_systems = syslut; });
        for (auto it = gpu_systems.rbegin(); it != gpu_systems.rend(); ++it) {
          it->second->_gpuUnstage(this, ctx);
        }
      });
      _unstage();
    } else if (inst->currentState() == _updateActiveSimState) {
      _runGpuPhaseOnRenderThread([this](lev2::Context* ctx) {
        SystemLut gpu_systems;
        _systems.atomicOp([&](const SystemLut& syslut) { gpu_systems = syslut; });
        for (auto it = gpu_systems.rbegin(); it != gpu_systems.rend(); ++it) {
          it->second->_gpuDeactivate(this, ctx);
        }
      });
      _deactivate();
      _runGpuPhaseOnRenderThread([this](lev2::Context* ctx) {
        SystemLut gpu_systems;
        _systems.atomicOp([&](const SystemLut& syslut) { gpu_systems = syslut; });
        for (auto it = gpu_systems.rbegin(); it != gpu_systems.rend(); ++it) {
          it->second->_gpuUnstage(this, ctx);
        }
      });
      _unstage();
    }
  };
  //
  _updateReadySimState->_onupdate = [this](fsm::fsminstance_ptr_t inst) {
    auto DB = _dbufctxSIM->acquireForWriteLocked();
    DB->Reset();
    _dbufctxSIM->releaseFromWriteLocked(DB);
  };
  //
  _updateReadySimState->_onexit = [this](fsm::fsminstance_ptr_t inst) {};
  ////////////////////////////////////////////////////////
  // EDIT STATE
  ////////////////////////////////////////////////////////
  _updateEditSimState->_onenter = [this](fsm::fsminstance_ptr_t inst) {
    logchan_simfsm->log("entering editmode");
    //////////////////////////
    // did we come from ready or active state ?
    //////////////////////////
    if (inst->currentState() == _updateReadySimState) {
      logchan_simfsm->log(" .. from ready mode");
      lev2::DrawQueue::BeginClearAndSyncReaders();
      _stage();
      _runGpuPhaseOnRenderThread([this](lev2::Context* ctx) {
        SystemLut gpu_systems;
        _systems.atomicOp([&](const SystemLut& syslut) { gpu_systems = syslut; });
        for (auto sys : gpu_systems) {
          sys.second->_gpuStage(this, ctx);
        }
      });
      lev2::DrawQueue::EndClearAndSyncReaders();
    } else if (inst->currentState() == _updateActiveSimState) {
      logchan_simfsm->log(" .. from active mode");
      lev2::DrawQueue::BeginClearAndSyncReaders();
      ork::opq::assertOnQueue2(opq::updateSerialQueue());
      _runGpuPhaseOnRenderThread([this](lev2::Context* ctx) {
        SystemLut gpu_systems;
        _systems.atomicOp([&](const SystemLut& syslut) { gpu_systems = syslut; });
        for (auto it = gpu_systems.rbegin(); it != gpu_systems.rend(); ++it) {
          it->second->_gpuDeactivate(this, ctx);
        }
      });
      _deactivate();
      lev2::DrawQueue::EndClearAndSyncReaders();
    } else {
      OrkAssert(false);
    }
  };
  //
  _updateEditSimState->_onupdate = [this](fsm::fsminstance_ptr_t inst) {
    _serviceEventQueues();
    _systems.atomicOp([&](const SystemLut& syslut) {
      auto it = syslut.find("SceneGraphSystem");
      if (it != syslut.end())
        it->second->_update(this);
    });
  };
  ////////////////////////////////////////////////////////
  // ACTIVE STATE
  ////////////////////////////////////////////////////////
  _updateActiveSimState->_onenter = [this](fsm::fsminstance_ptr_t inst) {
    logchan_simfsm->log("entering activemode");
    //////////////////////////
    // did we come from ready or pause state ?
    //////////////////////////
    if (inst->currentState() == _updateEditSimState) {

    } else if (inst->currentState() == _updatePausedSimState) {

    } else {
      OrkAssert(false);
    }

    _activate();
    _runGpuPhaseOnRenderThread([this](lev2::Context* ctx) {
      SystemLut gpu_systems;
      _systems.atomicOp([&](const SystemLut& syslut) { gpu_systems = syslut; });
      for (auto sys : gpu_systems) {
        sys.second->_gpuActivate(this, ctx);
      }
    });

    ///////////////////////////////////
  };
  _updateActiveSimState->_onupdate = [this](fsm::fsminstance_ptr_t inst) {
    _serviceEventQueues();
    this->_update_SIMSTATE();
  };
  ////////////////////////////////////////////////////////
  // PAUSE STATE
  ////////////////////////////////////////////////////////
  _updatePausedSimState->_onenter = [this](fsm::fsminstance_ptr_t inst) {
    OrkAssert(inst->currentState() == _updateActiveSimState);
  };
  _updatePausedSimState->_onupdate = [this](fsm::fsminstance_ptr_t inst) {
    _serviceEventQueues();
    // todo actually render...
    auto DB = _dbufctxSIM->acquireForWriteLocked();
    DB->Reset();
    _dbufctxSIM->releaseFromWriteLocked(DB);
  };
  ////////////////////////////////////////////////////////

  _updateTerminatedSimState->_onenter = [this](fsm::fsminstance_ptr_t inst) {
    _controller->_delopq.atomicOp([&](Controller::delayed_opq_t& unlocked) { unlocked.clear(); });
    // todo actually render...
    auto DB = _dbufctxSIM->acquireForWriteLocked();
    DB->Reset();
    _dbufctxSIM->releaseFromWriteLocked(DB);

    // Phase-locked reverse teardown. Each GPU phase rendezvouses onto the
    // render thread in reverse system order before its CPU sibling runs on
    // the update thread. Uncommitted (never-entered) states are harmlessly
    // no-op since the new hooks default to empty and the old CPU helpers
    // already handle "not currently in this state" gracefully.
    _runGpuPhaseOnRenderThread([this](lev2::Context* ctx) {
      SystemLut gpu_systems;
      _systems.atomicOp([&](const SystemLut& syslut) { gpu_systems = syslut; });
      for (auto it = gpu_systems.rbegin(); it != gpu_systems.rend(); ++it) {
        it->second->_gpuDeactivate(this, ctx);
      }
    });
    _deactivate();
    _runGpuPhaseOnRenderThread([this](lev2::Context* ctx) {
      SystemLut gpu_systems;
      _systems.atomicOp([&](const SystemLut& syslut) { gpu_systems = syslut; });
      for (auto it = gpu_systems.rbegin(); it != gpu_systems.rend(); ++it) {
        it->second->_gpuUnstage(this, ctx);
      }
    });
    _unstage();
    // Sync with render thread before unlinking — ensures no stale
    // draw queue entries reference systems/components being destroyed
    lev2::DrawQueue::BeginClearAndSyncReaders();
    _runGpuPhaseOnRenderThread([this](lev2::Context* ctx) {
      SystemLut gpu_systems;
      _systems.atomicOp([&](const SystemLut& syslut) { gpu_systems = syslut; });
      for (auto it = gpu_systems.rbegin(); it != gpu_systems.rend(); ++it) {
        it->second->_gpuUnlink(this, ctx);
      }
    });
    _unlink();
    _runGpuPhaseOnRenderThread([this](lev2::Context* ctx) {
      SystemLut gpu_systems;
      _systems.atomicOp([&](const SystemLut& syslut) { gpu_systems = syslut; });
      for (auto it = gpu_systems.rbegin(); it != gpu_systems.rend(); ++it) {
        it->second->_onGpuExit(this, ctx);
      }
      // Controller::gpuExit is called later for final FSM cleanup; skip its
      // _onGpuExit loop so hooks don't fire twice.
      _gpuExitDone = true;
    });
    _decompose();
    _uninitialize();
    lev2::DrawQueue::EndClearAndSyncReaders();
  };

  ///////////////////////////////////////////////////////////
  // set up gpuUpdate state machine
  ///////////////////////////////////////////////////////////

  _gpuUpdateSMData = std::make_shared<fsm::FsmData>();
  auto gpu_root    = _gpuUpdateSMData->newState<RootState>();
  _gpuInitState    = _gpuUpdateSMData->newState<fsm::LambdaState>(gpu_root);
  _gpuReadyState   = _gpuUpdateSMData->newState<fsm::LambdaState>(gpu_root);
  _gpuTerminatedState = _gpuUpdateSMData->newState<fsm::LambdaState>(gpu_root);
  _gpuUpdateSMInst = fsm::FsmInstance::create(_gpuUpdateSMData);

  // GPU INIT STATE — body intentionally empty. Phase-locked init is now
  // driven explicitly by rendezvous from the update-thread FSM; this state
  // exists only as the initial resting state and to let the render FSM's
  // ren_init wait on a known starting value.
  _gpuInitState->_onupdate = [this](fsm::fsminstance_ptr_t inst) {
  };

  // GPU READY STATE (steady-state per-frame)
  _gpuReadyState->_onupdate = [this](fsm::fsminstance_ptr_t inst) {
    auto ctx = inst->vars()->typedValueForKey<lev2::Context*>("ctx");
    OrkAssert(ctx);
    SystemLut gpu_systems;
    _systems.atomicOp([&](const SystemLut& syslut) { gpu_systems = syslut; });
    for (auto sys : gpu_systems) {
      sys.second->_gpuUpdate(this, ctx.value());
    }
  };

  // GPU TERMINATED STATE
  _gpuTerminatedState->_onenter = [this](fsm::fsminstance_ptr_t inst) {};

  _gpuUpdateSMInst->changeState(_gpuInitState);

  ///////////////////////////////////////////////////////////
  // set up render thread state machine
  ///////////////////////////////////////////////////////////

  _renderThreadSMData       = std::make_shared<fsm::FsmData>();
  auto ren_root             = _renderThreadSMData->newState<RootState>();
  auto ren_init_state       = _renderThreadSMData->newState<fsm::LambdaState>(ren_root);
  auto ren_sim_state        = _renderThreadSMData->newState<fsm::LambdaState>(ren_root);
  _renderTerminatedSimState = _renderThreadSMData->newState<fsm::LambdaState>(ren_root);

  // Create instance
  _renderThreadSMInst = fsm::FsmInstance::create(_renderThreadSMData);

  //////////////////
  // RENDER INIT (waits for gpuUpdate FSM to reach ready state)
  //////////////////
  ren_init_state->_onenter = [this](fsm::fsminstance_ptr_t inst) {};
  //
  ren_init_state->_onupdate = [=](fsm::fsminstance_ptr_t inst) {
    if (_gpuUpdateSMInst->currentState() == _gpuReadyState) {
      if (auto sframe = inst->vars()->typedValueForKey<lev2::standardcompositorframe_ptr_t>("sframe")) {
        sframe.value()->attachDrawQueueContext(_dbufctxSIM);
      }
      _renderThreadSMInst->changeState(ren_sim_state);
    }
  };
  //////////////////
  // RENDER SIMULATION
  //////////////////
  ren_sim_state->_onenter = [this](fsm::fsminstance_ptr_t inst) {};
  //
  ren_sim_state->_onupdate = [this](fsm::fsminstance_ptr_t inst) {
    OrkProfilerSampleScope(CHANNEL_MAIN, "ecs::sim::fsm_render");
    if (auto as_sframe = inst->vars()->typedValueForKey<lev2::standardcompositorframe_ptr_t>("sframe")) {
      SystemLut render_systems;
      _systems.atomicOp([&](const SystemLut& syslut) { render_systems = syslut; });
      for (auto sys : render_systems) {
        sys.second->_beginRender();
      }
      for (auto sys : render_systems) {
        sys.second->_renderWithStandardCompositorFrame(this, as_sframe.value());
      }
      for (auto sys : render_systems) {
        sys.second->_endRender();
      }
    } else {
      if (_currentdrwev) {
        OrkAssert(_currentdrwev);
        SystemLut render_systems;
        {
          OrkProfilerSampleScope(CHANNEL_MAIN, "ecs::fsm_render::syslut");
          _systems.atomicOp([&](const SystemLut& syslut) { render_systems = syslut; });
        }
        {
          OrkProfilerSampleScope(CHANNEL_MAIN, "ecs::fsm_render::beginRender");
          for (auto sys : render_systems) {
            sys.second->_beginRender();
          }
        }
        {
          OrkProfilerSampleScope(CHANNEL_MAIN, "ecs::fsm_render::render");
          for (auto sys : render_systems)
            sys.second->_render(this, _currentdrwev);
        }
        {
          OrkProfilerSampleScope(CHANNEL_MAIN, "ecs::fsm_render::endRender");
          for (auto sys : render_systems) {
            sys.second->_endRender();
          }
        }
      }
    }
  };

  _renderTerminatedSimState->_onenter = [this](fsm::fsminstance_ptr_t inst) {
    //_unlink();
    //_decompose();
    //_uninitialize();
  };

  ///////////////////////////////////////////////////////////

  _renderThreadSMInst->changeState(ren_init_state);
}

void Simulation::_resetClock() {
  mStartTime      = float(OldSchool::GetRef().GetLoResTime());
  mGameTime       = 0.0f;
  mUpDeltaTime    = 0.0f;
  mPrevDeltaTime  = 1.0f / 30.0f;
  mDeltaTime      = 1.0f / 30.0f;
  mDeltaTimeAccum = 0.0f;
  mUpTime         = mStartTime;
  mLastGameTime   = 0.0f;
}
///////////////////////////////////////////////////////////////////////////////
void Simulation::SetSimulationMode(ESimulationMode emode) {
  // Called from both main thread (initial load) and update thread (hotload)
  // Serialization is provided by Controller::_simulation.atomicOp mutex
  switch (emode) {
    case ESimulationMode::NEW:
      break;
    ///////////////////////////////////////
    case ESimulationMode::READY:
      logchan_simfsm->log("SetMode: READY");
      switch (_currentSimulationMode) {
        case ork::ecs::ESimulationMode::NEW:
          _updateThreadSMInst->changeState(_updateReadySimState);
          break;
        default:
          OrkAssert(false);
          break;
      }
      break;
    ///////////////////////////////////////
    case ESimulationMode::EDIT:
      logchan_simfsm->log("SetMode: EDIT");
      switch (_currentSimulationMode) {
        case ork::ecs::ESimulationMode::NEW:
          _updateThreadSMInst->changeState(_updateReadySimState);
          _updateThreadSMInst->changeState(_updateEditSimState);
          break;
        case ork::ecs::ESimulationMode::READY:
        case ork::ecs::ESimulationMode::ACTIVE:
          _updateThreadSMInst->changeState(_updateEditSimState);
          break;
        case ork::ecs::ESimulationMode::EDIT:
          break;
        default:
          OrkAssert(false);
          break;
      }
      break;
    ///////////////////////////////////////
    case ESimulationMode::ACTIVE:
      logchan_simfsm->log("SetMode: ACTIVE");
      switch (_currentSimulationMode) {
        case ork::ecs::ESimulationMode::NEW:
          _updateThreadSMInst->changeState(_updateReadySimState);
          _updateThreadSMInst->changeState(_updateEditSimState);
          _updateThreadSMInst->changeState(_updateActiveSimState);
          break;
        case ork::ecs::ESimulationMode::READY:
        case ork::ecs::ESimulationMode::EDIT:
          _updateThreadSMInst->changeState(_updateActiveSimState);
          break;
        case ork::ecs::ESimulationMode::ACTIVE:
          break;
        default:
          OrkAssert(false);
          break;
      }
      break;
    ///////////////////////////////////////
    case ESimulationMode::PAUSE:
      switch (_currentSimulationMode) {
        case ork::ecs::ESimulationMode::ACTIVE:
          _updateThreadSMInst->changeState(_updatePausedSimState);
          break;
        default:
          OrkAssert(false);
          break;
      }
      break;
    ///////////////////////////////////////
    case ESimulationMode::TERMINATED:
      _updateThreadSMInst->changeState(_updateTerminatedSimState);
      _renderThreadSMInst->changeState(_renderTerminatedSimState);
      _gpuUpdateSMInst->changeState(_gpuTerminatedState);
      break;
    ///////////////////////////////////////
    case ESimulationMode::NONE:
      OrkAssert(false);
      break;
  }
  _currentSimulationMode = emode;
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_initialize() {
  _initializeEntities();
  _transportState = ESimulationTransport::INITIALIZED;
  logchan_simfsm->log("Simulation<%p> _initialized", (void*)this);
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_compose() {
  if (_controller) for (auto& cb : _controller->_onUpdPreCompose) cb(this);
  _composeSystems();
  _composeEntities();
  _transportState = ESimulationTransport::COMPOSED;
  if (_controller) for (auto& cb : _controller->_onUpdPostCompose) cb(this);
  logchan_simfsm->log("Simulation<%p> _composed", (void*)this);
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_link() {
  if (_controller) for (auto& cb : _controller->_onUpdPreLink) cb(this);
  _linkSystems();
  _linkEntities();
  if (_onLink)
    _onLink();
  _transportState = ESimulationTransport::LINKED;
  if (_controller) for (auto& cb : _controller->_onUpdPostLink) cb(this);
  logchan_simfsm->log("Simulation<%p> _linked", (void*)this);
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_stage() {
  if (_controller) for (auto& cb : _controller->_onUpdPreStage) cb(this);
  _stageSystems();
  _stageEntities();
  _resetClock();
  _serviceDeactivateQueue();
  _transportState = ESimulationTransport::STAGED;
  if (_controller) for (auto& cb : _controller->_onUpdPostStage) cb(this);
  logchan_simfsm->log("Simulation<%p> _staged", (void*)this);
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_activate() {
  if (_controller) for (auto& cb : _controller->_onUpdPreActivate) cb(this);
  // Detach entity transforms from spawner transforms
  // so physics (and other runtime systems) don't corrupt spawner data.
  // In edit mode, entities share the spawner's DecompTransform pointer
  // so gizmo manipulations propagate. _activate() is only called for
  // play/run mode, so this preserves the edit-mode sharing.
  for (auto& item : mEntities) {
    auto pent = item.second;
    auto shared_xf = pent->transform();
    auto independent_xf = std::make_shared<DecompTransform>();
    independent_xf->_translation = shared_xf->_translation;
    independent_xf->_rotation = shared_xf->_rotation;
    independent_xf->_uniformScale = shared_xf->_uniformScale;
    pent->setTransform(independent_xf);
  }
  _activateSystems();
  _activateEntities();
  _transportState = ESimulationTransport::ACTIVATED;
  if (_controller) for (auto& cb : _controller->_onUpdPostActivate) cb(this);
  logchan_simfsm->log("Simulation<%p> _activated", (void*)this);
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_uninitialize() {
  _uninitializeEntities();
  _transportState = ESimulationTransport::TERMINATED;
  logchan_simfsm->log("Simulation<%p> _uninitialized", (void*)this);
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_decompose() {
  _decomposeEntities();
  _decomposeSystems();
  logchan_simfsm->log("Simulation<%p> _decomposed", (void*)this);
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_unlink() {
  _unlinkEntities();
  _unlinkSystems();
  logchan_simfsm->log("Simulation<%p> _unlinked", (void*)this);
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_unstage() {
  if (_controller) for (auto& cb : _controller->_onUpdPreUnstage) cb(this);
  _unstageEntities();
  _unstageSystems();
  if (_controller) for (auto& cb : _controller->_onUpdPostUnstage) cb(this);
  logchan_simfsm->log("Simulation<%p> _unstaged", (void*)this);
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_deactivate() {
  if (_controller) for (auto& cb : _controller->_onUpdPreDeactivate) cb(this);
  _deactivateEntities();
  _deactivateSystems();
  mActiveEntities.clear();
  mEntityDeactivateQueue.clear();
  if (_controller) for (auto& cb : _controller->_onUpdPostDeactivate) cb(this);
  logchan_simfsm->log("Simulation<%p> _deactivated", (void*)this);
}
///////////////////////////////////////////////////////////////////////////
static thread_local std::mt19937 _spawn_rng{std::random_device{}()};
static thread_local std::uniform_real_distribution<float> _spawn_dist(-1.0f, 1.0f);

///////////////////////////////////////////////////////////////////////////

static fvec3 _generateScatteredPosition(
    const fvec3& basePos,
    const fvec3& posRandRadius,
    const fvec3& minDist,
    const std::vector<fvec3>& existingPositions) {

  bool hasScatter = (posRandRadius.x > 0 || posRandRadius.y > 0 || posRandRadius.z > 0);
  if (!hasScatter)
    return basePos;

  bool hasMinDist = (minDist.x > 0 || minDist.y > 0 || minDist.z > 0);

  const int maxRetries = 64;
  for (int retry = 0; retry < maxRetries; retry++) {
    fvec3 candidate = basePos + fvec3(
        posRandRadius.x * _spawn_dist(_spawn_rng),
        posRandRadius.y * _spawn_dist(_spawn_rng),
        posRandRadius.z * _spawn_dist(_spawn_rng));

    if (!hasMinDist || existingPositions.empty())
      return candidate;

    bool tooClose = false;
    for (auto& prev : existingPositions) {
      fvec3 delta = candidate - prev;
      // rectangular exclusion zone: candidate is too close if it's
      // within minDist on ALL checked axes simultaneously
      bool insideX = (minDist.x <= 0) || (fabsf(delta.x) < minDist.x);
      bool insideY = (minDist.y <= 0) || (fabsf(delta.y) < minDist.y);
      bool insideZ = (minDist.z <= 0) || (fabsf(delta.z) < minDist.z);
      if (insideX && insideY && insideZ) {
        tooClose = true;
        break;
      }
    }
    if (!tooClose)
      return candidate;
  }
  // exhausted retries — return last attempt
  return basePos + fvec3(
      posRandRadius.x * _spawn_dist(_spawn_rng),
      posRandRadius.y * _spawn_dist(_spawn_rng),
      posRandRadius.z * _spawn_dist(_spawn_rng));
}

///////////////////////////////////////////////////////////////////////////

static fvec3 _computeInitialVelocity(const SpawnData& sd) {
  if (sd._initialSpeed <= 0.0f)
    return fvec3(0, 0, 0);
  fvec3 dir = sd._initialDirection.normalized();
  if (sd._directionRandomize > 0.0f) {
    fvec3 rnd = fvec3(_spawn_dist(_spawn_rng), _spawn_dist(_spawn_rng), _spawn_dist(_spawn_rng)).normalized();
    dir = (dir * (1.0f - sd._directionRandomize) + rnd * sd._directionRandomize).normalized();
  }
  return dir * sd._initialSpeed;
}

///////////////////////////////////////////////////////////////////////////

void Simulation::_initializeEntities() {

  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  ///////////////////////////////////
  // clear runtime containers
  ///////////////////////////////////

  mEntities.clear();
  _cameraDataLUT.clear();
  _spawnerContexts.clear();

  ///////////////////////////////////
  // Compose Entities
  ///////////////////////////////////

  logchan_simfsm->log("simulation<%p> Composing AutoSpawn Entities..", (void*)this);

  auto scene = _controller->_scenedata;

  for (auto it : scene->GetSceneObjects()) {
    auto sobj = it.second;
    if (auto spawner = std::dynamic_pointer_cast<SpawnData>(sobj)) {

      if (!spawner->autoSpawn())
        continue;

      auto arch = spawner->GetArchetype();
      int spawnCount = std::max(1, spawner->_spawnCount);
      bool timed = (spawnCount > 1) && (spawner->_spawnInterval > 0.0f);

      // how many to create during initialization (all at once, or just 1 if timed)
      int immediateCount = timed ? 1 : spawnCount;

      // set up spawner context for multi-spawn
      spawnercontext_ptr_t ctx;
      if (spawnCount > 1) {
        ctx = std::make_shared<SpawnerContext>();
        ctx->_spawnData = spawner;
        _spawnerContexts[spawner] = ctx;
      }

      // determine draw layer once
      std::string actualLayerName = "Default";
      ConstString layer_name = spawner->GetUserProperty("DrawLayer");
      if (strlen(layer_name.c_str()) != 0) {
        actualLayerName = layer_name.c_str();
      }
      auto layer_data = GetLayerData(actualLayerName);
      if (!layer_data) {
        layer_data = new lev2::LayerData;
        AddLayerData(actualLayerName, layer_data);
      }

      // base transform from spawner
      auto baseXf = spawner->_dagnode->_xfnode->_transform;

      for (int si = 0; si < immediateCount; si++) {

        // unique name per spawn
        PoolString entName;
        if (spawnCount == 1) {
          entName = spawner->GetName();
        } else {
          entName = AddPooledString(FormatString("%s_%d", spawner->GetName().c_str(), si).c_str());
        }

        uint64_t entref = _controller->_objectIdCounter.fetch_add(1);
        Entity* pent = new Entity(spawner, this, entref);
        _controller->_mutateObject([&](Controller::id2obj_map_t& unlocked) { unlocked[entref].set<Entity*>(pent); });

        if(0)logchan_simfsm->log(
            "Compose AutoSpawn Entity<%p> arch<%p> layer<%s>",
            (void*)pent, (void*)arch.get(), layer_name.c_str());

        // position: base + scatter with min-distance enforcement
        fvec3 pos = baseXf->_translation;
        if (ctx) {
          pos = _generateScatteredPosition(
              baseXf->_translation,
              spawner->_positionRandomRadius,
              spawner->_minDistance,
              ctx->_spawnedPositions);
          ctx->_spawnedPositions.push_back(pos);
          ctx->_spawnedSoFar++;
        }

        auto entXf = std::make_shared<DecompTransform>();
        entXf->_translation = pos;
        entXf->_rotation = baseXf->_rotation;
        entXf->_uniformScale = baseXf->_uniformScale;
        pent->setTransform(entXf);

        // initial velocity stored in entity varmap
        fvec3 vel = _computeInitialVelocity(*spawner);
        if (vel.magnitudeSquared() > 0.0f) {
          pent->_varmap->makeValueForKey<fvec3>("initialVelocity") = vel;
        }

        // lifetime-based despawn (0,0 = lives forever)
        if (spawner->_lifetimeMin > 0.0f || spawner->_lifetimeMax > 0.0f) {
          float ltMin = spawner->_lifetimeMin;
          float ltMax = std::max(ltMin, spawner->_lifetimeMax);
          std::uniform_real_distribution<float> ltDist(ltMin, ltMax);
          pent->_despawnTime = mGameTime + ltDist(_spawn_rng);
        }

        mEntities[entName] = pent;

        if (spawner->_onSpawn) {
          auto invocation         = std::make_shared<deferred_script_invokation>();
          invocation->_cb         = spawner->_onSpawn;
          auto& datatable         = *invocation->_data.makeShared<DataTable>();
          EntityRef eref          = {pent->_entref};
          datatable["entity"_tok] = pyentity_ptr_t(pent);
          datatable["entref"_tok] = eref;
          this->_enqueueDeferredInvokation(invocation);
        }
      }

      // for timed spawning, set first spawn time
      if (timed && ctx) {
        float jitter = spawner->_stochasticInterval * fabsf(_spawn_dist(_spawn_rng));
        ctx->_nextSpawnTime = spawner->_spawnInterval + jitter;
      }

      // if all spawns done, remove context
      if (ctx && ctx->_spawnedSoFar >= spawnCount) {
        _spawnerContexts.erase(spawner);
      }
    }
  }

  // Also iterate imported scenes' spawners (filtered by selection)
  for (auto& [ns, importedScene] : _controller->_importedScenes) {
    // Find the selection manifest for this namespace
    sceneimportdata_ptr_t importData;
    auto colonPos = ns.rfind(':');
    if (colonPos == std::string::npos) {
      auto& imports = _controller->_scenedata->getImports();
      auto iit = imports.find(ns);
      if (iit != imports.end()) importData = iit->second;
    } else {
      auto parentNs = ns.substr(0, colonPos);
      auto childNs = ns.substr(colonPos + 1);
      auto parentScene = _controller->findImportedScene(parentNs);
      if (parentScene) {
        auto parentSceneMut = std::const_pointer_cast<SceneData>(parentScene);
        auto& imports = parentSceneMut->getImports();
        auto iit = imports.find(childNs);
        if (iit != imports.end()) importData = iit->second;
      }
    }

    for (auto it : importedScene->GetSceneObjects()) {
      auto sobj = it.second;
      if (auto spawner = std::dynamic_pointer_cast<SpawnData>(sobj)) {

        // Check if this spawner is selected
        if (importData) {
          auto spawnerName = std::string(spawner->GetName().c_str());
          auto& sel = importData->_selectedSpawners;
          if (std::find(sel.begin(), sel.end(), spawnerName) == sel.end())
            continue;
        }

        if (!spawner->autoSpawn())
          continue;

        auto arch = spawner->GetArchetype();
        int spawnCount = std::max(1, spawner->_spawnCount);
        bool timed = (spawnCount > 1) && (spawner->_spawnInterval > 0.0f);
        int immediateCount = timed ? 1 : spawnCount;

        spawnercontext_ptr_t ctx;
        if (spawnCount > 1) {
          ctx = std::make_shared<SpawnerContext>();
          ctx->_spawnData = spawner;
          _spawnerContexts[spawner] = ctx;
        }

        std::string actualLayerName = "Default";
        ConstString layer_name = spawner->GetUserProperty("DrawLayer");
        if (strlen(layer_name.c_str()) != 0) {
          actualLayerName = layer_name.c_str();
        }
        auto layer_data = GetLayerData(actualLayerName);
        if (!layer_data) {
          layer_data = new lev2::LayerData;
          AddLayerData(actualLayerName, layer_data);
        }

        auto baseXf = spawner->_dagnode->_xfnode->_transform;

        for (int si = 0; si < immediateCount; si++) {
          PoolString entName;
          if (spawnCount == 1) {
            // Prefix with namespace to avoid name collisions
            entName = AddPooledString(FormatString("%s:%s", ns.c_str(), spawner->GetName().c_str()).c_str());
          } else {
            entName = AddPooledString(FormatString("%s:%s_%d", ns.c_str(), spawner->GetName().c_str(), si).c_str());
          }

          uint64_t entref = _controller->_objectIdCounter.fetch_add(1);
          Entity* pent = new Entity(spawner, this, entref);
          _controller->_mutateObject([&](Controller::id2obj_map_t& unlocked) { unlocked[entref].set<Entity*>(pent); });

          fvec3 pos = baseXf->_translation;
          if (ctx) {
            pos = _generateScatteredPosition(
                baseXf->_translation,
                spawner->_positionRandomRadius,
                spawner->_minDistance,
                ctx->_spawnedPositions);
            ctx->_spawnedPositions.push_back(pos);
            ctx->_spawnedSoFar++;
          }

          auto entXf = std::make_shared<DecompTransform>();
          entXf->_translation = pos;
          entXf->_rotation = baseXf->_rotation;
          entXf->_uniformScale = baseXf->_uniformScale;
          pent->setTransform(entXf);

          fvec3 vel = _computeInitialVelocity(*spawner);
          if (vel.magnitudeSquared() > 0.0f) {
            pent->_varmap->makeValueForKey<fvec3>("initialVelocity") = vel;
          }

          if (spawner->_lifetimeMin > 0.0f || spawner->_lifetimeMax > 0.0f) {
            float ltMin = spawner->_lifetimeMin;
            float ltMax = std::max(ltMin, spawner->_lifetimeMax);
            std::uniform_real_distribution<float> ltDist(ltMin, ltMax);
            pent->_despawnTime = mGameTime + ltDist(_spawn_rng);
          }

          mEntities[entName] = pent;

          if (spawner->_onSpawn) {
            auto invocation         = std::make_shared<deferred_script_invokation>();
            invocation->_cb         = spawner->_onSpawn;
            auto& datatable         = *invocation->_data.makeShared<DataTable>();
            EntityRef eref          = {pent->_entref};
            datatable["entity"_tok] = pyentity_ptr_t(pent);
            datatable["entref"_tok] = eref;
            this->_enqueueDeferredInvokation(invocation);
          }
        }

        if (timed && ctx) {
          float jitter = spawner->_stochasticInterval * fabsf(_spawn_dist(_spawn_rng));
          ctx->_nextSpawnTime = spawner->_spawnInterval + jitter;
        }

        if (ctx && ctx->_spawnedSoFar >= spawnCount) {
          _spawnerContexts.erase(spawner);
        }
      }
    }
  }
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_updateSpawnerContexts() {
  if (_spawnerContexts.empty())
    return;

  float gameTime = mGameTime;
  std::vector<spawndata_constptr_t> exhausted;

  for (auto& [spawnData, ctx] : _spawnerContexts) {
    int spawnCount = std::max(1, spawnData->_spawnCount);
    if (ctx->_spawnedSoFar >= spawnCount) {
      exhausted.push_back(spawnData);
      continue;
    }
    if (gameTime < ctx->_nextSpawnTime)
      continue;

    // time to spawn the next entity
    auto baseXf = spawnData->_dagnode->_xfnode->_transform;
    fvec3 pos = _generateScatteredPosition(
        baseXf->_translation,
        spawnData->_positionRandomRadius,
        spawnData->_minDistance,
        ctx->_spawnedPositions);
    ctx->_spawnedPositions.push_back(pos);
    ctx->_spawnedSoFar++;

    // build override transform
    auto ovxf = std::make_shared<DecompTransform>();
    ovxf->_translation = pos;
    ovxf->_rotation = baseXf->_rotation;
    ovxf->_uniformScale = baseXf->_uniformScale;

    // spawn via the dynamic entity path
    auto SAD = std::make_shared<SpawnAnonDynamic>();
    SAD->_edataname = spawnData->GetName();
    SAD->_overridexf = ovxf;

    impl::_SpawnAnonDynamic internal_SAD;
    internal_SAD._SAD = SAD;
    internal_SAD._spawn_rec = spawnData;
    internal_SAD._entref._entID = _controller->_objectIdCounter.fetch_add(1);

    auto entName = AddPooledString(
        FormatString("%s_%d", spawnData->GetName().c_str(), ctx->_spawnedSoFar - 1).c_str());
    Entity* pent = _spawnNamedDynamicEntity(internal_SAD, entName);

    // initial velocity + lifetime
    if (pent) {
      fvec3 vel = _computeInitialVelocity(*spawnData);
      if (vel.magnitudeSquared() > 0.0f) {
        pent->_varmap->makeValueForKey<fvec3>("initialVelocity") = vel;
      }
      if (spawnData->_lifetimeMin > 0.0f || spawnData->_lifetimeMax > 0.0f) {
        float ltMin = spawnData->_lifetimeMin;
        float ltMax = std::max(ltMin, spawnData->_lifetimeMax);
        std::uniform_real_distribution<float> ltDist(ltMin, ltMax);
        pent->_despawnTime = gameTime + ltDist(_spawn_rng);
      }
    }

    // schedule next
    if (ctx->_spawnedSoFar < spawnCount) {
      float jitter = spawnData->_stochasticInterval * fabsf(_spawn_dist(_spawn_rng));
      ctx->_nextSpawnTime = gameTime + spawnData->_spawnInterval + jitter;
    } else {
      exhausted.push_back(spawnData);
    }
  }

  for (auto& key : exhausted) {
    _spawnerContexts.erase(key);
  }
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_updateEntityLifetimes() {
  if (mGameTime <= 0.0f)
    return;
  for (auto it = mActiveEntities.begin(); it != mActiveEntities.end(); ++it) {
    Entity* pent = *it;
    if (pent->_despawnTime > 0.0f && mGameTime >= pent->_despawnTime) {
      enqueueDespawnEntity(pent);
    }
  }
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_uninitializeEntities() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  for (auto item : mEntities) {
    const ork::PoolString& name = item.first;
    ork::ecs::Entity* pent      = item.second;
    delete pent;
  }
  mEntities.clear();
  _spawnerContexts.clear();
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_composeEntities() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  for (auto item : mEntities) {
    ork::ecs::Entity* pent = item.second;
    auto edata             = pent->data();
    if (edata and edata->GetArchetype()) {
      edata->GetArchetype()->composeEntity(this, pent);
    }
  }
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_decomposeEntities() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  for (auto item : mEntities) {
    ork::ecs::Entity* pent = item.second;
    auto edata             = pent->data();
    OrkAssert(pent);
    if (edata and edata->GetArchetype()) {
      edata->GetArchetype()->decomposeEntity(this, pent);
    }
  }
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_composeSystems() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());

  // Primary scene systems
  auto scene     = _controller->_scenedata;
  auto& sysdatas = scene->getSystemDatas();
  std::set<std::string> createdSystemTypes;
  for (auto it : sysdatas) {
    auto pscd = it.second;
    if (pscd != nullptr) {
      auto sys = pscd->createSystem(this);
      auto sysType = sys->systemTypeDynamic();
      addSystem(sysType, sys);
      createdSystemTypes.insert(it.first);
    }
  }

  // Imported scene systems (filtered by selection, skip duplicates)
  for (auto& [ns, importedScene] : _controller->_importedScenes) {
    // Find the selection manifest for this namespace
    sceneimportdata_ptr_t importData;
    // Walk the namespace hierarchy to find the import data
    // For top-level "env", look in primary scene
    // For nested "env:props", look in parent imported scene
    auto colonPos = ns.rfind(':');
    if (colonPos == std::string::npos) {
      // top-level import
      auto& imports = _controller->_scenedata->getImports();
      auto iit = imports.find(ns);
      if (iit != imports.end()) importData = iit->second;
    } else {
      // nested import — parent ns is everything before last colon
      auto parentNs = ns.substr(0, colonPos);
      auto childNs = ns.substr(colonPos + 1);
      auto parentScene = _controller->findImportedScene(parentNs);
      if (parentScene) {
        auto parentSceneMut = std::const_pointer_cast<SceneData>(parentScene);
        auto& imports = parentSceneMut->getImports();
        auto iit = imports.find(childNs);
        if (iit != imports.end()) importData = iit->second;
      }
    }

    auto& importedSysDatas = importedScene->getSystemDatas();
    for (auto& sit : importedSysDatas) {
      auto pscd = sit.second;
      if (pscd == nullptr) continue;

      // Skip if this system type was already created (local wins)
      if (createdSystemTypes.count(sit.first)) continue;

      // If we have selection data, check if this system is selected
      if (importData) {
        auto& sel = importData->_selectedSystems;
        if (std::find(sel.begin(), sel.end(), sit.first) == sel.end())
          continue;
      }

      auto sys = pscd->createSystem(this);
      auto sysType = sys->systemTypeDynamic();
      addSystem(sysType, sys);
      createdSystemTypes.insert(sit.first);
    }
  }
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_decomposeSystems() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  _systems.atomicOp([&](SystemLut& syslut) {
    for (auto item : syslut) {
      auto comp = item.second;
      delete comp;
    }
    syslut.clear();
  });
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_linkEntities() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  for (auto item : mEntities) {
    ork::ecs::Entity* pent = item.second;
    auto edata             = pent->data();
    OrkAssert(pent);
    if (edata and edata->GetArchetype()) {
      edata->GetArchetype()->linkEntity(this, pent);
    }
  }
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_unlinkEntities() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  for (auto item : mEntities) {
    ork::ecs::Entity* pent = item.second;
    auto edata             = pent->data();
    OrkAssert(pent);
    if (edata->GetArchetype()) {
      edata->GetArchetype()->unlinkEntity(this, pent);
    }
  }
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_linkSystems() {
  _systems.atomicOp([&](const SystemLut& syslut) {
    for (auto it : syslut) {
      System* ci = it.second;
      ci->_link(this);
    }
  });
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_unlinkSystems() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  _systems.atomicOp([&](const SystemLut& syslut) {
    for (auto it : syslut) {
      System* ci = it.second;
      ci->_unlink(this);
    }
  });
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_stageEntities() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  for (auto item : mEntities) {
    ork::ecs::Entity* pent = item.second;
    auto edata             = pent->data();
    OrkAssert(pent);
    if (edata and edata->GetArchetype()) {
      edata->GetArchetype()->stageEntity(this, pent);
    }
  }
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_unstageEntities() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  for (auto item : mEntities) {
    ork::ecs::Entity* pent = item.second;
    auto edata             = pent->data();
    OrkAssert(pent);
    if (edata->GetArchetype()) {
      edata->GetArchetype()->unstageEntity(this, pent);
    }
  }
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_stageSystems() {
  _systems.atomicOp([&](const SystemLut& syslut) {
    for (auto it : syslut) {
      System* ci = it.second;
      ci->_stage(this);
    }
  });
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_unstageSystems() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  _systems.atomicOp([&](const SystemLut& syslut) {
    for (auto it : syslut) {
      System* ci = it.second;
      ci->_unstage(this);
    }
  });
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_activateEntities() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  for (orkmap<ork::PoolString, ork::ecs::Entity*>::const_iterator it = mEntities.begin(); it != mEntities.end(); it++) {
    ork::ecs::Entity* pent = it->second;
    auto edata             = pent->data();
    OrkAssert(pent);
    if (edata->GetArchetype()) {
      edata->GetArchetype()->activateEntity(this, pent);
    }
    // Static-spawn publication. Dynamic-spawn path is covered separately
    // by registerActivatedEntity (called from the activate queue
    // service). Both endpoints share the same publishEntityXf so the
    // counter and key namespace are unified across static/dynamic.
    if (edata && !edata->_publishxf_name.empty()) {
      publishEntityXf(pent, edata->_publishxf_name);
    }
  }
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_deactivateEntities() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  for (orkmap<ork::PoolString, ork::ecs::Entity*>::const_iterator it = mEntities.begin(); it != mEntities.end(); it++) {
    ork::ecs::Entity* pent = it->second;
    if (pent) {
      auto edata = pent->data();
      OrkAssert(pent);
      if (edata->GetArchetype()) {
        edata->GetArchetype()->deactivateEntity(this, pent);
      }
      unpublishEntityXf(pent);
    }
  }
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_activateSystems() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  _systems.atomicOp([&](const SystemLut& syslut) {
    for (auto it : syslut) {
      System* ci = it.second;
      ci->_activate(this);
    }
  });
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_deactivateSystems() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  _systems.atomicOp([&](SystemLut& syslut) {
    for (auto it : syslut) {
      System* ci = it.second;
      ci->_deactivate(this);
    }
  });
}
///////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
