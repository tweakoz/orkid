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

  mathtest->_fvec3.x = 4.0f;
  mathtest->_fvec3.y = 5.0f;
  mathtest->_fvec3.z = 6.0f;

  mathtest->_fvec4.x = 7.0f;
  mathtest->_fvec4.y = 8.0f;
  mathtest->_fvec4.z = 9.0f;
  mathtest->_fvec4.w = 10.0f;

  mathtest->_fquat.x = 11.0f;
  mathtest->_fquat.y = 12.0f;
  mathtest->_fquat.z = 13.0f;
  mathtest->_fquat.w = 14.0f;

  mathtest->_fmtx3.RotateX(PI * 0.5);
  mathtest->_fmtx4.RotateX(PI * 0.5);

  serdes::JsonSerializer ser;
  auto rootnode = ser.serializeRoot(mathtest);
  return ser.output();
}

TEST(SerdesMathProperties) {
  auto objstr = math_generate();
  // printf("objstr<%s>\n", objstr.c_str());
  object_ptr_t instance_out;
  serdes::JsonDeserializer deser(objstr.c_str());
  deser.deserializeTop(instance_out);
  auto clone = std::dynamic_pointer_cast<MathTest>(instance_out);
  //
  CHECK_EQUAL(clone->_fvec2.x, 2.0f);
  CHECK_EQUAL(clone->_fvec2.y, 3.0f);
  //
  CHECK_EQUAL(clone->_fvec3.x, 4.0f);
  CHECK_EQUAL(clone->_fvec3.y, 5.0f);
  CHECK_EQUAL(clone->_fvec3.z, 6.0f);
  //
  CHECK_EQUAL(clone->_fvec4.x, 7.0f);
  CHECK_EQUAL(clone->_fvec4.y, 8.0f);
  CHECK_EQUAL(clone->_fvec4.z, 9.0f);
  CHECK_EQUAL(clone->_fvec4.w, 10.0f);
  //
  CHECK_EQUAL(clone->_fquat.x, 11.0f);
  CHECK_EQUAL(clone->_fquat.y, 12.0f);
  CHECK_EQUAL(clone->_fquat.z, 13.0f);
  CHECK_EQUAL(clone->_fquat.w, 14.0f);
  //
  const float* mtx3e = clone->_fmtx3.GetArray();
  CHECK_EQUAL(mtx3e[0], 1.0f);
  CHECK_EQUAL(mtx3e[1], 0.0f);
  CHECK_EQUAL(mtx3e[2], 0.0f);
  CHECK_EQUAL(mtx3e[3], 0.0f);
  CHECK_EQUAL(mtx3e[4], -4.371138828673793e-8f);
  CHECK_EQUAL(mtx3e[5], 1.0f);
  CHECK_EQUAL(mtx3e[6], 0.0f);
  CHECK_EQUAL(mtx3e[7], -1.0f);
  CHECK_EQUAL(mtx3e[8], -4.371138828673793e-8f);
  //
  const float* mtx4e = clone->_fmtx4.GetArray();
  CHECK_EQUAL(mtx4e[0], 1.0f);
  CHECK_EQUAL(mtx4e[1], 0.0f);
  CHECK_EQUAL(mtx4e[2], 0.0f);
  CHECK_EQUAL(mtx4e[3], 0.0f);
  CHECK_EQUAL(mtx4e[4], 0.0f);
  CHECK_EQUAL(mtx4e[5], -4.371138828673793e-8f);
  CHECK_EQUAL(mtx4e[6], 1.0f);
  CHECK_EQUAL(mtx4e[7], 0.0f);
  CHECK_EQUAL(mtx4e[8], 0.0f);
  CHECK_EQUAL(mtx4e[9], -1.0f);
  CHECK_EQUAL(mtx4e[10], -4.371138828673793e-8f);
  CHECK_EQUAL(mtx4e[11], 0.0f);
  CHECK_EQUAL(mtx4e[12], 0.0f);
  CHECK_EQUAL(mtx4e[13], 0.0f);
  CHECK_EQUAL(mtx4e[14], 0.0f);
  CHECK_EQUAL(mtx4e[15], 1.0f);
}
