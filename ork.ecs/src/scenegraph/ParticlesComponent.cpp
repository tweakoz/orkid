////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/opq.h>
#include <ork/reflect/properties/registerX.inl>

#include <ork/ecs/ecs.h>
#include <ork/ecs/system.h>
#include <ork/ecs/ParticlesComponent.h>
#include <ork/ecs/SceneGraphComponent.h>
#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>
#include <ork/ecs/simulation.inl>

// Full lev2 particle drawable type — only included here in the .cpp (not
// the header) to avoid pulling lev2 reflectables into the ECS dylib's
// static-init phase, which double-registers when lev2's dylib loads later.
#include <ork/lev2/gfx/particle/drawable_data.h>
#include <ork/dataflow/all.h>            // GraphInst::compute
#include <ork/lev2/gfx/particle/particle.h>  // particle::Context (DRAINING)
#include <ork/lev2/gfx/particle/modular_particles2.h>  // set_param_on_graphinst
#include <ork/ecs/datatable.h>           // DataTable for SET_PARAM evdata

#include <ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_ptccomp =
    logger()->configureChannel("ecs.ptccomp", fvec3(0.7, 0.9, 0.3));
///////////////////////////////////////////////////////////////////////////////
using namespace ork;
using namespace ork::lev2;
///////////////////////////////////////////////////////////////////////////////

void ParticlesComponentData::describeX(ComponentDataClass* clazz) {
  clazz->directProperty("LayerName",         &ParticlesComponentData::_layername);
  clazz->directProperty("NodeName",          &ParticlesComponentData::_nodename);
  clazz->directProperty("PoolSize",          &ParticlesComponentData::_pool_size);
  clazz->directProperty("Duration",          &ParticlesComponentData::_duration);
  clazz->directProperty("DrainLinger",       &ParticlesComponentData::_drain_linger);
  clazz->directProperty("StartDelay",        &ParticlesComponentData::_start_delay);
  clazz->directProperty("ParticlesAssetName",
                        &ParticlesComponentData::_particles_asset_name);
  // _drawabledata is a polymorphic ParticlesDrawableData; the runtime
  // graphdata it holds doesn't round-trip yet, so on load the asset
  // graph rebuilds it and the post-deserialize wire step (see
  // ork.ecs.scene.assets.wire_scene_data) swaps in the materialized
  // drawable by _particles_asset_name.
  clazz->directObjectProperty("DrawableData", &ParticlesComponentData::_drawabledata)
      ->annotate<ConstString>("editor.factorylistbase", "DrawableData");
}

ParticlesComponentData::ParticlesComponentData() {
}

Component* ParticlesComponentData::createComponent(ecs::Entity* pent) const {
  return new ParticlesComponent(*this, pent);
}

void ParticlesComponentData::DoRegisterWithScene(ecs::SceneComposer& sc) const {
  sc.Register<ParticlesGlobalSystemData>();
}

object::ObjectClass* ParticlesComponentData::componentClass() {
  return ParticlesComponent::GetClassStatic();
}

///////////////////////////////////////////////////////////////////////////////

void ParticlesComponent::describeX(object::ObjectClass* clazz) {
}

ParticlesComponent::ParticlesComponent(const ParticlesComponentData& cd, ecs::Entity* pent)
    : Component(&cd, pent)
    , _PCD(cd) {
}

ParticlesComponent::~ParticlesComponent() {
}

void ParticlesComponent::_onUninitialize(Simulation* psi) {
}

bool ParticlesComponent::_onLink(Simulation* psi) {
  _sgsys  = psi->findSystem<SceneGraphSystem>();
  _system = psi->findSystem<ParticlesGlobalSystem>();
  // FAIL EARLY (owner contract): components only link to DECLARED systems.
  // A scene whose only particle components live on DYNAMIC-spawn archetypes
  // never trips the composition-time auto-register, the link failure gets
  // swallowed, and the first spawn null-derefs in _onStage. A scene with
  // particles MUST declare the system: self.system_data("ParticlesGlobalSystem").
  if (!_system) {
    printf("ParticlesComponent::_onLink: scene has ParticlesComponents but NO "
           "ParticlesGlobalSystem — declare it (self.system_data(\"ParticlesGlobalSystem\"))\n");
    OrkAssert(false);
  }
  return _sgsys != nullptr && _system != nullptr;
}

