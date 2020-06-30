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
using namespace ork::file;
using namespace ork::reflect;
using namespace ork::rtti;
using namespace ork::stream;

////////////////////////////////////////////////////////////////////////////////
// test suite of leaf node properties, direct, accessor and lambda
////////////////////////////////////////////////////////////////////////////////

TEST(SerializeLeafProperties) {
  ///////////////////////////////////////////
  // serialize an object
  ///////////////////////////////////////////
  auto rootobj           = std::make_shared<SharedTest>();
  auto child1            = std::make_shared<SharedTest>();
  auto child2            = std::make_shared<SharedTest>();
  rootobj->_directInt    = 0;
  rootobj->_directBool   = 0;
  rootobj->_directString = "yo-0";
  rootobj->_directFloat  = 0.01f;
  rootobj->_directDouble = 0.02;
  rootobj->_directUint32 = 5;
  rootobj->_directSizeT  = 6;
  child1->_directInt     = 1;
  child1->_directString  = "yo-1";
  child2->_directInt     = 2;
  child2->_directString  = "yo-2";

  rootobj->_directChild = child1;
  rootobj->setChild(child2);

  serdes::JsonSerializer ser;
  auto rootnode   = ser.serializeRoot(rootobj);
  auto resultdata = ser.output();
  printf("mutstr<%s>\n", resultdata.c_str());
}

////////////////////////////////////////////////////////////////////////////////
