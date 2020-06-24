#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <string.h>

#include "reflectionclasses.inl"
#include <ork/reflect/IObjectPropertyType.hpp>
#include <ork/reflect/AccessorObjectPropertyType.hpp>

using namespace ork;
using namespace ork::object;
using namespace ork::reflect;
using namespace ork::rtti;

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(SharedTest, "SharedTest");

void SharedTest::describeX(ObjectClass* clazz) {
  clazz->accessorProperty(
      "prop_sharedobj", //
      &SharedTest::getChild,
      &SharedTest::setChild);
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

TEST(ReflectionSharedPropertySet) {

  auto sht1        = std::make_shared<SharedTest>();
  auto clazz       = sht1->GetClass();
  auto clazzstatic = SharedTest::GetClassStatic();
  auto sht2        = std::dynamic_pointer_cast<Object>(clazz->createShared());
  auto& desc       = clazz->Description();
  auto p           = desc.FindProperty("prop_sharedobj");
  using ptype      = AccessorObjectPropertyType<object_ptr_t>;
  auto passh       = dynamic_cast<const ptype*>(p);
  passh->Set(sht2, sht1.get());
  CHECK_EQUAL(sht1->_childObject, sht2);
}