void ParticlesComponent::_onUnlink(Simulation* psi) {
}

bool ParticlesComponent::_onStage(Simulation* psi) {
  _system->_onStageComponent(this);
  return true;
}

void ParticlesComponent::_onUnstage(Simulation* psi) {
  _system->_onUnstageComponent(this);
}

bool ParticlesComponent::_onActivate(Simulation* psi) {
  _system->_onActivateComponent(this);
  return true;
}

void ParticlesComponent::_onDeactivate(Simulation* psi) {
  _system->_onDeactivateComponent(this);
}

// SET_PARAM "value" arrives EXACT-typed from heterogeneous hosts (python floats
// land as doubles, C++ literals as ints/doubles) and svar get<float> ASSERTS on
// any mismatch (HOST-AUTHOR CONTRACT #3, the D.5 trap). Widen numerics instead
// of crashing — 2.20 SET_PARAM payload widening.
bool decodeSetParamValue(const svar64_t& v, float& out) {
  if (auto as_f = v.tryAs<float>())    { out = as_f.value(); return true; }
  if (auto as_d = v.tryAs<double>())   { out = float(as_d.value()); return true; }
  if (auto as_i = v.tryAs<int>())      { out = float(as_i.value()); return true; }
  if (auto as_u = v.tryAs<uint32_t>()) { out = float(as_u.value()); return true; }
  return false;
}

void ParticlesComponent::_onNotify(Simulation* psi, token_t evID, evdata_t data) {
  if (!_system) return;
  // SET_PARAM carries a DataTable payload — decode and apply to this
  // component's slots only (per-component scope is the whole point of
  // controller.componentNotify vs systemNotify).
  if (evID == ParticlesGlobalSystem::SET_PARAM._hashed) {
    auto table_ptr = data.getShared<DataTable>();
    if (!table_ptr) return;
    auto const& table = *table_ptr;
    auto name = table["name"_tok].get<std::string>();
    float value;
    if (not decodeSetParamValue(table["value"_tok], value)) {
      logchan_ptccomp->log("SET_PARAM<%s>: value is not numeric — ignored", name.c_str());
      return;
    }
    for (auto& slot : _slots) {
      lev2::particle::set_param_on_graphinst(slot.graphinst, name, value);
    }
    return;
  }
  _system->_applyNotifyToComponent(this, evID, psi ? psi->gameTime() : 0.0f);
}

///////////////////////////////////////////////////////////////////////////////

void ParticlesGlobalSystemData::describeX(SystemDataClass* clazz) {
  clazz->directProperty("parallel_compute", &ParticlesGlobalSystemData::_parallel_compute);
}

ParticlesGlobalSystemData::ParticlesGlobalSystemData() {
}

System* ParticlesGlobalSystemData::createSystem(ecs::Simulation* pinst) const {
  return new ParticlesGlobalSystem(*this, pinst);
}

///////////////////////////////////////////////////////////////////////////////

void ParticlesGlobalSystem::describeX(object::ObjectClass* clazz) {
}

ParticlesGlobalSystem::ParticlesGlobalSystem(const ParticlesGlobalSystemData& data, Simulation* pinst)
    : System(&data, pinst)
    , _PGSD(data) {
  _updata           = std::make_shared<ui::UpdateData>();
  _updata->_abstime = 0.0f;
  _updata->_dt      = 0.0f;
}

ParticlesGlobalSystem::~ParticlesGlobalSystem() {
}

bool ParticlesGlobalSystem::_onLink(Simulation* psi) {
  _sgsys = psi->findSystem<SceneGraphSystem>();
  return _sgsys != nullptr;
}

void ParticlesGlobalSystem::_onUnLink(Simulation* psi) {}
bool ParticlesGlobalSystem::_onStage(Simulation* psi) { return true; }
void ParticlesGlobalSystem::_onUnstage(Simulation* inst) {
  // scene teardown — orphaned trails don't outlive the simulation
  for (auto& orphan : _orphans) {
    if (!orphan.sgnode) continue;
    auto sgnode = orphan.sgnode;
    auto detach_op = [sgnode]() {
      for (auto l : sgnode->_layers) {
        l->removeDrawableNode(sgnode);
      }
      sgnode->_layers.clear();
    };
    _sgsys->_renderops.push(detach_op);
  }
  _orphans.clear();
}
bool ParticlesGlobalSystem::_onActivate(Simulation* psi) { return true; }
void ParticlesGlobalSystem::_onDeactivate(Simulation* inst) {}

