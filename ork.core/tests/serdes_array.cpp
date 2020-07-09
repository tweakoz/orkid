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
// test suite of array properties, direct, accessor and lambda
////////////////////////////////////////////////////////////////////////////////

std::string sap_generate() {
  auto arytest                  = std::make_shared<ArrayTest>();
  arytest->_directstdaryvect[0] = 1;
  arytest->_directstdaryvect[1] = 2;
  arytest->_directstdaryvect[2] = 3;
  arytest->_directstdaryvect[3] = 42;

  arytest->_directcaryvect[0] = 1;
  arytest->_directcaryvect[1] = 2;
  arytest->_directcaryvect[2] = 3;
  arytest->_directcaryvect[3] = 42;

  arytest->_directcaryobjvect[0] = std::make_shared<SimpleTest>("yo");

  serdes::JsonSerializer ser;
  auto rootnode = ser.serializeRoot(arytest);
  return ser.output();
}

TEST(SerdesArrayProperties) {
  auto objstr = sap_generate();
  // printf("objstr<%s>\n", objstr.c_str());
  object_ptr_t instance_out;
  serdes::JsonDeserializer deser(objstr.c_str());
  deser.deserializeTop(instance_out);
  auto typed = std::dynamic_pointer_cast<ArrayTest>(instance_out);

  auto nil = simpletest_ptr_t(nullptr);

  CHECK_EQUAL(typed->_directstdaryvect[0], 1);
  CHECK_EQUAL(typed->_directstdaryvect[1], 2);
  CHECK_EQUAL(typed->_directstdaryvect[2], 3);
  CHECK_EQUAL(typed->_directstdaryvect[3], 42);

  CHECK_EQUAL(typed->_directcaryvect[0], 1);
  CHECK_EQUAL(typed->_directcaryvect[1], 2);
  CHECK_EQUAL(typed->_directcaryvect[2], 3);
  CHECK_EQUAL(typed->_directcaryvect[3], 42);

  CHECK_EQUAL(typed->_directcaryobjvect[0]->_strvalue, "yo");
  CHECK_EQUAL(typed->_directcaryobjvect[1], nil);
  CHECK_EQUAL(typed->_directcaryobjvect[2], nil);
  CHECK_EQUAL(typed->_directcaryobjvect[3], nil);
}
