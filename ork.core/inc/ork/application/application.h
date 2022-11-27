////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

//#include <ork/orkstl.h>

#include <ork/kernel/core/singleton.h>
#include <ork/kernel/string/StringPool.h>
#include <ork/kernel/any.h>
#include <ork/util/Context.h>
#include <ork/file/file.h>

#include <ork/rtti/RTTI.h>
#include <ork/object/Object.h>

#include <boost/program_options.hpp>


namespace ork {

/**
 * Very high-level application code.
 */

struct AppInitData;

using appinitdata_ptr_t = std::shared_ptr<AppInitData>;

namespace po = ::boost::program_options;

struct StdFileSystemInitalizer {
  StdFileSystemInitalizer(const AppInitData& initdata);
  ~StdFileSystemInitalizer();
  const AppInitData& _initdata;
};

using stdfilesysinit_p = std::shared_ptr<StdFileSystemInitalizer>;

struct AppInitData{

  using opts_desc_t = po::options_description;
  using opts_desc_ptr_t = std::shared_ptr<po::options_description>;
  using opts_var_map_t = po::variables_map;
  using opts_var_map_ptr_t = std::shared_ptr<po::variables_map>;

  void enqueuePreInitOp(void_lambda_t l) { _preinitoperations.push_back(l); }
  void enqueuePostInitOp(void_lambda_t l) { _postinitoperations.push_back(l); }

  opts_desc_ptr_t commandLineOptions(const char* header_text);

  opts_var_map_ptr_t parse();

  AppInitData(int argc=0, char** argv=nullptr, char** envp = nullptr);
  ~AppInitData();

  int _argc = 0;
  char** _argv = nullptr;
  char** _envp = nullptr;

  std::shared_ptr<StdFileSystemInitalizer> _fsinit;

  std::map<std::string,svar64_t> _miscvars;

  opts_desc_ptr_t _commandline_desc;
  opts_var_map_ptr_t _commandline_vars;

  bool _fullscreen = false;
  bool _offscreen = false;
  bool _imgui = false;
  int _top = 100;
  int _left = 100;
  int _width = 1280;
  int _height = 720;
  int _msaa_samples = 1;
  int _ssaa_samples = 1;
  int _swap_interval = 1;
  bool _update_rendersync = false;
  bool _allowHIDPI = true;
  std::string _monitor_id = "";
  std::string _application_name = "orkid_app";
  std::vector<void_lambda_t> _preinitoperations;
  std::vector<void_lambda_t> _postinitoperations;
};

struct StringPoolContext {

	static PoolString AddPooledString(const PieceString &);
	static PoolString AddPooledLiteral(const ConstString &);
	static PoolString FindPooledString(const PieceString &);

	StringPoolContext();

private:

    StringPool _stringpool;

};

PoolString addPooledStringFromStdString(const std::string& str);
PoolString AddPooledString(const PieceString &ps);
PoolString AddPooledLiteral(const ConstString &cs);
PoolString FindPooledString(const PieceString &ps);

PoolString operator"" _pool(const char* s, size_t len);

}

using stringpoolctx_ptr_t = std::shared_ptr<ork::StringPoolContext>;
using StringPoolStack = ork::util::GlobalStack<stringpoolctx_ptr_t>;