///////////////////////////////////////////////////////////////////////////////
// Slot-level helpers
///////////////////////////////////////////////////////////////////////////////

// Toggle the emission-inhibit flag on a slot's graphinst. Reaches into the
// particle::Context that lives in graphinst->_impl; that's the flag emitter
// modules check before _emit() to decide whether to make new particles.
static void _set_emission_inhibit(ParticlesComponent::Slot& slot, bool inhibit) {
  if (!slot.graphinst) return;
  auto ctx = slot.graphinst->_impl.getShared<lev2::particle::Context>();
  if (ctx) ctx->_inhibit_emission = inhibit;
}

// Reset a slot to FREE state — graphinst.reset() clears Globals/Pool state,
// inhibit gets reset, slot time clears, state goes FREE.
static void _recycle_slot(ParticlesComponent::Slot& slot) {
  if (slot.graphinst) slot.graphinst->reset();
  _set_emission_inhibit(slot, false);
  slot.state     = ParticlesComponent::FREE;
  slot.slot_time = 0.0f;
  slot.t_drain   = 0.0f;
}

// Fire a slot: reset graphinst, mark RUNNING, restart slot_time at
// -start_delay (the SYSTEM-WIDE delayed start: the update loop skips
// compute while slot_time is negative, so nothing emits until it
// crosses 0 — e.g. projectile fire that must not ignite in the
// shooter's face).
static void _start_slot(ParticlesComponent::Slot& slot, float game_time, float start_delay) {
  if (slot.graphinst) slot.graphinst->reset();
  _set_emission_inhibit(slot, false);
  slot.state     = ParticlesComponent::RUNNING;
  slot.t_started = game_time;
  slot.slot_time = -start_delay;
  slot.t_drain   = 0.0f;
}

// Pick the slot to fire for a START event:
//   - prefer the first FREE slot
//   - otherwise evict the oldest active slot (smallest t_started)
// Returns nullptr only if the slot vector is empty (shouldn't happen).
static ParticlesComponent::Slot* _pick_slot_for_start(ParticlesComponent* c) {
  if (c->_slots.empty()) return nullptr;
  // 1) any FREE slot?
  for (auto& slot : c->_slots) {
    if (slot.state == ParticlesComponent::FREE) return &slot;
  }
  // 2) oldest among the rest (smallest t_started)
  auto* oldest = &c->_slots[0];
  for (auto& slot : c->_slots) {
    if (slot.t_started < oldest->t_started) oldest = &slot;
  }
  return oldest;
}

///////////////////////////////////////////////////////////////////////////////

