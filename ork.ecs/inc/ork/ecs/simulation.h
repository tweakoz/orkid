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

  Simulation(Controller* controller);

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
  void gpuExit(lev2::Context* ctx);

  ///////////////////////////////////////////////////

  void updateExit();

  bool IsEntityActive(Entity* entity) const;

  void enqueueActivateDynamicEntity(const EntityActivationQueueItem& item);
  void registerActivatedEntity(Entity* pent);

  void enqueueDespawnEntity(Entity* entity);
  void registerDeactivatedEntity(Entity* pent);

  //////////////////////////////////////////////////////////

  Entity* _spawnNamedDynamicEntity(spawndata_constptr_t spawn_rec, PoolString name, int entref,decompxf_ptr_t ovxf=nullptr);
  Entity* _spawnAnonDynamicEntity(spawndata_constptr_t spawn_rec, int entref, decompxf_ptr_t ovxf=nullptr);

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
  impl::sys_response_ptr_t _findSystemResponseFromRef(response_ref_t ref);
  impl::comp_response_ptr_t _findComponentResponseFromRef(response_ref_t ref);

  lev2::dbufcontext_ptr_t dbufcontext() { return _dbufctxSIM; }

  void debugBanner( int r, int g, int b, const char* formatstring, ... );

  void _onSimulationRequest(impl::sim_response_ptr_t response, token_t evID, svar64_t data);

  void _stashRenderThreadDestructable(svar64_t var);

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

  fsm::statemachine_ptr_t _updateThreadSM;
  fsm::statemachine_ptr_t _renderThreadSM;

  fsm::lambdastate_ptr_t _updateReadySimState;
  fsm::lambdastate_ptr_t _updateEditSimState;
  fsm::lambdastate_ptr_t _updateActiveSimState;
  fsm::lambdastate_ptr_t _updatePausedSimState;
  fsm::lambdastate_ptr_t _updateTerminatedSimState;

  fsm::lambdastate_ptr_t _renderTerminatedSimState;


  ui::drawevent_constptr_t _currentdrwev;

  size_t mEntityUpdateCount = 0;

  orkmap<PoolString, Archetype*> mDynamicArchetypes;

  orkmap<std::string, lev2::LayerData*> _layerdataMap;
  orkmap<PoolString, Entity*> mEntities;
  EntitySet mActiveEntities;

  ComponentList mEmptyList;
  LockedResource<SystemLut> _systems;
  SystemLut _updsyslutcopy;
  mutable SystemLut _rensyslutcopy;

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

  bool _needsGpuInit = false;
  bool _needsGpuExit = false;
  bool _waitingForRLock = false;

  lev2::dbufcontext_ptr_t _dbufctxSIM;

  using destructables_vect_t = std::vector<svar64_t>;

  LockedResource<destructables_vect_t> _renderthreaddestructables;
  //////////////////////////////////////////////////////////

};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
