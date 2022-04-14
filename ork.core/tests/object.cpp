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

TEST(ObjectClone) {
  ///////////////////////////////////////////
  // create an object that we can clone from
  ///////////////////////////////////////////
  auto original = std::make_shared<ork::HotKeyConfiguration>();
  original->Default();
  auto orig_save = original->GetHotKey("save");
  printf("orig_save<%p>\n", (void*) orig_save);

  CHECK_EQUAL(orig_save->mbAlt, false);
  CHECK_EQUAL(orig_save->mbCtrl, true);
  CHECK_EQUAL(orig_save->miKeyCode, 83);

  auto orig_UUID      = boost::uuids::to_string(original->_uuid);
  auto orig_save_UUID = boost::uuids::to_string(orig_save->_uuid);

  ///////////////////////////////////////////
  // clone the object
  ///////////////////////////////////////////

  auto clone        = Object::clone(original);
  auto clone_as_hkc = std::dynamic_pointer_cast<HotKeyConfiguration>(clone);
  auto clone_save   = clone_as_hkc->GetHotKey("save");
  printf("clone_save<%p>\n", (void*) clone_save);

  ///////////////////////////////////////////
  // compare clone to original
  ///////////////////////////////////////////

  CHECK_EQUAL(clone_save->mbAlt, false);
  CHECK_EQUAL(clone_save->mbCtrl, true);
  CHECK_EQUAL(clone_save->miKeyCode, 83);

  CHECK_EQUAL(boost::uuids::to_string(clone_as_hkc->_uuid), orig_UUID);
  CHECK_EQUAL(boost::uuids::to_string(clone_save->_uuid), orig_save_UUID);
}
