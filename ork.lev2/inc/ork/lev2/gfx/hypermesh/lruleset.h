////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// LRuleSet — the grammar SCHEMA + REWRITE pass now live family-neutrally in ork.core
// (ork/grammar/lruleset.h + ork/grammar/rewrite.h): a second consumer (the audio family)
// reads the same reflected objects, so nothing about them may know a mesh exists.
//
// What stays lev2-side is the TURTLE: the pass-2 interpreter that turns a resolved op
// stream into XfNodes/XfSlots (hmdflow_lruleset.cpp) and the deriveLRuleSet entry point
// (declared in hmdflow.h next to LSystemModuleData).
//
// This header re-exports the schema names into ork::lev2::hypermesh so the hypermesh
// modules, the pyext bindings and the Python DSL keep their existing spellings, and it
// DECLARES THE MESH FAMILY'S OP VOCABULARY (LMeshOp): the six turtle ops core no longer
// knows, registered into ork::grammar's registry at lev2 init.
//
////////////////////////////////////////////////////////////////
#pragma once

#include <ork/grammar/lruleset.h>

namespace ork::lev2::hypermesh {

using ork::grammar::LExpr;
using ork::grammar::LSymbolDef;
using ork::grammar::LTurtleOp;
using ork::grammar::LParamBinding;
using ork::grammar::LRuleDef;
using ork::grammar::LRuleSet;

using ork::grammar::lexpr_ptr_t;
using ork::grammar::lsymboldef_ptr_t;
using ork::grammar::lturtleop_ptr_t;
using ork::grammar::lparam_binding_ptr_t;
using ork::grammar::lruledef_ptr_t;
using ork::grammar::lruleset_ptr_t;

using ork::grammar::LExprKind;
using ork::grammar::LExprOp;
using ork::grammar::LOpCode;

using ork::grammar::kLExprMaxDepth;

///////////////////////////////////////////////////////////////////////////////
// LMeshOp — the MESH family's registered alphabet: what the turtle (hmdflow_lruleset.cpp)
// interprets. The codes are PINNED at 0..5 because they are the wire codes the python DSL,
// the pybind boundary and every already-serialized grammar speak; the saved tokens are the
// lowercase spellings registered in registerMeshVocabulary(). SEGMENT is the COUNTED op —
// the one charged against LRuleSet's segment_budget.
///////////////////////////////////////////////////////////////////////////////
enum class LMeshOp : int32_t {
  SEGMENT = 0, // emit one skeleton node (_attrs[0]=rad; _tags gid band)
  PITCH   = 1, // rotate frame about local X
  ROLL    = 2, // rotate frame about local Z (heading)
  YAW     = 3, // rotate frame about local Y
  TAPER   = 4, // scale the radius accumulator
  SLOT    = 5, // emit an attachment slot (GR-2 consumes; emit-only in GR-1)
};

// idempotent; called from lev2 class registration (lev2_init.cpp) and self-defensively by
// meshVocabId(), so no mesh grammar can be derived against an unregistered alphabet.
void                     registerMeshVocabulary();
ork::grammar::lvocab_id_t meshVocabId();

} // namespace ork::lev2::hypermesh
