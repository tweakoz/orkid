#include <regex>
#include <stdlib.h>
#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/util/scanner.h>
#include <ork/kernel/string/deco.inl>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork {
/////////////////////////////////////////////////////////////////////////////////////////////////

ScanViewRegex::ScanViewRegex(const char* pr, bool inverse)
    : mRegex(pr)
    , mInverse(inverse) {
}
bool ScanViewRegex::Test(const Token& t) {
  bool match = std::regex_match(t.text, mRegex);
  return match xor mInverse;
}

ScannerView::ScannerView(const Scanner& s, ScanViewFilter& f)
    : _scanner(s)
    , _filter(f)
    , _blockTerminators(s._blockregex.c_str())
    , _start(0)
    , _end(0)
    , _blockType(0)
    , _blockName(0)
    , _blockOk(false) {
}

ScannerView::ScannerView(const ScannerView& oth, int startingpoint)
    : _scanner(oth._scanner)
    , _filter(oth._filter)
    , _blockTerminators(_scanner._blockregex.c_str())
    , _start(0)
    , _end(0)
    , _blockType(0)
    , _blockName(0)
    , _blockOk(false) {

  size_t idx = oth.globalTokenIndex(startingpoint);
  scanBlock(idx, false, false);
}

void ScannerView::checktoken(int actual_index, std::string expected) const {
  auto chktok = this->token(actual_index)->text;
  if (chktok != expected) {
    deco::printf(fvec3::Red(), "invalid token: ");
    deco::printf(fvec3::White(), "%s ", chktok.c_str());
    deco::printf(fvec3::Red(), ", expected '%s'\n", expected.c_str());

    for (int i = -2; i < 3; i++) {
      int tokidx = actual_index + i;
      auto tok   = this->token(tokidx)->text;
      deco::printf(fvec3::Yellow(), "tok<%d:%s>\n", tokidx, tok.c_str());
    }
    OrkAssert(false);
  }
}

size_t ScannerView::blockEnd() const {
  return globalTokenIndex(_end);
}

std::string ScannerView::blockName() const {
  return token(_blockName)->text;
}

void ScannerView::scanBlock(size_t is, bool checkterm, bool checkdecos) {

  size_t max_t = _scanner.tokens.size();

  int ibracelev = 0;
  int istate    = checkterm ? 0 : 1;

  const Token& block_name = _scanner.tokens[is];

  if (checkterm) {
    _blockName = is;
  }

  _indices.clear();

  // printf( "ScanBlock name<%s> is<%zu>\n", block_name.text.c_str(), is );

  for (size_t i = is; i < max_t; i++) {
    const Token& t = _scanner.tokens[i];
    bool is_term   = std::regex_match(t.text, _blockTerminators);

    bool is_open  = (t.text == "{");
    bool is_close = (t.text == "}");

    // printf( "itok<%zu> t<%s> istate<%d> is_open<%d> is_close<%d> is_term<%d>\n",
    //		i, t.text.c_str(), istate, int(is_open), int(is_close), int(is_term) );

    fflush(stdout);

    switch (istate) {
      case 0: // have not yet found block type
      {
        if (is_term) {
          _blockType = _indices.size();
          _blockName = _blockType + 1;
          istate     = 1;
          _indices.push_back(i);
        } else {
          _indices.push_back(i);
          assert(false == is_open && false == is_close);
        }
        break;
      }
      case 1: // have not yet found starting brace
      {
        if (is_open) {
          assert(ibracelev == 0);
          _start = _indices.size();
          ibracelev++;
          istate = 2;
          _indices.push_back(i);
        } else if (checkdecos and (t.text == ":")) {
          i++;
          _blockDecorators.push_back(i);
        } else {
          assert(false == is_close);
          if (is_term)
            assert(i > is);
          if (_filter.Test(t))
            _indices.push_back(i);
        }
        break;
      }
      case 2: // content
        if (is_open) {
          ibracelev++;
          _indices.push_back(i);
        } else if (is_close) {
          ibracelev--;
          _indices.push_back(i);
          if (ibracelev == 0) {
            _end     = _indices.size() - 1;
            _blockOk = true;
            return;
          }
        } else {
          if (_filter.Test(t))
            _indices.push_back(i);
        }
        break;
    }
  }
}

