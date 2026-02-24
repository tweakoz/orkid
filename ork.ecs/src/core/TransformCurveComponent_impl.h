////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/ecs/TransformCurveComponent.h>

namespace ork::ecs {

struct TransformCurveSystem;

struct TransformCurveComponent : public ecs::Component {
  DeclareAbstractX(TransformCurveComponent, ecs::Component);

public:
  TransformCurveComponent(const TransformCurveComponentData& cd, ork::ecs::Entity* pent);
  const TransformCurveComponentData& GetCD() const {
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

  const TransformCurveComponentData& _CD;
  TransformCurveSystem* _system = nullptr;
  float _curveTime = 0.0f;
};

///////////////////////////////////////////////////////////////////////////////

struct TransformCurveSystem final : public ork::ecs::System {

public:
  static constexpr systemkey_t SystemType = "TransformCurveSystem";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }

  TransformCurveSystem(const TransformCurveSystemData& data, ork::ecs::Simulation* pinst);

private:
  friend struct TransformCurveComponent;

  void _onStageComponent(TransformCurveComponent* component);
  void _onUnstageComponent(TransformCurveComponent* component);
  void _onActivateComponent(TransformCurveComponent* component);
  void _onDeactivateComponent(TransformCurveComponent* component);

  bool _onLink(Simulation* psi) override;
  void _onUnLink(Simulation* psi) override;
  void _onUpdate(Simulation* inst) override;
  bool _onStage(Simulation* psi) override;
  void _onUnstage(Simulation* inst) override;
  bool _onActivate(Simulation* psi) override;
  void _onDeactivate(Simulation* inst) override;

  std::unordered_set<TransformCurveComponent*> _components;
};

////////////////////////////////////////////////////////////////

} // namespace ork::ecs
