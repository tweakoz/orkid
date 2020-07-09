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

std::string svp_generate() {
  auto arytest = std::make_shared<VectorTest>();
  arytest->_directintvect.push_back(1);
  arytest->_directintvect.push_back(2);
  arytest->_directintvect.push_back(3);
  arytest->_directintvect.push_back(42);

  arytest->_directstrvect.push_back("one");
  arytest->_directstrvect.push_back("two");
  arytest->_directstrvect.push_back("three");
  arytest->_directstrvect.push_back("four");

  arytest->_directobjvect.push_back(std::make_shared<SimpleTest>("one"));
  arytest->_directobjvect.push_back(std::make_shared<SimpleTest>("two"));
  arytest->_directobjvect.push_back(std::make_shared<SimpleTest>("three"));

  serdes::JsonSerializer ser;
  auto rootnode = ser.serializeRoot(arytest);
  return ser.output();
}

TEST(SerdesVectorProperties) {
  auto objstr = svp_generate();
  // printf("objstr<%s>\n", objstr.c_str());
  object_ptr_t instance_out;
  serdes::JsonDeserializer deser(objstr.c_str());
  deser.deserializeTop(instance_out);
  auto typed = std::dynamic_pointer_cast<VectorTest>(instance_out);

  CHECK_EQUAL(typed->_directintvect[0], 1);
  CHECK_EQUAL(typed->_directintvect[1], 2);
  CHECK_EQUAL(typed->_directintvect[2], 3);
  CHECK_EQUAL(typed->_directintvect[3], 42);

  CHECK_EQUAL(typed->_directstrvect[0], "one");
  CHECK_EQUAL(typed->_directstrvect[1], "two");
  CHECK_EQUAL(typed->_directstrvect[2], "three");
  CHECK_EQUAL(typed->_directstrvect[3], "four");

  CHECK_EQUAL(typed->_directobjvect[0]->_strvalue, "one");
  CHECK_EQUAL(typed->_directobjvect[1]->_strvalue, "two");
  CHECK_EQUAL(typed->_directobjvect[2]->_strvalue, "three");
}
