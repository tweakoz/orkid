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

void Scanner::addRule(std::string rule, int state) {
  _rules.push(rule, state);
}
void Scanner::buildStateMachine() {
  lexertl::generator::build(_rules, _statemachine);
}

Scanner::Scanner(
    std::string blockregex)
    : ss(ESTA_NONE)
    , cur_token("", 0, 0)
    , ifilelen(0)
    , _blockregex(blockregex){
}
/////////////////////////////////////////
void Scanner::scanString(std::string str){
  std::copy( str.begin(), str.end(), std::back_inserter(_fxbuffer));
}
/////////////////////////////////////////
void Scanner::scan() {
  std::string as_str = _fxbuffer.data();
  lexertl::siterator iter(as_str.begin(), as_str.end(), _statemachine);
  lexertl::siterator end;

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

void Scanner::discardTokensOfClass(int tokclass) {

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
} // namespace ork
/////////////////////////////////////////////////////////////////////////////////////////////////
