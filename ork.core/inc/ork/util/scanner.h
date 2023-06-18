////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <regex>
#include <lexertl/generator.hpp>
#include <lexertl/lookup.hpp>
#include <lexertl/iterator.hpp>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork {
/////////////////////////////////////////////////////////////////////////////////////////////////

struct Scanner;
struct ScannerView;
struct ScannerLightView;

using scanner_ptr_t = std::shared_ptr<Scanner>;
using scanner_constptr_t = std::shared_ptr<const Scanner>;
using scannerview_ptr_t = std::shared_ptr<ScannerView>;
using scannerlightview_ptr_t      = std::shared_ptr<ScannerLightView>;
using scannerlightview_constptr_t = std::shared_ptr<const ScannerLightView>;
using match_fn_t = std::function<scannerview_ptr_t(const ScannerView&)>;

struct Token {
  int iline;
  int icol;
  uint64_t _class = -1;
  std::string text;
  Token(const std::string& txt, int il, int ic)
      : iline(il)
      , icol(ic)
      , text(txt) {
  }
};

enum scan_state {
  ESTA_NONE = 0,
  ESTA_C_COMMENT,
  ESTA_CPP_COMMENT,
  ESTA_DQ_STRING,
  ESTA_SQ_STRING,
  ESTA_WSPACE,
  ESTA_CONTENT

};

/////////////////////////////////////////
inline char to_lower(char ch) {
  return ((ch >= 'A') && (ch <= 'Z')) ? (ch - 'A' + 'a') : ch;
}
/////////////////////////////////////////
inline bool is_alf(char ch) {
  return (to_lower(ch) >= 'a') && (to_lower(ch <= 'z'));
}
/////////////////////////////////////////
inline bool is_num(char ch) {
  return (ch >= '0') && (ch <= '9');
}
/////////////////////////////////////////
inline bool is_alfnum(char ch) {
  return is_alf(ch) || is_num(ch);
}
/////////////////////////////////////////
inline bool is_spc(char ch) {
  return (ch == ' ') || (ch == ' ') || (ch == '\t');
}
/////////////////////////////////////////
inline bool is_septok(char ch) {
  return (ch == ';') || (ch == ':') || (ch == '{') || (ch == '}') || (ch == '[') || (ch == ']') || (ch == '(') || (ch == ')') ||
         (ch == '*') || (ch == '+') || (ch == '-') || (ch == '=') || (ch == ',') || (ch == '?') || (ch == '%') || (ch == '<') ||
         (ch == '>') || (ch == '&') || (ch == '|') || (ch == '!') || (ch == '/') || (ch == '#');
}
/////////////////////////////////////////
inline bool is_content(char ch) {
  return is_alfnum(ch) || (ch == '_') || (ch == '.');
}
/////////////////////////////////////////
struct ScanViewFilter {
  virtual bool Test(const Token& t) {
    return true;
  }
};
/////////////////////////////////////////

struct Scanner {
  using id_t = uint64_t;
  Scanner(
      std::string blockregex, //
      size_t capacity = 64 << 10);
  /////////////////////////////////////////
  void addRule(std::string rule, id_t state);
  void buildStateMachine();
  void scan();
  void scanString(std::string str);
  /////////////////////////////////////////
  inline size_t length() const {
    return _fxbuffer.size();
  }
  inline void resize(size_t length) {
    _fxbuffer.resize(length);
  }
  /////////////////////////////////////////
  const Token* token(size_t i) const;
  /////////////////////////////////////////
  void discardTokensOfClass(uint64_t tokclass);
  /////////////////////////////////////////
  ScannerView createTopView() const;
  /////////////////////////////////////////
  const size_t _kcapacity;
  std::vector<char> _fxbuffer;
  size_t ifilelen;
  std::vector<Token> tokens;
  std::string _blockregex;
  Token cur_token;
  scan_state ss;
  bool _quotedstrings = true;

  using match_t = lexertl::match_results<std::string::const_iterator,id_t>;
  using rules_t = lexertl::basic_rules<char,char,id_t>;
  using statemachine_t = lexertl::basic_state_machine<char,id_t>;
  using gen_t = lexertl::basic_generator<rules_t, statemachine_t>;
  using iter_t = lexertl::iterator<std::string::const_iterator,statemachine_t,match_t>;

  rules_t _rules;
  statemachine_t _statemachine;
};

struct ScanViewRegex : public ScanViewFilter {
  ScanViewRegex(const char*, bool inverse);

  bool Test(const Token& t) override;

  std::regex mRegex;
  bool mInverse;
};

struct ScanRange {
  ScanRange()
      : mStart(0)
      , mEnd(0) {
  }
  size_t mStart;
  size_t mEnd;
};

struct ScannerView {
  ScannerView(const Scanner& s, ScanViewFilter& f);
  ScannerView(const ScannerView& oth, int start_offset);

  size_t numTokens() const {
    return _indices.size();
  }
  const Token* token(size_t i) const;
  size_t globalTokenIndex(size_t i) const;
  void scanBlock(size_t is, bool checkterm = true, bool checkdecos = true);
  void scanUntil(size_t start, std::string terminator, bool includeterminator);
  void dump(const std::string& dumpid) const;
  size_t blockEnd() const;
  std::string blockName() const;

  std::string asString(bool use_spaces=true) const;

  const int numBlockDecorators() const {
    return _blockDecorators.size();
  }
  const Token* blockDecorator(size_t i) const;

  void checktoken(int actual_index, std::string expected) const;

  std::vector<int> _indices;
  ScanViewFilter& _filter;
  const Scanner& _scanner;
  std::regex _blockTerminators;

  size_t _start; // will point to lev0 { if exists in blockmode
  size_t _end;   // will point to lev0 } if exists in blockmode
  std::vector<int> _blockDecorators;
  size_t _blockType;
  size_t _blockName;
  bool _blockOk;
};

//////////////////////////////////////////////////////////////

struct ScannerLightView {
  ScannerLightView(const ScannerView& inp_view);
  ScannerLightView(const ScannerLightView& oth);
  void clear();
  void validate() const;
  int numTokens() const;
  bool empty() const;
  void advanceTo(size_t i);
  const Token* token(size_t i) const;
  void dump(const std::string& dumpid) const;
  const ScannerView& _input_view;
  size_t _start = -1;
  size_t _end   = -1;
};

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork
/////////////////////////////////////////////////////////////////////////////////////////////////
