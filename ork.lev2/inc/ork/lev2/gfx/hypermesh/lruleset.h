////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// LRuleSet — a plant/turtle GRAMMAR as reflected DATA (GRAMMARS GR-1).
//
// Replaces "species = hardcoded C++ archetype procedure" with "species = data":
// the Python DSL authors these reflected objects, they serialize as JSON inside
// the scene/module (byte-identical round-trip is the GR1.a gate), and a C++
// evaluator (GR1.b) rewrites + turtle-interprets them into the existing
// XfNodeGraph -> LSweep pipeline at bake time. This header is the SCHEMA ONLY —
// no evaluator lives here (see hmdflow_lruleset.cpp / GR1.b).
//
// SERDES DISCIPLINE (see JUL05_GR1.md §8):
//   * T8 — kinds/ops are plain `int` + named constants (below), NOT reflected
//     enum TYPES (the crc-enum-array truncation trap). Symbols are STRINGS
//     (grammars are open alphabets). No fvec-typed fields (angles are floats;
//     directions are derived in the turtle, not stored).
//   * T2 — LTurtleOp/_params is a std::vector<LParamBinding> (proven object-vector
//     serdes), NOT a std::map<string,LExpr*> (unproven directObjectMap territory).
//   * T6 — LExpr graphs are TREES (no sharing/cycles): the evaluator depth-caps
//     recursion at kLExprMaxDepth; shared/cyclic nodes are rejected loudly there.
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

namespace ork::lev2::hypermesh {

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
// named int constants (T8: named constants, not reflected enum types — the field
// stays `int`; these plain enums are author-time names only, never reflected).
///////////////////////////////////////////////////////////////////////////////

namespace LExprKind { // LExpr::_kind
enum : int {
  CONST = 0, // literal float (_const)
  PARAM = 1, // symbol-instance param / module reflected scalar of that name (_ref)
  ENV   = 2, // environment field (_ref) — GR-6; evaluates to 0.0f stub in GR1.b
  RNG   = 3, // counter-hash draw in [a,b] (args = two CONST children)
  BINOP = 4, // arithmetic (_op in ADD..DIV) over _args[0],_args[1]
  CMP   = 5, // comparison (_op in LT..NE) over _args[0],_args[1] -> 1.0f/0.0f
};
} // namespace LExprKind

namespace LExprOp { // LExpr::_op
enum : int {
  ADD = 0, SUB = 1, MUL = 2, DIV = 3, // BINOP
  LT = 10, LE = 11, GT = 12, GE = 13, EQ = 14, NE = 15, // CMP
};
} // namespace LExprOp

namespace LTurtleKind { // LTurtleOp::_kind
enum : int {
  SEGMENT = 0, // emit an XfNode (parent = stack top; _attrs[0]=rad; _tags gid band)
  PITCH   = 1, // rotate frame about local X
  ROLL    = 2, // rotate frame about local Z (heading)
  YAW     = 3, // rotate frame about local Y
  TAPER   = 4, // scale the radius accumulator
  SLOT    = 5, // emit an XfSlot (GR-2 consumes; emit-only in GR-1)
  FORK    = 6, // push N child frames (children run each in a pushed frame)
  CHOOSE  = 7, // weighted-choose ONE child (resolved in rewrite pass 1)
  WHEN    = 8, // guarded child body (resolved in rewrite pass 1)
  CALL    = 9, // instantiate a non-terminal (_symbol) with evaluated params
};
} // namespace LTurtleKind

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

  int         _kind = 0;   // LExprKind
  float       _const = 0.0f; // CONST value
  std::string _ref;        // PARAM name ("len","gen") / ENV field ("moisture")
  int         _op = 0;     // LExprOp (BINOP arithmetic / CMP comparison)
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
// LTurtleOp — one op in an axiom / rule RHS. Terminal ops (SEGMENT/PITCH/...) the
// turtle interprets; structural ops (FORK/CHOOSE/WHEN) carry child op bodies;
// CALL instantiates a non-terminal. _params are (key,expr) bindings (T2 workaround
// for a string->expr map).
///////////////////////////////////////////////////////////////////////////////
struct LTurtleOp : public ork::Object {
  DeclareConcreteX(LTurtleOp, ork::Object);

public:
  LTurtleOp()           = default;
  ~LTurtleOp() override = default;

  int      _kind = 0;   // LTurtleKind
  std::vector<lparam_binding_ptr_t> _params; // named (key,expr) params (T2)
  uint32_t _gid  = 0;   // gid band written by SEGMENT ops (A1 tag space)
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
// Python DSL, serialized inside LSystemModuleData, derived by the C++ evaluator
// (GR1.b) into the XfNodeGraph the LSweep skinner consumes.
//   NOTE (T8/serdes): _seed is uint32_t — Darwin's uint64_t is `unsigned long
//   long`, absent from the JsonSerializer leaf tryAs chain (it would OrkAssert on
//   serialize). A 32-bit authored seed is folded into the 64-bit counter-hash by
//   the evaluator. _depth/_segmentBudget/_gid are uint32_t (serdes-proven).
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
  uint32_t _segmentBudget = 4096; // SEGMENT-emission cap (bake-cost bound)
  uint32_t _seed = 0;            // deterministic stochastic seed (see NOTE above)
};

} // namespace ork::lev2::hypermesh
