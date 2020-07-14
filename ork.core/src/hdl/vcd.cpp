#include <ork/hdl/vcd.h>
#include <ork/file/file.h>

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
  _scanner.addRule("[bB][0-1]+", int(TokenClass::BITVECTOR));
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
  std::set<std::string> varset;
  ////////////////////////////////////
  ParseState parse_state = ParseState::INIT;
  ////////////////////////////////////
  size_t index = 0;
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
          printf("index<%zu> SCOPE\n", index);
        }
        ///////////////////////////////
        else if (toktext == "$upscope") {
          printf("index<%zu> UPSCOPE\n", index);
        }
        ///////////////////////////////
        else if (toktext == "$var") {
          auto varname = _scanner.tokens[index + 6];
          printf("index<%zu> VARDECL: %s\n", index, varname.text.c_str());
          varset.insert(varname.text);
        }
        ///////////////////////////////
        else if (toktext == "$enddefinitions") {
          printf("index<%zu> ENDDEFINITIONS\n", index);
        }
        ///////////////////////////////
        else if (toktext == "$dumpvars") {
          printf("index<%zu> DUMPVARS\n", index);
        } else {
          OrkAssert(false);
        }
        size_t end = find_end(index);
        if (end)
          index = end;
        parse_state = ParseState::VALUE;
        break;
      }
        /*    case TokenClass::IDENTIFIER: {
              auto it = varset.find(toktext);
              if (it == varset.end()) {
                printf("IDENTIFIER<%s> not found\n", toktext.c_str());
                OrkAssert(false);
              }
              index++;
              break;
            }*/
      case TokenClass::WHITESPACE:
        index++;
        break;
      case TokenClass::NEWLINE:
        index++;
        break;
      case TokenClass::PRINTABLE: {
        switch (parse_state) {
          case ParseState::KEY: {
            auto it = varset.find(toktext);
            OrkAssert(it != varset.end());
            printf("index<%zu> KEY: %s\n", index, toktext.c_str());
            parse_state = ParseState::VALUE;
            break;
          }
          case ParseState::VALUE: {
            parse_state = ParseState::KEY;

            bool starts_bool =
                (toktext[0] == '0' //
                 or toktext[0] == '1');

            if (starts_bool) {
              printf("mushed VALUE: %c\n", toktext[0]);
              auto key = toktext.substr(1);
              printf("mushed KEY: %s\n", key.c_str());
              auto it = varset.find(key);
              OrkAssert(it != varset.end());
              parse_state = ParseState::VALUE;
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
        printf("index<%zu> BITVECTOR<%s>\n", index, toktext.c_str());
        index++;
        parse_state = ParseState::KEY;
        break;
      }
      case TokenClass::TIMESTAMP:
        printf("index<%zu> TIMESTAMP<%s>\n", index, toktext.c_str());
        index += 2;
        parse_state = ParseState::VALUE;
        break;
      default:
        OrkAssert(false);
        break;
    }
  }
}

} // namespace ork::hdl::vcd
