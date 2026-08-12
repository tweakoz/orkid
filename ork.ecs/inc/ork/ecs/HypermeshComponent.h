////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

// HypermeshComponent (HYPERECS D.3) — ECS hosting for HyperSyn hypermesh graphs.
// Clones the ParticlesComponent stage contract: each component owns a configured
// HypermeshDrawableData; on _onStage the system creates the drawable + scenegraph node via
// SceneGraphSystem and wires the material resolver against the AssetSystem artifact
// registry (resolution is by NAME — the same contract the C++ host uses).
//
// CLOCK delta vs particles: a hypermesh graph computes ON THE RENDER THREAD, in-frame
// (GPU dispatch phases inside ComputeDrawable::onGpuUpdate) — the system does NOT pump
// graphinst->compute() from the update thread. The B.4 clock contract still holds at the
// host boundary: PAUSE/RESUME map to LiveHypermesh::_paused (the live hook stops
// accumulating time; dt=0, abstime holds, resume is seamless). When the core Time modules
// land (plan 1.2c) the component will feed UpdateData explicitly, completing the verbatim
// ParticlesComponent clock contract.

#pragma once

#include <ork/rtti/RTTIX.inl>
#include <ork/lev2/gfx/scenegraph/scenegraph.h>

#include "component.h"
#include "componenttable.h"

// Forward-declare the lev2 hypermesh types — including the full header from this ECS
// header would pull lev2 reflectables in at the ECS dylib's static-init time,
// double-registering classes when lev2's own dylib loads later (same DSO-isolation
// pattern as ParticlesComponent.h).
namespace ork::lev2::hypermesh {
struct HypermeshDrawableData;
using hypermesh_drawable_data_ptr_t = std::shared_ptr<HypermeshDrawableData>;
}

namespace ork::ecs {

struct HypermeshComponentData;
struct HypermeshComponent;
struct HypermeshSystemData;
struct HypermeshSystem;
struct SceneGraphSystem;
struct AssetSystem;

///////////////////////////////////////////////////////////////////////////////

struct HypermeshComponentData : public ComponentData {
  DeclareConcreteX(HypermeshComponentData, ComponentData);

public:
  HypermeshComponentData();

  ecs::Component* createComponent(Entity* pent) const final;
  static object::ObjectClass* componentClass();
  void DoRegisterWithScene(SceneComposer& sc) const final;

  // Configured by the author (Python or C++) before the scene is staged. Carries the
  // embedded hypermesh graph + material reference + viz flags (reflected — the whole
  // component round-trips through the scene JSON).
  lev2::hypermesh::hypermesh_drawable_data_ptr_t _drawabledata;

  // Which scenegraph layer to attach the drawable to. Empty ⇒ the default layer.
  std::string _layername;

  // Scenegraph node name. Empty ⇒ "hypermesh_<entref>".
  std::string _nodename;

  // NOT A SHADOW CASTER. A hypermesh joins the depth_prepass layer by default
  // (early-z), and that layer IS the sun-cascade caster set — so a drawable that
  // is on it pays a full depth pass PER BAND whether or not its shadow can be
  // seen. Ankle-high scatter (a grass tussock) is the case this exists for: its
  // blade shadows read as noise at any distance the cascades reach, and at five
  // bands it was paying six dispatches a frame to produce them. Same spelling
  // and same meaning as SceneGraphComponent's node-level skip_auto_dpp, which is
  // what the grass CARPET already uses; the drawable still RECEIVES shadows
  // (receiving samples the atlas and never depended on caster membership).
  bool _skipAutoDpp = false;

