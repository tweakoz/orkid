#pragma once

#include "componentfamily.h"
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>

namespace ork::ent {

///////////////////////////////////////////////////////////////////////////////

class InputComponentData : public ComponentData {
  RttiDeclareConcrete(InputComponentData, ComponentData);

public:
  InputComponentData();

protected:

  ComponentInst *createComponent(Entity *pent) const final ;
};

///////////////////////////////////////////////////////////////////////////////

class InputComponent : public ComponentInst {

  RttiDeclareNoFactory(InputComponent, ComponentInst);

public:

  InputComponent(const InputComponentData& data, Entity *entity);

protected:

  void DoUpdate(Simulation *psi) final {}
  bool DoStart(Simulation *psi, const fmtx4 &world) final { return true; }
  bool DoLink(Simulation *psi) final { return true; }
  void DoUnLink(Simulation *psi) final {}
  void DoStop(Simulation *psi) final {}
  void onActivate(Simulation* psi) final;

  const char* scriptName() final {
      return "Input";
  }

  void doNotify(const ComponentEvent& e) final;
  svar64_t doQuery(const ComponentQuery& q) final;


};

///////////////////////////////////////////////////////////////////////////////

class InputSystemData : public SystemData {
  RttiDeclareConcrete(InputSystemData, SystemData);

public:
  InputSystemData();

protected:
  System* createSystem(Simulation* psi) const final;
};

///////////////////////////////////////////////////////////////////////////////

class InputSystem : public System {

  void DoUpdate(Simulation *inst) final;

public:

	static constexpr systemkey_t SystemType = "InputSystem";
	systemkey_t systemTypeDynamic() final { return SystemType; }

  InputSystem(const InputSystemData &data, Simulation* psi);
  ~InputSystem();

  svar256_t _impl;

};

}
