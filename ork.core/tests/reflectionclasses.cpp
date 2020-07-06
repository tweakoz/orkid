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
#include <ork/asset/DynamicAssetLoader.h>

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
void AssetTest::describeX(ObjectClass* clazz) {
  ///////////////////////////////////
  clazz->memberProperty(
      "asset", //
      &AssetTest::_assetptr);
  ///////////////////////////////////
  auto dyn_loader = std::make_shared<asset::DynamicAssetLoader>();

  dyn_loader->mEnumFn = [=]() -> asset::set_t {
    asset::set_t rval;
    rval.insert("dyn://yo");
    return rval;
  };
  dyn_loader->mCheckFn = [=](const AssetPath& path) { //
    return ork::IsSubStringPresent("dyn://", path.c_str());
  };
  dyn_loader->mLoadFn = [=](asset::asset_ptr_t asset) {
    auto asset_name = asset->name().c_str();
    printf("DynamicAssetLoader test name<%s> ptr<%p>\n", asset_name, asset.get());
    return true;
  };

  asset::registerLoader<asset::Asset>(dyn_loader);
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
