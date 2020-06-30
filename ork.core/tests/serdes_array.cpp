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
  auto arytest = std::make_shared<ArrayTest>();
  arytest->_directintvect.push_back(1);
  arytest->_directintvect.push_back(2);
  arytest->_directintvect.push_back(3);
  arytest->_directintvect.push_back(42);

  serdes::JsonSerializer ser;
  auto rootnode = ser.serializeRoot(arytest);
  return ser.output();
}

TEST(SerializeArrayProperties) {
  auto str = sap_generate();
  printf("mutstr<%s>\n", str.c_str());
}

TEST(DeserializeArrayProperties) {
  auto objstr = sap_generate();
  object_ptr_t instance_out;
  serdes::JsonDeserializer deser(objstr.c_str());
  deser.deserializeTop(instance_out);
  auto typed = std::dynamic_pointer_cast<ArrayTest>(instance_out);
}
