#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <string.h>

#include "reflectionclasses.inl"
#include <ork/reflect/properties/ITyped.hpp>
#include <ork/reflect/properties/AccessorTyped.hpp>
#include <ork/reflect/properties/DirectObject.h>

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
  auto sht2        = clazz->createShared();
  auto& desc       = clazz->Description();
  using ptype      = AccessorTyped<object_ptr_t>;
  auto passh       = desc.findTypedProperty<ptype>("prop_sharedobj_accessor");
  passh->set(sht2, sht1);
  CHECK_EQUAL(sht1->_accessorChild, sht2);
  object_ptr_t sht3;
  passh->get(sht3, sht1);
  CHECK_EQUAL(sht1->_accessorChild, sht3);
}

///////////////////////////////////////////////////////////////////////////////

TEST(ReflectionDirectSharedProperty) {

  auto top         = std::make_shared<SharedTest>();
  auto clazz       = top->GetClass();
  auto clazzstatic = SharedTest::GetClassStatic();
  auto sht2        = clazz->createShared();
  auto& desc       = clazz->Description();
  using ptype      = DirectObject<sharedtest_ptr_t>;
  auto passh       = desc.findTypedProperty<ptype>("prop_sharedobj_direct");
  passh->set(objcast<SharedTest>(sht2), top);
  CHECK_EQUAL(top->_accessorChild, sht2);
  sharedtest_ptr_t sht3;
  passh->get(sht3, top);
  CHECK_EQUAL(top->_accessorChild, sht3);
}
