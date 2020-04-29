#include <ork/lev2/ezapp.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/dbgfontman.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::EzApp, "Lev2EzApp");
using namespace std::string_literals;

namespace ork::lev2 {
void ClassInit();
void GfxInit(const std::string& gfxlayer);
constexpr uint64_t KAPPSTATEFLAG_UPDRUNNING = 1 << 0;
constexpr uint64_t KAPPSTATEFLAG_JOINED     = 1 << 1;
////////////////////////////////////////////////////////////////////////////////
QtAppInit::QtAppInit() {
  _argc   = 1;
  _arg    = "whatupyo";
  _argv   = (char*)_arg.c_str();
  _argvp  = &_argv;
  _fsinit = std::make_shared<StdFileSystemInitalizer>(_argc, _argvp);
}
QtAppInit::QtAppInit(int argc, char** argv) {
  _argc   = argc;
  _arg    = "";
  _argv   = nullptr;
  _argvp  = argv;
  _fsinit = std::make_shared<StdFileSystemInitalizer>(_argc, _argvp);
}
QtAppInit::~QtAppInit() {
}
////////////////////////////////////////////////////////////////////////////////
QtAppInit& qtinit() {
  static QtAppInit qti;
  return qti;
};
////////////////////////////////////////////////////////////////////////////////
QtAppInit& qtinit(int& argc, char** argv) {
  static QtAppInit qti(argc, argv);
  return qti;
};
////////////////////////////////////////////////////////////////////////////////
ezapp_ptr_t EzApp::get(int& argc, char** argv) {
  static auto& qai = qtinit(argc, argv);
  static auto app  = std::shared_ptr<EzApp>(new EzApp(qai._argc, qai._argvp));
  return app;
}
////////////////////////////////////////////////////////////////////////////////
ezapp_ptr_t EzApp::get() {
  static auto& qai = qtinit();
  static auto app  = std::shared_ptr<EzApp>(new EzApp(qai._argc, qai._argvp));
  return app;
}
////////////////////////////////////////////////////////////////////////////////
void EzApp::Describe() {
}
////////////////////////////////////////////////////////////////////////////////
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
///////////////////////////////////////////////////////////////////////////////

qtezapp_ptr_t OrkEzQtApp::create() {
  static auto& qti = qtinit();
  QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
  return std::make_shared<OrkEzQtApp>(qti._argc, qti._argvp);
}
qtezapp_ptr_t OrkEzQtApp::create(int argc, char** argv) {
  static auto& qti = qtinit(argc, argv);
  QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
  return std::make_shared<OrkEzQtApp>(qti._argc, qti._argvp);
}
///////////////////////////////////////////////////////////////////////////////
qtezapp_ptr_t OrkEzQtApp::createWithScene(varmap::varmap_ptr_t sceneparams) {
  static auto& qti = qtinit();
  QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);

