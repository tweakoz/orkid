#pragma once

#include <regex>
#include <lexertl/generator.hpp>
#include <lexertl/lookup.hpp>
#include <lexertl/iterator.hpp>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork {
/////////////////////////////////////////////////////////////////////////////////////////////////

struct Token {
  int iline;
  int icol;
  int _class = -1;
  std::string text;
  Token(const std::string& txt, int il, int ic)
      : text(txt)
      , iline(il)
      , icol(ic) {
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

struct Scanner {
  Scanner(
      std::string blockregex, //
      size_t capacity = 64 << 10);
  /////////////////////////////////////////
  void addRule(std::string rule, int state);
  void buildStateMachine();
  void scan();
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
  void discardTokensOfClass(int tokclass);
  /////////////////////////////////////////
  const size_t _kcapacity;
  std::vector<char> _fxbuffer;
  size_t ifilelen;
  std::vector<Token> tokens;
  std::string _blockregex;
  Token cur_token;
  scan_state ss;
  bool _quotedstrings = true;

  lexertl::rules _rules;
  lexertl::state_machine _statemachine;
};

struct ScanViewFilter {
  virtual bool Test(const Token& t) {
    return true;
  }
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
  void dump();
  size_t blockEnd() const;
  std::string blockName() const;

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
/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork
/////////////////////////////////////////////////////////////////////////////////////////////////
