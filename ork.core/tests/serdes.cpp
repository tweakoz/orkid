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

using namespace ork;
using namespace ork::file;
using namespace ork::reflect;
using namespace ork::rtti;
using namespace ork::stream;

TEST(SerdesObject) {
  auto fname = Path();
  FileInputStream istream(fname.c_str());
  serialize::XMLDeserializer iser(istream);
  ICastable* pcastable = nullptr;
  bool bloadOK         = iser.deserializeObject(pcastable);
}

TEST(SerdesSharedObject) {
  auto fname = Path();
  FileInputStream istream(fname.c_str());
  serialize::XMLDeserializer iser(istream);
  castable_ptr_t pcastable;
  bool bloadOK = iser.deserializeObject(pcastable);
}
