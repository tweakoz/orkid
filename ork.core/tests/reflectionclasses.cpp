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
#include <ork/reflect/enum_serializer.inl>

using namespace ork;
using namespace ork::object;
using namespace ork::reflect;
using namespace ork::rtti;

///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(SimpleTest, "SimpleTest");
ImplementReflectionX(EnumTest, "EnumTest");
ImplementReflectionX(SharedTest, "SharedTest");
ImplementReflectionX(MapTest, "MapTest");
ImplementReflectionX(ArrayTest, "ArrayTest");
///////////////////////////////////////////////////////////////////////////////
void SimpleTest::describeX(ObjectClass* clazz) {
  ///////////////////////////////////
  clazz->memberProperty(
      "value", //
      &SimpleTest::_strvalue);
}
///////////////////////////////////////////////////////////////////////////////
SimpleTest::SimpleTest(std::string str)
    : _strvalue(str) {
}
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
      "directintstr_map", //
      &MapTest::_directintstrmap);
  ///////////////////////////////////
  clazz->directMapProperty(
      "directstrint_map", //
      &MapTest::_directstrintmap);
  ///////////////////////////////////
  clazz->directMapProperty(
      "directintstr_unorderedmap", //
      &MapTest::_directintstrumap);
  ///////////////////////////////////
  clazz->directMapProperty(
      "directstrint_unordered_map", //
      &MapTest::_directstrintumap);
  ///////////////////////////////////
  clazz->directMapProperty(
      "directstrint_lut", //
      &MapTest::_directstrintlut);
}
///////////////////////////////////////////////////////////////////////////////
ArrayTest::ArrayTest() {
}
///////////////////////////////////////////////////////////////////////////////
void ArrayTest::describeX(ObjectClass* clazz) {
  ///////////////////////////////////
  clazz->directVectorProperty(
      "directint_vect", //
      &ArrayTest::_directintvect);
  ///////////////////////////////////
  clazz->directVectorProperty(
      "directstr_vect", //
      &ArrayTest::_directstrvect);
  ///////////////////////////////////
  clazz->directVectorProperty(
      "directobj_vect", //
      &ArrayTest::_directobjvect);
}
///////////////////////////////////////////////////////////////////////////////
template <> //
inline void ITyped<MultiCurveSegmentType>::serialize(serdes::node_ptr_t leafnode) const {
  auto serializer = leafnode->_serializer;
  auto instance   = leafnode->_ser_instance;
  MultiCurveSegmentType value;
  get(value, instance);
  auto registrar = reflect::serdes::EnumRegistrar::instance();
  auto enumtype  = registrar->findEnumClass<MultiCurveSegmentType>();
  auto enumname  = enumtype->findNameFromValue(int(value));
  leafnode->_value.template Set<std::string>(enumname);
  serializer->serializeLeaf(leafnode);
}
///////////////////////////////////////////////////////////////////////////////
template <> //
inline void ITyped<MultiCurveSegmentType>::deserialize(serdes::node_ptr_t desernode) const {
  auto instance      = desernode->_deser_instance;
  const auto& var    = desernode->_value;
  const auto& as_str = var.Get<std::string>();
  auto registrar     = reflect::serdes::EnumRegistrar::instance();
  auto enumtype      = registrar->findEnumClass<MultiCurveSegmentType>();
  int intval         = enumtype->findValueFromName(as_str);
  auto enumval       = MultiCurveSegmentType(intval);
  set(enumval, instance);
}
///////////////////////////////////////////////////////////////////////////////
EnumTest::EnumTest() {
}
///////////////////////////////////////////////////////////////////////////////
void EnumTest::describeX(ObjectClass* clazz) {
  ///////////////////////////////////
  clazz->memberProperty(
      "enum_direct", //
      &EnumTest::_mcst);
}
///////////////////////////////////////////////////////////////////////////////
