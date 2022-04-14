////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/ecs/component.h>

//#include <ork/ecs/entity.h>
//#include <ork/ecs/componenttable.h>

#include <ork/kernel/orklut.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/DirectTypedVector.hpp>
#include <ork/reflect/properties/register.h>
#include <ork/application/application.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ecs::ComponentFragmentDataClass, "ComponentFragmentDataClass");
ImplementReflectionX(ork::ecs::ComponentFragmentData, "ComponentFragmentData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ecs::ComponentFragment, "ComponentFragment");

namespace ork::ecs {

ComponentFragmentDataClass::ComponentFragmentDataClass(const rtti::RTTIData& data)
    : object::ObjectClass(data) {
}

void ComponentFragmentData::describeX(class_t* c) {
}

ComponentFragmentData::ComponentFragmentData() {
}

void ComponentFragment::Describe() {
}

} //namespace ork::ecs {
