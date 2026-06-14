////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <utpp/UnitTest++.h>
#include <cmath>
#include <string>

#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include "reflectionclasses.inl"

// JSON round-trip for a reflected ork::varmap::VarMap field. Exercises
// the DirectVarMap property type + the var_t-as-tagged-string codec
// (NODEENC<var_t> on the serialize side, decode_value<svar128_t> on
// the deserialize side).

using namespace ork;
using namespace ork::reflect;

////////////////////////////////////////////////////////////////////////////////

static std::string svm_generate() {
  auto vt = std::make_shared<VarMapTest>();
  // Holder starts with _params null. The test caller verifies this.
  vt->_params = std::make_shared<varmap::VarMap>();
  vt->_params->set<float>("radius",    1.5f);
  vt->_params->set<float>("thickness", 0.25f);
  vt->_params->set<int>  ("segments",  48);
  vt->_params->set<int>  ("seed",      42);
  vt->_params->set<bool> ("enabled",   true);

  serdes::JsonSerializer ser;
  ser.serializeRoot(vt);
  return ser.output();
}

TEST(SerdesVarMapProperty) {
  auto js = svm_generate();
  printf("varmap json<%s>\n", js.c_str());

  // Verify tagged-string encoding present in the JSON for at least
  // the float and int variants (smoke check on the encode path).
  CHECK(js.find("float:1.5") != std::string::npos
        || js.find("float:1.500000") != std::string::npos);
  CHECK(js.find("int:48")     != std::string::npos);
  CHECK(js.find("bool:1")     != std::string::npos);

  object_ptr_t out;
  serdes::JsonDeserializer deser(js.c_str());
  deser.deserializeTop(out);
  auto clone = objcast<VarMapTest>(out);
  CHECK(clone != nullptr);
  CHECK(clone->_params != nullptr);

  auto const& m = clone->_params->_themap;
  CHECK_EQUAL(m.size(), size_t(5));

  auto radius = clone->_params->typedValueForKey<float>("radius");
  auto thick  = clone->_params->typedValueForKey<float>("thickness");
  auto segs   = clone->_params->typedValueForKey<int>("segments");
  auto seed   = clone->_params->typedValueForKey<int>("seed");
  auto enabled= clone->_params->typedValueForKey<bool>("enabled");

  CHECK(bool(radius));
  CHECK(bool(thick));
  CHECK(bool(segs));
  CHECK(bool(seed));
  CHECK(bool(enabled));

  CHECK_CLOSE(radius.value(), 1.5f,  1e-5f);
  CHECK_CLOSE(thick.value(),  0.25f, 1e-5f);
  CHECK_EQUAL(segs.value(),   48);
  CHECK_EQUAL(seed.value(),   42);
  CHECK_EQUAL(enabled.value(), true);
}

////////////////////////////////////////////////////////////////////////////////

TEST(SerdesVarMapEmptyAndNull) {
  // Default-constructed VarMapTest has _params = nullptr.
  // Serializing should produce an empty map for the params property;
  // deserializing should leave _params null OR allocate-then-empty
  // depending on the deserializer's iteration policy. Either way no
  // crash.
  auto vt = std::make_shared<VarMapTest>();
  CHECK(not vt->_params);

  serdes::JsonSerializer ser;
  ser.serializeRoot(vt);
  auto js = ser.output();
  printf("null-varmap json<%s>\n", js.c_str());

  object_ptr_t out;
  serdes::JsonDeserializer deser(js.c_str());
  deser.deserializeTop(out);
  auto clone = objcast<VarMapTest>(out);
  CHECK(clone != nullptr);
  // _params is either still null OR points at an empty VarMap.
  if (clone->_params) {
    CHECK_EQUAL(clone->_params->_themap.size(), size_t(0));
  }
}
