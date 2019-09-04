///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "CharacterLocoComponent.h"
#include <ork/kernel/msgrouter.inl>
#include <ork/reflect/RegisterProperty.h>

///////////////////////////////////////////////////////////////////////////////
using namespace ork::reflect;
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
void CharacterLocoData::Describe() {
  RegisterFloatMinMaxProp(&CharacterLocoData::mWalkSpeed, "WalkSpeed", "0", "250");
  RegisterFloatMinMaxProp(&CharacterLocoData::mRunSpeed, "RunSpeed", "0", "500");
  RegisterFloatMinMaxProp(&CharacterLocoData::mSpeedLerpRate, "SpeedLerpRate", "0.1", "100");
}
///////////////////////////////////////////////////////////////////////////////
class CharacterLocoComponent : public ComponentInst {
  RttiDeclareNoFactory(CharacterLocoComponent, ComponentInst)

      public :

      ///////////////////////////////////////////////////////////////////////////

      CharacterLocoComponent(const CharacterLocoData& data, Entity* pent)
      : ComponentInst(&data, pent), _data(data) {}
  ~CharacterLocoComponent() {}

  ///////////////////////////////////////////////////////////////////////////

  void DoUpdate(SceneInst* psi) final {}

  void doNotify(const ComponentEvent& e) final {
    if(e._eventID == "locostate")
    {   auto state = e._eventData.Get<std::string>();
        if( state=="stop" ){

        }
        else if( state=="walk" ){

        }
        else{
          assert(false);
        }
        printf( "loco got state change request id<%s>\n", state.c_str() );
    }

  }

  const char* scriptName() final {
      return "Loco";
  }

  ///////////////////////////////////////////////////////////////////////////

  bool DoLink(ork::ent::SceneInst* psi) final { return true; }

  ///////////////////////////////////////////////////////////////////////////

  const CharacterLocoData& _data;
};

void CharacterLocoComponent::Describe() {}

//////////////////////////////////////////////////////////

ComponentInst* CharacterLocoData::createComponent(Entity* pent) const { return new CharacterLocoComponent(*this, pent); }

}} // namespace ork::ent

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::CharacterLocoData, "CharacterLocoData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::CharacterLocoComponent, "CharacterLocoComponent");
