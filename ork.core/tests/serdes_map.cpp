#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <string.h>

#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include <ork/stream/FileInputStream.h>
#include <ork/stream/FileOutputStream.h>
#include <ork/stream/StringInputStream.h>
#include <ork/stream/StringOutputStream.h>
#include <ork/kernel/string/ArrayString.h>
#include "reflectionclasses.inl"

using namespace ork;
using namespace ork::reflect;

////////////////////////////////////////////////////////////////////////////////
// test suite of leaf node properties, direct, accessor and lambda
////////////////////////////////////////////////////////////////////////////////

TEST(SerializeMapProperties) {
}

TEST(DeserializeMapProperties) {
}
