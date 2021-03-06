////////////////////////////////////////////////////////////////////////////////
// Copyright 2007, Michael T. Mayers, all rights reserved.
////////////////////////////////////////////////////////////////////////////////

#if defined(ORK_OSX)
#include <mach-o/dyld.h>
#endif

#include <orktool/orktool_pch.h>
#include <orktool/qtui/qtui_tool.h>
#include <orktool/toolcore/selection.h>
#include <ork/util/choiceman.h>
#include <ork/file/file.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxanim.h>
#include <ork/kernel/string/string.h>
#include <utpp/UnitTest++.h>

#include <orktool/toolcore/FunctionManager.h>

#include <QtWidgets/QMessageBox>
#include <QtCore/QSettings>

#include <sys/resource.h>

#include <unistd.h>

///////////////////////////////////////////////////////////////////////////////

static bool exit_gracefully = false;
extern char errorbuffer[];

///////////////////////////////////////////////////////////////////////////////

int SingularityMain(int argc, const char** argv);

namespace ork {

void LoadLocalization(const char langcode[2]);

namespace tool {

static std::string gexecdir = "";
static std::string gdatadir = "";
const std::string& getExecutableDir() {
  return gexecdir;
}
const std::string& getDataDir() {
  return gdatadir;
}
void setDataDir(const std::string& dir) {
  gdatadir = dir;
  QSettings settings("TweakoZ", "OrkidTool");
  settings.beginGroup("App");
  settings.setValue("datadir", qs(dir));
  settings.endGroup();
}

int Main_Filter(tokenlist toklist);
int Main_FilterTree(tokenlist toklist);

static void ToolStartupDataFolder() {
  //////////////////////////////////////////
  // Register data:// urlbase
  // static auto WorkingDirContext = std::make_shared<FileDevContext>();
  // WorkingDirContext->SetFilesystemBaseRel(data_dir.c_str());
  // WorkingDirContext->SetFilesystemBaseEnable(true);

  QSettings settings("TweakoZ", "OrkidTool");
  settings.beginGroup("App");
  /*if (settings.contains("datadir")) {
    auto existdir = settings.value("datadir").toString();
    gdatadir      = existdir.toStdString();
    WorkingDirContext->SetFilesystemBaseAbs(gdatadir.c_str());
    printf("DATADIR from QSETTINGS<%s>\n", gdatadir.c_str());
  }*/
  settings.endGroup();

  // FileEnv::createContextForUriBase("data://", WorkingDirContext);
  //////////////////////////////////////////
}

int toolmain(int& argc, char** argv, char** argp) {
  struct rlimit oldlimit, newlimit;

  int el = getrlimit(RLIMIT_STACK, &oldlimit);

  // printf( "stack cur<%p> max<%p<\n", (void*) oldlimit.rlim_cur, oldlimit.rlim_max );

  // el = setrlimit( RLIMIT_STACK, & newlimit );

#if defined(__APPLE__)
  char path[1024];
  uint32_t size = sizeof(path);
  if (_NSGetExecutablePath(path, &size) == 0) {
    ork::file::Path p(path);
    ork::file::Path::NameType l, r, l2, r2;
    p.Split(l, r, '/');
    ork::file::Path p2(l);
    p2.Split(l2, r2, '/');

    printf("executable path is %s\n", path);
    printf("l:%s\n", l.c_str());
    printf("r:%s\n", r.c_str());
    printf("l2:%s\n", l2.c_str());
    printf("r2:%s\n", r2.c_str());

    gexecdir = l.c_str();

    if (r2 == ork::file::Path::NameType("MacOS")) // we are in a bundle
      chdir(l2.c_str());
  } else
    printf("buffer too small; need size %u\n", size);

  QSettings settings("TweakoZ", "OrkidTool");
  settings.beginGroup("App");
  if (settings.contains("datadir") == false) {
    settings.setValue("datadir", qs(gexecdir + "../"));
  }
  settings.endGroup();

#else
  gexecdir = getenv("ORKID_WORKSPACE_DIR");

  QSettings settings("TweakoZ", "OrkidTool");
  settings.beginGroup("App");
  if (settings.contains("datadir") == false) {
    settings.setValue("datadir", qs(gexecdir + "/ork.data"));
  }
  settings.endGroup();

#endif

  int iret = 0;

  try {
    //////////////////////////////////////////
    // Register lev2:// urlbase

    tokenlist toklist = ork::tool::Init(argc, argv, argp);

    //////////////////////////////////////////

    if (toklist.front() == std::string("--help")) {
      orkprintf("usage:\n");
      orkprintf("miniork_tool --edit moxfile                                  : edit a specific moxfile\n");
      orkprintf("miniork_tool --filter list                                   : list registered asset filters (eg miniork_tool "
                "-filter sf2:pxv test.sf2 yo.pxv)\n");
      orkprintf("miniork_tool --filter <filtername> source dest               : filter a single asset\n");
      orkprintf("miniork_tool --filtertree <filtername> sourcebase destbase   : filter a tree of assets\n");
    } else if (toklist.front() == std::string("--filter")) {
      exit_gracefully = true;
      iret            = ork::tool::Main_Filter(toklist);
    } else if (toklist.front() == std::string("--execute")) {
      exit_gracefully = true;
      toklist.pop_front(); // Remove -execute
      FunctionManager::GetRef().ExecuteFunction(toklist);
    } else if (toklist.front() == std::string("--filtertree")) {
      exit_gracefully = true;
      ork::tool::Main_FilterTree(toklist);
    } else {
      ToolStartupDataFolder();
      ork::tool::QtTest(argc, argv, false, false);
    }
  } catch (std::exception&) {
    if (exit_gracefully) {
      orkprintf("OrkAssert Occured, exiting... [%s]\n", errorbuffer);
      return -1;
    } else {
      assert(false);
    }
  }

  return iret;
}

} // namespace tool
} // namespace ork
