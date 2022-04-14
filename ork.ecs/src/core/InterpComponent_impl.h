////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/ecs/InterpComponent.h>

namespace ork::ecs {

struct InterpSystem;

struct InterpComponent : public ecs::Component {
  DeclareAbstractX(InterpComponent, ecs::Component);

public:
  InterpComponent(const InterpComponentData& cd, ork::ecs::Entity* pent);
  const InterpComponentData& GetCD() const {
    return mCD;
  }

  void _onUninitialize(Simulation* psi) final;
  bool _onLink(Simulation* psi) final;
  void _onUnlink(Simulation* psi) final;
  bool _onStage(Simulation* psi) final;
  void _onUnstage(Simulation* psi) final;
  bool _onActivate(Simulation* psi) final;
  void _onDeactivate(Simulation* psi) final;
  void _onNotify(Simulation* psi, token_t evID, svar64_t data ) final;

  const InterpComponentData& mCD;

  fvec3 _target_pos;
  fvec3 _current_pos;
	InterpSystem* _system = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct InterpSystem final : public ork::ecs::System {

public:
  static constexpr systemkey_t SystemType = "InterpSystem";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }

  InterpSystem(const InterpSystemData& data, ork::ecs::Simulation* pinst);

private:

	friend struct InterpComponent;

  void _onStageComponent(InterpComponent* component);
  void _onUnstageComponent(InterpComponent* component);
  void _onActivateComponent(InterpComponent* component);
  void _onDeactivateComponent(InterpComponent* component);

  bool _onLink(Simulation* psi) override;
  void _onUnLink(Simulation* psi) override;
  void _onUpdate(Simulation* inst) override;
  bool _onStage(Simulation* psi) override;
  void _onUnstage(Simulation* inst) override;
  bool _onActivate(Simulation* psi) override;
  void _onDeactivate(Simulation* inst) override;

  std::unordered_set<InterpComponent*> _components;

};

////////////////////////////////////////////////////////////////

} //namespace ork::ecs {
