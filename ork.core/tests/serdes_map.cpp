////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

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
// test suite of map properties, direct, accessor and lambda
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

  maptest->_directstrobjmap["yo"]    = std::make_shared<SimpleTest>("one");
  maptest->_directstrobjmap["two"]   = std::make_shared<SimpleTest>("two");
  maptest->_directstrobjmap["three"] = std::make_shared<SimpleTest>("three");

  serdes::JsonSerializer ser;
  auto rootnode = ser.serializeRoot(maptest);
  return ser.output();
}

TEST(SerdesMapProperties) {
  auto objstr = smp_generate();
  printf("objstr<%s>\n", objstr.c_str());
  object_ptr_t instance_out;
  serdes::JsonDeserializer deser(objstr.c_str());
  deser.deserializeTop(instance_out);
  auto clone = objcast<MapTest>(instance_out);

  CHECK_EQUAL(clone->_directintstrmap[1], "one");
  CHECK_EQUAL(clone->_directintstrmap[2], "two");
  CHECK_EQUAL(clone->_directintstrmap[3], "three");
  CHECK_EQUAL(clone->_directintstrmap[42], "theanswer");

  CHECK_EQUAL(clone->_directstrintmap["one"], 1);
  CHECK_EQUAL(clone->_directstrintmap["two"], 2);
  CHECK_EQUAL(clone->_directstrintmap["three"], 3);
  CHECK_EQUAL(clone->_directstrintmap["theanswer"], 42);

  CHECK_EQUAL(clone->_directstrintumap["one"], 1);
  CHECK_EQUAL(clone->_directstrintumap["two"], 2);
  CHECK_EQUAL(clone->_directstrintumap["three"], 3);
  CHECK_EQUAL(clone->_directstrintumap["theanswer"], 42);

  CHECK_EQUAL(clone->_directstrintlut.find("one")->second, 1);
  CHECK_EQUAL(clone->_directstrintlut.find("two")->second, 2);
  CHECK_EQUAL(clone->_directstrintlut.find("three")->second, 3);
  CHECK_EQUAL(clone->_directstrintlut.find("theanswer")->second, 42);

  CHECK_EQUAL(clone->_directstrobjmap["yo"]->_strvalue, "one");
  CHECK_EQUAL(clone->_directstrobjmap["two"]->_strvalue, "two");
  CHECK_EQUAL(clone->_directstrobjmap["three"]->_strvalue, "three");
}
