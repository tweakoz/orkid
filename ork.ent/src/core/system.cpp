////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <pkg/ent/component.h>

#include <pkg/ent/entity.h>
#include <pkg/ent/componenttable.h>

#include <ork/kernel/orklut.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/DirectTypedVector.hpp>
#include <ork/reflect/properties/register.h>
#include <ork/application/application.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SystemData, "SystemData")
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SystemDataClass, "SystemDataClass")

namespace ork::ent {

  SystemDataClass::SystemDataClass(const rtti::RTTIData &data) : object::ObjectClass(data)
  {
  }

  PoolString SystemData::GetFamily() const
  {
  	const SystemDataClass *clazz = rtti::autocast(GetClass());
  	OrkAssert(clazz);
  	return clazz->GetFamily();
  }

  void SystemData::Describe()
  {
  }
  void System::Link( Simulation* psi )
  {
  	DoLink(psi);
  }
  void System::UnLink( Simulation* psi )
  {
  	DoUnLink(psi);
  }
  void System::Start( Simulation* psi )
  {
  	DoStart(psi);
  }
  void System::Stop( Simulation* psi )
  {
  	DoStop(psi);
  }
  void System::Update( Simulation* psi )
  {
  	DoUpdate( psi );
  }
} // namespace ork::ent {
