#include <pkg/ent/input.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::InputSystemData, "InputSystemData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::InputComponentData, "InputComponentData");

namespace ork::ent {

  ///////////////////////////////////////////////////////////////////////////////

  InputComponentData::InputComponentData(){

  }

  void InputComponentData::Describe(){

  }

  ComponentInst* InputComponentData::createComponent(Entity *pent) const {
      return new InputComponentInst(*this,pent);
  }


  InputComponentInst::InputComponentInst(const InputComponentData& data, Entity *entity)
    : ComponentInst(&data,entity) {
  }
///////////////////////////////////////////////////////////////////////////////

InputSystemData::InputSystemData(){

}

void InputSystemData::Describe(){

}

System* InputSystemData::createSystem(SceneInst* psi) const {
    return new InputSystem(*this,psi);
}

InputSystem::InputSystem(const InputSystemData& data, SceneInst* psi)
    : System(&data,psi){

}

InputSystem::~InputSystem(){

}

void InputSystem::DoUpdate(SceneInst* psi){

}
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::ent {
