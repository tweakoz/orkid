////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <regex>
#include <stdlib.h>
#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/util/scanner.h>
#include <ork/kernel/string/deco.inl>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork {
/////////////////////////////////////////////////////////////////////////////////////////////////

const Token* Scanner::token(size_t i) const {
  const Token* pt = nullptr;
  if (i < tokens.size()) {
    pt = &tokens[i];
  }
  return pt;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void Scanner::addRule(std::string rule, id_t state) {
  _rules.push(rule, state);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void Scanner::buildStateMachine() {
  gen_t::build(_rules, _statemachine);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

Scanner::Scanner(
    std::string blockregex, //
    size_t capacity)
    : _kcapacity(capacity)
    , ifilelen(0)
    , _blockregex(blockregex)
    , cur_token("", 0, 0)
    , ss(ESTA_NONE) {
  _fxbuffer.resize(capacity);
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void Scanner::clear() {
  ifilelen = 0;
  tokens.clear();
  _fxbuffer.clear();
  _fxbuffer.resize(_kcapacity);
  ss = ESTA_NONE;
  cur_token = Token("", 0, 0);
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void Scanner::scanString(std::string str) {
  resize(str.length() + 1);
  memcpy(_fxbuffer.data(), str.c_str(), str.length());
  _fxbuffer[str.length()] = 0;
  scan();
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void Scanner::scan() {
  std::string as_str = _fxbuffer.data();
  // printf( "scan<%s>\n", as_str.c_str() );
  iter_t iter(as_str.begin(), as_str.end(), _statemachine);
  iter_t end;

  int index = 0;
  for (; iter != end; ++iter) {
    auto tok   = Token(iter->str(), 0, 0);
    tok._class = iter->id;
    tokens.push_back(tok);
    // std::cout << "index<" << index << "> Id: " << iter->id << ", Token: '" << iter->str() << "'\n";
    index++;
  }
  // OrkAssert(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void Scanner::discardTokensOfClass(uint64_t tokclass) {

  tokens.erase(
      std::remove_if(
          tokens.begin(), //
          tokens.end(),
          [&](Token& tok) { //
            return tok._class == tokclass;
          }),
      tokens.end());
}

/////////////////////////////////////////////////////////////////////////////////////////////////

ScannerView Scanner::createTopView() const {
  static ScanViewFilter filter;
  ScannerView rval(*this, filter);
  rval._start = 0;
  rval._end   = tokens.size() - 1;
  for (int i = 0; i < tokens.size(); i++) {
    rval._indices.push_back(i);
  }
  return rval;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork
/////////////////////////////////////////////////////////////////////////////////////////////////
