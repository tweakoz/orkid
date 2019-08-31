#include <pkg/ent/input.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::InputSystemData, "InputSystemData");

namespace ork::ent {
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
