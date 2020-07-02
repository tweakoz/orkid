#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <string.h>

#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include "reflectionclasses.inl"

using namespace ork;
using namespace ork::reflect;

std::string math_generate() {
  auto mathtest = std::make_shared<MathTest>();

  mathtest->_fvec2.x = 2.0f;
  mathtest->_fvec2.y = 3.0f;

  serdes::JsonSerializer ser;
  auto rootnode = ser.serializeRoot(mathtest);
  return ser.output();
}

TEST(SerdesMathProperties) {
  auto objstr = math_generate();
  printf("objstr<%s>\n", objstr.c_str());
  object_ptr_t instance_out;
  serdes::JsonDeserializer deser(objstr.c_str());
  deser.deserializeTop(instance_out);
  auto clone = std::dynamic_pointer_cast<MathTest>(instance_out);
  CHECK_EQUAL(clone->_fvec2.x, 2.0f);
  CHECK_EQUAL(clone->_fvec2.y, 3.0f);
}
