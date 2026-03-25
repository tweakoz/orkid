////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/ecs/ProbeComponent.h>
#include <ork/ecs/SceneGraphComponent.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/scenegraph/scenegraph.h>

namespace ork::ecs {

struct ProbeSystem;

struct ProbeComponent : public ecs::Component {
  DeclareAbstractX(ProbeComponent, ecs::Component);

public:
  ProbeComponent(const ProbeComponentData& cd, ork::ecs::Entity* pent);

  void _onUninitialize(Simulation* psi) final;
  bool _onLink(Simulation* psi) final;
  void _onUnlink(Simulation* psi) final;
  bool _onStage(Simulation* psi) final;
  void _onUnstage(Simulation* psi) final;
  bool _onActivate(Simulation* psi) final;
  void _onDeactivate(Simulation* psi) final;
  void _onNotify(Simulation* psi, token_t evID, evdata_t data) final;

  const ProbeComponentData& _CD;
  ProbeSystem* _system = nullptr;
  lev2::lightprobe_ptr_t _probe;
  lev2::scenegraph::probenode_ptr_t _probeNode;
};

///////////////////////////////////////////////////////////////////////////////

struct ProbeSystem final : public ork::ecs::System {

public:
  static constexpr systemkey_t SystemType = "ProbeSystem";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }

  // Event tokens
  DeclareToken(Bake);
  DeclareToken(BakeSelected);

  ProbeSystem(const ProbeSystemData& data, ork::ecs::Simulation* pinst);

private:

  friend struct ProbeComponent;

  // Request handler + GPU update
  void _onRequest(impl::sys_response_ptr_t response, token_t reqID, evdata_t data) override;
  void _onGpuUpdate(Simulation* psi, lev2::Context* ctx) override;

  // Internal operations
  int _bakeAll(lev2::Context* ctx, const std::string& output_base);
  int _bakeSelected(lev2::Context* ctx, spawndata_ptr_t spawner);
  void _markAllDirty();
  bool _areAllClean() const;
  void _activateBakeOnly();
  void _deactivateBakeOnly();

  // Pending bake request (queued by Bake request, executed in _onGpuUpdate when probes clean)
  struct PendingBake {
    std::string _outputBase;
    spawndata_ptr_t _selectedSpawner; // nullptr = bake all
    impl::sys_response_ptr_t _response;
    bool _activated = false;
  };
  std::vector<PendingBake> _pendingBakes;

  void _onStageComponent(ProbeComponent* component);
  void _onUnstageComponent(ProbeComponent* component);
  void _onActivateComponent(ProbeComponent* component);
  void _onDeactivateComponent(ProbeComponent* component);

  bool _onLink(Simulation* psi) override;
  void _onUnLink(Simulation* psi) override;
  void _onUpdate(Simulation* inst) override;
  bool _onStage(Simulation* psi) override;
  void _onUnstage(Simulation* inst) override;
  bool _onActivate(Simulation* psi) override;
  void _onDeactivate(Simulation* inst) override;

  std::unordered_set<ProbeComponent*> _components;
  SceneGraphSystem* _sgSystem = nullptr;
};

////////////////////////////////////////////////////////////////

} // namespace ork::ecs