void ParticlesGlobalSystem::_onUpdate(Simulation* inst) {
  float sim_dt = inst->deltaTime();
  _updata->_dt = sim_dt;

  // Phase 1 — gather active-slot compute jobs (serial; advances slot_time
  // so post-compute bookkeeping below sees the same values used by the
  // tick). State transitions/recycling happen in phase 3 strictly after
  // all computes have joined.
  struct ComputeJob {
    dataflow::graphinst_ptr_t gi;
    double abstime;
  };
  std::vector<ComputeJob> jobs;
  _components.atomicOp([&jobs, sim_dt](component_set_t& unlocked) {
    for (auto* c : unlocked) {
      for (auto& slot : c->_slots) {
        bool should_compute =
            (slot.state == ParticlesComponent::RUNNING) ||
            (slot.state == ParticlesComponent::DRAINING);
        if (!should_compute) continue;
        slot.slot_time += sim_dt;
        if (slot.slot_time < 0.0f) continue; // StartDelay holdoff (system-wide)
        if (slot.graphinst) {
          jobs.push_back({slot.graphinst, slot.slot_time});
        }
      }
    }
  });

  // Orphans tick alongside the live slots (emission already inhibited at
  // transfer — these computes only age/move/render what's in flight).
  for (auto& orphan : _orphans) {
    orphan.slot_time += sim_dt;
    if (orphan.graphinst)
      jobs.push_back({orphan.graphinst, orphan.slot_time});
  }

  // Phase 2 — compute (parallel or serial). When parallel, each op gets
  // its own stack-local UpdateData so no two graphinsts share mutable
  // tick state. Atomic counter barrier: incr on submit, decr in the
  // op, main thread spins until zero.
  //
  // AUDITED (E.6/2.20): incr-before-enqueue closes the early-zero race; decr
  // release / spin acquire publishes every compute's writes to this thread;
  // captures are by value except `remaining`, whose lifetime the join
  // guarantees. CONTRACT for code reachable from compute(): (1) NEVER touch
  // python/GIL on these worker threads; (2) particle graphs must stay
  // non-_cacheable (GraphInst::compute would branch into the SYNCHRONOUS
  // cook path); (3) per-graphinst state only — KNOWN BENIGN EXCEPTION: the
  // E2B emission EMA publishes to the SHARED material (per-component), so
  // parallel slots can interleave those float writes (visual-only jitter on
  // the emission light, no corruption). A compute that THROWS would skip its
  // decr and spin forever — particle modules abort on assert instead.
  if (_PGSD._parallel_compute && jobs.size() > 1) {
    std::atomic<int> remaining{0};
    double dt_for_ops = sim_dt;
    for (auto& j : jobs) {
      remaining.fetch_add(1, std::memory_order_relaxed);
      opq::concurrentQueue()->enqueue([j, dt_for_ops, &remaining]() {
        auto ud = std::make_shared<ui::UpdateData>();
        ud->_dt      = dt_for_ops;
        ud->_abstime = j.abstime;
        j.gi->compute(ud);
        remaining.fetch_sub(1, std::memory_order_release);
      });
    }
    // Spin until joined. Yield each iter so the OS scheduler doesn't
    // starve the worker threads we just enqueued onto.
    while (remaining.load(std::memory_order_acquire) != 0) {
      std::this_thread::yield();
    }
  } else {
    for (auto& j : jobs) {
      _updata->_abstime = j.abstime;
      j.gi->compute(_updata);
    }
  }

  // Phase 3 — state-transition bookkeeping (serial, main thread).
  // Same semantics as the original loop: duration auto-completion +
  // drain linger → recycle. Uses the slot_time values advanced in phase 1.
  _components.atomicOp([this](component_set_t& unlocked) {
    for (auto* c : unlocked) {
      auto& PCD = c->_PCD;
      for (auto& slot : c->_slots) {
        if (slot.state == ParticlesComponent::RUNNING &&
            PCD._duration > 0.0f &&
            slot.slot_time >= PCD._duration) {
          _set_emission_inhibit(slot, true);
          slot.state   = ParticlesComponent::DRAINING;
          slot.t_drain = slot.slot_time;
        }
        if (slot.state == ParticlesComponent::DRAINING &&
            slot.slot_time >= slot.t_drain + PCD._drain_linger) {
          _recycle_slot(slot);
        }
      }
    }
  });

  // Orphan release — emission was inhibited at transfer, so once every pool
  // reports zero alive the trail has fully played out: detach the node on
  // the render thread and drop the orphan (drawable/graphinst free with it).
  for (auto it = _orphans.begin(); it != _orphans.end();) {
    if (lev2::particle::aliveCountOnParticleGraph(it->graphinst) == 0) {
      auto sgnode = it->sgnode;
      auto detach_op = [sgnode]() {
        for (auto l : sgnode->_layers) {
          l->removeDrawableNode(sgnode);
        }
        sgnode->_layers.clear();
      };
      _sgsys->_renderops.push(detach_op);
      it = _orphans.erase(it);
    } else {
      ++it;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// Event semantics (A3.3 — multi-slot):
//   START   — fire ONE slot: FREE if any, else evict oldest active. The
//             others are unaffected. With pool_size=1 this matches A3.2.
//   STOP    — DRAIN every RUNNING/PAUSED slot. In-flight particles drain
//             naturally via the per-slot linger.
//   PAUSE   — freeze every RUNNING/DRAINING slot.
//   RESUME  — un-freeze every PAUSED slot (state restored to its own
//             pre-pause state — RUNNING or DRAINING).
// PAUSED slots remember their pre-pause state via a stash on the slot
// (not implemented yet; v0 assumes PAUSED → RUNNING on RESUME). Add a
// `pre_pause_state` field to Slot when DRAIN→PAUSE→RESUME becomes a real
// use case.
///////////////////////////////////////////////////////////////////////////////

void ParticlesGlobalSystem::_applyNotifyToComponent(
    ParticlesComponent* c, token_t evID, float game_time) {
  if (evID == SET_PARAM._hashed) {
    // Mutate the exposed parameter on every slot. Slot-local: gameplay
    // tuning persists for the lifetime of each slot until the next reset.
    // (Caveat: payload is read from a captured DataTable; if SET_PARAM is
    // routed via the system broadcast path, the data should come through
    // _onNotify which captures evdata before invoking this helper. The
    // helper variant called from broadcast doesn't have evdata, so the
    // broadcast path handles SET_PARAM inline rather than via this fn.)
    return;
  }
  if (evID == START._hashed) {
    auto* slot = _pick_slot_for_start(c);
    if (slot) _start_slot(*slot, game_time, c->_PCD._start_delay);
  } else if (evID == STOP._hashed) {
    for (auto& slot : c->_slots) {
      if (slot.state == ParticlesComponent::RUNNING ||
          slot.state == ParticlesComponent::PAUSED) {
        _set_emission_inhibit(slot, true);
        slot.state   = ParticlesComponent::DRAINING;
        slot.t_drain = slot.slot_time;
      }
    }
  } else if (evID == PAUSE._hashed) {
    for (auto& slot : c->_slots) {
      if (slot.state == ParticlesComponent::RUNNING ||
          slot.state == ParticlesComponent::DRAINING) {
        slot.state = ParticlesComponent::PAUSED;
      }
    }
  } else if (evID == RESUME._hashed) {
    for (auto& slot : c->_slots) {
      if (slot.state == ParticlesComponent::PAUSED) {
        // v0 simplification — RESUME always returns to RUNNING. To preserve
        // a paused-while-DRAINING distinction, add a pre_pause_state field.
        slot.state = ParticlesComponent::RUNNING;
      }
    }
  }
}

void ParticlesGlobalSystem::_onNotify(token_t evID, evdata_t data) {
  auto game_time = _simulation ? _simulation->gameTime() : 0.0f;

  // SET_PARAM is a per-component data dispatch — decode once, apply to each
  // component's slots. Other events flow through _applyNotifyToComponent.
  if (evID == SET_PARAM._hashed) {
    auto table_ptr = data.getShared<DataTable>();
    if (!table_ptr) return;
    auto const& table = *table_ptr;
    auto name = table["name"_tok].get<std::string>();
    float value;
    if (not decodeSetParamValue(table["value"_tok], value)) { // 2.20 widened decode
      logchan_ptccomp->log("SET_PARAM<%s>: value is not numeric — ignored", name.c_str());
      return;
    }
    _components.atomicOp([&](component_set_t& unlocked) {
      for (auto* c : unlocked) {
        for (auto& slot : c->_slots) {
          lev2::particle::set_param_on_graphinst(slot.graphinst, name, value);
        }
      }
    });
    return;
  }

  _components.atomicOp([this, evID, game_time](component_set_t& unlocked) {
    for (auto* c : unlocked) {
      _applyNotifyToComponent(c, evID, game_time);
    }
  });
}

///////////////////////////////////////////////////////////////////////////////
// Stage / unstage — allocate the per-component slot pool, attach each slot
// to the scenegraph under its own node name.
///////////////////////////////////////////////////////////////////////////////

void ParticlesGlobalSystem::_onStageComponent(ParticlesComponent* component) {
  _components.atomicOp([component](component_set_t& unlocked) {
    unlocked.insert(component);
  });

  auto& PCD = component->_PCD;
  if (!PCD._drawabledata) {
    logchan_ptccomp->log("ParticlesComponent staged with NULL drawabledata");
    return;
  }

  // Per-frame compute is system-driven for all slots.
  PCD._drawabledata->_externalCompute = true;

  int pool_size = std::max(1, PCD._pool_size);
  component->_slots.resize(pool_size);

  // LayerName is CSV (E2B item D): the first layer hosts the node, the rest
  // ALSO get it (e.g. "std_transparent,aux_heat" — the same drawable renders
  // in the color pass AND the heat aux pass; materials without the channel's
  // technique pair are skipped there, so over-attaching is harmless).
  std::vector<std::string> layer_names;
  {
    std::string csv = PCD._layername;
    size_t pos      = 0;
    while (pos != std::string::npos) {
      size_t comma    = csv.find(',', pos);
      std::string tok = (comma == std::string::npos) ? csv.substr(pos) : csv.substr(pos, comma - pos);
      if (not tok.empty())
        layer_names.push_back(tok);
      pos = (comma == std::string::npos) ? std::string::npos : comma + 1;
    }
  }
  auto layer = layer_names.empty()
      ? _sgsys->_default_layer
      : _sgsys->_scene->findLayer(layer_names[0]);
  std::vector<lev2::scenegraph::layer_ptr_t> extra_layers;
  for (size_t i = 1; i < layer_names.size(); ++i)
    extra_layers.push_back(_sgsys->_scene->findLayer(layer_names[i]));

  auto ent = component->GetEntity();
  std::string name_prefix = PCD._nodename.empty() ? "particles" : PCD._nodename;

  Simulation* sim_ptr = _simulation;
  for (int i = 0; i < pool_size; ++i) {
    auto& slot = component->_slots[i];
    slot.drawable  = PCD._drawabledata->createDrawable();
    // SG-attach hook — wires scene-coupled drawable internals. For
    // particles this hands over the scene's LightManager (the emission
    // point light registers through it); NOTHING else invoked this in the
    // ECS path, so particle lights silently never existed here.
    PCD._drawabledata->attachSGDrawable(slot.drawable, _sgsys->_scene);
    slot.graphinst = lev2::particles_drawable_graphinst(slot.drawable);
    slot.state     = ParticlesComponent::FREE;

    // Wire the host resolver once per slot. The lambda captures the
    // Simulation pointer (stable for the sim's lifetime) and delegates
    // to its published transform registry — see SpawnData::_publishxf_name
    // on the publisher side and VdbColliderModuleData::_follow_entity on
    // the consumer side. No per-tick cost beyond what the consuming
    // module already pays.
    //
    // "@host" is the RESERVED key for the entity HOSTING this component
    // (HYPERECS's implicit Expr.entity) — the per-instance binding that a
    // published NAME cannot express: graphdata is shared by every spawn of
    // an archetype, so E.entity("@host") makes each spawned entity's own
    // graph follow its own live transform (e.g. a fire trail on every
    // projectile of a pool). The captured Entity outlives the component's
    // hooks (despawn unstages the component before the entity is reaped).
    if (slot.graphinst && sim_ptr) {
      auto host_ent = ent;
      slot.graphinst->_resolveEntityXf =
          [sim_ptr, host_ent](const std::string& key) -> decompxf_ptr_t {
            if (key == "@host")
              return host_ent->transform();
            return sim_ptr->lookupPublishedXf(key);
          };
    }

    std::string node_name = FormatString("%s_%llu_%d",
        name_prefix.c_str(),
        (unsigned long long)ent->_entref,
        i);

    auto* slot_ptr = &slot;
    auto attach_op = [component, slot_ptr, layer, extra_layers, node_name]() {
      auto sgnode = layer->createDrawableNode(node_name, slot_ptr->drawable);
      for (auto& xl : extra_layers)
        xl->addDrawableNode(sgnode);
      // Particles emitted by the HyperSyn DSL run in world space — the
      // graphinst writes world coordinates directly (and forces/colliders
      // operate in world space too). Setting the scenegraph node's world
      // transform to the host entity would double-compose: each emitted
      // particle's position would be multiplied by the host xf at render
      // time on top of the position the graph already produced. Keep the
      // node at identity (its default-constructed DecompTransform) so the
      // renderer doesn't add a transform. Authors who want the host xf
      // to drive emit position should bind it explicitly via
      // Expr.entity("<publish_name>").pos on the emitter Offset.
      slot_ptr->sgnode = sgnode;
    };
    _sgsys->_renderops.push(attach_op);
  }

  // Auto-start slot 0 so spawned entities are immediately visible (matches
  // A1/A2 behavior). Components with pool_size > 1 only auto-fire the first
  // slot; the rest wait for explicit START events. If you don't want any
  // auto-fire, send STOP immediately after spawn.
  if (pool_size > 0) {
    _start_slot(component->_slots[0], _simulation ? _simulation->gameTime() : 0.0f,
                component->_PCD._start_delay);
  }
}

void ParticlesGlobalSystem::_onUnstageComponent(ParticlesComponent* component) {
  _components.atomicOp([component](component_set_t& unlocked) {
    unlocked.erase(component);
  });

  for (auto& slot : component->_slots) {
    // ORPHAN HANDOFF — a despawned entity's already-emitted particles live
    // out their lifetime (a projectile's trail outlasts the projectile).
    // Any slot still holding live particles transfers to the system-owned
    // orphan list instead of detaching: emission inhibited (no NEW
    // particles), and the "@host" resolver frozen to the entity's FINAL
    // composed transform — the captured Entity is reaped after unstage,
    // and emitter Offset plugs keep evaluating every tick (published-name
    // lookups stay live; only the dead-entity key freezes). PAUSED slots
    // resume as orphans (orphans always tick).
    int alive = (slot.state != ParticlesComponent::FREE)
        ? lev2::particle::aliveCountOnParticleGraph(slot.graphinst)
        : 0;
    if (alive > 0 && slot.graphinst && slot.sgnode) {
      _set_emission_inhibit(slot, true);
      auto frozen   = std::make_shared<DecompTransform>();
      auto host_ent = component->GetEntity();
      if (host_ent && host_ent->transform())
        frozen->decompose(host_ent->transform()->composed());
      Simulation* sim_ptr = _simulation;
      slot.graphinst->_resolveEntityXf =
          [sim_ptr, frozen](const std::string& key) -> decompxf_ptr_t {
            if (key == "@host")
              return frozen;
            return sim_ptr->lookupPublishedXf(key);
          };
      _orphans.push_back(Orphan{
          slot.drawable, slot.sgnode, slot.graphinst,
          std::max(slot.slot_time, 0.0f)});
    } else if (slot.sgnode) {
      auto sgnode = slot.sgnode;
      auto detach_op = [sgnode]() {
        for (auto l : sgnode->_layers) {
          l->removeDrawableNode(sgnode);
        }
        sgnode->_layers.clear();
      };
      _sgsys->_renderops.push(detach_op);
    }
  }
  component->_slots.clear();
}

void ParticlesGlobalSystem::_onActivateComponent(ParticlesComponent* component) {
  // PBR2 Phase 0 — resolve per-particle-system probe override at activate
  // time. By now, every probe entity has gone through ProbeComponent's
  // _onStageComponent → its LightProbe has been added to
  // LightManager::_lightprobes via SG layer::createProbeNode. Single
  // late lookup; the resolved pointer rides on each slot's drawable
  // for the rest of the component's life — no per-frame name lookup,
  // no stage-order coupling.
  auto& PCD = component->_PCD;
  if (!PCD._drawabledata) return;
  const std::string& probe_name = PCD._drawabledata->_probeEntityName;
  if (probe_name.empty()) return;
  if (!_sgsys || !_sgsys->_scene || !_sgsys->_scene->_lightManager) {
    logchan_ptccomp->log(
        "ParticlesComponent: no LightManager to resolve probe %s",
        probe_name.c_str());
    return;
  }
  auto probe = _sgsys->_scene->_lightManager->findProbeByName(probe_name);
  if (!probe) {
    logchan_ptccomp->log(
        "ParticlesComponent: probe %s not registered with LightManager "
        "at activate time", probe_name.c_str());
    return;
  }
  for (auto& slot : component->_slots) {
    if (slot.drawable) {
      slot.drawable->_probeOverride = probe;
    }
  }
}
void ParticlesGlobalSystem::_onDeactivateComponent(ParticlesComponent* component) {}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs

ImplementReflectionX(ork::ecs::ParticlesComponentData,     "ParticlesComponentData");
ImplementReflectionX(ork::ecs::ParticlesComponent,         "ParticlesComponent");
ImplementReflectionX(ork::ecs::ParticlesGlobalSystemData,  "ParticlesGlobalSystemData");
ImplementReflectionX(ork::ecs::ParticlesGlobalSystem,      "ParticlesGlobalSystem");
