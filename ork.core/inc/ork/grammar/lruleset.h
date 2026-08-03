////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// LRuleSet — a parametric L-system GRAMMAR as reflected DATA (GRAMMARS GR-1).
//
// Replaces "species = hardcoded C++ archetype procedure" with "species = data":
// a front-end DSL authors these reflected objects, they serialize as JSON inside
// the consuming module (byte-identical round-trip is the GR1.a gate), and the
// rewrite pass (grammar/rewrite.h) resolves them into a flat terminal op stream
// that a CONSUMING FAMILY interprets. This header is the SCHEMA ONLY.
//
// FAMILY-NEUTRAL (ork.core): nothing here — and nothing in grammar/rewrite.h —
// may reference meshes, transforms, or any lev2/gfx type. The turtle that turns
// the resolved stream into geometry lives in lev2 (hmdflow_lruleset.cpp); a second
// consumer (the audio family) reads the same schema with a different interpreter.
//
// SERDES DISCIPLINE (see JUL05_GR1.md §8):
//   * kinds/ops are REFLECTED ENUM TYPES (below) serialized BY NAME through the
//     scalar EnumSerializer — json, propsheet choice lists and any emitter read
//     the one registered spelling. Symbols are STRINGS (grammars are open
//     alphabets). No fvec-typed fields (angles are floats; directions are
//     derived in the interpreter, not stored).
//   * T2 — LTurtleOp/_params is a std::vector<LParamBinding> (proven object-vector
//     serdes), NOT a std::map<string,LExpr*> (unproven directObjectMap territory).
//   * T6 — LExpr graphs are TREES (no sharing/cycles): the evaluator depth-caps
//     recursion at kLExprMaxDepth; shared/cyclic nodes are rejected loudly there.
//   * the registered class names keep their original "hypermesh::" spelling: the
//     saved name is the on-disk contract for every grammar already serialized into
//     a scene, and this relocation is a MOVE, not a format change.
//
////////////////////////////////////////////////////////////////
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <ork/object/Object.h>
#include <ork/rtti/RTTIX.inl>
#include <ork/grammar/vocabulary.h> // LOpCode (control ops) + the family vocabulary registry

namespace ork::grammar {

///////////////////////////////////////////////////////////////////////////////
// forward ptr typedefs (the schema is mutually-referential: LExpr holds LExpr
// children, LTurtleOp holds LTurtleOp children + LParamBinding params + a guard
// LExpr, LRuleSet holds all of the above)
///////////////////////////////////////////////////////////////////////////////

struct LExpr;
struct LSymbolDef;
struct LTurtleOp;
struct LParamBinding;
struct LRuleDef;
struct LRuleSet;
using lexpr_ptr_t         = std::shared_ptr<LExpr>;
using lsymboldef_ptr_t    = std::shared_ptr<LSymbolDef>;
using lturtleop_ptr_t     = std::shared_ptr<LTurtleOp>;
using lparam_binding_ptr_t = std::shared_ptr<LParamBinding>;
using lruledef_ptr_t      = std::shared_ptr<LRuleDef>;
using lruleset_ptr_t      = std::shared_ptr<LRuleSet>;

///////////////////////////////////////////////////////////////////////////////
// the grammar's selector enums — REFLECTED types (registered + serialized by name
// in src/grammar/lruleset.cpp). The numeric values are the wire codes the Python DSL
// front-end and the pybind layer still speak (lsystem/__init__.py mirrors them);
// an int outside the registered set fails LOUD at serialize time.
///////////////////////////////////////////////////////////////////////////////

enum class LExprKind : int { // LExpr::_kind
  CONST = 0, // literal float (_const)
  PARAM = 1, // symbol-instance param / host reflected scalar of that name (_ref)
  ENV   = 2, // environment field (_ref) — GR-6; evaluates to 0.0f stub in GR1.b
  RNG   = 3, // counter-hash draw in [a,b] (args = two CONST children)
  BINOP = 4, // arithmetic (_op in ADD..DIV) over _args[0],_args[1]
  CMP   = 5, // comparison (_op in LT..NE) over _args[0],_args[1] -> 1.0f/0.0f
};

enum class LExprOp : int { // LExpr::_op
  ADD = 0, SUB = 1, MUL = 2, DIV = 3, // BINOP
  LT = 10, LE = 11, GT = 12, GE = 13, EQ = 14, NE = 15, // CMP
};

// The op alphabet is SPLIT (grammar/vocabulary.h): core defines only the CONTROL ops
// (LOpCode FORK/CHOOSE/WHEN/CALL, which the rewrite pass resolves); every other op belongs
// to a consuming family and is registered at runtime with its own code, saved token and
// counted-op flag. The mesh alphabet (segment/pitch/roll/yaw/taper/slot) is registered by
// ork.lev2 hypermesh; an audio alphabet would register alongside it, no core edit.

// LExpr tree recursion cap (T6): trees only; the evaluator rejects deeper/cyclic.
static constexpr int kLExprMaxDepth = 64;

///////////////////////////////////////////////////////////////////////////////
// LExpr — an expression AST node (TREE, no sharing/cycles; T6). The bake-time
// symbol-decision language: CONST/PARAM/ENV/RNG leaves + BINOP/CMP internal nodes.
// This is DELIBERATELY not the SelExpr GLSL ABI (§2 #3) — plain reflected data.
///////////////////////////////////////////////////////////////////////////////
struct LExpr : public ork::Object {
  DeclareConcreteX(LExpr, ork::Object);

public:
  LExpr()           = default;
  ~LExpr() override = default;

