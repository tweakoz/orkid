////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
#include <orktool/qtui/qtapp.h>
#include <ork/kernel/environment.h>

bool ENABLE_PROFILER = false;

///////////////////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtmainwin.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/input/inputdevice.h>
//
#include <ork/lev2/gfx/dbgfontman.h>

#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleFactory>
#include <QtGui/QFontDatabase>
#include <QtCore/QDebug>
#include <QDesktopWidget>

///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/qtui/qtui.hpp>
#include <pkg/ent/scene.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <pkg/ent/editor/edmainwin.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/thread.h>

#include <ork/profiling.inl>

#if defined(USE_PYTHON)
#include <Python.h>
namespace ork { namespace tool {
void InitPython();
}} // namespace ork::tool
#endif

///////////////////////////////////////////////////////////////////////////////

namespace ork {

static const std::string ReadMangledName(const char*& input, orkvector<std::string>& names, std::string& subresult) {
  const char* q = input;

  if (*input == 'S') {
    int nameref;
    input++;
    if (*input == '_') {
      nameref = 0;
    } else if (isdigit(*input)) {
      nameref = 1 + strtol(input, const_cast<char**>(&input), 10);
      OrkAssert(*input == '_');
    } else
      switch (*input++) {
        case 'a':
          return "std::allocator";
        case 'b':
          return "std::basic_string";
        case 's':
          return "std::string";
        case 't':
          return "std::";
        default:
          return "std::???";
      }
    input++;
    OrkAssert(nameref < int(names.size()));
    return names[orkvector<std::string>::size_type(nameref)];
  } else if (isdigit(*input)) {
    int namelen = strtol(input, const_cast<char**>(&input), 10);
    std::string result;
    result.reserve(std::string::size_type(namelen));
    for (int i = 0; i < namelen; i++) {
      OrkAssert(*input);
      result += *input++;
    }
    return result;
  } else
    switch (*input++) {
      case 'i':
        return "int";
      case 'c':
        return "char";
      case 'f':
        return "float";
      case 's':
        return "short";
      case 'b':
        return "bool";
      case 'l':
        return "long";
      default: // flf: incomplete, I'm sure.
        return "???";
    }
}

static void GccDemangleRecurse(std::string& result, orkvector<std::string>& names, const char*& input) {
  std::string subresult;
  if (strchr("IN", *input) == NULL) {
    do {
      subresult += ReadMangledName(input, names, subresult);
    } while (subresult[subresult.size() - 1] == ':');
  } else
    switch (*input) {
      case 'I':
        input++;
        result += '<';
        while (*input != 'E') {
          OrkAssert(input && *input);
          GccDemangleRecurse(subresult, names, input);
          names.push_back(subresult);
          if (strchr("INE", *input) == NULL)
            subresult += ",";
          else if (*input == 'E')
            subresult += ">";
        }
        input++;
        break;
      case 'N':
        input++;
        while (*input != 'E') {
          OrkAssert(input && *input);
          GccDemangleRecurse(subresult, names, input);
          names.push_back(subresult);
          if (strchr("INE", *input) == NULL)
            subresult += "::";
        }
        input++;
        break;
    }
  result += subresult;
}

// Function prototype to make CW happy
std::string GccDemangle(const std::string& mangled);

///////////////////////////////////////////////////////////////////////////////

// Function prototype to make CW happy
std::string MethodIdNameStrip(const char* name);

namespace tool {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

OrkQtApp::OrkQtApp(int& argc, char** argv)
    : QApplication(argc, argv)
    , mpMainWindow(0) {

  QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath());
  setOrganizationDomain("tweakoz.com");
  setApplicationDisplayName("OrkidTool");
  setApplicationName("OrkidTool");

  bool bcon = mIdleTimer.connect(&mIdleTimer, SIGNAL(timeout()), this, SLOT(OnTimer()));

  _mainq = ork::opq::mainSerialQueue();

  mIdleTimer.setInterval(1);
  mIdleTimer.setSingleShot(false);
  mIdleTimer.start();
}

///////////////////////////////////////////////////////////////////////////////

void OrkQtApp::OnTimer() {
  opq::TrackCurrent opqtest(_mainq);
  while (_mainq->Process())
    ;
}

///////////////////////////////////////////////////////////////////////////////

struct InputArgs {
  int& argc;
  char** argv;

  InputArgs(int& ac, char** av)
      : argc(ac)
      , argv(av) {
  }
};

OrkQtApp* gpQtApplication = nullptr;

int BootQtThreadImpl(void* arg_opaq) {

#if !defined(__APPLE__)
  setenv("QT_QPA_PLATFORMTHEME", "gtk2", 1); // qt5 file dialog crashes otherwise...
// QFont arialFont("Ubuntu Regular", 15);
// QGuiApplication::setFont(arialFont);
#endif

  InputArgs* args = (InputArgs*)arg_opaq;

  auto mainthreadopq = ork::opq::mainSerialQueue();
  opq::TrackCurrent ot(mainthreadopq);

  int iret = 0;

  QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);

  gpQtApplication = new OrkQtApp(args->argc, args->argv);

  std::string AppClassName = OldSchool::GetGlobalStringVariable("ProjectApplicationClassName");

  ork::lev2::AudioDevice* paudio = ork::lev2::AudioDevice::GetDevice();
  ork::lev2::InputManager::instance()->poll();

  ent::gEditorMainWindow = new ent::EditorMainWindow(0, AppClassName, *gpQtApplication);
  ent::gEditorMainWindow->setGeometry(QStyle::alignedRect(
      Qt::LeftToRight, //
      Qt::AlignCenter,
      QSize(1280, 720),
      qApp->desktop()->availableGeometry()));
  // ent::gEditorMainWindow->showMaximized();
  ent::gEditorMainWindow->raise(); // for MacOS

/////////////////////////////////////////////////////////////////////
// for some reason fonts do not get set up consistently on linux
//  a hack for now
/////////////////////////////////////////////////////////////////////
#if !defined(__APPLE__)
  QFile fontFile("/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf");
  fontFile.open(QFile::ReadOnly);
  qDebug() << fontFile.exists();
  qDebug() << fontFile.size();
  int appFontId = QFontDatabase::addApplicationFontFromData(fontFile.readAll());
  assert(appFontId != -1);
#endif
  /////////////////////////////////////////////////////////////////////

  gpQtApplication->mpMainWindow = ent::gEditorMainWindow;

  file::Path fname;

  for (int i = 0; i < args->argc; i++)
    if (0 == strcmp(args->argv[i], "--edit") && (i < args->argc - 1))
      fname = args->argv[++i];

  if (fname.IsFile())
    ent::gEditorMainWindow->QueueLoadScene(fname.c_str());

  iret = gpQtApplication->exec();

  // TODO - we never get here...
  //  guessing qtapp.exec never returns ?

  OrkAssert(false);

  lev2::DrawableBuffer::ClearAndSyncWriters();

  // delete paudio;
  // delete gpQtApplication;

  gpQtApplication = nullptr;

  OrkAssert(false);

  return 0;
}
////////////////////////////////////////////////////////////////////////////////
int QtTest(int& argc, char** argv, bool bgamemode, bool bmenumode) {

#if defined(USE_PYTHON)
  InitPython();
#endif
  InputArgs args(argc, argv);
  int rval = BootQtThreadImpl(&args);

  return rval;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} // namespace tool
} // namespace ork
