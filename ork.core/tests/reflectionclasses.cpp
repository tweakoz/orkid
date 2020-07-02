#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <string.h>

#include <ork/application/application.h>
#include "reflectionclasses.inl"
#include <ork/reflect/properties/DirectObject.h>
#include <ork/reflect/properties/ITyped.hpp>
#include <ork/reflect/properties/AccessorTyped.hpp>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/math/cvector2.hpp>
#include <ork/math/cvector3.hpp>
#include <ork/math/cvector4.hpp>
#include <ork/math/quaternion.hpp>
#include <ork/math/cmatrix3.hpp>
#include <ork/math/cmatrix4.hpp>

using namespace ork;
using namespace ork::object;
using namespace ork::reflect;
using namespace ork::rtti;

///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(SimpleTest, "SimpleTest");
ImplementReflectionX(EnumTest, "EnumTest");
ImplementReflectionX(MathTest, "MathTest");
ImplementReflectionX(AssetTest, "AssetTest");
ImplementReflectionX(SharedTest, "SharedTest");
ImplementReflectionX(MapTest, "MapTest");
ImplementReflectionX(ArrayTest, "ArrayTest");
///////////////////////////////////////////////////////////////////////////////
namespace ork::reflect {
using namespace serdes;
template <> //
inline void ::ork::reflect::ITyped<asset::asset_ptr_t>::serialize(serdes::node_ptr_t sernode) const {
  auto serializer = sernode->_serializer;
  auto instance   = sernode->_ser_instance;
  asset::asset_ptr_t value;
  get(value, instance);
  auto as_asset = std::dynamic_pointer_cast<asset::Asset>(value);
  if (as_asset) {
    auto mapnode           = serializer->pushNode(_name, serdes::NodeType::MAP);
    mapnode->_parent       = sernode;
    mapnode->_ser_instance = instance;
    serializeMapSubLeaf<std::string>(mapnode, "type", as_asset->GetType().c_str());
    serializeMapSubLeaf<std::string>(mapnode, "path", as_asset->_path.toStdString());
    serializer->popNode(); // pop mapnode
  } else {
    sernode->_value.template Set<void*>(nullptr);
    serializer->serializeLeaf(sernode);
  }
}
template <> //
inline void ::ork::reflect::ITyped<asset::asset_ptr_t>::deserialize(serdes::node_ptr_t mapnode) const {
  using namespace serdes;
  auto deserializer = mapnode->_deserializer;
  auto instance     = mapnode->_deser_instance;

  std::string key1_out, key2_out;
  std::string val1 = deserializeMapSubLeaf<std::string>(mapnode, key1_out);
  std::string val2 = deserializeMapSubLeaf<std::string>(mapnode, key2_out);
  OrkAssert(key1_out == "type");
  OrkAssert(key2_out == "path");

  auto asset = std::make_shared<asset::Asset>();
  OrkAssert(asset->GetType() == AddPooledString(val1.c_str()));
  asset->_path = val2;
  set(asset, instance);
}
} // namespace ork::reflect
///////////////////////////////////////////////////////////////////////////////
void AssetTest::describeX(ObjectClass* clazz) {
  ///////////////////////////////////
  clazz->memberProperty(
      "asset", //
      &AssetTest::_assetptr);
}
///////////////////////////////////////////////////////////////////////////////
AssetTest::AssetTest()
    : _assetptr(nullptr) {
}
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
EnumTest::EnumTest() {
}
///////////////////////////////////////////////////////////////////////////////
void EnumTest::describeX(ObjectClass* clazz) {
  clazz->memberProperty(
      "enum_direct", //
      &EnumTest::_mcst);
}
///////////////////////////////////////////////////////////////////////////////
MathTest::MathTest() {
}
///////////////////////////////////////////////////////////////////////////////
void MathTest::describeX(ObjectClass* clazz) {
  clazz->memberProperty(
      "direct_fvec2", //
      &MathTest::_fvec2);
  clazz->memberProperty(
      "direct_fvec3", //
      &MathTest::_fvec3);
  clazz->memberProperty(
      "direct_fvec4", //
      &MathTest::_fvec4);
  clazz->memberProperty(
      "direct_fquat", //
      &MathTest::_fquat);
  clazz->memberProperty(
      "direct_fmtx3", //
      &MathTest::_fmtx3);
  clazz->memberProperty(
      "direct_fmtx4", //
      &MathTest::_fmtx4);
}
///////////////////////////////////////////////////////////////////////////////
