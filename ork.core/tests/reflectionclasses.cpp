////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

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

#include <ork/reflect/IDeserializer.inl>

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
ImplementReflectionX(VectorTest, "VectorTest");
ImplementReflectionX(ArrayTest, "ArrayTest");
ImplementReflectionX(InterfaceTest, "InterfaceTest");
ImplementReflectionX(TheTestInterface, "TheTestInterface");
///////////////////////////////////////////////////////////////////////////////
void AssetTest::describeX(ObjectClass* clazz) {
  ///////////////////////////////////
  clazz->directProperty(
      "asset", //
      &AssetTest::_assetptr);
  ///////////////////////////////////
  auto dyn_loader = std::make_shared<asset::DynamicAssetLoader>();

  dyn_loader->_enumFn = [=]() -> asset::set_t {
    asset::set_t rval;
    rval.insert("dyn://yo");
    return rval;
  };
  dyn_loader->_checkFn = [=](const AssetPath& path) { //
    return ork::IsSubStringPresent("dyn://", path.c_str());
  };
  dyn_loader->_loadFn = [=](asset::loadrequest_ptr_t loadreq) -> asset::asset_ptr_t {
    AssetPath assetpath = loadreq->_asset_path;
    printf("DynamicAssetLoader test name<%s>\n", assetpath.c_str());
    auto instance = asset::Asset::objectClassStatic()->createShared();
    asset::asset_ptr_t rval = objcast<asset::Asset>(instance);
    return rval;
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
  clazz->directProperty(
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
  clazz->directProperty(
      "int_direct", //
      &SharedTest::_directInt);
  ///////////////////////////////////
  clazz->directProperty(
      "uint32_direct", //
      &SharedTest::_directUint32);
  ///////////////////////////////////
  clazz->directProperty(
      "sizet_direct", //
      &SharedTest::_directSizeT);
  ///////////////////////////////////
  clazz->directProperty(
      "bool_direct", //
      &SharedTest::_directBool);
  ///////////////////////////////////
  clazz->directProperty(
      "float_direct", //
      &SharedTest::_directFloat);
  ///////////////////////////////////
  clazz->directProperty(
      "double_direct", //
      &SharedTest::_directDouble);
  ///////////////////////////////////
  clazz->directProperty(
      "string_direct", //
      &SharedTest::_directString);
  ///////////////////////////////////
  clazz->directObjectProperty(
      "sharedobj_direct", //
      &SharedTest::_directChild);
  ///////////////////////////////////
  clazz->accessorProperty(
      "sharedobj_accessor", //
      &SharedTest::getChild,
      &SharedTest::setChild);
  ///////////////////////////////////
  clazz->lambdaProperty<SharedTest, int>(
      "int_lambda", //
      [](const SharedTest* obj_inp, int& valout) { valout = obj_inp->_lambdaint; },
      [](SharedTest* obj_out, const int& valinp) { obj_out->_lambdaint = valinp; });
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
  ///////////////////////////////////
  clazz->directObjectMapProperty(
      "directstrobj_map", //
      &MapTest::_directstrobjmap);
  ///////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
VectorTest::VectorTest() {
}
///////////////////////////////////////////////////////////////////////////////
void VectorTest::describeX(ObjectClass* clazz) {
  ///////////////////////////////////
  clazz->directVectorProperty(
      "directint_vect", //
      &VectorTest::_directintvect);
  ///////////////////////////////////
  clazz->directVectorProperty(
      "directstr_vect", //
      &VectorTest::_directstrvect);
  ///////////////////////////////////
  clazz->directVectorProperty(
      "directobj_vect", //
      &VectorTest::_directobjvect);
}
///////////////////////////////////////////////////////////////////////////////
ArrayTest::ArrayTest() {
}
///////////////////////////////////////////////////////////////////////////////
void ArrayTest::describeX(ObjectClass* clazz) {
  ///////////////////////////////////
  clazz->directArrayProperty(
      "directstdaryvect", //
      &ArrayTest::_directstdaryvect);
  ///////////////////////////////////
  clazz->directArrayProperty(
      "directcaryvect", //
      &ArrayTest::_directcaryvect);
  ///////////////////////////////////
  clazz->directArrayProperty(
      "directcaryobjvect", //
      &ArrayTest::_directcaryobjvect);
}
///////////////////////////////////////////////////////////////////////////////
EnumTest::EnumTest() {
}
///////////////////////////////////////////////////////////////////////////////
void EnumTest::describeX(ObjectClass* clazz) {
  clazz->directProperty(
      "enum_direct", //
      &EnumTest::_mcst);
}
///////////////////////////////////////////////////////////////////////////////
MathTest::MathTest() {
}
///////////////////////////////////////////////////////////////////////////////
void MathTest::describeX(ObjectClass* clazz) {
  clazz->directProperty(
      "direct_fvec2", //
      &MathTest::_fvec2);
  clazz->directProperty(
      "direct_fvec3", //
      &MathTest::_fvec3);
  clazz->directProperty(
      "direct_fvec4", //
      &MathTest::_fvec4);
  clazz->directProperty(
      "direct_fquat", //
      &MathTest::_fquat);
  clazz->directProperty(
      "direct_fmtx3", //
      &MathTest::_fmtx3);
  clazz->directProperty(
      "direct_fmtx4", //
      &MathTest::_fmtx4);
}
///////////////////////////////////////////////////////////////////////////////
InterfaceTest::InterfaceTest(){

}
///////////////////////////////////////////////////////////////////////////////
void InterfaceTest::describeX(class_t* clazz) {
    using factory_t = std::function<std::shared_ptr<TheTestInterface>()>;
    clazz->annotateTyped<factory_t>("TheTestInterface", [] -> std::shared_ptr<TheTestInterface> {
        auto instance = TheTestInterface::objectClassStatic()->createShared();
        return objcast<TheTestInterface>(instance);
    });

}
///////////////////////////////////////////////////////////////////////////////
void TheTestInterface::describeX(class_t* clazz) {
}

