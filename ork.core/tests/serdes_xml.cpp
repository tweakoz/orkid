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
#include "reflectionclasses.inl"
#include <boost/uuid/uuid_io.hpp>

using namespace ork;
using namespace ork::file;
using namespace ork::reflect;
using namespace ork::rtti;
using namespace ork::stream;

TEST(SerializeObjectXML) {

  ork::HotKeyConfiguration hkeys;
  hkeys.Default();

  auto resultdata = std::make_shared<ArrayString<65536>>();
  stream::StringOutputStream out_stream(*resultdata);
  serialize::XMLSerializer ser(out_stream);
  ICastable* pcastable = nullptr;
  bool serok           = ser.serializeObject(&hkeys);

  printf("mutstr<%s>\n", resultdata->c_str());
}

TEST(SerializeSharedObjectXML) {
  auto hkeys = std::make_shared<ork::HotKeyConfiguration>();
  hkeys->Default();
  auto resultdata = std::make_shared<ArrayString<65536>>();
  stream::StringOutputStream out_stream(*resultdata);
  serialize::XMLSerializer ser(out_stream);
  bool serok = ser.serializeSharedObject(hkeys);
  printf("mutstr<%s>\n", resultdata->c_str());
}

std::string getXmlStr() {
  return R"xxx(
  <reference category='ObjectClass'>
   <object type='HotKeyConfiguration' id='07000000-0000-0020-0000-000000000020'>
    <property name='HotKeys'>
     <item key='copy'>
      <reference category='ObjectClass'>
       <object type='HotKey' id='0000e08b-c67f-0000-20f5-3d0500700000'>
        <property name='Alt'>false</property>
        <property name='Ctrl'>true</property>
        <property name='KeyCode'>67</property>
        <property name='LMB'>false</property>
        <property name='MMB'>false</property>
        <property name='RMB'>false</property>
        <property name='Shift'>false</property>
       </object>
      </reference>
     </item>
     <item key='open'>
      <reference category='ObjectClass'>
       <object type='HotKey' id='30f63d05-0070-0000-30f6-3d0500700000'>
        <property name='Alt'>false</property>
        <property name='Ctrl'>true</property>
        <property name='KeyCode'>79</property>
        <property name='LMB'>false</property>
        <property name='MMB'>false</property>
        <property name='RMB'>false</property>
        <property name='Shift'>false</property>
       </object>
      </reference>
     </item>
     <item key='paste'>
      <reference category='ObjectClass'>
       <object type='HotKey' id='f8f43d05-0070-0000-0100-000000000000'>
        <property name='Alt'>false</property>
        <property name='Ctrl'>true</property>
        <property name='KeyCode'>86</property>
        <property name='LMB'>false</property>
        <property name='MMB'>false</property>
        <property name='RMB'>false</property>
        <property name='Shift'>false</property>
       </object>
      </reference>
     </item>
     <item key='save'>
      <reference category='ObjectClass'>
       <object type='HotKey' id='e0f43d05-0070-0000-d0f4-3d0500700000'>
        <property name='Alt'>false</property>
        <property name='Ctrl'>true</property>
        <property name='KeyCode'>83</property>
        <property name='LMB'>false</property>
        <property name='MMB'>false</property>
        <property name='RMB'>false</property>
        <property name='Shift'>false</property>
       </object>
      </reference>
     </item>
    </property>
   </object>
  </reference>
)xxx";
}

TEST(DeserializeObjectXML) {

  auto objstr = getXmlStr();
  stream::StringInputStream inp_stream(objstr.c_str());
  serialize::XMLDeserializer deser(inp_stream);
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
TEST(DeserializeSharedObjectXML) {

  auto objstr = getXmlStr();
  stream::StringInputStream inp_stream(objstr.c_str());
  serialize::XMLDeserializer deser(inp_stream);
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