  LExprKind   _kind = LExprKind::CONST;
  float       _const = 0.0f; // CONST value
  std::string _ref;        // PARAM name ("len","gen") / ENV field ("moisture")
  LExprOp     _op = LExprOp::ADD; // BINOP arithmetic / CMP comparison
  std::vector<lexpr_ptr_t> _args; // operands (BINOP/CMP: 2; RNG: 2 CONST bounds)
};

///////////////////////////////////////////////////////////////////////////////
// LSymbolDef — one alphabet entry (a non-terminal / module symbol) + its default
// param environment. _defaults is string->float (T2/proven directMapProperty).
///////////////////////////////////////////////////////////////////////////////
struct LSymbolDef : public ork::Object {
  DeclareConcreteX(LSymbolDef, ork::Object);

public:
  LSymbolDef()           = default;
  ~LSymbolDef() override = default;

  std::string _name;
  std::map<std::string, float> _defaults; // per-symbol default params
};

///////////////////////////////////////////////////////////////////////////////
// LTurtleOp — one op in an axiom / rule RHS. Its _kind is either a CONTROL op (FORK/CHOOSE/WHEN
// carry child op bodies, CALL instantiates a non-terminal) or a family VOCABULARY code the
// registry resolves. _params are (key,expr) bindings (T2 workaround for a string->expr map).
// The class keeps its "turtle" name: it is the saved-name contract of every serialized grammar.
///////////////////////////////////////////////////////////////////////////////
struct LTurtleOp : public ork::Object {
  DeclareConcreteX(LTurtleOp, ork::Object);

public:
  LTurtleOp()           = default;
  ~LTurtleOp() override = default;

  // the default is op code 0 — deliberately NOT a core-known op: core owns no vocabulary, and 0
  // is the first family code (mesh pins SEGMENT there for wire compat). Every authoring path
  // (DSL, pybind kwargs, deserialize) sets _kind explicitly.
  LOpCode _kind = LOpCode(0);
  std::vector<lparam_binding_ptr_t> _params; // named (key,expr) params (T2)
  uint32_t _gid  = 0;   // gid band an emitting op writes (A1 tag space)
  std::string _symbol;  // CALL target non-terminal
  std::vector<lturtleop_ptr_t> _children; // FORK/CHOOSE/WHEN bodies
  std::vector<float>           _weights;  // CHOOSE per-child weights
  lexpr_ptr_t _guard;   // WHEN predicate (nullable)
};

///////////////////////////////////////////////////////////////////////////////
// LParamBinding — a named (key, expr) pair. The proven object-vector element that
// stands in for a string->LExpr map (T2).
///////////////////////////////////////////////////////////////////////////////
struct LParamBinding : public ork::Object {
  DeclareConcreteX(LParamBinding, ork::Object);

public:
  LParamBinding()           = default;
  ~LParamBinding() override = default;

  std::string _key;
  lexpr_ptr_t _value;
};

///////////////////////////////////////////////////////////////////////////////
// LRuleDef — one production: _lhs -> _rhs, gated by _guard, weighted by _weight
// (weighted-choose among rules sharing an _lhs).
///////////////////////////////////////////////////////////////////////////////
struct LRuleDef : public ork::Object {
  DeclareConcreteX(LRuleDef, ork::Object);

public:
  LRuleDef()           = default;
  ~LRuleDef() override = default;

  std::string _lhs;
  lexpr_ptr_t _guard;    // nullable
  float       _weight = 1.0f;
  std::vector<lturtleop_ptr_t> _rhs;
};

///////////////////////////////////////////////////////////////////////////////
// LRuleSet — THE grammar: reflected DATA (NOT a GraphData). Authored by the
// Python DSL, serialized inside the consuming module, resolved by the rewrite pass
// (grammar/rewrite.h) into the terminal op stream a family interpreter consumes.
//   NOTE (T8/serdes): _seed is uint32_t — Darwin's uint64_t is `unsigned long
//   long`, absent from the JsonSerializer leaf tryAs chain (it would OrkAssert on
//   serialize). A 32-bit authored seed is folded into the 64-bit counter-hash by
//   the evaluator. _depth/_countedBudget/_gid are uint32_t (serdes-proven).
///////////////////////////////////////////////////////////////////////////////
struct LRuleSet : public ork::Object {
  DeclareConcreteX(LRuleSet, ork::Object);

public:
  LRuleSet()           = default;
  ~LRuleSet() override = default;

  std::vector<lsymboldef_ptr_t> _symbols; // the alphabet
  std::vector<lturtleop_ptr_t>  _axiom;   // start string
  std::vector<lruledef_ptr_t>   _rules;   // productions
  uint32_t _depth = 8;           // rewrite iterations
  // emission cap on the consuming vocabulary's COUNTED op (mesh: segment) — the bake-cost bound.
  // Saved (and authored, in python) as "segment_budget": the name predates the vocabulary split
  // and is the on-disk contract.
  uint32_t _countedBudget = 4096;
  uint32_t _seed = 0;            // deterministic stochastic seed (see NOTE above)
};

} // namespace ork::grammar