  auto rval                           = std::make_shared<OrkEzQtApp>(qti._argc, qti._argvp);
  rval->_mainWindow->_execsceneparams = sceneparams;
  rval->_mainWindow->_onDraw          = [=](const ui::DrawEvent& drwev) { //
    ork::opq::mainSerialQueue()->Process();
    auto context = drwev.GetTarget();
    context->beginFrame();
    rval->_mainWindow->_execscene->renderOnContext(context);
    context->endFrame();
  };
  return rval;
}
///////////////////////////////////////////////////////////////////////////////

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

    bool do_gpu_init = bool(_mainwin->_onGpuInit);
    do_gpu_init |= bool(_mainwin->_onGpuInitWithScene);
    do_gpu_init &= _mainwin->_dogpuinit;

    if (do_gpu_init) {
      drwev.GetTarget()->makeCurrentContext();
      FontMan::gpuInit(drwev.GetTarget());
      drwev.GetTarget()->makeCurrentContext();

      if (_mainwin->_onGpuInit)
        _mainwin->_onGpuInit(drwev.GetTarget());
      else if (_mainwin->_onGpuInitWithScene) {
        _mainwin->_execscene = std::make_shared<scenegraph::Scene>(_mainwin->_execsceneparams);
        _mainwin->_onGpuInitWithScene(drwev.GetTarget(), _mainwin->_execscene);
      }
      while (asset::AssetManager<TextureAsset>::AutoLoad()) {
      }
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
bool OrkEzQtApp::checkAppState(uint64_t singlebitmask) {
  uint64_t chk = _appstate.load() & singlebitmask;
  return chk == singlebitmask;
}
OrkEzQtAppBase* OrkEzQtAppBase::_staticapp = nullptr;
OrkEzQtAppBase* OrkEzQtAppBase::get() {
  return _staticapp;
}
OrkEzQtAppBase::OrkEzQtAppBase(int& argc, char** argv)
    : QApplication(argc, argv) {
  _staticapp = this;
  _ezapp     = EzApp::get(argc, argv);
}

OrkEzQtApp::OrkEzQtApp(int& argc, char** argv)
    : OrkEzQtAppBase(argc, argv)
    , _updateThread("updatethread")
    , _mainWindow(0) {
  _update_data = std::make_shared<UpdateData>();
  _appstate    = 0;

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
    _appstate.fetch_xor(KAPPSTATEFLAG_UPDRUNNING);
    _update_timer.Start();
    _update_prevtime        = _update_timer.SecsSinceStart();
    _update_timeaccumulator = 0.0;
    ork::SetCurrentThreadName("update");
    opq::TrackCurrent opqtest(_updq);
    double stats_timeaccum = 0;
    double state_numiters  = 0.0;
    while (not checkAppState(KAPPSTATEFLAG_JOINED)) {
      double this_time = _update_timer.SecsSinceStart();
      double raw_delta = this_time - _update_prevtime;
      _update_prevtime = this_time;
      _update_timeaccumulator += raw_delta;
      double step = 1.0 / 120.0;
      while (_update_timeaccumulator > step) {
        bool do_update = bool(_mainWindow->_onUpdate);
        do_update |= bool(_mainWindow->_onUpdateWithScene);
        do_update &= bool(not _mainWindow->_dogpuinit);

        if (do_update) {
          _update_data->_dt = step;
          _update_data->_abstime += step;
          /////////////////////////////
          /////////////////////////////
          if (not checkAppState(KAPPSTATEFLAG_JOINED)) {
            if (_mainWindow->_onUpdate)
              _mainWindow->_onUpdate(_update_data);
            else if (_mainWindow->_onUpdateWithScene)
              _mainWindow->_onUpdateWithScene(_update_data, _mainWindow->_execscene);
          }
          /////////////////////////////
          /////////////////////////////
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
    } // while (not checkAppState(KAPPSTATEFLAG_UPDSIGKILL)) {
    _appstate.fetch_xor(KAPPSTATEFLAG_UPDRUNNING);
  });
}

///////////////////////////////////////////////////////////////////////////////

OrkEzQtApp::~OrkEzQtApp() {
  joinUpdate();
}

///////////////////////////////////////////////////////////////////////////////

void OrkEzQtApp::joinUpdate() {
  uint64_t prevappsate    = _appstate.fetch_or(KAPPSTATEFLAG_JOINED);
  bool has_joined_already =    //
      (KAPPSTATEFLAG_JOINED == //
       (prevappsate & KAPPSTATEFLAG_JOINED));
  if (not has_joined_already) {
    while (checkAppState(KAPPSTATEFLAG_UPDRUNNING)) {
      opq::TrackCurrent opqtest(_mainq);
      _mainq->Process();
    }
    _updq->drain();
    _updateThread.join();
    DrawableBuffer::ClearAndSyncWriters();
  }
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
void OrkEzQtApp::onGpuInitWithScene(EzMainWin::ongpuinitwitchscene_t cb) {
  _mainWindow->_onGpuInitWithScene = cb;
}
void OrkEzQtApp::onUpdateWithScene(EzMainWin::onupdatewithscene_t cb) {
  _mainWindow->_onUpdateWithScene = cb;
}

filedevctxptr_t OrkEzQtApp::newFileDevContext(std::string uriproto, const file::Path& basepath) {
  return FileEnv::createContextForUriBase(uriproto, basepath);
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
  _execsceneparams = std::make_shared<varmap::VarMap>();
}
EzMainWin::~EzMainWin() {
}

} // namespace ork::lev2
