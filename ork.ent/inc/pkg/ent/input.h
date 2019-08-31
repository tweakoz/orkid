#pragma once

#include "componentfamily.h"
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>

namespace ork::ent {
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
