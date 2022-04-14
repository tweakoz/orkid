////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/register.h>
#include <ork/rtti/downcast.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.hpp>
#include <pkg/ent/entity.hpp>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/enum_serializer.inl>
#include <pkg/ent/CompositingSystem.h>
#include <pkg/ent/LightingSystem.h>
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::CompositingSystemData, "CompositingSystemData");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void CompositingSystemData::Describe() {
  using namespace ork::reflect;
  reflect::RegisterProperty("CompositorData", &CompositingSystemData::_accessor);
}

///////////////////////////////////////////////////////////////////////////////

CompositingSystemData::CompositingSystemData() {
}

void CompositingSystemData::defaultSetup() {
  _compositingData.presetDefault();
}

///////////////////////////////////////////////////////////////////////////////

ork::ent::System* CompositingSystemData::createSystem(ork::ent::Simulation* pinst) const {
  return new CompositingSystem(*this, pinst);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CompositingSystem::CompositingSystem(const CompositingSystemData& data, Simulation* psim)
    : ork::ent::System(&data, psim)
    , _compositingSystemData(data)
    , _impl(data._compositingData.createImpl()) {
}

bool CompositingSystem::enabled() const {
  return _impl->IsEnabled();
}

CompositingSystem::~CompositingSystem() {
}

void CompositingSystem::DoUpdate(Simulation* psim) {
}

bool CompositingSystem::DoLink(Simulation* psi) {
  if (auto lsys = psi->findSystem<LightingSystem>()) {
    _impl->bindLighting(&lsys->GetLightManager());
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
///////////////////////////////////////////////////////////////////////////////
