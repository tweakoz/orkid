#include <pkg/ent/input.h>
#include <ork/lev2/input/inputdevice.h>

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

///////////////////////////////////////////////////////////////////////////////

struct _InputSystemIMPL {

  _InputSystemIMPL(){

  }

  size_t _numDevices = 0;
  std::vector<lev2::InputDevice*> _devices;
};

///////////////////////////////////////////////////////////////////////////////

InputSystem::InputSystem(const InputSystemData& data, SceneInst* psi)
    : System(&data,psi){

      _impl.Make<_InputSystemIMPL>();
}

InputSystem::~InputSystem(){

}

void InputSystem::DoUpdate(SceneInst* psi){
  lev2::InputManager::poll();
}
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::ent {
