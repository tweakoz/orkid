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
  UNKNOWN = 65535,
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
  _scanner.addRule("[a-zA-Z_]+[a-zA-Z0-9_]+", int(TokenClass::IDENTIFIER));
  _scanner.addRule("#[0-9]+", int(TokenClass::TIMESTAMP));
  _scanner.addRule("[0-9]+", int(TokenClass::NUMBER));
  _scanner.addRule("\\[", int(TokenClass::OPENBRACKET));
  _scanner.addRule("\\]", int(TokenClass::CLOSEBRACKET));
  _scanner.addRule(":", int(TokenClass::COLON));
  _scanner.addRule("\\s+", int(TokenClass::WHITESPACE));
  _scanner.addRule("\\n", int(TokenClass::NEWLINE));
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
  size_t index = 0;
  while (index < numtoks) {
    auto& tok     = _scanner.tokens[index];
    auto& toktext = tok.text;
    switch (TokenClass(tok._class)) {
      case TokenClass::SECTION: {
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
        break;
      }
      case TokenClass::IDENTIFIER: {
        auto it = varset.find(toktext);
        if (it == varset.end()) {
          printf("IDENTIFIER<%s> not found\n", toktext.c_str());
          OrkAssert(false);
        }
        index++;
        break;
      }
      case TokenClass::WHITESPACE:
        index++;
        break;
      case TokenClass::NEWLINE:
        index++;
        break;
      case TokenClass::OPENBRACKET:
        index++;
        break;
      case TokenClass::CLOSEBRACKET:
        index++;
        break;
      case TokenClass::COLON:
        index++;
        break;
      case TokenClass::NUMBER: {
        auto it = varset.find(toktext);
        if (it != varset.end()) {
          printf("index<%zu> IDENTIFIER(n): %s\n", index, toktext.c_str());
        } else {
          printf("index<%zu>  NUMBER: %s\n", index, toktext.c_str());
        }
        index += 2;
        break;
      }
      case TokenClass::BITVECTOR:
        printf("index<%zu> BITVECTOR<%s>\n", index, toktext.c_str());
        index += 2;
        break;
      case TokenClass::TIMESTAMP:
        printf("index<%zu> TIMESTAMP<%s>\n", index, toktext.c_str());
        index += 2;
        break;
      case TokenClass::UNKNOWN: {
        auto it = varset.find(toktext);
        if (it != varset.end()) {
          printf("index<%zu> IDENTIFIER(u): %s\n", index, toktext.c_str());
        } else {
          printf("index<%zu>  UNKNOWN: %s\n", index, toktext.c_str());
          OrkAssert(false);
        }
        index++;
        break;
      }
      default:
        OrkAssert(false);
        break;
    }
  }
}

} // namespace ork::hdl::vcd
