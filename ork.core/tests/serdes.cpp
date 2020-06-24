#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <string.h>

#include <ork/reflect/serialize/XMLDeserializer.h>
#include <ork/reflect/serialize/XMLSerializer.h>
#include <ork/stream/FileInputStream.h>
#include <ork/stream/FileOutputStream.h>
#include <ork/stream/StringInputStream.h>
#include <ork/stream/StringOutputStream.h>
#include <ork/kernel/string/ArrayString.h>
#include <ork/util/hotkey.h>
using namespace ork;
using namespace ork::file;
using namespace ork::reflect;
using namespace ork::rtti;
using namespace ork::stream;

TEST(SerializeObject) {

  ork::HotKeyConfiguration hkeys;
  hkeys.Default();

  ArrayString<1024> resultdata;
  stream::StringOutputStream out_stream(resultdata);
  serialize::XMLSerializer ser(out_stream);
  ICastable* pcastable = nullptr;
  bool serok           = ser.serializeObject(&hkeys);

  printf("mutstr<%s>\n", resultdata.c_str());
}

TEST(SerializeSharedObject) {
  auto hkeys = std::make_shared<ork::HotKeyConfiguration>();
  hkeys->Default();
  ArrayString<1024> resultdata;
  stream::StringOutputStream out_stream(resultdata);
  serialize::XMLSerializer ser(out_stream);
  bool serok = ser.serializeSharedObject(hkeys);
  printf("mutstr<%s>\n", resultdata.c_str());
}

std::string getXmlStr() {
  return R"xxx(
  <reference category='ObjectClass'>
  <object type='HotKeyConfiguration' id='0'>
   <property name='HotKeys'>
     <item key='copy'></item>
     <item key='open'></item>
     <item key='paste'></item>
     <item key='save'></item>
   </property>
  </object>
  </reference>>)xxx";
}

TEST(DeserializeObject) {

  auto objstr = getXmlStr();
  stream::StringInputStream inp_stream(objstr.c_str());
  serialize::XMLDeserializer deser(inp_stream);
  rtti::castable_rawptr_t pcastable = nullptr;
  bool serok                        = deser.deserializeObject(pcastable);
  CHECK(serok);
}
TEST(DeserializeSharedObject) {

  auto objstr = getXmlStr();
  stream::StringInputStream inp_stream(objstr.c_str());
  serialize::XMLDeserializer deser(inp_stream);
  rtti::castable_ptr_t pcastable = nullptr;
  bool serok                     = deser.deserializeObject(pcastable);
  CHECK(serok);
}
