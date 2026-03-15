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

  ProbeSystem(const ProbeSystemData& data, ork::ecs::Simulation* pinst);

  // Call from Python _onGpuUpdate (outside beginFrame/endFrame)
  int bakeAll(lev2::Context* ctx, const std::string& output_base);
  void markAllDirty();
  bool areAllClean() const;
  void activateBakeOnly();
  void deactivateBakeOnly();

private:

  friend struct ProbeComponent;

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
