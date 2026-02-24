////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/ecs/BoidsComponent.h>
#include <ork/ecs/physics/bullet.h>
#include "../physics/bullet_impl.h"

namespace ork::ecs {

struct BoidsSystem;

///////////////////////////////////////////////////////////////////////////////

struct BoidsComponent : public ecs::Component {
  DeclareAbstractX(BoidsComponent, ecs::Component);

public:
  BoidsComponent(const BoidsComponentData& cd, ork::ecs::Entity* pent);

  const BoidsComponentData& GetCD() const {
    return _CD;
  }

  void _onUninitialize(Simulation* psi) final;
  bool _onLink(Simulation* psi) final;
  void _onUnlink(Simulation* psi) final;
  bool _onStage(Simulation* psi) final;
  void _onUnstage(Simulation* psi) final;
  bool _onActivate(Simulation* psi) final;
  void _onDeactivate(Simulation* psi) final;
  void _onNotify(Simulation* psi, token_t evID, evdata_t data) final;

  const BoidsComponentData& _CD;
  BoidsSystem* _system = nullptr;
  BulletObjectComponent* _bulletComponent = nullptr;
  fvec3 _spawnPosition;  // captured at activation for home force
};

///////////////////////////////////////////////////////////////////////////////

struct BoidsSystem final : public ork::ecs::System {
  DeclareAbstractX(BoidsSystem, ork::ecs::System);

public:
  static constexpr systemkey_t SystemType = "BoidsSystem";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }

  BoidsSystem(const BoidsSystemData& data, ork::ecs::Simulation* pinst);

private:
  friend struct BoidsComponent;

  void _onStageComponent(BoidsComponent* component);
  void _onUnstageComponent(BoidsComponent* component);
  void _onActivateComponent(BoidsComponent* component);
  void _onDeactivateComponent(BoidsComponent* component);

  bool _onLink(Simulation* psi) override;
  void _onUnLink(Simulation* psi) override;
  void _onUpdate(Simulation* inst) override;
  bool _onStage(Simulation* psi) override;
  void _onUnstage(Simulation* inst) override;
  bool _onActivate(Simulation* psi) override;
  void _onDeactivate(Simulation* inst) override;

  void _computeBoidsForces(Simulation* inst, float dt);

  std::unordered_set<BoidsComponent*> _components;
  BulletSystem* _bulletSystem = nullptr;

  // per-frame cache to avoid repeated lookups
  struct BoidState {
    BoidsComponent* _component;
    fvec3 _position;
    fvec3 _velocity;
    fvec3 _spawnPosition;
    btRigidBody* _rigidBody;
  };
  std::vector<BoidState> _stateCache;

  // grouped by flock ID
  std::unordered_map<int, std::vector<size_t>> _flockIndices;
};

////////////////////////////////////////////////////////////////

} // namespace ork::ecs
