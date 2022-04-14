////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

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
  auto passh       = desc.findTypedProperty<ptype>("sharedobj_accessor");
  passh->set(sht2, sht1);
  CHECK_EQUAL(sht1->_accessorChild, sht2);
  object_ptr_t sht3;
  passh->get(sht3, sht1);
  CHECK_EQUAL(sht1->_accessorChild, sht3);
}

///////////////////////////////////////////////////////////////////////////////

TEST(ReflectionDirectSharedProperty) {
  auto top        = std::make_shared<SharedTest>();
  auto topclazz   = top->GetClass();
  auto childclazz = SimpleTest::GetClassStatic();
  auto child      = childclazz->createShared();
  auto& topdesc   = topclazz->Description();
  using ptype     = DirectObject<simpletest_ptr_t>;
  auto passh      = topdesc.findTypedProperty<ptype>("sharedobj_direct");
  passh->set(objcast<SimpleTest>(child), top);
  CHECK_EQUAL(top->_directChild, child);

  simpletest_ptr_t get_test;
  passh->get(get_test, top);
  CHECK_EQUAL(top->_directChild, get_test);
}
