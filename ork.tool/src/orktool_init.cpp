///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <ork/file/file.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/gfx/gfxanim.h> // For AnimManager
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/init.h>
#include <ork/rtti/Class.h>
#include <orktool/orktool_pch.h>
#include <orktool/toolcore/FunctionManager.h>
#include <orktool/toolcore/choiceman.h>
#include <orktool/toolcore/selection.h>

//#include <boost/python.hpp>

// extern "C" void xmlInitParser(); // must init libxml in main thread

// using namespace boost::python;

namespace ork {

namespace ent {
//class LightMapperArchetype;
class EntArchDeRef;
class EntArchReRef;
class EntArchSplit;
} // namespace ent

namespace tweakout {
void Init();
}

namespace lev2 {
void ClassInit();
void GfxInit(const std::string& gfxlayer);
}

namespace tool {

namespace ged {
///////////////////////////////////////////////////////////////////////////////
class GedFactoryPlug;
class GraphImportDelegate;
class GraphExportDelegate;
///////////////////////////////////////////////////////////////////////////////
} // namespace ged

void LinkMe() {
  ork::rtti::Link<ork::tool::ged::GedFactoryPlug>();
  //ork::rtti::Link<ork::ent::LightMapperArchetype>();

  ork::rtti::Link<ork::tool::ged::GraphImportDelegate>();
  ork::rtti::Link<ork::tool::ged::GraphExportDelegate>();

  ork::rtti::Link<ork::ent::EntArchDeRef>();
  ork::rtti::Link<ork::ent::EntArchReRef>();
  ork::rtti::Link<ork::ent::EntArchSplit>();

}

///////////////////////////////////////////////////////////////////////////////

tokenlist Init(int argc, char** argv) {
  //printf("ork::tool::Init()\n");
  LinkMe();

  static ork::lev2::StdFileSystemInitalizer filesysteminit(argc,argv);

  ork::lev2::ClassInit();
  ork::rtti::Class::InitializeClasses();
  ork::lev2::GfxInit("");

  //	xmlInitParser(); // must init libxml in main thread

  //printf("CPX\n");
  tokenlist toklist;
  for (int iarg = 1; iarg < argc; iarg++) {
    const char* parg = argv[iarg];
    if (strcmp(parg, "-datafolder") == 0) {}
    else if (strcmp(parg, "-lev2folder") == 0) {}
    else {
      toklist.push_back(parg);
    }
  }
  return toklist;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace tool
} // namespace ork