  // MUTUALLY-EXCLUSIVE PRESENTATION SETS. Hypermeshes carrying the same non-empty
  // _visgroup are one switchable set: a host sends SET_VISGROUP{group,enable} and
  // every member's scenegraph node flips together. Empty = ungrouped (always on).
  //
  // _visible is the LAUNCH state, and it is load-bearing rather than cosmetic: a
  // scenegraph node that comes up disabled is skipped by Scene::gpuUpdate AND
  // Scene::preRender, so a hidden member never runs its lazy bootstrap — no mesh
  // materialization, no instance cull, no impostor atlas, no section bake. Two
  // alternative barks can therefore sit co-resident in a scene at ZERO cost until
  // one is asked for (it pays its build on the frame it is first enabled).
  std::string _visgroup;
  bool        _visible = true;
};

using hypermeshcomponentdata_ptr_t = std::shared_ptr<HypermeshComponentData>;

///////////////////////////////////////////////////////////////////////////////

struct HypermeshComponent : public Component {
  DeclareAbstractX(HypermeshComponent, Component);

public:
  HypermeshComponent(const HypermeshComponentData& cd, Entity* pent);
  ~HypermeshComponent();

  bool _onLink(Simulation* psi) final;
  void _onUnlink(Simulation* psi) final;
  bool _onStage(Simulation* psi) final;
  void _onUnstage(Simulation* psi) final;
  bool _onActivate(Simulation* psi) final;
  void _onDeactivate(Simulation* psi) final;
  // Per-entity notify (controller.componentNotify): PAUSE / RESUME.
  void _onNotify(Simulation* psi, token_t evID, evdata_t data) final;

  const HypermeshComponentData&         _HCD;
  lev2::drawable_ptr_t                  _drawable;
  lev2::scenegraph::drawable_node_ptr_t _sgnode;
  SceneGraphSystem*                     _sgsys  = nullptr;
  HypermeshSystem*                      _system = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct HypermeshSystemData : public SystemData {
  DeclareConcreteX(HypermeshSystemData, SystemData);

public:
  HypermeshSystemData();

private:
  friend struct HypermeshSystem;
  System* createSystem(Simulation* pinst) const final;
};

using hypermesh_system_data_ptr_t = std::shared_ptr<HypermeshSystemData>;

///////////////////////////////////////////////////////////////////////////////

struct HypermeshSystem final : public System {
  DeclareAbstractX(HypermeshSystem, System);

  static constexpr systemkey_t SystemType = "HypermeshSystem";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }

  // Broadcast event tokens. PAUSE freezes every hosted graph's clock (dt=0, abstime
  // holds); RESUME continues seamlessly — the LiveHypermesh pause contract (B.4).
  DeclareToken(PAUSE);
  DeclareToken(RESUME);
  // SET_PARAM (E.6/2.12 ECS leg) — DataTable {name, value:numeric}: pokes the float
  // "value" DATA plug of every MaterialParamSink whose param_name matches `name`, on
  // every hosted graph. The drawable's per-frame drain then bindParam()s the bound
  // materials (live in all cached pipelines) — gameplay drives a material UBO param
  // by name in the zero-Python player.
  DeclareToken(SET_PARAM);
  // SET_VISGROUP — DataTable {group:string, enable:int}: enables/disables the
  // scenegraph node of every hosted component declaring that _visgroup. The write
  // rides the SceneGraphSystem render-op queue, so the render thread never reads
  // a node's enable while the update thread is changing it.
  DeclareToken(SET_VISGROUP);

  HypermeshSystem(const HypermeshSystemData& data, Simulation* pinst);
  ~HypermeshSystem();

  void _onStageComponent(HypermeshComponent* component);
  void _onUnstageComponent(HypermeshComponent* component);

  void _onNotify(token_t evID, evdata_t data) final;
  void _applyNotifyToComponent(HypermeshComponent* c, token_t evID);

  bool _onLink(Simulation* psi) final;
  void _onUnLink(Simulation* psi) final;
  bool _onActivate(Simulation* psi) final;
  void _onDeactivate(Simulation* inst) final;
  bool _onStage(Simulation* psi) final;
  void _onUnstage(Simulation* inst) final;
  void _onUpdate(Simulation* inst) final; // no-op: hypermesh computes in-frame on the render thread

  using component_set_t = std::unordered_set<HypermeshComponent*>;
  LockedResource<component_set_t> _components;

  SceneGraphSystem* _sgsys  = nullptr;
  AssetSystem*      _assets = nullptr;
  const HypermeshSystemData& _HSD;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
