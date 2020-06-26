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

  ork::HotKeyConfiguration hkeys;
  hkeys.Default();

  auto resultdata = std::make_shared<ArrayString<65536>>();
  stream::StringOutputStream out_stream(*resultdata);
  serialize::JsonSerializer ser(out_stream);
  ICastable* pcastable = nullptr;
  bool serok           = ser.serializeObject(&hkeys);

  printf("mutstr<%s>\n", resultdata->c_str());
}

TEST(SerializeSharedObjectJSON) {
  auto hkeys = std::make_shared<ork::HotKeyConfiguration>();
  hkeys->Default();
  auto resultdata = std::make_shared<ArrayString<65536>>();
  stream::StringOutputStream out_stream(*resultdata);
  serialize::JsonSerializer ser(out_stream);
  bool serok = ser.serializeSharedObject(hkeys);
  printf("mutstr<%s>\n", resultdata->c_str());
}

std::string getJsonStr() {
  return R"xxx(
{
  reference: {
    id: "07000000-0000-0020-0000-000000000020",
    category: "ObjectClass",
    type: "HotKeyConfiguration",
    properties: {
      "HotKeys": {
        "one": {
          reference: {
            id: "0700001-0000-0020-0000-000000000020",
            category: "ObjectClass",
            type: "HotKey",
          }
        },
        "two": {
          reference: {
            id: "0700002-0000-0020-0000-000000000020",
            category: "ObjectClass",
            type: "HotKey",
          }
        },
      }
    }
  }
}
)xxx";
}

TEST(DeserializeObjectJSON) {

  auto objstr = getJsonStr();
  stream::StringInputStream inp_stream(objstr.c_str());
  serialize::JsonDeserializer deser(inp_stream);
  rtti::castable_rawptr_t pcastable = nullptr;
  bool serok                        = deser.deserializeObject(pcastable);
  CHECK(serok);
  auto as_hkc = dynamic_cast<HotKeyConfiguration*>(pcastable);
  auto save   = as_hkc->GetHotKey("save");
  CHECK_EQUAL(save->mbAlt, false);
  CHECK_EQUAL(save->mbCtrl, true);
  CHECK_EQUAL(save->miKeyCode, 83);

  std::string uuids = boost::uuids::to_string(save->_uuid);
  CHECK_EQUAL(uuids, "e0f43d05-0070-0000-d0f4-3d0500700000");
}
TEST(DeserializeSharedObjectJSON) {

  auto objstr = getJsonStr();
  stream::StringInputStream inp_stream(objstr.c_str());
  serialize::JsonDeserializer deser(inp_stream);
  rtti::castable_ptr_t pcastable = nullptr;
  bool serok                     = deser.deserializeSharedObject(pcastable);
  CHECK(serok);
  auto as_hkc = std::dynamic_pointer_cast<HotKeyConfiguration>(pcastable);
  auto save   = as_hkc->GetHotKey("save");
  CHECK_EQUAL(save->mbAlt, false);
  CHECK_EQUAL(save->mbCtrl, true);
  CHECK_EQUAL(save->miKeyCode, 83);

  std::string uuids = boost::uuids::to_string(save->_uuid);
  CHECK_EQUAL(uuids, "e0f43d05-0070-0000-d0f4-3d0500700000");
}
