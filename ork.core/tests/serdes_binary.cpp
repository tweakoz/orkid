#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <string.h>

//#include <ork/reflect/serialize/BinaryDeserializer.h>
//#include <ork/reflect/serialize/BinarySerializer.h>
#include <ork/stream/FileInputStream.h>
#include <ork/stream/FileOutputStream.h>
#include <ork/stream/StringInputStream.h>
#include <ork/stream/StringOutputStream.h>
#include <ork/kernel/string/ArrayString.h>
#include <ork/util/hotkey.h>
#include "reflectionclasses.inl"
#include <boost/uuid/uuid_io.hpp>
#include <ork/util/hexdump.inl>

///////////////////////////////////////////////////////////////////////////////

using namespace ork;
using namespace ork::file;
using namespace ork::reflect;
using namespace ork::rtti;
using namespace ork::stream;
using storage_type = ArrayString<65536>;
/*
///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<storage_type> hkeyToStorage() {
  ork::HotKeyConfiguration hkeys;
  hkeys.Default();
  auto resultdata = std::make_shared<storage_type>();
  stream::StringOutputStream out_stream(*resultdata);
  serialize::BinarySerializer ser(out_stream);
  bool serok = ser.serializeObject(&hkeys);
  OrkAssert(serok);
  return resultdata;
}

///////////////////////////////////////////////////////////////////////////////

TEST(SerializeObjectBinary) {
  auto resultdata = hkeyToStorage();
  printf("////////////////////////////////////////////////////////////////////////\n");
  printf("SerializeObjectBinary dump\n");
  hexdumpbytes((const uint8_t*)resultdata->data(), resultdata->length());
  printf("////////////////////////////////////////////////////////////////////////\n");
}

///////////////////////////////////////////////////////////////////////////////

TEST(SerializeSharedObjectBinary) {
  auto hkeys = std::make_shared<ork::HotKeyConfiguration>();
  hkeys->Default();
  auto resultdata = std::make_shared<storage_type>();
  stream::StringOutputStream out_stream(*resultdata);
  serialize::BinarySerializer ser(out_stream);
  bool serok = ser.serializeSharedObject(hkeys);
  printf("////////////////////////////////////////////////////////////////////////\n");
  printf("SerializeSharedObjectBinary dump\n");
  hexdumpbytes((const uint8_t*)resultdata->data(), resultdata->length());
  printf("////////////////////////////////////////////////////////////////////////\n");
}

///////////////////////////////////////////////////////////////////////////////

TEST(DeserializeObjectBinary) {
  auto resultdata = hkeyToStorage();
  auto pstr       = PieceString(resultdata->data(), resultdata->length());
  stream::StringInputStream inp_stream(pstr);
  serialize::BinaryDeserializer deser(inp_stream);
  rtti::castable_rawptr_t pcastable = nullptr;
  bool serok                        = deser.deserializeObject(pcastable);
  CHECK(serok);
}
*/
///////////////////////////////////////////////////////////////////////////////
