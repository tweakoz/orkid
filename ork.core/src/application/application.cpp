////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <iostream>
#include <ork/pch.h>
#include <ork/application/application.h>
#include <ork/rtti/Class.h>
#include <ork/kernel/string/ResizableString.h>
#include <ork/kernel/string/PoolString.h>

#include <ork/util/Context.hpp>
#include <ork/kernel/environment.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

AppInitData::AppInitData(int argc, char** argv, char** envp) {
  _argc             = argc;
  _argv             = argv;
  _envp             = envp;
  _commandline_vars = std::make_shared<opts_var_map_t>();
  _fsinit           = std::make_shared<StdFileSystemInitalizer>(*this);
}

///////////////////////////////////////////////////////////////////////////////

AppInitData::~AppInitData() {
}

///////////////////////////////////////////////////////////////////////////////

AppInitData::opts_desc_ptr_t AppInitData::commandLineOptions(const char* header_text) {
  _commandline_desc = std::make_shared<opts_desc_t>(header_text);
  _commandline_vars = std::make_shared<opts_var_map_t>();
  return _commandline_desc;
}

///////////////////////////////////////////////////////////////////////////////

const po::variable_value& AppInitData::commandLineOption(const std::string& named) {
  return (*_commandline_vars)[named];
}

///////////////////////////////////////////////////////////////////////////////

AppInitData::opts_var_map_ptr_t AppInitData::parse() {
  if (_commandline_desc) {
    auto cmdline = po::parse_command_line(_argc, _argv, *_commandline_desc);
    po::store(cmdline, *_commandline_vars);
    po::notify(*_commandline_vars);
  }
  if (_commandline_vars->count("help")) {
    std::cout << (*_commandline_desc) << "\n";
    exit(0);
  }
  auto& vars = *_commandline_vars;

  if (_commandline_vars->count("fullscreen")) {
    this->_fullscreen = vars["fullscreen"].as<bool>();
  }
  if (_commandline_vars->count("offscreen")) {
    this->_offscreen  = vars["offscreen"].as<bool>();
    this->_fullscreen = false;
  }
  if (_commandline_vars->count("top")) {
    this->_top = vars["top"].as<int>();
  }
  if (_commandline_vars->count("left")) {
    this->_left = vars["left"].as<int>();
  }
  if (_commandline_vars->count("width")) {
    this->_width = vars["width"].as<int>();
  }
  if (_commandline_vars->count("height")) {
    this->_height = vars["height"].as<int>();
  }
  if (_commandline_vars->count("msaa")) {
    this->_msaa_samples = vars["msaa"].as<int>();
  }
  if (_commandline_vars->count("ssaa")) {
    this->_ssaa_samples = vars["ssaa"].as<int>();
  }
  // https://download.nvidia.com/XFree86/Linux-x86_64/525.78.0/README/openglenvvariables.html
  if (_commandline_vars->count("nvsync")) {

    bool do_vsync = vars["nvsync"].as<bool>();
    genviron.set("__GL_SYNC_TO_VBLANK", do_vsync ? "1" : "0");
    if (do_vsync) {
      int vsport = vars["nvsport"].as<int>();
      genviron.set("__GL_SYNC_DISPLAY_DEVICE", FormatString("DFP-%d", vsport));
    }
  }
  if (_commandline_vars->count("nvmfa")) {
    int vmfa = vars["nvmfa"].as<int>();
    genviron.set("__GL_MaxFramesAllowed", FormatString("%d", vmfa));
  }

  //genviron.dump();

  printf("_msaa_samples<%d>\n", this->_msaa_samples);
  return _commandline_vars;
}

