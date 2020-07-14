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
struct Sample {
  Sample() {
    _packedbits[0] = 0;
    _packedbits[1] = 0;
    _packedbits[2] = 0;
    _packedbits[3] = 0;
  }
  uint64_t _packedbits[4];
  int _numbits = 0;
};

struct Signal {

  std::string _type      = "none";
  std::string _shortname = "";
  std::string _longname  = "";
  int _bit_width         = 0;
  int _word_width        = 0;
  std::unordered_map<uint64_t, sample_ptr_t> _unsorted_samples;
};

struct Scope {
  std::map<std::string, signal_ptr_t> _signals_by_shortname;
  std::map<std::string, scope_ptr_t> _child_scopes;
};

struct File : public Scope {
  File();
  void parse(ork::file::Path& path);
  Scanner _scanner;
};

} // namespace ork::hdl::vcd
