#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <string.h>

#include "reflectionclasses.inl"
#include <ork/reflect/properties/DirectObject.h>
#include <ork/reflect/properties/ITyped.hpp>
#include <ork/reflect/properties/AccessorTyped.hpp>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>

using namespace ork;
using namespace ork::object;
using namespace ork::reflect;
using namespace ork::rtti;

///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(SharedTest, "SharedTest");
ImplementReflectionX(MapTest, "MapTest");
///////////////////////////////////////////////////////////////////////////////
void SharedTest::describeX(ObjectClass* clazz) {
  ///////////////////////////////////
  clazz->memberProperty(
      "int_direct", //
      &SharedTest::_directInt);
  ///////////////////////////////////
  clazz->memberProperty(
      "uint32_direct", //
      &SharedTest::_directUint32);
  ///////////////////////////////////
  clazz->memberProperty(
      "sizet_direct", //
      &SharedTest::_directSizeT);
  ///////////////////////////////////
  clazz->memberProperty(
      "bool_direct", //
      &SharedTest::_directBool);
  ///////////////////////////////////
  clazz->memberProperty(
      "float_direct", //
      &SharedTest::_directFloat);
  ///////////////////////////////////
  clazz->memberProperty(
      "double_direct", //
      &SharedTest::_directDouble);
  ///////////////////////////////////
  clazz->memberProperty(
      "string_direct", //
      &SharedTest::_directString);
  ///////////////////////////////////
  clazz->sharedObjectProperty(
      "sharedobj_direct", //
      &SharedTest::_directChild);
  ///////////////////////////////////
  clazz->accessorProperty(
      "sharedobj_accessor", //
      &SharedTest::getChild,
      &SharedTest::setChild);
  ///////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
SharedTest::SharedTest() {
}
///////////////////////////////////////////////////////////////////////////////
MapTest::MapTest() {
}
///////////////////////////////////////////////////////////////////////////////
void MapTest::describeX(ObjectClass* clazz) {
  ///////////////////////////////////
  clazz->directMapProperty(
      "directintstrmap", //
      &MapTest::_directintstrmap);
  ///////////////////////////////////
  clazz->directMapProperty(
      "directstrintmap", //
      &MapTest::_directstrintmap);
}
///////////////////////////////////////////////////////////////////////////////
