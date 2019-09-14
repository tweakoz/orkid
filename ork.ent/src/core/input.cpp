#include <pkg/ent/input.h>
#include <ork/lev2/input/inputdevice.h>
#include <pkg/ent/LuaBindings.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::InputSystemData, "InputSystemData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::InputComponentData, "InputComponentData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::InputComponent, "InputComponent");

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

  void InputComponent::doNotify(const ComponentEvent& e)
	{
  }
  svar64_t InputComponent::doQuery(const ComponentQuery& q) {
    svar64_t rval(nullptr);

    if(q._eventID == "get.group"){
      auto grpname = q._eventData.Get<std::string>();
      auto grp = lev2::InputManager::inputGroup(grpname);
      rval.Set<void*>(grp);
    }
    else if(q._eventID == "read"){
      const auto& tbl = q._eventData.Get<ScriptTable>();
      //for( auto i : tbl._items ){
        //printf( "read tablekey<%s>\n", i.first.c_str() );
      //}
      auto itg = tbl._items.find("grp");
      assert(itg!=tbl._items.end());
      auto itch = tbl._items.find("channel");
      assert(itch!=tbl._items.end());
      auto grp = static_cast<lev2::InputGroup*>(itg->second._encoded.Get<void*>());
      assert(grp!=nullptr);
      auto channel_name = itch->second._encoded.Get<std::string>();
      auto chval = grp->getChannel(channel_name).rawValue();

      if(chval.IsSet() ){
        rval = chval;
      }
    }
    else{
      printf( "invalid evid<%s>\n", q._eventID.c_str() );
      assert(false);
    }
    return rval;
  }


  void InputComponent::onActivate(Simulation* psi) {

  }

///////////////////////////////////////////////////////////////////////////////

InputSystemData::InputSystemData(){

}

void InputSystemData::Describe(){

}

System* InputSystemData::createSystem(Simulation* psi) const {
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

InputSystem::InputSystem(const InputSystemData& data, Simulation* psi)
    : System(&data,psi){

      _impl.Make<_InputSystemIMPL>();
}

InputSystem::~InputSystem(){

}

void InputSystem::DoUpdate(Simulation* psi){
  lev2::InputManager::poll();
}
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::ent {
