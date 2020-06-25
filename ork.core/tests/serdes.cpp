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
using namespace ork;
using namespace ork::file;
using namespace ork::reflect;
using namespace ork::rtti;
using namespace ork::stream;

TEST(SerializeObject) {

  ork::HotKeyConfiguration hkeys;
  hkeys.Default();

  auto resultdata = std::make_shared<ArrayString<65536>>();
  stream::StringOutputStream out_stream(*resultdata);
  serialize::XMLSerializer ser(out_stream);
  ICastable* pcastable = nullptr;
  bool serok           = ser.serializeObject(&hkeys);

  printf("mutstr<%s>\n", resultdata->c_str());
}

TEST(SerializeSharedObject) {
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
 <object type='HotKeyConfiguration' id='0'>
  <property name='HotKeys'>
   <item key='copy'>
    <reference category='ObjectClass'>
     <object type='HotKey' id='1'>
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
     <object type='HotKey' id='2'>
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
     <object type='HotKey' id='3'>
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
     <object type='HotKey' id='4'>
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

TEST(DeserializeObject) {

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
}
TEST(DeserializeSharedObject) {

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
}
