////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// LRuleSet — reflection (describeX) for the six grammar-as-data types (GR1.a).
// Schema in ork/grammar/lruleset.h. Rewrite pass in rewrite.cpp.
//
// Reflection template (asset_gen.cpp HeightFieldGenData/ScatterSinkData):
//   directObjectVectorProperty  for object vectors  (_args/_children/_rhs/...)
//   directObjectProperty        for a single object (_guard/_value, nullable)
//   directMapProperty           for string->float   (_defaults)
//   directVectorProperty        for vector<float>    (_weights)
//   directProperty              for scalars          (uint32_t/float/string)
//   directEnumProperty          for the selectors    (_kind/_op — serialized BY NAME)
//
// The registered class names keep the "hypermesh::" spelling they were born with: the saved
// name is the on-disk contract of every grammar already serialized into a scene, and moving
// the C++ namespace must not rewrite anyone's data.
//
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/grammar/lruleset.h>
#include <ork/reflect/properties/DirectTypedMap.hpp> // _defaults (string->float)
#include <ork/reflect/properties/ITypedMap.hpp>
#include <ork/reflect/enum_serializer.inl>           // _kind/_op reflected enums

ImplementReflectionX(ork::grammar::LExpr,         "hypermesh::LExpr");
ImplementReflectionX(ork::grammar::LSymbolDef,    "hypermesh::LSymbolDef");
ImplementReflectionX(ork::grammar::LTurtleOp,     "hypermesh::LTurtleOp");
ImplementReflectionX(ork::grammar::LParamBinding, "hypermesh::LParamBinding");
ImplementReflectionX(ork::grammar::LRuleDef,      "hypermesh::LRuleDef");
ImplementReflectionX(ork::grammar::LRuleSet,      "hypermesh::LRuleSet");

ImplementEnumSerializer(ork::grammar::LExprKind);
ImplementEnumSerializer(ork::grammar::LExprOp);
// LOpCode's serializer + its (registry-fed) name table live in grammar/vocabulary.cpp: half of
// that table does not exist until a family registers its alphabet.

namespace ork::grammar {

///////////////////////////////////////////////////////////////////////////////
// EnumSerializer registration — the names are registered lowercase to match the DSL
// verb spelling (S.rnd / the binop verbs), so the reflected json, the propsheet choice
// list and any emitter read ONE string. EVERY enumerator of both expression selectors is
// registered here: an unregistered value aborts loudly at serialize time, so a partial
// table would be a load failure.
///////////////////////////////////////////////////////////////////////////////

BeginEnumRegistration(LExprKind);
  enumtype->addEnum("const", LExprKind::CONST);
  enumtype->addEnum("param", LExprKind::PARAM);
  enumtype->addEnum("env",   LExprKind::ENV);
  enumtype->addEnum("rng",   LExprKind::RNG);
  enumtype->addEnum("binop", LExprKind::BINOP);
  enumtype->addEnum("cmp",   LExprKind::CMP);
EndEnumRegistration();

BeginEnumRegistration(LExprOp);
  enumtype->addEnum("add", LExprOp::ADD);
  enumtype->addEnum("sub", LExprOp::SUB);
  enumtype->addEnum("mul", LExprOp::MUL);
  enumtype->addEnum("div", LExprOp::DIV);
  enumtype->addEnum("lt",  LExprOp::LT);
  enumtype->addEnum("le",  LExprOp::LE);
  enumtype->addEnum("gt",  LExprOp::GT);
  enumtype->addEnum("ge",  LExprOp::GE);
  enumtype->addEnum("eq",  LExprOp::EQ);
  enumtype->addEnum("ne",  LExprOp::NE);
EndEnumRegistration();

///////////////////////////////////////////////////////////////////////////////

void LExpr::describeX(object::ObjectClass* clazz) {
  InvokeEnumRegistration(LExprKind);
  InvokeEnumRegistration(LExprOp);
  clazz->directEnumProperty("kind", &LExpr::_kind);
  clazz->directProperty("const", &LExpr::_const);
  clazz->directProperty("ref", &LExpr::_ref);
  clazz->directEnumProperty("op", &LExpr::_op);
  clazz->directObjectVectorProperty("args", &LExpr::_args);
}

///////////////////////////////////////////////////////////////////////////////

void LSymbolDef::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("name", &LSymbolDef::_name);
  clazz->directMapProperty("defaults", &LSymbolDef::_defaults);
}

///////////////////////////////////////////////////////////////////////////////

void LTurtleOp::describeX(object::ObjectClass* clazz) {
  LVocabularyRegistry::instance(); // seeds the control ops into the LOpCode name table (order-independent)
  clazz->directEnumProperty("kind", &LTurtleOp::_kind);
  clazz->directObjectVectorProperty("params", &LTurtleOp::_params);
  clazz->directProperty("gid", &LTurtleOp::_gid);
  clazz->directProperty("symbol", &LTurtleOp::_symbol);
  clazz->directObjectVectorProperty("children", &LTurtleOp::_children);
  clazz->directVectorProperty("weights", &LTurtleOp::_weights);
  clazz->directObjectProperty("guard", &LTurtleOp::_guard);
}

///////////////////////////////////////////////////////////////////////////////

void LParamBinding::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("key", &LParamBinding::_key);
  clazz->directObjectProperty("value", &LParamBinding::_value);
}

///////////////////////////////////////////////////////////////////////////////

void LRuleDef::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("lhs", &LRuleDef::_lhs);
  clazz->directObjectProperty("guard", &LRuleDef::_guard);
  clazz->directProperty("weight", &LRuleDef::_weight);
  clazz->directObjectVectorProperty("rhs", &LRuleDef::_rhs);
}

///////////////////////////////////////////////////////////////////////////////

void LRuleSet::describeX(object::ObjectClass* clazz) {
  clazz->directObjectVectorProperty("symbols", &LRuleSet::_symbols);
  clazz->directObjectVectorProperty("axiom", &LRuleSet::_axiom);
  clazz->directObjectVectorProperty("rules", &LRuleSet::_rules);
  clazz->directProperty("depth", &LRuleSet::_depth);
  clazz->directProperty("segment_budget", &LRuleSet::_countedBudget); // saved name is the on-disk contract
  clazz->directProperty("seed", &LRuleSet::_seed);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::grammar
