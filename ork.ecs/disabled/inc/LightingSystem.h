////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <pkg/ent/entity.h>
#include <ork/math/cvector3.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

class LightingSystemData : public ork::ent::SystemData {
  RttiDeclareConcrete(LightingSystemData, ork::ent::SystemData);

public:
  ///////////////////////////////////////////////////////
  LightingSystemData();
  ///////////////////////////////////////////////////////

  const ork::lev2::LightManagerData& Lmd() const {
    return mLmd;
  }

private:
  ork::ent::System* createSystem(ork::ent::Simulation* pinst) const final;
  ork::Object* LmdAccessor() {
    return &mLmd;
  }

  ork::lev2::LightManagerData mLmd;
};

///////////////////////////////////////////////////////////////////////////////

class LightingSystem : public ork::ent::System {
public:
  static constexpr systemkey_t SystemType = "LightingSystem";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }

  LightingSystem(const LightingSystemData& data, ork::ent::Simulation* pinst);

  ork::lev2::LightManager& GetLightManager() {
    return mLightManager;
  }

private:
  ork::lev2::LightManager mLightManager;
};

///////////////////////////////////////////////////////////////////////////////

class LightingComponentData : public ork::ent::ComponentData {
  RttiDeclareConcrete(LightingComponentData, ork::ent::ComponentData);

public:
  ///////////////////////////////////////////////////////
  LightingComponentData();
  ~LightingComponentData();
  ///////////////////////////////////////////////////////
  ork::lev2::LightData* GetLightData() const {
    return _lightdata;
  }
  bool isDynamic() const {
    return _dynamic;
  }

private:
  ork::ent::ComponentInst* createComponent(ork::ent::Entity* pent) const final;

  void LdGetter(ork::rtti::ICastable*& val) const {
    val = _lightdata;
  }
  void LdSetter(ork::rtti::ICastable* const& val) {
    _lightdata = ork::rtti::downcast<ork::lev2::LightData*>(val);
  }
  void DoRegisterWithScene(ork::ent::SceneComposer& sc) final;

  ork::lev2::LightData* _lightdata;
  bool _dynamic;
};

///////////////////////////////////////////////////////////////////////////////

class LightingComponentInst final : public ork::ent::ComponentInst {
  RttiDeclareAbstract(LightingComponentInst, ork::ent::ComponentInst);

public:
  LightingComponentInst(const LightingComponentData& data, ork::ent::Entity* pent);

  ork::lev2::Light* GetLight() const {
    return _light;
  }

private:
  ~LightingComponentInst() override;
  void DoUpdate(ork::ent::Simulation* inst) override;
  bool DoLink(ork::ent::Simulation* psi) override;

  ork::lev2::Light* _light;
  const LightingComponentData& _lcdata;
};

///////////////////////////////////////////////////////////////////////////////

class LightArchetype : public Archetype {
  RttiDeclareConcrete(LightArchetype, Archetype);

public:
  LightArchetype();

private:
  void DoCompose(ArchComposer& composer) final; // virtual
  void DoStartEntity(Simulation*, const fmtx4& mtx, Entity* pent) const final {
  }
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::ent
