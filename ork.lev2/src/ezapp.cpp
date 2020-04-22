#include <ork/lev2/ezapp.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/dbgfontman.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::EzApp, "Lev2EzApp");
using namespace std::string_literals;

namespace ork::lev2 {
void ClassInit();
void GfxInit(const std::string& gfxlayer);

std::shared_ptr<EzApp> EzApp::create(int& argc, char** argv) {
  static ork::lev2::StdFileSystemInitalizer filesysteminit(argc, argv);
  return std::shared_ptr<EzApp>(new EzApp(argc, argv));
}

void EzApp::Describe() {
}
EzApp::EzApp(int& argc, char** argv) {
  ork::SetCurrentThreadName("main");
#if !defined(__APPLE__)
  setenv("QT_QPA_PLATFORMTHEME", "gtk2", 1); // qt5 file dialog crashes otherwise...
  // QFont arialFont("Ubuntu Regular", 15);
  // QGuiApplication::setFont(arialFont);

#endif
  ApplicationStack::Push(this);
  /////////////////////////////////////////////
  _mainq  = ork::opq::mainSerialQueue();
  _trackq = new opq::TrackCurrent(_mainq);
  /////////////////////////////////////////////
  ork::lev2::ClassInit();
  ork::rtti::Class::InitializeClasses();
  ork::lev2::GfxInit("");
}
EzApp::~EzApp() {
  ApplicationStack::Pop();
}

///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<OrkEzQtApp> OrkEzQtApp::create(int& argc, char** argv) {
  QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
  return std::shared_ptr<OrkEzQtApp>(new OrkEzQtApp(argc, argv));
}

struct EzViewport : public ui::Viewport {

