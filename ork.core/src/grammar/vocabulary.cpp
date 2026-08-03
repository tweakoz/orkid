////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// Grammar op-vocabulary registry (contract in ork/grammar/vocabulary.h). Family-neutral: this TU
// names no family — it holds the four CONTROL ops and whatever alphabets families register.
//
// The LOpCode EnumSerializer table is populated from HERE, not from a static BeginEnumRegistration
// block, because half of it does not exist until a family registers: the saved token for a family
// op ("segment") is minted at registerVocabulary() time.
//
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/grammar/vocabulary.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>

ImplementEnumSerializer(::ork::grammar::LOpCode);

namespace ork::grammar {

///////////////////////////////////////////////////////////////////////////////

LVocabularyRegistry& LVocabularyRegistry::instance() {
  static LVocabularyRegistry _instance;
  return _instance;
}

///////////////////////////////////////////////////////////////////////////////

LVocabularyRegistry::LVocabularyRegistry() {
  auto registrar = reflect::serdes::EnumRegistrar::instance();
  // created exactly ONCE for the process — addEnumClass REPLACES the table, so a second call
  // anywhere would silently drop every family op registered before it.
  _opEnumType = registrar->addEnumClass<LOpCode>("LOpCode");
  _vocabNames.push_back("control");
  // the names are the wire tokens, lowercase to match the DSL verb spelling (L.fork / L.choose).
  _declare(kVocabControl, {int32_t(LOpCode::FORK),   "fork",   false});
  _declare(kVocabControl, {int32_t(LOpCode::CHOOSE), "choose", false});
  _declare(kVocabControl, {int32_t(LOpCode::WHEN),   "when",   false});
  _declare(kVocabControl, {int32_t(LOpCode::CALL),   "call",   false});
}

///////////////////////////////////////////////////////////////////////////////

void LVocabularyRegistry::_declare(lvocab_id_t vocab, const LOpDesc& op) {
  OrkAssertIFMT(op._code >= 0,
    "[grammar vocabulary] op '%s' declares negative code %d — codes index a by-name enum table and must be >= 0.",
    op._name.c_str(), op._code);
  OrkAssertIFMT(not op._name.empty(),
    "[grammar vocabulary] op code %d declares an EMPTY name — the name is the saved token.", op._code);

  auto itc      = _byCode.find(op._code);
  bool codefree = (itc == _byCode.end());
  OrkAssertIFMT(codefree,
    "[grammar vocabulary] op code %d ('%s') is already registered as '%s' — op codes are GLOBAL across "
    "vocabularies (one reflected scalar carries them all); pick a free band.",
    op._code, op._name.c_str(), codefree ? "" : itc->second._name.c_str());

  auto itn      = _byName.find(op._name);
  bool namefree = (itn == _byName.end());
  OrkAssertIFMT(namefree,
    "[grammar vocabulary] op name '%s' is already registered at code %d — op names are the SAVED tokens and "
    "must resolve to exactly one op.",
    op._name.c_str(), namefree ? 0 : itn->second);

  LOpDesc d = op;
  d._vocab  = vocab;
  _byCode[d._code] = d;
  _byName[d._name] = d._code;
  _vocabOps[_vocabNames[vocab]].push_back(d);
  _opEnumType->addEnum(d._name, LOpCode(d._code));
}

///////////////////////////////////////////////////////////////////////////////

lvocab_id_t LVocabularyRegistry::registerVocabulary(const std::string& vocabname, const std::vector<LOpDesc>& ops) {
  OrkAssertIFMT(not vocabname.empty(), "[grammar vocabulary] a vocabulary must be named (got \"\").%s", "");

  auto itv = std::find(_vocabNames.begin(), _vocabNames.end(), vocabname);
  if (itv != _vocabNames.end()) {
    auto        id   = lvocab_id_t(itv - _vocabNames.begin());
    const auto& prev = _vocabOps[vocabname];
    bool        same = (prev.size() == ops.size());
    for (size_t i = 0; same and (i < ops.size()); i++)
      same = (prev[i]._code == ops[i]._code) and (prev[i]._name == ops[i]._name) and
             (prev[i]._counted == ops[i]._counted);
    OrkAssertIFMT(same,
      "[grammar vocabulary] vocabulary '%s' is already registered with a DIFFERENT alphabet — a vocabulary is "
      "fixed at first registration (already-serialized grammars name its ops).",
      vocabname.c_str());
    return id;
  }

  int counted = 0;
  for (auto& o : ops)
    counted += o._counted ? 1 : 0;
  OrkAssertIFMT(counted <= 1,
    "[grammar vocabulary] vocabulary '%s' declares %d counted ops — AT MOST ONE op may be charged against "
    "segment_budget (the budget is a single scalar).",
    vocabname.c_str(), counted);

  auto id = lvocab_id_t(_vocabNames.size());
  _vocabNames.push_back(vocabname);
  for (auto& o : ops)
    _declare(id, o);
  return id;
}

///////////////////////////////////////////////////////////////////////////////

const LOpDesc* LVocabularyRegistry::findOp(int32_t code) const {
  auto it = _byCode.find(code);
  return (it != _byCode.end()) ? &it->second : nullptr;
}

const LOpDesc* LVocabularyRegistry::findOp(const std::string& name) const {
  auto it = _byName.find(name);
  return (it != _byName.end()) ? findOp(it->second) : nullptr;
}

const LOpDesc& LVocabularyRegistry::requireOp(int32_t code) const {
  auto it = _byCode.find(code);
  if (it == _byCode.end()) {
    printf("[lsystem grammar] op code %d belongs to NO registered vocabulary — the consuming family did not "
           "register its alphabet (LVocabularyRegistry::registerVocabulary) before the grammar was derived.\n", code);
    fflush(stdout);
    OrkAssert(false);
    abort(); // an op stream nobody can interpret must not continue, assert build or not
  }
  return it->second;
}

///////////////////////////////////////////////////////////////////////////////

lvocab_id_t LVocabularyRegistry::findVocabulary(const std::string& name) const {
  auto it = std::find(_vocabNames.begin(), _vocabNames.end(), name);
  return (it != _vocabNames.end()) ? lvocab_id_t(it - _vocabNames.begin()) : kVocabInvalid;
}

const std::string& LVocabularyRegistry::vocabularyName(lvocab_id_t id) const {
  static const std::string _unknown = "<unregistered>";
  return (id < lvocab_id_t(_vocabNames.size())) ? _vocabNames[id] : _unknown;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::grammar
