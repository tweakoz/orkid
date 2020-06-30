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

TEST(SerializeLeafProperties) {
  ///////////////////////////////////////////
  // serialize an object
  ///////////////////////////////////////////
  auto rootobj           = std::make_shared<SharedTest>();
  auto child1            = std::make_shared<SharedTest>();
  auto child2            = std::make_shared<SharedTest>();
  rootobj->_directInt    = 0;
  rootobj->_directBool   = true;
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

std::string getJsonStr_SLP() {
  return R"xxx(
  {
 "root": {
  "object": {
   "class": "SharedTest",
   "uuid": "31d8a600-7146-4b66-b546-2d3f6c9f3498",
   "properties": {
    "bool_direct": true,
    "double_direct": 0.02,
    "float_direct": 0.009999999776482582,
    "int_direct": 0,
    "sharedobj_accessor": {
     "object": {
      "class": "SharedTest",
      "uuid": "4d6f0ec2-b8f3-4f6c-a4f2-e8963b0d2283",
      "properties": {
       "bool_direct": false,
       "double_direct": 0.0,
       "float_direct": 0.0,
       "int_direct": 2,
       "sharedobj_accessor": "nil",
       "sharedobj_direct": "nil",
       "sizet_direct": 0,
       "string_direct": "yo-2",
       "uint32_direct": 0
      }
     }
    },
    "sharedobj_direct": {
     "object": {
      "class": "SharedTest",
      "uuid": "f0be81ef-d0de-4e7e-a721-490f2e9f37b0",
      "properties": {
       "bool_direct": false,
       "double_direct": 0.0,
       "float_direct": 0.0,
       "int_direct": 1,
       "sharedobj_accessor": "nil",
       "sharedobj_direct": "nil",
       "sizet_direct": 0,
       "string_direct": "yo-1",
       "uint32_direct": 0
      }
     }
    },
    "sizet_direct": 6,
    "string_direct": "yo-0",
    "uint32_direct": 5
   }
  }
 }
}
)xxx";
}

////////////////////////////////////////////////////////////////////////////////
// test suite of leaf node properties, direct, accessor and lambda
////////////////////////////////////////////////////////////////////////////////

TEST(DeserializeLeafProperties) {
  auto objstr = getJsonStr_SLP();
  object_ptr_t instance_out;
  serdes::JsonDeserializer deser(objstr.c_str());
  deser.deserializeTop(instance_out);
  auto typed = std::dynamic_pointer_cast<SharedTest>(instance_out);
}
