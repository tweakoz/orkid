#include <ork/hdl/vcd.h>
#include <ork/file/file.h>

namespace ork::hdl::vcd {

File::File()
    : _scanner("") {
}

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

  _scanner.Scan();

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
    ///////////////////////////////
    if (toktext == "\n") {
      index++;
    }
    ///////////////////////////////
    else if (toktext == "$date") {
      printf("GotDate\n");
      size_t end = find_end(index);
      if (end)
        index = end;
    }
    ///////////////////////////////
    else if (toktext == "$version") {
      printf("GotVersion\n");
      size_t end = find_end(index);
      if (end)
        index = end;
    }
    ///////////////////////////////
    else if (toktext == "$timescale") {
      printf("GotTimeScale\n");
      size_t end = find_end(index);
      if (end)
        index = end;
    }
    ///////////////////////////////
    else if (toktext == "$scope") {
      printf("GotScope\n");
      size_t end = find_end(index);
      if (end)
        index = end;
    }
    ///////////////////////////////
    else if (toktext == "$upscope") {
      printf("GotUpScope\n");
      size_t end = find_end(index);
      if (end)
        index = end;
    }
    ///////////////////////////////
    else if (toktext == "$var") {
      auto varname = _scanner.tokens[index + 3];
      printf("GotVar<%s>\n", varname.text.c_str());
      varset.insert(varname.text);
      size_t end = find_end(index);
      if (end)
        index = end;
    }
    ///////////////////////////////
    else if (toktext == "$enddefinitions") {
      printf("GotEndDefinitions\n");
      size_t end = find_end(index);
      if (end)
        index = end;
    }
    ///////////////////////////////
    else if (toktext == "$dumpvars") {
      size_t end = find_end(index);
      printf("GotDumpVars index<%zu> end<%zu>\n", index, end);
      if (end)
        index = end;
    }
    ///////////////////////////////
    else if (toktext[0] == '#' and tok.icol == 0) {
      printf("GotTimeSpec<%s>\n", toktext.c_str());
      index += 2;
    }
    ///////////////////////////////
    else if (toktext[0] == '0' and tok.icol == 0) {
      printf("GotZero index<%zu>\n", index);
      if (toktext.length() == 1) {
        index += 2;
      } else {
        index += 1;
      }
    }
    ///////////////////////////////
    else if (toktext[0] == '1' and tok.icol == 0) {
      printf("GotOne index<%zu>\n", index);
      if (toktext.length() == 1) {
        index += 2;
      } else {
        index += 1;
      }
    }
    ///////////////////////////////
    else if (toktext[0] == 'b' and tok.icol == 0) {
      printf("GotBin index<%zu> toktext<%s>\n", index, toktext.c_str());
      index += 2;
    }
    ///////////////////////////////
    else {
      auto it = varset.find(toktext);
      OrkAssert(it != varset.end());
      index++;
    }
  }
} // namespace ork::hdl::vcd

} // namespace ork::hdl::vcd