void ScannerView::scanUntil(size_t start, std::string terminator, bool includeterminator) {
  size_t max_t = _scanner.tokens.size();
  size_t i     = start;
  bool done    = false;
  while (not done) {
    const auto& tok = _scanner.tokens[i].text;
    done            = (tok == terminator);
    if ((not done) or includeterminator)
      _indices.push_back(i);
    i++;
  }
  _start = 0;
  _end   = _indices.size() - 1;
}

void ScannerView::dump() {
  printf("ScannerView<%p>::Dump()\n", this);

  printf(" _blockOk<%d>\n", int(_blockOk));
  printf(" _start<%d>\n", int(_start));
  printf(" _end<%d>\n", int(_end));
  printf(" _blockType<%d>\n", int(_blockType));
  printf(" _blockName<%d>\n", int(_blockName));

  int i = 0;
  for (int tokidx : _indices) {
    if (tokidx < _scanner.tokens.size()) {
      printf("tok<%d> idx<%d> val<%s>\n", i, tokidx, _scanner.tokens[tokidx].text.c_str());
    }
    i++;
  }
}

const Token* Scanner::token(size_t i) const {
  const Token* pt = nullptr;
  if (i < tokens.size()) {
    pt = &tokens[i];
  }
  return pt;
}

const Token* ScannerView::token(size_t i) const {
  const Token* pt = nullptr;
  if (i < _indices.size()) {
    int tokidx = _indices[i];
    pt         = _scanner.token(tokidx);
  }

  return pt;
}

const Token* ScannerView::blockDecorator(size_t i) const {
  const Token* pt = nullptr;

  if (i < _blockDecorators.size()) {
    int tokidx = _blockDecorators[i];
    pt         = _scanner.token(tokidx);
  }
  return pt;
}

size_t ScannerView::globalTokenIndex(size_t i) const {
  size_t ret = ~0;
  if (i < _indices.size()) {
    ret = _indices[i];
  }
  return ret;
}

Scanner::Scanner(
    std::string blockregex, //
    size_t capacity)
    : ss(ESTA_NONE)
    , cur_token("", 0, 0)
    , ifilelen(0)
    , _blockregex(blockregex)
    , _kcapacity(capacity) {
  _fxbuffer.resize(capacity);

  _rules.push("[0-9]+", 1);
  _rules.push("[a-z]+", 2);
  lexertl::generator::build(_rules, _statemachine);
}