StdFileSystemInitalizer::StdFileSystemInitalizer(const AppInitData& appinitdata)
    : _initdata(appinitdata) {
  // printf("CPA\n");
  //  OldSchool::SetGlobalStringVariable("temp://", CreateFormattedString("ork.data/temp/"));

  // printf("CPB\n");
  //////////////////////////////////////////
  // Register data:// urlbase

  // todo - hold somewhere not static
  static auto WorkingDirContext = std::make_shared<FileDevContext>();

  auto base_dir  = file::Path::orkroot_dir();
  auto src_core  = base_dir / "ork.core";
  auto src_lev2  = base_dir / "ork.lev2";
  auto data_dir  = base_dir / "ork.data";
  auto lev2_base = data_dir / "platform_lev2";
  auto srcd_base = data_dir / "src";

  //////////////////////////////////////////
  // Register urlbases
  //////////////////////////////////////////

  auto LocPlatformLevel2FileContext   = FileEnv::createContextForUriBase("lev2://", lev2_base);
  auto SrcPlatformLevel2FileContext   = FileEnv::createContextForUriBase("src://", srcd_base);
  auto LocPlatformMorkDataFileContext = FileEnv::createContextForUriBase("miniorkdata://", srcd_base);
  auto DataDirContext                 = FileEnv::createContextForUriBase("data://", data_dir);
  auto OrkidDirContext                = FileEnv::createContextForUriBase("orkid://", base_dir);
  auto OrkidCoreContext               = FileEnv::createContextForUriBase("ork_core://", src_core);
  auto OrkidLev2Context               = FileEnv::createContextForUriBase("ork_lev2://", src_lev2);

  //////////////
  // we dont want to see lev2:// in choice manager
  //////////////
  LocPlatformLevel2FileContext->_vars.makeValueForKey<void*>("disablechoices");
  //////////////
}
StdFileSystemInitalizer::~StdFileSystemInitalizer() {
}

// po::options_description desc("Allowed options");
// desc.add_options()                                                //
//   ("help", "produce help message")                              //
//("test", po::value<std::string>(), "test name (list,vo,nvo)") //
//("port", po::value<std::string>(), "midiport name (list)")    //
//("program", po::value<std::string>(), "program name")         //
//("hidpi", "hidpi mode");

// po::variables_map vars;
// po::store(po::parse_command_line(argc, argv, desc), vars);
// po::notify(vars);

PoolString addPooledStringFromStdString(const std::string& str) {
  return StringPoolContext::AddPooledString(PieceString(str.c_str()));
}
PoolString AddPooledString(const PieceString& ps) {
  return StringPoolContext::AddPooledString(ps);
}
PoolString AddPooledLiteral(const ConstString& cs) {
  return StringPoolContext::AddPooledLiteral(cs);
}
PoolString FindPooledString(const PieceString& ps) {
  return StringPoolContext::FindPooledString(ps);
}

StringPoolContext::StringPoolContext() {
}

///////////////////////////////////////////////////////////////////////////////

PoolString StringPoolContext::AddPooledString(const PieceString& string) {
  PoolString result = FindPooledString(string);
  if (result)
    return result;

  ResizableString copy(string);
  const char* data = copy.c_str();
  new (&copy) ResizableString;

  auto papp = StringPoolStack::top();
  OrkAssert(papp);

  return papp->_stringpool.String(data);
}

///////////////////////////////////////////////////////////////////////////////

PoolString StringPoolContext::AddPooledLiteral(const ConstString& string) {
  PoolString result = FindPooledString(string);
  if (result)
    return result;

  auto app = StringPoolStack::top();
  OrkAssert(app);

  return app->_stringpool.Literal(string);
}

///////////////////////////////////////////////////////////////////////////////

PoolString StringPoolContext::FindPooledString(const PieceString& string) {

  auto pAPP         = StringPoolStack::top();
  PoolString result = pAPP->_stringpool.Find(string);
  if (result)
    return result;
  return PoolString();
}

///////////////////////////////////////////////////////////////////////////////

PoolString operator"" _pool(const char* s, size_t len) {
  return AddPooledString(s);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////

template struct ork::util::GlobalStack<stringpoolctx_ptr_t>;
