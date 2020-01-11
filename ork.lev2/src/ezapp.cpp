#include <ork/lev2/ezapp.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/gfx/renderer/drawable.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::EzApp, "Lev2EzApp");
using namespace std::string_literals;

namespace ork::lev2 {
void ClassInit();
void GfxInit(const std::string& gfxlayer);

std::shared_ptr<EzApp> EzApp::create(int argc, char** argv) {
  static ork::lev2::StdFileSystemInitalizer filesysteminit(argc, argv);
  return std::shared_ptr<EzApp>(new EzApp(argc, argv));
}

void EzApp::Describe() {
}
EzApp::EzApp(int argc, char** argv)
    : _updateThread("updatethread")
    , _updatekill(false) {
  ork::SetCurrentThreadName("main");
#if !defined(__APPLE__)
  setenv("QT_QPA_PLATFORMTHEME", "gtk2", 1); // qt5 file dialog crashes otherwise...
// QFont arialFont("Ubuntu Regular", 15);
// QGuiApplication::setFont(arialFont);
#endif
  ApplicationStack::Push(this);
  /////////////////////////////////////////////
  _trackq = new opq::TrackCurrent(&ork::opq::mainSerialQueue());
  /////////////////////////////////////////////
  ork::lev2::ClassInit();
  ork::rtti::Class::InitializeClasses();
  ork::lev2::GfxInit("");
  /////////////////////////////////////////////
  _updateThread.start([&](anyp data) {
    ork::SetCurrentThreadName("update");
    opq::TrackCurrent opqtest(&opq::updateSerialQueue());
    while (false == _updatekill)
      opq::updateSerialQueue().Process();
  });
}
EzApp::~EzApp() {
  opq::mainSerialQueue().enqueue([this]() { _updatekill = true; });
  /////////////////////////////////////////////
  while (false == _updatekill) {
    opq::TrackCurrent opqtest(&opq::mainSerialQueue());
    opq::mainSerialQueue().Process();
  }
  opq::updateSerialQueue().drain();
  DrawableBuffer::ClearAndSyncWriters();
  ApplicationStack::Pop();
}

///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<OrkEzQtApp> OrkEzQtApp::create(int argc, char** argv) {
  QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
  return std::shared_ptr<OrkEzQtApp>(new OrkEzQtApp(argc, argv));
}

struct EzViewport : public ui::Viewport {

  EzViewport(EzMainWin* mainwin)
      : ui::Viewport("yo", 1, 1, 1, 1, fvec3(0, 0, 0), 1.0f)
      , _mainwin(mainwin) {
    lev2::DrawableBuffer::ClearAndSyncWriters();
  }
  void DoInit(ork::lev2::Context* pTARG) final {
    pTARG->FBI()->SetClearColor(fcolor4(0.0f, 0.0f, 0.0f, 0.0f));
  }
  void DoDraw(ui::DrawEvent& drwev) final {

    if (_mainwin->_onGpuInit and _mainwin->_dogpuinit) {
      _mainwin->_onGpuInit(drwev.GetTarget());
      _mainwin->_dogpuinit = false;
    }
    if (_mainwin->_onDraw) {
      _mainwin->_onDraw(drwev);
    }
  }
  void DoSurfaceResize() final {
    if (_mainwin->_onResize)
      _mainwin->_onResize(GetW(), GetH());
  }
  ui::HandlerResult DoOnUiEvent(const ui::Event& ev) final {
    if (_mainwin->_onUiEvent)
      return _mainwin->_onUiEvent(ev);
    else
      return ui::HandlerResult();
  }
  EzMainWin* _mainwin;
};

OrkEzQtApp::OrkEzQtApp(int& argc, char** argv)
    : QApplication(argc, argv)
    , _mainWindow(0) {

  _ezapp = EzApp::create(argc, argv);

  QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath());
  setOrganizationDomain("tweakoz.com");
  setApplicationDisplayName("OrkidEzApp");
  setApplicationName("OrkidEzApp");

  bool bcon = mIdleTimer.connect(&mIdleTimer, SIGNAL(timeout()), this, SLOT(OnTimer()));

  mIdleTimer.setInterval(5);
  mIdleTimer.setSingleShot(false);
  mIdleTimer.start();

  //////////////////////////////////////////////

  _mainWindow = new EzMainWin();
  _mainWindow->show();
  _mainWindow->raise(); // for MacOS

  //////////////////////////////////////
  // create leve gfxwindow
  //////////////////////////////////////
  auto gfxwin = new CQtWindow(nullptr);
  GfxEnv::GetRef().RegisterWinContext(gfxwin);
  //////////////////////////////////////
  auto vp             = new EzViewport(_mainWindow);
  gfxwin->mRootWidget = vp;

  _mainWindow->_ctqt = new CTQT(gfxwin, _mainWindow);
  _mainWindow->_ctqt->Show();

  _mainWindow->_ctxw = _mainWindow->_ctqt->GetQWidget();
  _mainWindow->_ctxw->Enable();
  // gpvp->Init();

  _mainWindow->activateWindow();
  _mainWindow->show();
  _mainWindow->_ctxw->activateWindow();
  _mainWindow->_ctxw->show();

  _mainWindow->setCentralWidget(_mainWindow->_ctxw);
  //////////////////////////////////////////////
  _mainWindow->_ctqt->pushRefreshPolicy(RefreshPolicyItem{EREFRESH_WHENDIRTY});
}

///////////////////////////////////////////////////////////////////////////////

void OrkEzQtApp::OnTimer() {
  opq::TrackCurrent opqtest(&opq::mainSerialQueue());
  while (opq::mainSerialQueue().Process())
    ;
}

void OrkEzQtApp::setDrawCallback(EzMainWin::drawcb_t cb) {
  _mainWindow->_onDraw = cb;
}
void OrkEzQtApp::setResizeCallback(EzMainWin::onresizecb_t cb) {
  _mainWindow->_onResize = cb;
}
void OrkEzQtApp::setGpuInitCallback(EzMainWin::ongpuinit_t cb) {
  _mainWindow->_onGpuInit = cb;
}
void OrkEzQtApp::setUiEventHandler(EzMainWin::onuieventcb_t cb) {
  _mainWindow->_onUiEvent = cb;
}

///////////////////////////////////////////////////////////////////////////////

EzMainWin::EzMainWin()
    : _ctxw(nullptr) {
}
EzMainWin::~EzMainWin() {
}

} // namespace ork::lev2
