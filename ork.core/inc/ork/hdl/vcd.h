#pragma once

#include <ork/file/path.h>
#include <ork/util/scanner.h>

namespace ork::hdl::vcd {

struct Signal;
struct Scope;
struct File;

using signal_ptr_t = std::shared_ptr<Signal>;
using scope_ptr_t  = std::shared_ptr<Scope>;
using file_ptr_t   = std::shared_ptr<File>;

struct Signal {

  std::string _type      = "none";
  std::string _shortname = "";
  std::string _longname  = "";
  int _bit_width         = 0;
  int _word_width        = 0;
  std::vector<uint64_t> _storage;
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
