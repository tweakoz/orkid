////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/hdl/vcd.h>
#include <ork/file/file.h>
#include <stack>
#include <ork/util/crc.h>


namespace ork::hdl::vcd {
////////////////////////////////////////////////////////////////////////////////
File::File()
    : _scanner("") {
}
////////////////////////////////////////////////////////////////////////////////
enum class TokenClass : uint64_t {
  CrcEnum(SECTION),
  CrcEnum(IDENTIFIER),
  CrcEnum(TIMESTAMP),
  CrcEnum(BITVECTOR),
  CrcEnum(NUMBER),
  CrcEnum(OPENBRACKET),
  CrcEnum(CLOSEBRACKET),
  CrcEnum(COLON),
  CrcEnum(WHITESPACE),
  CrcEnum(NEWLINE),
  CrcEnum(PRINTABLE),
  CrcEnum(UNKNOWN),
};
////////////////////////////////////////////////////////////////////////////////
enum class ParseState {
  INIT = 0, //
  SECTION,
  TIMESTAMP,
  VALUE,
  KEY
};
////////////////////////////////////////////////////////////////////////////////
void File::parse(ork::file::Path& inppath) {

  _root = std::make_shared<Scope>();

  ::ork::File vcdfile(inppath, EFM_READ);
  size_t length = 0;
  vcdfile.GetLength(length);
  std::string vcdstr;
  vcdstr.resize(length + 1);

  _scanner.resize(length + 1);
  vcdfile.Read(_scanner._fxbuffer.data(), length);
  _scanner._fxbuffer.data()[length] = 0;
  _scanner.ifilelen                 = length;
  _scanner._quotedstrings           = false;

  _scanner.addRule("$[a-z]+", uint64_t(TokenClass::SECTION));
  _scanner.addRule("[bB][0-1xz]+", uint64_t(TokenClass::BITVECTOR));
  _scanner.addRule("#[0-9]+", uint64_t(TokenClass::TIMESTAMP));
  _scanner.addRule("\\s+", uint64_t(TokenClass::WHITESPACE));
  _scanner.addRule("\\n+", uint64_t(TokenClass::NEWLINE));
  //_scanner.addRule("[a-zA-Z_]+[a-zA-Z0-9_]+", uint64_t(TokenClass::IDENTIFIER));
  _scanner.addRule("[!-~]+", uint64_t(TokenClass::PRINTABLE));
  _scanner.addRule("[0-9]+", uint64_t(TokenClass::NUMBER));
  _scanner.addRule("\\[", uint64_t(TokenClass::OPENBRACKET));
  _scanner.addRule("\\]", uint64_t(TokenClass::CLOSEBRACKET));
  _scanner.addRule(":", uint64_t(TokenClass::COLON));
  _scanner.buildStateMachine();

  _scanner.scan();

  size_t numtoks = _scanner.tokens.size();
  ////////////////////////////////////
  auto find_end = [&](size_t start) -> size_t {
    size_t index = start;
    while (index < numtoks) {
      auto& tok = _scanner.tokens[index].text;
      if (tok == "$end") {
        return index + 1;
      }
      index++;
    }
    return 0;
  };
  std::map<std::string, std::string> varmap;
  ////////////////////////////////////
  ParseState parse_state = ParseState::INIT;
  size_t cur_timestamp   = 0;
  std::stack<scope_ptr_t> scope_stack;
  scope_stack.push(_root);
  ////////////////////////////////////
  sample_ptr_t cursample;
  ////////////////////////////////////
  size_t index               = 0;
  std::string __prev_section = "";
  while (index < numtoks) {
    auto& tok     = _scanner.tokens[index];
    auto& toktext = tok.text;
    auto tokclass = TokenClass(tok._class);
    switch (tokclass) {
      case TokenClass::SECTION: {
        parse_state = ParseState::SECTION;
        ///////////////////////////////
        if (toktext == "$date") {
          printf("DATE\n");
        }
        ///////////////////////////////
        else if (toktext == "$version") {
          printf("VERSION\n");
        }
        ///////////////////////////////
        else if (toktext == "$timescale") {
          auto val  = _scanner.tokens[index + 2].text;
          auto unit = _scanner.tokens[index + 3].text;
          int ival  = atoi(val.c_str());
          printf("index<%zu> TIMESCALE %s %s\n", index, val.c_str(), unit.c_str());
        }
        ///////////////////////////////
        else if (toktext == "$scope") {

          auto top = scope_stack.top();

          auto type    = _scanner.tokens[index + 2].text;
          auto named   = _scanner.tokens[index + 4].text;
          auto scope   = std::make_shared<Scope>();
          scope->_type = type;
          scope->_name = named;

          top->_child_scopes[named] = scope;
          scope_stack.push(scope);

          printf("index<%zu> SCOPE\n", index);
        }
        ///////////////////////////////
        else if (toktext == "$upscope") {
          printf("index<%zu> UPSCOPE\n", index);
          scope_stack.pop();
        }
        ///////////////////////////////
        else if (toktext == "$var") {
          auto vartype  = _scanner.tokens[index + 2];
          auto varwidth = _scanner.tokens[index + 4];
          auto varshort = _scanner.tokens[index + 6];
          auto varlong  = _scanner.tokens[index + 8];
          printf(
              "index<%zu> VARDECL: %s -> %s\n", //
              index,
              varshort.text.c_str(),
              varlong.text.c_str());
          varmap[varshort.text] = varlong.text;

          auto sig         = std::make_shared<Signal>();
          sig->_shortname  = varshort.text;
          sig->_longname   = varlong.text;
          sig->_type       = vartype.text;
          sig->_bit_width  = atoi(varwidth.text.c_str());
          sig->_word_width = sig->_bit_width >> 6;

          _signals_by_shortname[varshort.text] = sig;

          auto top                    = scope_stack.top();
          top->_signals[varlong.text] = sig;

          OrkAssert(sig->_bit_width <= kmaxbitlen);
        }
        ///////////////////////////////
        else if (toktext == "$enddefinitions") {
          printf("index<%zu> ENDDEFINITIONS\n", index);
        }
        ///////////////////////////////
        else if (toktext == "$dumpvars") {
          printf("index<%zu> DUMPVARS\n", index);
          index++;
          parse_state = ParseState::VALUE;
          break;
        }
        ///////////////////////////////
        else if (toktext == "$end") {
          if (__prev_section == "$dumpvars") {
            index++;
            break;
          }
        }
        ///////////////////////////////
        else {
          OrkAssert(false);
        }
        __prev_section = toktext;
        size_t end     = find_end(index);
        if (end)
          index = end;
        parse_state = ParseState::VALUE;
        break;
      }
      case TokenClass::WHITESPACE:
        index++;
        break;
      case TokenClass::NEWLINE:
        index++;
        break;
      case TokenClass::PRINTABLE: {
        switch (parse_state) {
          case ParseState::KEY: {
            auto top = scope_stack.top();

            auto it = _signals_by_shortname.find(toktext);
            OrkAssert(it != _signals_by_shortname.end());
            auto sig = it->second;
            printf("index<%zu> SIG<%s:%d:%s>\n", index, sig->_type.c_str(), sig->_bit_width, sig->_longname.c_str());
            cursample->left_extend(sig->_bit_width);
            sig->_samples[cur_timestamp] = cursample;
            cursample                    = nullptr;
            parse_state                  = ParseState::VALUE;
            break;
          }
          case ParseState::VALUE: {
            parse_state = ParseState::KEY;

            bool starts_bool =
                (toktext[0] == '0'    //
                 or toktext[0] == '1' //
                 or toktext[0] == 'x' //
                 or toktext[0] == 'z');

            OrkAssert(starts_bool);

            char ch = toktext[0];

            printf("mushed VALUE: %c\n", ch);
            LineState lstate;
            switch (ch) {
              case '0':
                lstate = LineState::FALSE;
                break;
              case '1':
                lstate = LineState::TRUE;
                break;
              case 'x':
                lstate = LineState::X;
                break;
              case 'z':
                lstate = LineState::Z;
                break;
              default:
                lstate = LineState::FALSE;
                OrkAssert(false);
                break;
            }

            auto key = toktext.substr(1);
            auto it  = _signals_by_shortname.find(key);
            OrkAssert(it != _signals_by_shortname.end());
            auto sig            = it->second;
            cursample           = std::make_shared<Sample>();
            cursample->_numbits = 1;
            cursample->write(0, lstate);
            cursample->left_extend(sig->_bit_width);

            printf(
                "mushed SIG<%s:%d:%s>\n", //
                sig->_type.c_str(),
                sig->_bit_width,
                sig->_longname.c_str());

            sig->_samples[cur_timestamp] = cursample;
            cursample                    = nullptr;
            parse_state                  = ParseState::VALUE;

            break;
          }
          default: {
            OrkAssert(false);
            break;
          }
        }
        index++;
        break;
      }
      case TokenClass::BITVECTOR: {
        OrkAssert(parse_state == ParseState::VALUE);
        int numbitsinsample = toktext.length() - 1;
        printf("index<%zu> BITVECTOR<%d:%s>\n", index, numbitsinsample, toktext.c_str());
        cursample           = std::make_shared<Sample>();
        cursample->_numbits = numbitsinsample;
        for (int ibit = 0; ibit < numbitsinsample; ibit++) {
          char bitval = toktext[ibit + 1];
          int abit    = (numbitsinsample - 1) - ibit;
          switch (bitval) {
            case '0':
              cursample->write(abit, LineState::FALSE);
              break;
            case '1': {
              cursample->write(abit, LineState::TRUE);
              break;
            }
            case 'x': {
              cursample->write(abit, LineState::X);
              break;
            }
            case 'z': {
              cursample->write(abit, LineState::Z);
              break;
            }
            default:
              OrkAssert(false);
              break;
          }
        }

        index++;
        parse_state = ParseState::KEY;
        break;
      }
      case TokenClass::TIMESTAMP: {
        auto timestr  = toktext.substr(1);
        auto timeval  = size_t(atol(timestr.c_str()));
        cur_timestamp = timeval;
        _timestamps.insert(timeval);
        printf("index<%zu> TIMESTAMP<%zu>\n", index, cur_timestamp);
        parse_state = ParseState::VALUE;
        index++;
        break;
      }
      default:
        OrkAssert(false);
        break;
    }
  }
  OrkAssert(scope_stack.top() == _root);
}
////////////////////////////////////////////////////////////////////////////////
void Sample::write(int bit, LineState value) {
  _bits[bit] = value;
}
////////////////////////////////////////////////////////////////////////////////
LineState Sample::read(int bit) const {
  return _bits[bit];
}
////////////////////////////////////////////////////////////////////////////////
void Sample::left_extend(int width) {
  LineState extension = _bits[_numbits - 1];
  // extension rules
  // https://www.eg.bucknell.edu/~csci320/2016-fall/wp-content/uploads/2015/08/verilog-std-1364-2005.pdf
  //  page 331
  switch (extension) {
    case LineState::FALSE:
    case LineState::TRUE:
      extension = LineState::FALSE;
      break;
    case LineState::X:
    case LineState::Z:
      break;
  }
  for (int i = _numbits; i < width; i++) {
    _bits[i] = extension;
  }
  _numbits = width;
}
////////////////////////////////////////////////////////////////////////////////
std::string Sample::strvalue() const {

  std::string rval;

  int bits_pending = _numbits;

  auto nyb2char = [](int nyb) -> char {
    OrkAssert(nyb >= -2 and nyb < 16);
    char rval = 0;
    if (nyb == -1) {
      rval = 'z';
    } else if (nyb == -2) {
      rval = 'x';
    } else if (nyb < 10) {
      rval = '0' + nyb;
    } else {
      rval = 'a' + (nyb - 10);
    }
    return rval;
  };

  size_t bitsadded = 0;
  while (bits_pending) {

    int numthisiter = std::clamp(bits_pending, 1, 4);

    int nyb = 0;
    for (int i = 0; i < numthisiter; i++)
      if (_bits[bitsadded + i] == LineState::TRUE)
        nyb |= (1 << i);

    for (int i = 0; i < numthisiter; i++)
      if (_bits[bitsadded + i] == LineState::Z)
        nyb = -1;

    for (int i = 0; i < numthisiter; i++)
      if (_bits[bitsadded + i] == LineState::X)
        nyb = -2;

    rval.push_back(nyb2char(nyb));

    bits_pending -= numthisiter;
    bitsadded += numthisiter;

    if (bits_pending and //
        ((bitsadded & 0xf) == 0))
      rval.push_back('.');
  }
  auto reversed = rval;
  std::reverse(reversed.begin(), reversed.end());

  return reversed;
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::hdl::vcd
