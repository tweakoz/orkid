////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/math/TransformNode.h>
#include <ork/util/fsm.h>
#include <ork/kernel/future.hpp>
#include <ork/python/wraprawpointer.inl>

#include <atomic>
#include <future>
#include <mutex>

#include "types.h"
#include "controller.h"

///////////////////////////////////////////////////////////////////////////////
/// Simulation is all the work data associated with running a scene
///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {

///////////////////////////////////////////////////////////////////////////////

struct EntityActivationQueueItem {
  decompxf_ptr_t _world;
  Entity* _entity;

  EntityActivationQueueItem(decompxf_ptr_t mtx = std::make_shared<DecompTransform>(), Entity* pent = nullptr)
      : _world(mtx)
      , _entity(pent) {
  }
};

struct EntityPosRecord{
  int _entref = -1;
  PoolString _name;
  archetype_constptr_t _archetype = nullptr;
  decompxf_ptr_t _xform;
};
using entityposmap_t = std::vector<EntityPosRecord>;
using entityposmap_ptr_t = std::shared_ptr<entityposmap_t>;

///////////////////////////////////////////////////////////////////////////////

struct Simulation {

  typedef orkmap<PoolString, orklist<Component*>> ActiveComponentType;
  typedef orklut<systemkey_t, System*> SystemLut;
  typedef orklist<Component*> ComponentList;
  typedef orkset<Entity*> EntitySet;

  Simulation(Controller* controller, varmap::varmap_ptr_t injected_varmap = nullptr);

  ~Simulation();

  ///////////////////////////////////////////////////

  scenedata_constptr_t GetData() const;

  Entity* findEntity(PoolString entity) const;
  Entity* findEntityLoose(PoolString entity) const;

  ///////////////////////////////////////////////////
  template <typename T> T* findTypedEntityComponent(const PoolString& entname) const;
  ///////////////////////////////////////////////////
  template <typename T> T* findTypedEntityComponent(const char* entname) const;
  ///////////////////////////////////////////////////

  const orkmap<PoolString, Entity*>& Entities() const {
    return mEntities;
  }

  ///////////////////////////////////////////////////

  void setCameraData(const std::string& name, lev2::cameradata_constptr_t camdat);
  lev2::cameradata_constptr_t cameraData(const std::string& name) const;

  ///////////////////////////////////////////////////

  void render(ui::drawevent_constptr_t drwev);
  void renderWithStandardCompositorFrame(lev2::standardcompositorframe_ptr_t sframe);
  void gpuUpdate(lev2::Context* ctx);
  void gpuExit(lev2::Context* ctx);

  ///////////////////////////////////////////////////

  void updateExit();

  bool IsEntityActive(Entity* entity) const;

  void enqueueActivateDynamicEntity(const EntityActivationQueueItem& item);
  void registerActivatedEntity(Entity* pent);

  void enqueueDespawnEntity(Entity* entity);
  void registerDeactivatedEntity(Entity* pent);

  //////////////////////////////////////////////////////////

  Entity* _spawnNamedDynamicEntity(const impl::_SpawnAnonDynamic& SAD, PoolString name);
  Entity* _spawnAnonDynamicEntity(const impl::_SpawnAnonDynamic& SAD);
  // in-simulation dynamic spawn (the PythonSystem script entry — update-thread
  // only; same path the controller command takes). Resolve the spawner HANDLE
  // once at init/link time with findSpawner (the only string lookup), then
  // spawn through it. `sad` is optional (override xf / spawn table).
  spawndata_constptr_t findSpawner(const std::string& spawner_name) const;
  Entity* spawnDynamic(spawndata_constptr_t spawner, sad_ptr_t sad = nullptr);

  //////////////////////////////////////////////////////////

  template <typename T> T* findSystem() const;

  inline void setOnLinkLambda(void_lambda_t l) {
    _onLink = l;
  }

  float gameTime() const {
    return mGameTime;
  }
  float deltaTime() const {
    return mDeltaTime;
  }
  float upDeltaTime() const {
    return mUpDeltaTime;
  }

  void _mutateControllerObject(std::function<void(Controller::id2obj_map_t&)>);

  Entity* _findEntityFromRef(ent_ref_t ref);
  Component* _findComponentFromRef(comp_ref_t ref);
  System* _findSystemFromRef(sys_ref_t ref);
  System* _findSystemFromName(const std::string& name);
  impl::sys_response_ptr_t _findSystemResponseFromRef(response_ref_t ref);
  impl::comp_response_ptr_t _findComponentResponseFromRef(response_ref_t ref);

  lev2::dbufcontext_ptr_t dbufcontext() { return _dbufctxSIM; }

  void debugBanner( int r, int g, int b, const char* formatstring, ... );

  void _onSimulationRequest(impl::sim_response_ptr_t response, token_t evID, svar64_t data);

  void _stashRenderThreadDestructable(svar64_t var);

  void _enqueueDeferredInvokation(deferred_script_invokation_ptr_t i);
  std::vector<deferred_script_invokation_ptr_t> dequeueDeferredInvokations();

