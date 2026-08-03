////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// GRAMMAR OP VOCABULARY — the seam by which a consuming FAMILY teaches ork.core the alphabet
// of its terminal ops (GRAMMARS GR-1).
//
// ork.core owns exactly FOUR ops: the CONTROL ops the rewrite pass resolves (LOpCode below).
// Every other op in a grammar belongs to a family and is registered here: the mesh family
// registers segment/pitch/roll/yaw/taper/slot (ork.lev2 hypermesh), an audio family would
// register note/rest/chord/tie without ork.core changing at all. Core assigns a registered op
// NO semantics beyond the two facts it needs in order to rewrite:
//   * which VOCABULARY it belongs to — so a family interpreter can reject a foreign op stream
//     instead of silently skipping ops it does not recognize, and
//   * whether it is the vocabulary's COUNTED op — the one charged against LRuleSet::_countedBudget,
//     the bake-cost bound (mesh: SEGMENT; audio: NOTE). At most one per vocabulary.
// What an op MEANS, and which params it reads, is known only to the family's pass-2 interpreter.
//
// CODE SPACE. Op codes are int32 and GLOBALLY UNIQUE across vocabularies, as are op NAMES:
// LTurtleOp::_kind is ONE reflected scalar serialized BY NAME, so a saved token ("segment",
// "note") must resolve to exactly one (vocabulary, code) pair — there is no disambiguating
// field on the wire, and adding one would rewrite every already-serialized grammar. A duplicate
// code or name fails LOUD at registration.
//     0..5   the mesh family     (PINNED — these are the on-disk and python wire codes)
//     6..9   core CONTROL        (PINNED — same reason)
//     >=100  a new family picks a private band (convention; uniqueness is what is enforced)
//
// REGISTRATION IS ORDER-INDEPENDENT. The registry seeds the control ops in its own constructor
// and pushes each registered op straight into the ONE EnumRegistrar table for LOpCode; nothing
// ever re-creates that table (a second addEnumClass would wipe the family entries), so a family
// may register before or after the grammar classes are reflected.
//
////////////////////////////////////////////////////////////////
#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <ork/reflect/enum_serializer.inl>

namespace ork::grammar {

///////////////////////////////////////////////////////////////////////////////
// LOpCode — LTurtleOp::_kind. The enumerators are core's CONTROL alphabet; family vocabulary
// codes legitimately live in this same value space (see CODE SPACE above), so an LOpCode may
// hold a value outside the enumerator list — classify those through the registry, never by
// assuming a switch is exhaustive.
///////////////////////////////////////////////////////////////////////////////
enum class LOpCode : int32_t {
  FORK   = 6, // push N child branches (the children are rewritten once per branch)
  CHOOSE = 7, // weighted-choose ONE child (resolved in rewrite pass 1)
  WHEN   = 8, // guarded child body (resolved in rewrite pass 1)
  CALL   = 9, // instantiate a non-terminal (_symbol) with evaluated params
};

using lvocab_id_t = uint32_t;
static constexpr lvocab_id_t kVocabControl = 0;          // core's own alphabet (the four ops above)
static constexpr lvocab_id_t kVocabInvalid = 0xffffffffu;

///////////////////////////////////////////////////////////////////////////////
// LOpDesc — one op of a family alphabet. `_vocab` is assigned by the registry; a family fills
// only code/name/counted.
///////////////////////////////////////////////////////////////////////////////
struct LOpDesc {
  int32_t     _code    = 0;
  std::string _name;    // the SAVED token — lowercase, matching the DSL verb spelling
  bool        _counted = false;
  lvocab_id_t _vocab   = kVocabInvalid;
};

///////////////////////////////////////////////////////////////////////////////
// LVocabularyRegistry — process-wide, one instance (function-local static: whichever library
// asks first constructs it).
///////////////////////////////////////////////////////////////////////////////
struct LVocabularyRegistry {

  static LVocabularyRegistry& instance();

  // register a family alphabet under `vocabname`; returns its id. Re-registering the SAME name
  // with the SAME ops is a no-op returning the same id (family init paths may run more than
  // once). A duplicate code/name, a second counted op, or a CHANGED re-registration fails LOUD.
  lvocab_id_t registerVocabulary(const std::string& vocabname, const std::vector<LOpDesc>& ops);

  const LOpDesc* findOp(int32_t code) const;
  const LOpDesc* findOp(const std::string& name) const;

  // the rewrite pass's lookup. An op code no family claims is not "unknown but harmless" — it is
  // a grammar nobody can interpret, so this fails LOUD rather than passing the op through.
  const LOpDesc& requireOp(int32_t code) const;

  lvocab_id_t        findVocabulary(const std::string& name) const; // kVocabInvalid if absent
  const std::string& vocabularyName(lvocab_id_t id) const;

private:
  LVocabularyRegistry();
  void _declare(lvocab_id_t vocab, const LOpDesc& op);

  std::map<int32_t, LOpDesc>                  _byCode;
  std::map<std::string, int32_t>              _byName;
  std::vector<std::string>                    _vocabNames;
  std::map<std::string, std::vector<LOpDesc>> _vocabOps;
  ork::reflect::serdes::enumtype_ptr_t        _opEnumType;
};

///////////////////////////////////////////////////////////////////////////////

inline bool isControlOp(LOpCode k) {
  return (k == LOpCode::FORK) or (k == LOpCode::CHOOSE) or (k == LOpCode::WHEN) or (k == LOpCode::CALL);
}

} // namespace ork::grammar

DeclareEnumSerializer(::ork::grammar::LOpCode);