void Scanner::FlushToken() {
  if (cur_token.text.length())
    tokens.push_back(cur_token);
  assert(cur_token.text != ".0");
  cur_token.text  = "";
  cur_token.iline = 0;
  cur_token.icol  = 0;
  ss              = ESTA_NONE;
}
/////////////////////////////////////////
void Scanner::AddToken(const Token& tok) {
  assert(tok.text != ".0");
  tokens.push_back(tok);
  cur_token.text  = "";
  cur_token.iline = 0;
  cur_token.icol  = 0;
  ss              = ESTA_NONE;
}
/////////////////////////////////////////
void Scanner::Scan() {

  int iscanst_cpp_comment = 0;
  int iscanst_c_comment   = 0;
  int iscanst_whitespace  = 0;
  int iscanst_dqstring    = 0;
  int iscanst_sqstring    = 0;

  int iline = 0;
  int icol  = 0;

  int itoksta_line = 0;
  int itoksta_colm = 0;

  for (size_t i = 0; i < ifilelen; i++) {
    char PCH = (i == 0) ? 0 : _fxbuffer[i - 1];
    char CH  = _fxbuffer[i];
    char NCH = (i < ifilelen - 1) ? _fxbuffer[i + 1] : 0;

    char ch_buf[2];
    ch_buf[0] = CH;
    ch_buf[1] = 0;

    bool benctok = false;

    int adv_col = 1;
    int adv_lin = 0;

    switch (ss) {
      case ESTA_NONE:
        if ((CH == '/') && (NCH == '/')) {
          ss = ESTA_CPP_COMMENT;
          iscanst_cpp_comment++;
          i++;
        } else if ((CH == '/') && (NCH == '*')) {
          ss = ESTA_C_COMMENT;
          iscanst_c_comment++;
          i++;
        } else if (CH == '\'') {
          if (_quotedstrings) {
            ss      = ESTA_SQ_STRING;
            benctok = true;
          } else {
            AddToken(Token("\'", iline, icol));
          }
        } else if (CH == '\"') {
          if (_quotedstrings) {
            ss      = ESTA_DQ_STRING;
            benctok = true;
          } else {
            AddToken(Token("\"", iline, icol));
          }
        } else if (is_spc(CH)) {
          ss = ESTA_WSPACE;
        } else if (CH == '\n') {
          AddToken(Token("\n", iline, icol));
          adv_lin = 1;
        } else if (
            CH == '.'           //
            and not is_num(PCH) //
            and not is_num(NCH)) {
          AddToken(Token(".", iline, icol));
        } else if (is_septok(CH)) {
          if (((CH == '=') && (NCH == '=')) || ((CH == '!') && (NCH == '=')) || ((CH == '*') && (NCH == '=')) ||
              ((CH == '/') && (NCH == '=')) || ((CH == '&') && (NCH == '=')) || ((CH == '|') && (NCH == '=')) ||
              ((CH == '&') && (NCH == '&')) || ((CH == '|') && (NCH == '|')) || ((CH == '<') && (NCH == '<')) ||
              ((CH == '>') && (NCH == '>')) || ((CH == '<') && (NCH == '=')) || ((CH == '>') && (NCH == '=')) ||
              ((CH == ':') && (NCH == ':')) || ((CH == '$') && (NCH == '(')) || ((CH == '+') && (NCH == '+')) ||
              ((CH == '-') && (NCH == '-')) || ((CH == '+') && (NCH == '=')) || ((CH == '-') && (NCH == '=')) //
          ) {
            char ch_buf2[3];
            ch_buf2[0] = CH;
            ch_buf2[1] = NCH;
            ch_buf2[2] = 0;
            AddToken(Token(ch_buf2, iline, icol));
            i++;
          } else
            AddToken(Token(ch_buf, iline, icol));

        } else {
          ss              = ESTA_CONTENT;
          benctok         = true;
          cur_token.iline = iline;
          cur_token.icol  = icol;
        }
        break;
      case ESTA_C_COMMENT:
        if ((CH == '/') && (PCH == '*')) {
          iscanst_c_comment--;
          if (iscanst_c_comment == 0)
            ss = ESTA_NONE;
        }
        break;
      case ESTA_CPP_COMMENT:
        if (CH == '/') {
        }
        if (CH == '\n') {
          ss      = ESTA_NONE;
          adv_lin = 1;
        }
        break;
      case ESTA_DQ_STRING:
        if (CH == '\"') {
          cur_token.text += ch_buf;
          FlushToken();
        } else {
          benctok = true;
        }
        break;
      case ESTA_SQ_STRING:
        if (CH == '\'') {
          cur_token.text += ch_buf;
          FlushToken();
        } else {
          benctok = true;
        }
        break;
      case ESTA_WSPACE:
        if ((false == is_spc(CH)) && (CH != '\n')) {
          ss = ESTA_NONE;
          i--;
        }
        break;
      case ESTA_CONTENT: {
        if (CH == '.' and is_num(PCH)) {
          benctok = true;
        } else if (PCH == '.' and is_num(CH)) {
          benctok = true;
        } else if (is_septok(CH)) {
          FlushToken();
          i--; // put sep back
        } else if (CH == '\n') {
          FlushToken();
          adv_lin = 1;
        } else if (is_spc(CH)) {
          FlushToken();
        } else if (CH == '.') {
          FlushToken();
          i--; // put . back
        } else if (is_content(CH)) {
          benctok = true;
        }
        break;
      }
    }
    if (benctok) {
      cur_token.text += ch_buf;
    }
    if (adv_col) {
      icol += adv_col;
    }
    if (adv_lin) {
      iline++;
      icol = 0;
    }
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork
/////////////////////////////////////////////////////////////////////////////////////////////////
