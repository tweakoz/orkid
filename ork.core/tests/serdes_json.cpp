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
#include <ork/util/hotkey.h>
#include "reflectionclasses.inl"
#include <boost/uuid/uuid_io.hpp>

using namespace ork;
using namespace ork::reflect;

TEST(SerdesJson) {
  ///////////////////////////////////////////
  // serialize an object
  ///////////////////////////////////////////
  auto hkeys = std::make_shared<ork::HotKeyConfiguration>();
  hkeys->Default();
  serdes::JsonSerializer ser;
  auto topnode    = ser.serializeRoot(hkeys);
  auto resultdata = ser.output();
  // printf("mutstr<%s>\n", resultdata.c_str());
}

std::string getJsonStr() {
  return R"xxx(
    {
     "root": {
      "object": {
       "class": "HotKeyConfiguration",
       "uuid": "6c499e0f-212d-465a-b88b-60b8cc6928ab",
       "properties": {
        "DropProp": {"drop":"me"},
        "HotKeys": {
         "copy": {
          "object": {
           "class": "HotKey",
           "uuid": "2392ad12-d0c2-44fa-9395-9b94f5d8ff61",
           "properties": {
            "DropProp": {"drop":"me"},
            "Alt": false,
            "Ctrl": true,
            "KeyCode": 67,
            "LMB": false,
            "MMB": false,
            "RMB": false,
            "Shift": false
           }
          }
         },
         "open": {
          "object": {
           "class": "HotKey",
           "uuid": "a232d953-f883-4174-a7bf-d3672f19058a",
           "properties": {
            "DropProp2": {"dropme":"too"},
            "Alt": false,
            "Ctrl": true,
            "KeyCode": 79,
            "LMB": false,
            "MMB": false,
            "RMB": false,
            "Shift": false
           }
          }
         },
         "paste": {
          "object": {
           "class": "HotKey",
           "uuid": "c81b8de4-ba76-4953-a214-7c73b31f89c2",
           "properties": {
            "Alt": false,
            "Ctrl": true,
            "KeyCode": 86,
            "LMB": false,
            "MMB": false,
            "RMB": false,
            "Shift": false
           }
          }
         },
         "save": {
          "object": {
           "class": "HotKey",
           "uuid": "9b82a7c8-dbb6-4475-8490-9ae212fc5b61",
           "properties": {
            "Alt": false,
            "Ctrl": true,
            "KeyCode": 83,
            "LMB": false,
            "MMB": false,
            "RMB": false,
            "Shift": false
           }
          }
         }
        }
       }
      }
     }
    })xxx"; //
}

TEST(DeserializeObjectJSON) {
  ///////////////////////////////////////////
  // deserialize an object
  ///////////////////////////////////////////

  auto objstr = getJsonStr();
  object_ptr_t instance_out;
  serdes::JsonDeserializer deser(objstr.c_str());
  deser.deserializeTop(instance_out);
  auto as_hkc = std::dynamic_pointer_cast<HotKeyConfiguration>(instance_out);
  auto save   = as_hkc->GetHotKey("save");
  // printf("save<%p>\n", save);

  ///////////////////////////////////////////
  // check that properties were correctly deserialized
  ///////////////////////////////////////////

  CHECK_EQUAL(save->mbAlt, false);
  CHECK_EQUAL(save->mbCtrl, true);
  CHECK_EQUAL(save->miKeyCode, 83);

  ///////////////////////////////////////////
  // check that UUID's were correctly deserialized
  ///////////////////////////////////////////

  CHECK_EQUAL(boost::uuids::to_string(as_hkc->_uuid), "6c499e0f-212d-465a-b88b-60b8cc6928ab");
  CHECK_EQUAL(boost::uuids::to_string(save->_uuid), "9b82a7c8-dbb6-4475-8490-9ae212fc5b61");
}
