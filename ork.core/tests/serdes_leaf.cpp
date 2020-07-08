#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <string.h>

#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include "reflectionclasses.inl"

using namespace ork;
using namespace ork::reflect;

////////////////////////////////////////////////////////////////////////////////
// test suite of leaf node properties, direct, accessor and lambda
////////////////////////////////////////////////////////////////////////////////

std::string slp_generate() {
  ///////////////////////////////////////////
  // serialize an object
  ///////////////////////////////////////////
  auto rootobj           = std::make_shared<SharedTest>();
  auto child1            = std::make_shared<SimpleTest>();
  auto child2            = std::make_shared<SharedTest>();
  rootobj->_directInt    = 0;
  rootobj->_directBool   = true;
  rootobj->_directString = "yo-0";
  rootobj->_directFloat  = 0.01f;
  rootobj->_directDouble = 0.02;
  rootobj->_directUint32 = 5;
  rootobj->_directSizeT  = 6;
  child1->_strvalue      = "aaa";
  child2->_directInt     = 2;
  child2->_directString  = "yo-2";

  rootobj->_directChild = child1;
  rootobj->setChild(child2);

  serdes::JsonSerializer ser;
  auto rootnode = ser.serializeRoot(rootobj);
  return ser.output();
}

TEST(SerdesLeafProperties) {
  auto serstr = slp_generate();
  // printf("mutstr<%s>\n", serstr.c_str());
  object_ptr_t instance_out;
  serdes::JsonDeserializer deser(serstr.c_str());
  deser.deserializeTop(instance_out);
  auto clone = objcast<SharedTest>(instance_out);
  CHECK_EQUAL(clone->_directInt, 0);
  CHECK_EQUAL(clone->_directBool, true);
  CHECK_EQUAL(clone->_directString, "yo-0");
  CHECK_EQUAL(clone->_directFloat, 0.01f);
  CHECK_EQUAL(clone->_directDouble, 0.02);
  CHECK_EQUAL(clone->_directUint32, 5);
  CHECK_EQUAL(clone->_directSizeT, 6);
  auto child1 = clone->_directChild;
  auto child2 = objcast<SharedTest>(clone->_accessorChild);
  CHECK_EQUAL(child1->_strvalue, "aaa");
  CHECK_EQUAL(child2->_directInt, 2);
  CHECK_EQUAL(child2->_directString, "yo-2");
}
