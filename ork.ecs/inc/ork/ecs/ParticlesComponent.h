////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

// ParticlesComponent — ECS hosting for HyperSyn (or imperative) particle
// graphs. Each component owns a configured ParticlesDrawableData; on _onStage
// the system creates the drawable + scenegraph node via SceneGraphSystem and
// attaches to the requested layer. The drawable's internal graphinst handles
// per-frame compute (existing closure path) — Slice A3 lifts that into a
// component-owned pool of graphinsts with explicit reset / event control.

#pragma once

#include <ork/rtti/RTTIX.inl>
#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/lev2/ui/event.h>           // ui::UpdateData

#include "component.h"
#include "componenttable.h"

// Forward-declare lev2 particle types — including the full header from this
// ECS header would pull lev2 reflectables in at the ECS dylib's static-init
// time, double-registering classes when lev2's own dylib loads later (the
// observed symptom: SIGTRAP in `from orkengine import ecs, lev2`).
namespace ork::lev2 {
struct ParticlesDrawableData;
using particles_drawable_data_ptr_t = std::shared_ptr<ParticlesDrawableData>;
}

// Forward-declare dataflow graphinst — same DSO-isolation reason.
namespace ork::dataflow {
struct GraphInst;
using graphinst_ptr_t = std::shared_ptr<GraphInst>;
}

namespace ork::ecs {

struct ParticlesComponentData;
struct ParticlesComponent;
struct ParticlesGlobalSystemData;
struct ParticlesGlobalSystem;
struct SceneGraphSystem;

///////////////////////////////////////////////////////////////////////////////

struct ParticlesComponentData : public ComponentData {
  DeclareConcreteX(ParticlesComponentData, ComponentData);

public:
  ParticlesComponentData();

  ecs::Component* createComponent(Entity* pent) const final;
  static object::ObjectClass* componentClass();
  void DoRegisterWithScene(SceneComposer& sc) const final;

  // Configured by the author (Python or C++) before the scene is staged.
  // The ParticlesDrawableData carries the graphdata produced by the
  // HyperSyn DSL (or an imperative ptc graph). The Drawable created from
  // it owns its own per-instance graphinst.
  lev2::particles_drawable_data_ptr_t _drawabledata;

  // Which scenegraph layer to attach the drawable to. Empty string ⇒ the
  // SceneGraphSystem's default layer.
  std::string _layername;

  // Scenegraph node name prefix. Each pool slot gets its own scenegraph
  // node named "<prefix>_<entref>_<slot_index>". Empty string ⇒
  // "particles" prefix.
  std::string _nodename;

  // Number of concurrent slots per component instance. Each slot owns its
  // own (drawable, graphinst, sgnode) so they render independently. START
  // grabs a FREE slot if available, else evicts the oldest. pool_size=1
  // gives you the A1/A2 behavior (one slot per entity).
  int _pool_size = 1;

  // If > 0, RUNNING slots auto-transition to DRAINING after this many
  // seconds of slot-local time (since their last START / autospawn).
  // 0 = never auto-drain (slot runs until explicit STOP).
  float _duration = 0.0f;

  // When a slot enters DRAINING (either via STOP event or duration
  // expiry), it stays DRAINING for this many slot-local seconds before
  // being recycled (reset + FREE). Should be ≥ the max particle lifespan
  // so in-flight particles complete naturally. 0 = recycle immediately.
  float _drain_linger = 2.0f;

  // SYSTEM-WIDE delayed start: seconds between a slot firing (START /
  // autospawn) and the graph actually computing/emitting. slot_time runs
  // from -StartDelay; the update loop skips compute while negative. (For
  // per-EMITTER offsets within one graph use the emitters' StartDelay
  // plug — e.g. smoke igniting later than fire.)
  float _start_delay = 0.0f;

  // Name of the AssetSystemData ParticleSystemGenData entry whose
  // materialized ParticlesDrawableData should occupy the _drawabledata
  // slot after JSON deserialize. Empty for legacy inline construction
  // (no round-trip). Mirrors NodeDef::_drawable_asset_name.
  std::string _particles_asset_name;
};

using particlescomponentdata_ptr_t = std::shared_ptr<ParticlesComponentData>;

///////////////////////////////////////////////////////////////////////////////

struct ParticlesComponent : public Component {
  DeclareAbstractX(ParticlesComponent, Component);

public:
  ParticlesComponent(const ParticlesComponentData& cd, Entity* pent);
  ~ParticlesComponent();

  void _onUninitialize(Simulation* psi) final;
  bool _onLink(Simulation* psi) final;
  void _onUnlink(Simulation* psi) final;
  bool _onStage(Simulation* psi) final;
  void _onUnstage(Simulation* psi) final;
  bool _onActivate(Simulation* psi) final;
  void _onDeactivate(Simulation* psi) final;
  // Per-entity notify (controller.componentNotify path) — handles the same
  // START/STOP/PAUSE/RESUME tokens as ParticlesGlobalSystem::_onNotify but
  // targets only this entity, not the broadcast set.
  void _onNotify(Simulation* psi, token_t evID, evdata_t data) final;

  // Per-slot state (each component owns pool_size slots; A3.3).
  enum SlotState {
    FREE,       // not in use; graphinst quiescent (post-reset). Ready for START.
    RUNNING,    // compute fires, particles emit + simulate
    DRAINING,   // compute fires, emission gated — in-flight particles complete
    PAUSED,     // compute skipped (resumed via RESUME, no reset)
    INACTIVE,   // compute skipped (re-armed via START — with reset)
  };