  EzViewport(EzMainWin* mainwin)
      : ui::Viewport("yo", 1, 1, 1, 1, fvec3(0, 0, 0), 1.0f)
      , _mainwin(mainwin) {
    lev2::DrawableBuffer::ClearAndSyncWriters();
    _mainwin->_render_timer.Start();
    _mainwin->_render_prevtime = _mainwin->_render_timer.SecsSinceStart();
  }
  void DoInit(ork::lev2::Context* pTARG) final {
    pTARG->FBI()->SetClearColor(fcolor4(0.0f, 0.0f, 0.0f, 0.0f));
  }
  void DoDraw(ui::DrawEvent& drwev) final {

    if (_mainwin->_onGpuInit and _mainwin->_dogpuinit) {
      drwev.GetTarget()->makeCurrentContext();
      FontMan::gpuInit(drwev.GetTarget());
      drwev.GetTarget()->makeCurrentContext();
      _mainwin->_onGpuInit(drwev.GetTarget());
      while (ork::opq::mainSerialQueue()->Process()) {
      }
      _mainwin->_dogpuinit = false;
    }
    if (_mainwin->_onDraw) {
      drwev.GetTarget()->makeCurrentContext();
      _mainwin->_onDraw(drwev);
    }
    double this_time           = _mainwin->_render_timer.SecsSinceStart();
    double raw_delta           = this_time - _mainwin->_render_prevtime;
    _mainwin->_render_prevtime = this_time;
    _mainwin->_render_stats_timeaccum += raw_delta;
    if (_mainwin->_render_stats_timeaccum >= 5.0) {
      double FPS = _mainwin->_render_state_numiters / _mainwin->_render_stats_timeaccum;
      printf("FPS<%g>\n", FPS);
      _mainwin->_render_stats_timeaccum = 0.0;
      _mainwin->_render_state_numiters  = 0.0;
    } else {
      _mainwin->_render_state_numiters += 1.0;
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
    , _updateThread("updatethread")
    , _updatekill(false)
    , _mainWindow(0) {

  _ezapp = EzApp::create(argc, argv);

  QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath());
  setOrganizationDomain("tweakoz.com");
  setApplicationDisplayName("OrkidEzApp");
  setApplicationName("OrkidEzApp");

  bool bcon = mIdleTimer.connect(&mIdleTimer, SIGNAL(timeout()), this, SLOT(OnTimer()));

  mIdleTimer.setInterval(1);
  mIdleTimer.setSingleShot(false);
  mIdleTimer.start();

  //////////////////////////////////////////////

  _mainWindow = new EzMainWin();
  _mainWindow->show();
  _mainWindow->raise(); // for MacOS

  //////////////////////////////////////
  // create leve gfxwindow
  //////////////////////////////////////
  _mainWindow->_gfxwin = new CQtWindow(nullptr);
  GfxEnv::GetRef().RegisterWinContext(_mainWindow->_gfxwin);
  //////////////////////////////////////
  auto vp                           = new EzViewport(_mainWindow);
  _mainWindow->_gfxwin->mRootWidget = vp;

  _mainWindow->_ctqt = new CTQT(_mainWindow->_gfxwin, _mainWindow);
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

  /////////////////////////////////////////////
  _updq  = ork::opq::updateSerialQueue();
  _conq  = ork::opq::concurrentQueue();
  _mainq = ork::opq::mainSerialQueue();
  _updateThread.start([&](anyp data) {
    _update_timer.Start();
    _update_prevtime        = _update_timer.SecsSinceStart();
    _update_timeaccumulator = 0.0;
    ork::SetCurrentThreadName("update");
    opq::TrackCurrent opqtest(_updq);
    UpdateData updata;
    double stats_timeaccum = 0;
    double state_numiters  = 0.0;
    while (false == _updatekill) {

      double this_time = _update_timer.SecsSinceStart();
      double raw_delta = this_time - _update_prevtime;
      _update_prevtime = this_time;
      _update_timeaccumulator += raw_delta;
      double step = 1.0 / 120.0;
      while (_update_timeaccumulator > step) {

        if (_mainWindow->_onUpdate and not _mainWindow->_dogpuinit) {
          updata._dt = step;
          updata._abstime += step;
          _mainWindow->_onUpdate(updata);
          state_numiters += 1.0;
        }

        _update_timeaccumulator -= step;
        stats_timeaccum += step;
        if (stats_timeaccum >= 5.0) {
          printf("UPS<%g>\n", state_numiters / stats_timeaccum);
          stats_timeaccum = 0.0;
          state_numiters  = 0.0;
        }
      }

      opq::updateSerialQueue()->Process();
      usleep(1000);
      sched_yield();
    }
  });
}

///////////////////////////////////////////////////////////////////////////////

OrkEzQtApp::~OrkEzQtApp() {
  opq::mainSerialQueue()->enqueue([this]() { _updatekill = true; });
  /////////////////////////////////////////////
  while (false == _updatekill) {
    opq::TrackCurrent opqtest(_mainq);
    _mainq->Process();
  }
  _updq->drain();
  _updateThread.join();
  DrawableBuffer::ClearAndSyncWriters();
}

///////////////////////////////////////////////////////////////////////////////

void OrkEzQtApp::OnTimer() {
  opq::TrackCurrent opqtest(_mainq);
  while (_mainq->Process())
    ;
}

void OrkEzQtApp::onDraw(EzMainWin::drawcb_t cb) {
  _mainWindow->_onDraw = cb;
}
void OrkEzQtApp::onResize(EzMainWin::onresizecb_t cb) {
  _mainWindow->_onResize = cb;
}
void OrkEzQtApp::onGpuInit(EzMainWin::ongpuinit_t cb) {
  _mainWindow->_onGpuInit = cb;
}
void OrkEzQtApp::onUiEvent(EzMainWin::onuieventcb_t cb) {
  _mainWindow->_onUiEvent = cb;
}
void OrkEzQtApp::onUpdate(EzMainWin::onupdate_t cb) {
  _mainWindow->_onUpdate = cb;
}

filedevctxptr_t OrkEzQtApp::newFileDevContext(std::string uribase) {
  auto ctx = std::make_shared<FileDevContext>();
  FileEnv::registerUrlBase(uribase.c_str(), ctx);
  return ctx;
}

int OrkEzQtApp::runloop() {
  return exec();
}

void OrkEzQtApp::setRefreshPolicy(RefreshPolicyItem policy) {
  _mainWindow->_ctqt->_setRefreshPolicy(policy);
}

///////////////////////////////////////////////////////////////////////////////

EzMainWin::EzMainWin()
    : _ctxw(nullptr) {
}
EzMainWin::~EzMainWin() {
}

} // namespace ork::lev2
