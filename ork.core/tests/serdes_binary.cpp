#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <string.h>

#include <ork/reflect/serialize/BinaryDeserializer.h>
#include <ork/reflect/serialize/BinarySerializer.h>
#include <ork/stream/FileInputStream.h>
#include <ork/stream/FileOutputStream.h>
#include <ork/stream/StringInputStream.h>
#include <ork/stream/StringOutputStream.h>
#include <ork/kernel/string/ArrayString.h>
#include <ork/util/hotkey.h>
#include "reflectionclasses.inl"
#include <boost/uuid/uuid_io.hpp>
#include <ork/util/hexdump.inl>

using namespace ork;
using namespace ork::file;
using namespace ork::reflect;
using namespace ork::rtti;
using namespace ork::stream;

TEST(SerializeObjectBinary) {

  ork::HotKeyConfiguration hkeys;
  hkeys.Default();

  auto resultdata = std::make_shared<ArrayString<65536>>();
  stream::StringOutputStream out_stream(*resultdata);
  serialize::BinarySerializer ser(out_stream);
  ICastable* pcastable = nullptr;
  bool serok           = ser.serializeObject(&hkeys);
  printf("////////////////////////////////////////////////////////////////////////\n");
  printf("SerializeObjectBinary dump\n");
  hexdumpbytes((const uint8_t*)resultdata->data(), resultdata->length());
  printf("////////////////////////////////////////////////////////////////////////\n");
}

TEST(SerializeSharedObjectBinary) {
  auto hkeys = std::make_shared<ork::HotKeyConfiguration>();
  hkeys->Default();
  auto resultdata = std::make_shared<ArrayString<65536>>();
  stream::StringOutputStream out_stream(*resultdata);
  serialize::BinarySerializer ser(out_stream);
  bool serok = ser.serializeSharedObject(hkeys);
  printf("////////////////////////////////////////////////////////////////////////\n");
  printf("SerializeSharedObjectBinary dump\n");
  hexdumpbytes((const uint8_t*)resultdata->data(), resultdata->length());
  printf("////////////////////////////////////////////////////////////////////////\n");
}
