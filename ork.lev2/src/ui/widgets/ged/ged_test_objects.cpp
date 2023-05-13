//
////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/orklut.hpp>
#include <ork/util/Context.hpp>
#include <ork/kernel/thread.h>
#include <ork/application/application.h>
#include <ork/file/path.h>
#include <ork/file/fileenv.h>
#include <ork/stream/FileInputStream.h>
#include <ork/stream/FileOutputStream.h>
#include <ork/stream/StringInputStream.h>
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/ui/ged/ged_test_objects.h>

ImplementReflectionX(ork::lev2::ged::TestObject, "GedTestObject");
ImplementReflectionX(ork::lev2::ged::TestObjectConfiguration, "GedTestObjectConfiguration");

namespace ork{
template class orklut<std::string,ork::lev2::ged::testobject_ptr_t>;
}
namespace ork::lev2::ged {


void TestObject::describeX(class_t* clazz) {
  clazz->floatProperty("float_prop", float_range{-20,20}, &TestObject::float_prop);
  clazz->intProperty("int_prop", int_range{0,127}, &TestObject::int_prop);
  clazz->directProperty("bool_prop", &TestObject::bool_prop);
  clazz //
      ->directObjectMapProperty("Curves", &TestObject::_curves) //
      ->annotate<ConstString>("editor.factorylistbase", "MultiCurve1D");
  clazz //
      ->directObjectMapProperty("Gradients", &TestObject::_gradients) //
      ->annotate<ConstString>("editor.factorylistbase", "GradientV4");
}
void TestObjectConfiguration::describeX(class_t* clazz) {
  clazz //
      ->directObjectMapProperty("TestObjects", &TestObjectConfiguration::_testobjects) //
      ->annotate<ConstString>("editor.factorylistbase", "GedTestObject");
}

} // namespace ork::lev2::ged {
