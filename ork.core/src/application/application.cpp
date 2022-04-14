////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/application/application.h>
#include <ork/rtti/Class.h>
#include <ork/kernel/string/ResizableString.h>
#include <ork/kernel/string/PoolString.h>

#include <ork/util/Context.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

AppInitData::AppInitData(int argc, char** argv, char** envp){
    _argc = argc;
    _argv = argv;
    _envp = envp;

    _fsinit = std::make_shared<StdFileSystemInitalizer>(*this);
}

///////////////////////////////////////////////////////////////////////////////

AppInitData::~AppInitData(){
  
}

///////////////////////////////////////////////////////////////////////////////

AppInitData::opts_desc_ptr_t AppInitData::commandLineOptions(const char* header_text){
  _commandline_desc = std::make_shared<opts_desc_t>(header_text);
  _commandline_vars = std::make_shared<opts_var_map_t>();
  return _commandline_desc;
}

AppInitData::opts_var_map_ptr_t AppInitData::parse(){
  if(_commandline_desc){
    auto cmdline = po::parse_command_line(_argc,_argv,*_commandline_desc);
    po::store(cmdline,*_commandline_vars);
    po::notify(*_commandline_vars);
  }
  return _commandline_vars;
}

StdFileSystemInitalizer::StdFileSystemInitalizer(const AppInitData& appinitdata) : _initdata(appinitdata) {
  //printf("CPA\n");
  // OldSchool::SetGlobalStringVariable("temp://", CreateFormattedString("ork.data/temp/"));

  // printf("CPB\n");
  //////////////////////////////////////////
  // Register data:// urlbase

  // todo - hold somewhere not static
  static auto WorkingDirContext = std::make_shared<FileDevContext>();

  auto base_dir  = file::Path::orkroot_dir();
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

  //////////////
  // we dont want to see lev2:// in choice manager
  //////////////
  LocPlatformLevel2FileContext->_vars.makeValueForKey<void*>("disablechoices");
  //////////////
}
StdFileSystemInitalizer::~StdFileSystemInitalizer() {
}

  //po::options_description desc("Allowed options");
  //desc.add_options()                                                //
    //  ("help", "produce help message")                              //
      //("test", po::value<std::string>(), "test name (list,vo,nvo)") //
      //("port", po::value<std::string>(), "midiport name (list)")    //
      //("program", po::value<std::string>(), "program name")         //
      //("hidpi", "hidpi mode");

  //po::variables_map vars;
  //po::store(po::parse_command_line(argc, argv, desc), vars);
  //po::notify(vars);


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

  auto papp = StringPoolStack::Top();
  OrkAssert(papp);

  return papp->_stringpool.String(data);
}

///////////////////////////////////////////////////////////////////////////////

PoolString StringPoolContext::AddPooledLiteral(const ConstString& string) {
  PoolString result = FindPooledString(string);
  if (result)
    return result;

  auto app = StringPoolStack::Top();
  OrkAssert(app);

  return app->_stringpool.Literal(string);
}

///////////////////////////////////////////////////////////////////////////////

PoolString StringPoolContext::FindPooledString(const PieceString& string) {

  auto pAPP         = StringPoolStack::Top();
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

template class ork::util::GlobalStack<stringpoolctx_ptr_t>;
