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

class InputComponentInst : public ComponentInst {

public:

  InputComponentInst(const InputComponentData& data, Entity *entity);

protected:

  void DoUpdate(SceneInst *psi) final {}
  bool DoStart(SceneInst *psi, const CMatrix4 &world) final { return true; }
  bool DoLink(SceneInst *psi) final { return true; }
  void DoUnLink(SceneInst *psi) final {}
  void DoStop(SceneInst *psi) final {}
};

///////////////////////////////////////////////////////////////////////////////

class InputSystemData : public SystemData {
  RttiDeclareConcrete(InputSystemData, SystemData);

public:
  InputSystemData();

protected:
  System* createSystem(SceneInst* psi) const final;
};

///////////////////////////////////////////////////////////////////////////////

class InputSystem : public System {

  void DoUpdate(SceneInst *inst) final;

public:

	static constexpr systemkey_t SystemType = "InputSystem";
	systemkey_t systemTypeDynamic() final { return SystemType; }

  InputSystem(const InputSystemData &data, SceneInst* psi);
  ~InputSystem();

};

}
