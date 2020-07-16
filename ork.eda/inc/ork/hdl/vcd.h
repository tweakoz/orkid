#pragma once

#include <ork/file/path.h>
#include <ork/util/scanner.h>
#include <unordered_map>

namespace ork::hdl::vcd {

struct Signal;
struct Scope;
struct File;
struct Sample;

using signal_ptr_t = std::shared_ptr<Signal>;
using scope_ptr_t  = std::shared_ptr<Scope>;
using sample_ptr_t = std::shared_ptr<Sample>;
using file_ptr_t   = std::shared_ptr<File>;

constexpr size_t kmaxbitlen = 256;
constexpr size_t knumwords  = kmaxbitlen >> 6;
struct Sample {
  Sample() {
    for (int i = 0; i < knumwords; i++)
      _packedbits[i] = 0;
  }
  void write(int bit, bool value);
  bool read(int bit) const;
  std::string strvalue() const;
  uint64_t _packedbits[knumwords];
  int _numbits = 0;
};

struct Signal {

  std::string _type      = "none";
  std::string _shortname = "";
  std::string _longname  = "";
  int _bit_width         = 0;
  int _word_width        = 0;
  std::map<uint64_t, sample_ptr_t> _samples;
};

struct Scope {
  std::string _name;
  std::string _type = "none";
  std::map<std::string, signal_ptr_t> _signals;
  std::map<std::string, scope_ptr_t> _child_scopes;
};

struct File {
  File();
  void parse(ork::file::Path& path);
  Scanner _scanner;
  scope_ptr_t _root;
  std::map<std::string, scope_ptr_t> _child_scopes;
  std::map<std::string, signal_ptr_t> _signals_by_shortname;
  std::set<size_t> _timestamps;
};

} // namespace ork::hdl::vcd
