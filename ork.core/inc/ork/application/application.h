////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
using appinitdata_wkptr_t = std::weak_ptr<AppInitData>;

namespace po = ::boost::program_options;

struct StdFileSystemInitalizer {
  StdFileSystemInitalizer(const AppInitData& initdata);
  ~StdFileSystemInitalizer();
  const AppInitData& _initdata;
};

using stdfilesysinit_p = std::shared_ptr<StdFileSystemInitalizer>;

enum class AppInitOrder : uint64_t {
  REFLECTION_CLASS_REGISTRATION = 0,
  REFLECTION_LINK = 100,
  GRAPHICS_INIT = 200
};

struct AppInitData{

  using opts_desc_t = po::options_description;
  using opts_desc_ptr_t = std::shared_ptr<po::options_description>;
  using opts_var_map_t = po::variables_map;
  using opts_var_map_ptr_t = std::shared_ptr<po::variables_map>;

  void enqueuePreInitOp(AppInitOrder order, void_lambda_t l);
  void enqueuePostInitOp(AppInitOrder order, void_lambda_t l);
  
  opts_desc_ptr_t commandLineOptions(const char* header_text);

  opts_var_map_ptr_t parse();
  const po::variable_value& commandLineOption(const std::string& named);

  AppInitData(int argc=0, char** argv=nullptr, char** envp = nullptr);
  ~AppInitData();

  void executePreInitOps();
  void executePostInitOps();
  void finalizeInitialization();

  int _argc = 0;
  char** _argv = nullptr;
  char** _envp = nullptr;

  std::shared_ptr<StdFileSystemInitalizer> _fsinit;

  std::map<std::string,svar64_t> _miscvars;

  opts_desc_ptr_t _commandline_desc;
  opts_var_map_ptr_t _commandline_vars;
  std::vector<std::string> _dynaargs_storage;
  std::vector<char*> _dynaargs_refs;

  bool _enable_audio = false;
  bool _enable_audio_input = false;
  bool _enable_audio_output = false;
  bool _enable_audio_synth = false;
  bool _enable_graphics = true;
  bool _std_asset_catalog = true;

  bool _fullscreen = false;
  bool _offscreen = false;
  bool _canalwaysontop = false;
  int _top = 100;
  int _left = 100;
  int _width = 1280;
  int _height = 720;
  int _msaa_samples = 1;
  int _ssaa_samples = 0;
  int _swap_interval = 0;
  bool _update_rendersync = false;
  bool _allowHIDPI = false;
  bool _disableMouseCursor = false;
  std::string _audio_input_devname = "default";
  std::string _audio_output_devname = "default";
  std::string _audio_ioclass = "default";
  std::string _fullscreen_monitor = "none";
  size_t _audio_input_numchannels = 1;
  size_t _audio_output_numchannels = 2;
  bool _audio_stream_sync = false;
  bool _synchronous = false;
  float _target_ups = 480.0f;   // Updates per second (simulation tick rate)
  float _target_fps = 120.0f;   // Frames per second (render rate)
  std::string _monitor_id = "";
  std::string _application_name = "orkid_app";
  std::multimap<uint64_t,void_lambda_t> _preinitoperations;
  std::multimap<uint64_t,void_lambda_t> _postinitoperations;
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
