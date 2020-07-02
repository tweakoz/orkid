#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <string.h>

#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include "reflectionclasses.inl"

using namespace ork;
using namespace ork::reflect;

std::string asset_generate() {
  auto enutest = std::make_shared<AssetTest>();
  serdes::JsonSerializer ser;
  auto rootnode = ser.serializeRoot(enutest);
  return ser.output();
}

TEST(SerdesAssetProperties) {
  auto objstr = asset_generate();
  printf("objstr<%s>\n", objstr.c_str());
  object_ptr_t instance_out;
  serdes::JsonDeserializer deser(objstr.c_str());
  deser.deserializeTop(instance_out);
  auto clone = std::dynamic_pointer_cast<AssetTest>(instance_out);
}
