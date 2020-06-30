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

std::string smp_generate() {
  auto maptest                  = std::make_shared<MapTest>();
  maptest->_directintstrmap[1]  = "one";
  maptest->_directintstrmap[2]  = "two";
  maptest->_directintstrmap[3]  = "three";
  maptest->_directintstrmap[42] = "theanswer";

  maptest->_directstrintmap["one"]       = 1;
  maptest->_directstrintmap["two"]       = 2;
  maptest->_directstrintmap["three"]     = 3;
  maptest->_directstrintmap["theanswer"] = 42;

  maptest->_directintstrumap[1]  = "one";
  maptest->_directintstrumap[2]  = "two";
  maptest->_directintstrumap[3]  = "three";
  maptest->_directintstrumap[42] = "theanswer";

  maptest->_directstrintumap["one"]       = 1;
  maptest->_directstrintumap["two"]       = 2;
  maptest->_directstrintumap["three"]     = 3;
  maptest->_directstrintumap["theanswer"] = 42;

  maptest->_directstrintlut.AddSorted("one", 1);
  maptest->_directstrintlut.AddSorted("two", 2);
  maptest->_directstrintlut.AddSorted("three", 3);
  maptest->_directstrintlut.AddSorted("theanswer", 42);

  serdes::JsonSerializer ser;
  auto rootnode = ser.serializeRoot(maptest);
  return ser.output();
}

TEST(SerializeMapProperties) {
  auto str = smp_generate();
  printf("mutstr<%s>\n", str.c_str());
}

TEST(DeserializeMapProperties) {
  auto objstr = smp_generate();
  object_ptr_t instance_out;
  serdes::JsonDeserializer deser(objstr.c_str());
  deser.deserializeTop(instance_out);
  auto typed = std::dynamic_pointer_cast<MapTest>(instance_out);
}