  struct Slot {
    lev2::drawable_ptr_t                  drawable;
    lev2::scenegraph::drawable_node_ptr_t sgnode;
    // The graphinst the system drives. Extracted from `drawable` via
    // lev2::particles_drawable_graphinst() at stage time.
    dataflow::graphinst_ptr_t             graphinst;
    SlotState                             state       = FREE;
    // Slot-local time — advances by simulation deltaTime ONLY while
    // RUNNING or DRAINING. PAUSED/INACTIVE/FREE freeze it. Fed into
    // UpdateData::_abstime so Globals.RelTime stays continuous across
    // pause/resume. START resets to 0.
    float                                 slot_time   = 0.0f;
    // When this slot last transitioned to RUNNING (gameTime). Used by
    // ParticlesGlobalSystem to pick the oldest slot for eviction when
    // START arrives and no FREE slot exists.
    float                                 t_started   = 0.0f;
    // When this slot last transitioned to DRAINING (slot_time). Compared
    // against drain_linger to schedule recycle to FREE.
    float                                 t_drain     = 0.0f;
  };

  const ParticlesComponentData&            _PCD;
  std::vector<Slot>                        _slots;
  SceneGraphSystem*                        _sgsys = nullptr;
  ParticlesGlobalSystem*                   _system = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct ParticlesGlobalSystemData : public SystemData {
  DeclareConcreteX(ParticlesGlobalSystemData, SystemData);

public:
  ParticlesGlobalSystemData();

  // When true, _onUpdate fans each active slot's graphinst->compute()
  // out to opq::concurrentQueue and joins via an atomic-counter spin.
  // Each op gets its own UpdateData (no shared mutation). Post-compute
  // bookkeeping (state transitions, slot recycling) stays serial on
  // the main thread. Default false (preserves current serial behavior).
  // Assumption: distinct graphinsts have independent state; per-module
  // safety not audited — flip on only after verifying your DSL's
  // modules don't share mutable resources.
  bool _parallel_compute = false;

private:
  friend struct ParticlesGlobalSystem;
  System* createSystem(Simulation* pinst) const final;
};

using particles_global_system_data_ptr_t = std::shared_ptr<ParticlesGlobalSystemData>;

///////////////////////////////////////////////////////////////////////////////

struct ParticlesGlobalSystem final : public System {
  DeclareAbstractX(ParticlesGlobalSystem, System);

  static constexpr systemkey_t SystemType = "ParticlesGlobalSystem";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }

  // Broadcast event tokens.
  //   START     — fire the next available slot (FREE first; else evict oldest)
  //   STOP      — DRAIN every active slot (RUNNING → DRAINING)
  //   PAUSE     — freeze every active slot (RUNNING/DRAINING → PAUSED)
  //   RESUME    — un-freeze every PAUSED slot (PAUSED → its prior state)
  //   SET_PARAM — mutate an exposed parameter on every slot. Payload is a
  //               DataTable with "name"_tok (string) and "value"_tok (float).
  DeclareToken(START);
  DeclareToken(STOP);
  DeclareToken(PAUSE);
  DeclareToken(RESUME);
  DeclareToken(SET_PARAM);

  ParticlesGlobalSystem(const ParticlesGlobalSystemData& data, Simulation* pinst);
  ~ParticlesGlobalSystem();

  // Component lifecycle callbacks — called by ParticlesComponent's
  // _onStage/_onUnstage/_onActivate/_onDeactivate. The system owns drawable
  // creation + scenegraph attach so we get a single point of contact with
  // SceneGraphSystem.
  void _onStageComponent(ParticlesComponent* component);
  void _onUnstageComponent(ParticlesComponent* component);
  void _onActivateComponent(ParticlesComponent* component);
  void _onDeactivateComponent(ParticlesComponent* component);

  // Broadcast event handler — dispatches START/STOP/PAUSE/RESUME to every
  // tracked component.
  void _onNotify(token_t evID, evdata_t data) final;

  // Shared per-component event dispatch — used by both the broadcast
  // (system) path and the targeted (component) path. Encapsulates the
  // state machine transitions and side effects (graphinst reset, emission
  // gate toggle).
  void _applyNotifyToComponent(ParticlesComponent* c, token_t evID, float game_time);

  bool _onLink(Simulation* psi) final;
  void _onUnLink(Simulation* psi) final;
  bool _onActivate(Simulation* psi) final;
  void _onDeactivate(Simulation* inst) final;
  bool _onStage(Simulation* psi) final;
  void _onUnstage(Simulation* inst) final;
  void _onUpdate(Simulation* inst) final;

  using component_set_t = std::unordered_set<ParticlesComponent*>;
  LockedResource<component_set_t> _components;

  // A despawned entity's in-flight particles live out their lifetime:
  // _onUnstageComponent transfers each active slot that still has live
  // particles here instead of detaching it — emission inhibited, the
  // "@host" resolver frozen to the entity's final composed transform
  // (the captured Entity is reaped after unstage), sgnode left attached.
  // _onUpdate keeps computing each orphan and releases it (render-thread
  // detach + drop) once its pools report zero alive. Update-thread only.
  struct Orphan {
    lev2::drawable_ptr_t                  drawable; // keeps render path alive
    lev2::scenegraph::drawable_node_ptr_t sgnode;
    dataflow::graphinst_ptr_t             graphinst;
    float                                 slot_time = 0.0f;
  };
  std::vector<Orphan> _orphans;

  SceneGraphSystem* _sgsys = nullptr;
  const ParticlesGlobalSystemData& _PGSD;

  // One UpdateData reused across all components — compute() is called per
  // entity, but the time/dt scalar values are the same. Per-instance time
  // origins are still per-graphinst (Globals captures its own _timebasebase
  // on first compute), so a shared UpdateData is correct.
  ui::updatedata_ptr_t _updata;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
