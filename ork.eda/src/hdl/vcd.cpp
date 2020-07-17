#include <ork/hdl/vcd.h>
#include <ork/file/file.h>
#include <stack>

namespace ork::hdl::vcd {

File::File()
    : _scanner("") {
}

enum class TokenClass {
  SECTION = 1,
  IDENTIFIER,
  TIMESTAMP,
  BITVECTOR,
  NUMBER,
  OPENBRACKET,
  CLOSEBRACKET,
  COLON,
  WHITESPACE,
  NEWLINE,
  PRINTABLE,
  UNKNOWN = 65535,
};

enum class ParseState {
  INIT = 0, //
  SECTION,
  TIMESTAMP,
  VALUE,
  KEY
};

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

  _scanner.addRule("$[a-z]+", int(TokenClass::SECTION));
  _scanner.addRule("[bB][0-1z]+", int(TokenClass::BITVECTOR));
  _scanner.addRule("#[0-9]+", int(TokenClass::TIMESTAMP));
  _scanner.addRule("\\s+", int(TokenClass::WHITESPACE));
  _scanner.addRule("\\n+", int(TokenClass::NEWLINE));
  //_scanner.addRule("[a-zA-Z_]+[a-zA-Z0-9_]+", int(TokenClass::IDENTIFIER));
  _scanner.addRule("[!-~]+", int(TokenClass::PRINTABLE));
  _scanner.addRule("[0-9]+", int(TokenClass::NUMBER));
  _scanner.addRule("\\[", int(TokenClass::OPENBRACKET));
  _scanner.addRule("\\]", int(TokenClass::CLOSEBRACKET));
  _scanner.addRule(":", int(TokenClass::COLON));
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
  Sample cursample;
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

            auto copyofsample            = std::make_shared<Sample>(cursample);
            sig->_samples[cur_timestamp] = copyofsample;

            parse_state = ParseState::VALUE;
            break;
          }
          case ParseState::VALUE: {
            parse_state = ParseState::KEY;

            bool starts_bool =
                (toktext[0] == '0'    //
                 or toktext[0] == '1' //
                 or toktext[0] == 'z');

            if (starts_bool) {
              printf("mushed VALUE: %c\n", toktext[0]);
              cursample          = Sample();
              cursample._numbits = 1;
              if (toktext[0] == '1') {
                cursample.write(0, TriBool::TRUE);
              } else if (toktext[0] == '0') {
                cursample.write(0, TriBool::FALSE);
              } else if (toktext[0] == 'z') {
                cursample.write(0, TriBool::Z);
              }
              auto key = toktext.substr(1);
              auto it  = _signals_by_shortname.find(key);
              OrkAssert(it != _signals_by_shortname.end());
              auto sig = it->second;
              printf("mushed SIG<%s:%d:%s>\n", sig->_type.c_str(), sig->_bit_width, sig->_longname.c_str());
              auto copyofsample            = std::make_shared<Sample>(cursample);
              sig->_samples[cur_timestamp] = copyofsample;
              parse_state                  = ParseState::VALUE;
            } else {
              OrkAssert(false);
            }

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
        cursample          = Sample();
        cursample._numbits = numbitsinsample;
        for (int ibit = 0; ibit < numbitsinsample; ibit++) {
          char bitval = toktext[ibit + 1];
          int abit    = (numbitsinsample - 1) - ibit;
          switch (bitval) {
            case '0':
              cursample.write(abit, TriBool::FALSE);
              break;
            case '1': {
              cursample.write(abit, TriBool::TRUE);
              break;
            }
            case 'z': {
              cursample.write(abit, TriBool::Z);
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
} // namespace ork::hdl::vcd

void Sample::write(int bit, TriBool value) {
  _bits[bit] = value;
}

TriBool Sample::read(int bit) const {
  return _bits[bit];
}

std::string Sample::strvalue() const {

  std::string rval;

  int bits_pending = _numbits;

  auto nyb2char = [](int nyb) -> char {
    OrkAssert(nyb >= -1 and nyb < 16);
    char rval = 0;
    if (nyb < 0) {
      rval = 'z';
    } else if (nyb < 10) {
      rval = '0' + nyb;
    } else {
      rval = 'a' + (nyb - 10);
    }
    return rval;
  };

  size_t numadded = 0;
  while (bits_pending) {

    int ibit = numadded;
    int nyb  = 0;
    for (int i = 0; i < 4; i++)
      if (_bits[numadded + i] == TriBool::TRUE)
        nyb |= (1 << i);

    for (int i = 0; i < 4; i++)
      if (_bits[numadded + i] == TriBool::Z)
        nyb = -1;

    rval.push_back(nyb2char(nyb));

    bits_pending -= 4;
    numadded++;
    if (bits_pending < 0)
      bits_pending = 0;
    if (bits_pending and (numadded != 0) and ((numadded & 0x3) == 0))
      rval.push_back('.');
  }
  auto reversed = rval;
  std::reverse(reversed.begin(), reversed.end());

  return reversed;
}

} // namespace ork::hdl::vcd
