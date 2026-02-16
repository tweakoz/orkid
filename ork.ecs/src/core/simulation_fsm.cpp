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
#include <ork/ecs/system.h>
#include <ork/ecs/controller.h>
#include <ork/ecs/scene.inl>
#include <ork/ecs/datatable.h>
#include <ork/util/logger.h>
#include <ork/profiling.inl>

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan_simfsm = logger()->configureChannel("ecs.simfsm", fvec3(1.0, 0.9, 0));

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
      _needsGpuInit = true;
      _needsGpuExit = true;
      _initialize();
      _compose();
      _link();
    } else if (inst->currentState() == _updateEditSimState) {
      _unstage();
    } else if (inst->currentState() == _updateActiveSimState) {
      _deactivate();
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
    _needsGpuInit = true;
    logchan_simfsm->log("entering editmode");
    //////////////////////////
    // did we come from ready or active state ?
    //////////////////////////
    if (inst->currentState() == _updateReadySimState) {
      logchan_simfsm->log(" .. from ready mode");
      lev2::DrawQueue::BeginClearAndSyncReaders();
      _stage();
      lev2::DrawQueue::EndClearAndSyncReaders();
    } else if (inst->currentState() == _updateActiveSimState) {
      logchan_simfsm->log(" .. from active mode");
      lev2::DrawQueue::BeginClearAndSyncReaders();
      ork::opq::assertOnQueue2(opq::updateSerialQueue());
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
    _needsGpuInit = true;
    _needsGpuExit = true;

    _activate();

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

    if (inst->currentState() == _updateEditSimState) {
      _unstage();
    } else if (inst->currentState() == _updateActiveSimState) {
      _deactivate();
      _unstage();
    }
    _unlink();
    _decompose();
    _uninitialize();
  };

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
  // RENDER INIT
  //////////////////
  ren_init_state->_onenter = [this](fsm::fsminstance_ptr_t inst) { _needsGpuInit = true; };
  //
  ren_init_state->_onupdate = [=](fsm::fsminstance_ptr_t inst) {
    OrkAssert(_currentdrwev);
    if (_needsGpuInit) {

      ////////////////////////////////////////////
      // when all locks released after
      //  enqueing gpuinit's
      //  we can transition to the simulate state
      ////////////////////////////////////////////

      uint64_t l = lev2::GfxEnv::createLock();

      auto LOCKS = lev2::GfxEnv::dumpLocks();

      if (auto sframe = inst->vars()->typedValueForKey<lev2::standardcompositorframe_ptr_t>("sframe")) {
        sframe.value()->attachDrawQueueContext(_dbufctxSIM);
      }

      lev2::GfxEnv::onLocksDone([=]() {
        _renderThreadSMInst->changeState(ren_sim_state);
      });

      SystemLut render_systems;
      _systems.atomicOp([&](const SystemLut& syslut) { render_systems = syslut; });

      for (auto sys : render_systems) {
        sys.second->_onGpuInit(this, _currentdrwev->_target);
      }
      for (auto sys : render_systems) {
        sys.second->_onGpuLink(this, _currentdrwev->_target);
      }

      auto LOCKS2 = lev2::GfxEnv::dumpLocks();

      _needsGpuInit    = false;
      _waitingForRLock = true;

      lev2::GfxEnv::releaseLock(l);
    }
  };
  //////////////////
  // RENDER SIMULATION
  //////////////////
  ren_sim_state->_onenter = [this](fsm::fsminstance_ptr_t inst) {};
  //
  ren_sim_state->_onupdate = [this](fsm::fsminstance_ptr_t inst) {
    EASY_BLOCK("ecs::sim::fsm_render", profiler::colors::Red);
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
        EASY_BLOCK("ecs::sim::fsm_render::1", profiler::colors::Red);
        SystemLut render_systems;
        _systems.atomicOp([&](const SystemLut& syslut) { render_systems = syslut; });
        EASY_END_BLOCK;
        EASY_BLOCK("ecs::sim::fsm_render::2", profiler::colors::Red);
        for (auto sys : render_systems) {
          sys.second->_beginRender();
        }
        EASY_END_BLOCK;
        EASY_BLOCK("ecs::sim::fsm_render::2", profiler::colors::Red);
        for (auto sys : render_systems)
          sys.second->_render(this, _currentdrwev);
        EASY_END_BLOCK;
        EASY_BLOCK("ecs::sim::fsm_render::2", profiler::colors::Red);
        for (auto sys : render_systems) {
          sys.second->_endRender();
        }
        EASY_END_BLOCK;
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
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
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
  _composeSystems();
  _composeEntities();
  _transportState = ESimulationTransport::COMPOSED;
  logchan_simfsm->log("Simulation<%p> _composed", (void*)this);
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_link() {
  _linkSystems();
  _linkEntities();
  if (_onLink)
    _onLink();
  _transportState = ESimulationTransport::LINKED;
  logchan_simfsm->log("Simulation<%p> _linked", (void*)this);
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_stage() {
  _stageSystems();
  _stageEntities();
  _resetClock();
  _serviceDeactivateQueue();
  _transportState = ESimulationTransport::STAGED;
  logchan_simfsm->log("Simulation<%p> _staged", (void*)this);
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_activate() {
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
  _unstageEntities();
  _unstageSystems();
  logchan_simfsm->log("Simulation<%p> _unstaged", (void*)this);
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_deactivate() {
  _deactivateEntities();
  _deactivateSystems();
  mActiveEntities.clear();
  mEntityDeactivateQueue.clear();
  logchan_simfsm->log("Simulation<%p> _deactivated", (void*)this);
}
///////////////////////////////////////////////////////////////////////////
void Simulation::_initializeEntities() {

  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  ///////////////////////////////////
  // clear runtime containers
  ///////////////////////////////////

  mEntities.clear();
  _cameraDataLUT.clear();

  ///////////////////////////////////
  // Compose Entities
  ///////////////////////////////////

  logchan_simfsm->log("simulation<%p> Composing AutoSpawn Entities..", (void*)this);

  auto scene = _controller->_scenedata;

  for (auto it : scene->GetSceneObjects()) {
    auto sobj = it.second;
    if (auto spawner = std::dynamic_pointer_cast<SpawnData>(sobj)) {

      if (spawner->autoSpawn()) {

        auto arch = spawner->GetArchetype();

        uint64_t entref = _controller->_objectIdCounter.fetch_add(1);

        ork::ecs::Entity* pent = new ork::ecs::Entity(spawner, this, entref);
        _controller->_mutateObject([&](Controller::id2obj_map_t& unlocked) { unlocked[entref].set<Entity*>(pent); });

        std::string actualLayerName = "Default";

        ConstString layer_name = spawner->GetUserProperty("DrawLayer");
        if (strlen(layer_name.c_str()) != 0) {
          actualLayerName = layer_name.c_str();
        }

        auto layer_data = GetLayerData(actualLayerName);
        if (0 == layer_data) {
          layer_data = new lev2::LayerData;
          AddLayerData(actualLayerName, layer_data);
        }
        ////////////////////////////////////////////////////////////////

        logchan_simfsm->log(
            "Compose AutoSpawn Entity<%p> arch<%p> layer<%s>", //
            (void*)pent,                                       //
            (void*)arch.get(),                                 //
            layer_name.c_str());

        assert(pent != nullptr);

        auto node  = spawner->_dagnode;
        auto world = node->_xfnode->_transform;
        // auto init_xf          = pent->data()->_dagnode->_xfnode;
        // ent->GetDagNode()->_xfnode->_transform->set(init_xf->_transform);
        pent->setTransform(world);
        mEntities[spawner->GetName()] = pent;

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
  auto scene     = _controller->_scenedata;
  auto& sysdatas = scene->getSystemDatas();
  for (auto it : sysdatas) {
    auto pscd = it.second;
    if (pscd != nullptr) {
      auto sys = pscd->createSystem(this);
      addSystem(sys->systemTypeDynamic(), sys);
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
