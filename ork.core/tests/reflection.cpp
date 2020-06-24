#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <string.h>

#include "reflectionclasses.inl"
#include <ork/reflect/IObjectPropertyType.hpp>
#include <ork/reflect/AccessorObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertySharedObject.h>

using namespace ork;
using namespace ork::object;
using namespace ork::reflect;
using namespace ork::rtti;

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(SharedTest, "SharedTest");

void SharedTest::describeX(ObjectClass* clazz) {
  clazz->accessorProperty(
      "prop_sharedobj_accessor", //
      &SharedTest::getChild,
      &SharedTest::setChild);

  clazz->sharedObjectProperty(
      "prop_sharedobj_direct", //
      &SharedTest::_childObject);
}

///////////////////////////////////////////////////////////////////////////////

TEST(ReflectionClazz) {
  auto sht1         = std::make_shared<SharedTest>();
  auto clazzdynamic = sht1->GetClass();
  auto clazzstatic  = SharedTest::GetClassStatic();
  CHECK_EQUAL(clazzdynamic, clazzstatic);
  auto sht2 = clazzstatic->createShared();
  CHECK_EQUAL(sht2->GetClass(), clazzstatic);
}

///////////////////////////////////////////////////////////////////////////////

TEST(ReflectionAccessorSharedProperty) {

  auto sht1        = std::make_shared<SharedTest>();
  auto clazz       = sht1->GetClass();
  auto clazzstatic = SharedTest::GetClassStatic();
  auto sht2        = std::dynamic_pointer_cast<Object>(clazz->createShared());
  auto& desc       = clazz->Description();
  auto p           = desc.FindProperty("prop_sharedobj_accessor");
  using ptype      = AccessorObjectPropertyType<object_ptr_t>;
  auto passh       = dynamic_cast<const ptype*>(p);
  passh->Set(sht2, sht1.get());
  CHECK_EQUAL(sht1->_childObject, sht2);
}

///////////////////////////////////////////////////////////////////////////////

TEST(ReflectionDirectSharedProperty) {

  auto sht1        = std::make_shared<SharedTest>();
  auto clazz       = sht1->GetClass();
  auto clazzstatic = SharedTest::GetClassStatic();
  auto sht2        = std::dynamic_pointer_cast<Object>(clazz->createShared());
  auto& desc       = clazz->Description();
  auto p           = desc.FindProperty("prop_sharedobj_direct");
  using ptype      = DirectObjectPropertySharedObject;
  auto passh       = dynamic_cast<const ptype*>(p);
  passh->set(sht2, sht1.get());
  CHECK_EQUAL(sht1->_childObject, sht2);
}
