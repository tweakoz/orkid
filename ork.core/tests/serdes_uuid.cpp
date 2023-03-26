////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <string.h>

#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include "reflectionclasses.inl"

using namespace ork;
using namespace ork::reflect;

////////////////////////////////////////////////////////////////////////////////
// test preservation of UUIDs across serialize/deserialize cycles
////////////////////////////////////////////////////////////////////////////////

TEST(SerdesPreserveUUID) {
  //////////////////////////////////////////////////
  auto rootobj          = std::make_shared<SharedTest>();
  auto child1           = std::make_shared<SimpleTest>();
  auto child2           = std::make_shared<SharedTest>();
  rootobj->_directChild = child1;
  rootobj->setChild(child2);
  //////////////////////////////////////////////////
  // serialize to string
  //////////////////////////////////////////////////
  serdes::JsonSerializer ser;
  ser.serializeRoot(rootobj);
  auto serstr = ser.output();
  // printf("mutstr<%s>\n", serstr.c_str());
  //////////////////////////////////////////////////
  // deserialize from string
  //////////////////////////////////////////////////
  object_ptr_t deser_out;
  serdes::JsonDeserializer deser(serstr.c_str());
  deser.deserializeTop(deser_out);
  auto clone = objcast<SharedTest>(deser_out);
  //////////////////////////////////////////////////
  // verify UUIDS were preserved
  //////////////////////////////////////////////////
  CHECK(clone->_uuid == rootobj->_uuid);
  CHECK(clone->_directChild->_uuid == rootobj->_directChild->_uuid);
  CHECK(clone->_accessorChild->_uuid == rootobj->_accessorChild->_uuid);
  //////////////////////////////////////////////////
  // prove that these are seperate instances with the same uuid's
  //////////////////////////////////////////////////
  CHECK(clone != rootobj);
  CHECK(clone->_directChild != rootobj->_directChild);
  CHECK(clone->_accessorChild != rootobj->_accessorChild);
}
