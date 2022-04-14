////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/file/path.h>
#include <ork/util/scanner.h>
#include <unordered_map>

namespace ork::hdl::vcd {
////////////////////////////////////////////////////////////////////////////////
struct Signal;
struct Scope;
struct File;
struct Sample;
////////////////////////////////////////////////////////////////////////////////
using signal_ptr_t = std::shared_ptr<Signal>;
using scope_ptr_t  = std::shared_ptr<Scope>;
using sample_ptr_t = std::shared_ptr<Sample>;
using file_ptr_t   = std::shared_ptr<File>;
////////////////////////////////////////////////////////////////////////////////
constexpr size_t kmaxbitlen = 64;
enum class LineState { FALSE, TRUE, X, Z };
////////////////////////////////////////////////////////////////////////////////
struct Sample {
  Sample() {
    for (int i = 0; i < kmaxbitlen; i++)
      _bits[i] = LineState::FALSE;
  }
  void left_extend(int width);
  void write(int bit, LineState value);
  LineState read(int bit) const;
  std::string strvalue() const;
  LineState _bits[kmaxbitlen];
  int _numbits = 0;
};
////////////////////////////////////////////////////////////////////////////////
struct Signal {

  std::string _type      = "none";
  std::string _shortname = "";
  std::string _longname  = "";
  int _bit_width         = 0;
  int _word_width        = 0;
  std::map<uint64_t, sample_ptr_t> _samples;
};
////////////////////////////////////////////////////////////////////////////////
struct Scope {
  std::string _name;
  std::string _type = "none";
  std::map<std::string, signal_ptr_t> _signals;
  std::map<std::string, scope_ptr_t> _child_scopes;
};
////////////////////////////////////////////////////////////////////////////////
struct File {
  File();
  void parse(ork::file::Path& path);
  Scanner _scanner;
  scope_ptr_t _root;
  std::map<std::string, scope_ptr_t> _child_scopes;
  std::map<std::string, signal_ptr_t> _signals_by_shortname;
  std::set<size_t> _timestamps;
};
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::hdl::vcd