  Controller* controller() const { return _controller; }

  varmap::varmap_ptr_t varmap() { return _varmap; }

  //////////////////////////////////////////////////////////
  // Published entity transform registry.
  //
  // SpawnData with a non-empty _publishxf_name registers each newly
  // spawned entity here under a unique key derived from that name:
  //   1st spawn of "saddle" → key "saddle0"
  //   2nd spawn of "saddle" → key "saddle1"
  //   ...
  // The counter (_publishxf_counts) is monotonically increasing per
  // base name — never decremented on despawn — so a key, once assigned,
  // is unique for the lifetime of the Simulation. The value is the
  // entity's LIVE decompxf_ptr_t (no per-tick snapshot needed; readers
  // see the current value on deref).
  //
  // Consumers (e.g. VdbColliderModuleData::_follow_entity) reach this
  // through the GraphInst::_resolveEntityXf hook wired by
  // ParticlesComponent at stage time — modules don't know the
  // Simulation type, they call back through the resolver function.
  //////////////////////////////////////////////////////////
  std::string publishEntityXf(Entity* ent, const std::string& base_name);
  void unpublishEntityXf(Entity* ent);
  // Returns the live decompxf_ptr_t (caller composes to fmtx4 if
  // needed), or nullptr if no entity is registered under `key`.
  decompxf_ptr_t lookupPublishedXf(const std::string& key) const;

private:

  void _resetClock();

  friend struct Controller;
  friend struct LuaContext;

  ///////////////////////////////////////////////////

  float random(float mmin, float mmax);

  void AddLayerData(const std::string& name, lev2::LayerData* player);
  lev2::LayerData* GetLayerData(const std::string& name);
  const lev2::LayerData* GetLayerData(const std::string& name) const;

  size_t GetEntityUpdateCount() const {
    return mEntityUpdateCount;
  }

  float desiredFrameRate() const;

  bool _onControllerEvent(const Controller::Event& event);
  bool _onControllerRequest(const Controller::Request& request);

  void _update();
  void _update_SIMSTATE();

  void addSystem(systemkey_t key, System* pcomp);
  void clearSystems();

  void _initialize();
  void _uninitialize();
  void _compose();
  void _decompose();
  void _link();
  void _unlink();
  void _stage();
  void _unstage();
  void _activate();
  void _deactivate();

  void _initializeEntities();
  void _uninitializeEntities();
  void _updateSpawnerContexts();
  void _updateEntityLifetimes();
  void _composeEntities();
  void _decomposeEntities();
  void _linkEntities();
  void _unlinkEntities();
  void _stageEntities();
  void _unstageEntities();
  void _activateEntities();
  void _deactivateEntities();

  void _composeSystems();
  void _decomposeSystems();
  void _linkSystems();
  void _unlinkSystems();
  void _stageSystems();
  void _unstageSystems();
  void _activateSystems();
  void _deactivateSystems();

  void _enterEditState();
  void _enterPauseState();
  void _enterRunState();
  void _enterInitState();
  void _enterSingleStepState();

  // Phase-locked rendezvous between update thread and GPU/render thread.
  // If caller is already on the render thread (i.e. inside gpuUpdate(ctx)),
  // phase_fn runs inline with the current ctx. Otherwise phase_fn is queued
  // and the caller blocks on a future until the next gpuUpdate(ctx) drains
  // the queue and executes it. Used for ECS init/teardown phases that must
  // run on the GPU thread with a well-defined sequencing point.
  void _runGpuPhaseOnRenderThread(std::function<void(lev2::Context*)> phase_fn);

  //////////////////////////////////////////////////////////

  PoolString genDynamicEntityName();
  //////////////////////////////////////////////////////////
  float _computeDeltaTime();
  //////////////////////////////////////////////////////////
  void _serviceDeactivateQueue();
  void _serviceActivateQueue();
  void _serviceEventQueues();
  //////////////////////////////////////////////////////////

  void SetSimulationMode(ESimulationMode emode);

  //////////////////////////////////////////////////////////

  void _buildStateMachine();

  Controller* _controller = nullptr; // controller owns simulation
  //Application* mApplication = nullptr;

  ESimulationMode _currentSimulationMode = ESimulationMode::NEW;
  ESimulationTransport _transportState = ESimulationTransport::NEW;

  float mGameTime       = 0.0f; // current game clock time (stops on pause)
  float mDeltaTime      = 0.0f; // time since last update (0 on pause)
  float mPrevDeltaTime  = 0.0f; // time since last update (0 on pause)
  float mUpTime         = 0.0f; // time since program started (does not stop on pause)
  float mUpDeltaTime    = 0.0f; // time since last update (even in pause)
  float mStartTime      = 0.0f; // UpTime when game started
  float mLastGameTime   = 0.0f;
  float mDeltaTimeAccum = 0.0f;
  float mfAvgDtAcc      = 0.0f;
  float mfAvgDtCtr      = 0.0f;

