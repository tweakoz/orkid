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
#include <ork/util/hotkey.h>
#include "reflectionclasses.inl"
#include <boost/uuid/uuid_io.hpp>

using namespace ork;
using namespace ork::file;
using namespace ork::reflect;
using namespace ork::rtti;
using namespace ork::stream;

TEST(SerializeObjectJSON) {
  auto hkeys = std::make_shared<ork::HotKeyConfiguration>();
  hkeys->Default();
  serialize::JsonSerializer ser;
  auto topnode    = ser.serializeTop(hkeys);
  auto resultdata = ser.output();
  printf("mutstr<%s>\n", resultdata.c_str());
}

std::string getJsonStr() {
  return R"xxx(
    {
     "top": {
      "object": {
       "class": "HotKeyConfiguration",
       "uuid": "e0f43d05-0070-0000-d0f4-3d0500700000",
       "properties": {
        "HotKeys": {
          "save": {
            "object": {
             "class": "HotKey",
             "uuid": "ffffffff-0123-4567-89ab-cdef01234567",
             "properties": {
               "Alt": true,
               "Ctrl": false,
               "KeyCode": 10
             }
           }
          }
        },
        "DropProp": {"drop":"me"}
       }
      }
     }
    }
)xxx"; //
}

TEST(DeserializeObjectJSON) {

  auto objstr = getJsonStr();
  object_ptr_t instance_out;
  serialize::JsonDeserializer deser(objstr.c_str());
  deser.deserializeTop(instance_out);
  auto as_hkc = std::dynamic_pointer_cast<HotKeyConfiguration>(instance_out);
  auto save   = as_hkc->GetHotKey("save");
  printf("save<%p>\n", save);
  CHECK_EQUAL(save->mbAlt, true);
  CHECK_EQUAL(save->mbCtrl, false);
  CHECK_EQUAL(save->miKeyCode, 10);

  CHECK_EQUAL(boost::uuids::to_string(as_hkc->_uuid), "e0f43d05-0070-0000-d0f4-3d0500700000");
  CHECK_EQUAL(boost::uuids::to_string(save->_uuid), "ffffffff-0123-4567-89ab-cdef01234567");
}
