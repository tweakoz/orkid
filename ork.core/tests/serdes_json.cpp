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
  auto resultdata = std::make_shared<ArrayString<65536>>();
  stream::StringOutputStream out_stream(*resultdata);
  serialize::JsonSerializer ser(out_stream);
  ser.serializeSharedObject(hkeys);
  ser.finalize();
  printf("mutstr<%s>\n", resultdata->c_str());
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
          "a": {
            "object": {
             "class": "HotKey",
             "uuid": "ffffffff-0070-0000-d0f4-3d0500700000",
             "properties": {}
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
  // CHECK_EQUAL(save->mbAlt, false);
  // CHECK_EQUAL(save->mbCtrl, true);
  // CHECK_EQUAL(save->miKeyCode, 83);

  std::string uuids = boost::uuids::to_string(as_hkc->_uuid);
  CHECK_EQUAL(uuids, "e0f43d05-0070-0000-d0f4-3d0500700000");
}