  fsm::fsmdata_ptr_t _updateThreadSMData;
  fsm::fsmdata_ptr_t _renderThreadSMData;
  fsm::fsmdata_ptr_t _gpuUpdateSMData;
  fsm::fsminstance_ptr_t _updateThreadSMInst;
  fsm::fsminstance_ptr_t _renderThreadSMInst;
  fsm::fsminstance_ptr_t _gpuUpdateSMInst;

  fsm::lambdastate_ptr_t _updateReadySimState;
  fsm::lambdastate_ptr_t _updateEditSimState;
  fsm::lambdastate_ptr_t _updateActiveSimState;
  fsm::lambdastate_ptr_t _updatePausedSimState;
  fsm::lambdastate_ptr_t _updateTerminatedSimState;

  fsm::lambdastate_ptr_t _renderTerminatedSimState;

  fsm::lambdastate_ptr_t _gpuInitState;
  fsm::lambdastate_ptr_t _gpuReadyState;
  fsm::lambdastate_ptr_t _gpuTerminatedState;


  ui::drawevent_constptr_t _currentdrwev;

  size_t mEntityUpdateCount = 0;

  orkmap<PoolString, Archetype*> mDynamicArchetypes;

  orkmap<std::string, lev2::LayerData*> _layerdataMap;
  orkmap<PoolString, Entity*> mEntities;
  std::map<spawndata_constptr_t, spawnercontext_ptr_t> _spawnerContexts;
  // Live entity transform registry — see public publishEntityXf above.
  std::unordered_map<std::string, decompxf_ptr_t> _published_xfs;
  std::unordered_map<std::string, size_t> _publishxf_counts;
  // Reverse index so unpublishEntityXf can find the keys an entity owns
  // without scanning _published_xfs. An entity from a SpawnData with
  // _spawnCount > 1 isn't an issue here — each spawn registers under
  // its own key — but a SpawnData reused for repeated spawns
  // accumulates keys, so the value side is a small vector.
  std::unordered_map<Entity*, std::vector<std::string>> _entity_to_publish_keys;
  EntitySet mActiveEntities;

  ComponentList mEmptyList;
  LockedResource<SystemLut> _systems;
  SystemLut _updsyslutcopy;
  mutable SystemLut _rensyslutcopy;

  std::vector<deferred_script_invokation_ptr_t> _deferred_invokations;

  void_lambda_t _onLink;

  lev2::CameraDataLut _cameraDataLUT; // camera list
  //////////////////////////////////////////////////////////
  //ActiveComponentType mActiveEntityComponents;
  orkvector<EntityActivationQueueItem> mEntityActivateQueue;
  orkvector<Entity*> mEntityDeactivateQueue;

  std::atomic<int> _dynname_serno;

  PoolString _SimulationEvChanName;
  PoolString _AudioFamily;
  PoolString _CameraFamily;
  PoolString _ControlFamily;
  PoolString _MotionFamily;
  PoolString _PhysicsFamily;
  PoolString _PositionFamily;
  PoolString _FrustumFamily;
  PoolString _AnimateFamily;
  PoolString _ParticleFamily;
  PoolString _LightFamily;
  PoolString _InputFamily;
  PoolString _PreRenderFamily;

  //////////////////////////////////////////////////////////

  Controller::evq_t _current_events;

  // Deprecated: phase-locked init/teardown no longer polls these flags.
  // Kept for one release to avoid silently breaking any out-of-tree
  // subclass that happens to reference them. Will be removed in a
  // follow-up commit.
  [[deprecated("phase-locked init/teardown supersedes these flags")]]
  bool _needsGpuInit = false;
  [[deprecated("phase-locked init/teardown supersedes these flags")]]
  bool _needsGpuExit = false;
  bool _waitingForRLock = false;
  varmap::varmap_ptr_t _varmap;

  // Phase-locked rendezvous machinery. _onGpuThread is set true while
  // gpuUpdate(ctx) is executing so _runGpuPhaseOnRenderThread can detect
  // re-entry and run inline rather than deadlocking on itself.
  std::atomic<bool> _onGpuThread{false};
  lev2::Context* _currentRenderCtx = nullptr;
  using gpu_phase_fn_t = std::function<void(lev2::Context*)>;
  std::mutex _gpuPhaseMutex;
  std::vector<gpu_phase_fn_t> _pendingGpuPhases;
  // Teardown fires _onGpuExit as part of the update FSM's Terminated state
  // via rendezvous. Controller::gpuExit is called later (app shutdown) and
  // must not re-fire the hooks — this flag guards against double-invocation.
  bool _gpuExitDone = false;

  lev2::dbufcontext_ptr_t _dbufctxSIM;

  using destructables_vect_t = std::vector<svar64_t>;

  LockedResource<destructables_vect_t> _renderthreaddestructables;

  // Pending system response callbacks — swept at end of update
  using pending_response_t = impl::sys_response_ptr_t;
  std::vector<pending_response_t> _pendingResponseCallbacks;
  void _sweepResponseCallbacks();
  //////////////////////////////////////////////////////////

};

using pysim_ptr_t = ork::python::unmanaged_ptr<Simulation>;

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
