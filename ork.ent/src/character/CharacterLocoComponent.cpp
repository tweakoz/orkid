///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "CharacterLocoComponent.h"
#include <ork/kernel/msgrouter.inl>
#include <ork/reflect/properties/register.h>
#include <pkg/ent/bullet.h>
#include <pkg/ent/entity.hpp>
#include <ork/application/application.h>
#include <ork/kernel/string/deco.inl>

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
class LocomotionForceData : public BulletObjectForceControllerData {
  RttiDeclareConcrete(LocomotionForceData, BulletObjectForceControllerData);

public:
  LocomotionForceData()
      : mForce(1.0f)
      , mDirection(0.0f, 0.0f, 0.0f) {
  }

  float GetForce() const {
    return mForce;
  }
  const fvec3& GetDirection() const {
    return mDirection;
  }

  ~LocomotionForceData() final {
  }

private:
  BulletObjectForceControllerInst*
  CreateForceControllerInst(const BulletObjectControllerData& data, ork::ent::Entity* pent) const final;

  float mForce;
  fvec3 mDirection;
};

///////////////////////////////////////////////////////////////////////////////

struct LocomotionForceInst : public BulletObjectForceControllerInst {
  LocomotionForceInst(const LocomotionForceData& data);
  ~LocomotionForceInst() final;
  void UpdateForces(ork::ent::Simulation* inst, BulletObjectControllerInst* boci) final;
  bool DoLink(Simulation* psi) final;
  void setForce(fvec3 force) {
    // printf( "setting force<%g %g %g>\n", force.x, force.y, force.z );
    _force = force;
  }

  const LocomotionForceData& mData;
  fvec3 _force;
};

void LocomotionForceData::Describe() {
}

BulletObjectForceControllerInst*
LocomotionForceData::CreateForceControllerInst(const BulletObjectControllerData& data, ork::ent::Entity* pent) const {
  LocomotionForceInst* rval = new LocomotionForceInst(*this);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

LocomotionForceInst::LocomotionForceInst(const LocomotionForceData& data)
    : BulletObjectForceControllerInst(data)
    , mData(data) {
}
LocomotionForceInst::~LocomotionForceInst() {
}

bool LocomotionForceInst::DoLink(Simulation* psi) {
  return true;
}

///////////////////////////////////////////////////////////////////////////////
void LocomotionForceInst::UpdateForces(ork::ent::Simulation* inst, BulletObjectControllerInst* boci) { // final
  btRigidBody* rbody = boci->GetRigidBody();
  rbody->applyCentralForce(!_force);
}
///////////////////////////////////////////////////////////////////////////////
class CharacterLocoComponent : public ComponentInst {
  RttiDeclareNoFactory(CharacterLocoComponent, ComponentInst)

      public :

      ///////////////////////////////////////////////////////////////////////////

      CharacterLocoComponent(const CharacterLocoData& data, Entity* pent)
      : ComponentInst(&data, pent)
      , _data(data)
      , _boci(nullptr) {

    _subscriber = msgrouter::channel("eggytest")->subscribe([this](msgrouter::content_t c) {
      fmtx4 hmdrmtx = c.Get<fmtx4>();
      hmdrmtx.SetTranslation(fvec3(0, 0, 0));
      this->_headingmatrix.inverseOf(hmdrmtx);
    });
  }
  ~CharacterLocoComponent() {
  }

  ///////////////////////////////////////////////////////////////////////////

  void DoUpdate(Simulation* psi) final {
    if (nullptr == _locoforce)
      return;

    fvec4 nn(0, 0, -1);
    auto nnn    = nn.Transform(_headingmatrix);
    float forc  = _arewalking ? _walkingForce : 0.0;
    bool dirset = _setDir.Mag() > 0.0f;
    auto f      = dirset ? _setDir : nnn.xyz();
    f           = f * forc;
    // printf("force<%g %g %g>\n", f.x, f.y, f.z);
    _locoforce->setForce(f);
  }

  void doNotify(const ComponentEvent& e) final {
    if (e._eventID == "locostate") {
      auto state = e._eventData.Get<std::string>();
      if (state == "stop") {
        _arewalking = false;
      } else if (state == "walk") {
        _arewalking = true;
      } else {
        assert(false);
      }
      deco::printf(fvec3::Yellow(), "loco got state change request id<%s>\n", state.c_str());
    } else if (e._eventID == "setDir") {
      _setDir     = e._eventData.Get<fvec3>();
      _arewalking = true;
    } else if (e._eventID == "setWalkingForce") {
      _walkingForce = e._eventData.Get<double>();
    }
  }

  const char* scriptName() final {
    return "Loco";
  }

  ///////////////////////////////////////////////////////////////////////////

  bool DoLink(ork::ent::Simulation* psi) final {

    _boci = _entity->GetTypedComponent<BulletObjectControllerInst>();
    return true;
  }

  void onActivate(Simulation* psi) final {
    if (_boci) {
      _locoforce = dynamic_cast<LocomotionForceInst*>(_boci->getForceController(AddPooledString("loco")));
      if (_locoforce) {
        printf("yep, got locoforce<%p>\n", _locoforce);
      } else {
        printf("nope, no locoforce\n");
      }
    }
  }

  ///////////////////////////////////////////////////////////////////////////

  const CharacterLocoData& _data;
  BulletObjectControllerInst* _boci;
  LocomotionForceInst* _locoforce;
  fmtx4 _headingmatrix;
  msgrouter::subscriber_t _subscriber;
  bool _arewalking = false;
  fvec3 _setDir;
  float _walkingForce = 20.0f;
};

void CharacterLocoComponent::Describe() {
}

//////////////////////////////////////////////////////////

ComponentInst* CharacterLocoData::createComponent(Entity* pent) const {
  return new CharacterLocoComponent(*this, pent);
}

}} // namespace ork::ent

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::CharacterLocoData, "CharacterLocoData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::CharacterLocoComponent, "CharacterLocoComponent");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::LocomotionForceData, "LocomotionForceData");
