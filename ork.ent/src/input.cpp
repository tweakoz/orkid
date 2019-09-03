#include <pkg/ent/input.h>
#include <ork/lev2/input/inputdevice.h>
#include "LuaBindings.h"

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::InputSystemData, "InputSystemData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::InputComponentData, "InputComponentData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::InputComponent, "InputComponent");

using namespace LuaIntf;

namespace ork::ent {

  ///////////////////////////////////////////////////////////////////////////////

  InputComponentData::InputComponentData(){

    auto fam = GetFamily();

    //printf( "InputComponentData::Family<%s>\n", fam.c_str() );
    //assert(false);
  }

  void InputComponentData::Describe(){

  }

  ComponentInst* InputComponentData::createComponent(Entity *pent) const {
      return new InputComponent(*this,pent);
  }


  InputComponent::InputComponent(const InputComponentData& data, Entity *entity)
    : ComponentInst(&data,entity) {
  }

  void InputComponent::Describe(){

  }

  bool InputComponent::DoNotify(const ork::event::Event* event)
	{
    if(const ork::event::VEvent* vev = ork::rtti::autocast(event))
    {   const auto& LR = vev->mData.Get<LuaRef>();
        assert(false);
    }
    return false;
  }
  void InputComponent::onActivate(SceneInst* psi) {

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
