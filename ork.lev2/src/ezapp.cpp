#include <ork/lev2/ezapp.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/vr/vr.h>
#include <QtWidgets/QStyle>
#include <QtGui/QWindow>
#include <QtWidgets/QDesktopWidget>
#include <boost/program_options.hpp>

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
QtAppInit::QtAppInit(int argc, char** argv, const QtAppInitData& initdata)
  : _initdata(initdata) {
  _argc   = argc;
  _arg    = "";
  _argv   = nullptr;
  _argvp  = argv;
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
QtAppInit& qtinit(int& argc, char** argv,const QtAppInitData& initdata) {
  static QtAppInit qti(argc, argv,initdata);
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
  return std::make_shared<OrkEzQtApp>(qti._argc, qti._argvp,qti._initdata);
}
qtezapp_ptr_t OrkEzQtApp::create(int argc, char** argv) {
  static auto& qti = qtinit(argc, argv);
  QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
  return std::make_shared<OrkEzQtApp>(qti._argc, qti._argvp,qti._initdata);
}
qtezapp_ptr_t OrkEzQtApp::create(int argc, char** argv, const QtAppInitData& init) {
  static auto& qti = qtinit(argc, argv,init);
  QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
  return std::make_shared<OrkEzQtApp>(qti._argc, qti._argvp,qti._initdata);
}
///////////////////////////////////////////////////////////////////////////////
qtezapp_ptr_t OrkEzQtApp::createWithScene(varmap::varmap_ptr_t sceneparams) {
  static auto& qti = qtinit();
  QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
  QtAppInitData initdata;
  auto rval                           = std::make_shared<OrkEzQtApp>(qti._argc, qti._argvp,initdata);
  rval->_mainWindow->_execsceneparams = sceneparams;
  rval->_mainWindow->_onDraw          = [=](ui::drawevent_constptr_t drwev) { //
    ork::opq::mainSerialQueue()->Process();
    auto context = drwev->GetTarget();
    context->beginFrame();
    rval->_mainWindow->_execscene->renderOnContext(context);
    context->endFrame();
  };
  return rval;
}
///////////////////////////////////////////////////////////////////////////////

EzViewport::EzViewport(EzMainWin* mainwin)
    : ui::Viewport("ezviewport", 1, 1, 1, 1, fvec3(0, 0, 0), 1.0f)
    , _mainwin(mainwin) {
  _initstate.store(0);
  lev2::DrawableBuffer::ClearAndSyncWriters();
  _mainwin->_render_timer.Start();
  _mainwin->_render_prevtime = _mainwin->_render_timer.SecsSinceStart();
}
/////////////////////////////////////////////////
void EzViewport::DoInit(ork::lev2::Context* pTARG) {
  pTARG->FBI()->SetClearColor(fcolor4(0.0f, 0.0f, 0.0f, 0.0f));
  FontMan::gpuInit(pTARG);
  _initstate.store(1);
}
/////////////////////////////////////////////////
void EzViewport::DoDraw(ui::drawevent_constptr_t drwev) {

  bool do_gpu_init = bool(_mainwin->_onGpuInit);
  do_gpu_init |= bool(_mainwin->_onGpuInitWithScene);
  do_gpu_init &= _mainwin->_dogpuinit;

  if (do_gpu_init) {
    drwev->GetTarget()->makeCurrentContext();
    drwev->GetTarget()->makeCurrentContext();

    if (_mainwin->_onGpuInit)
      _mainwin->_onGpuInit(drwev->GetTarget());
    else if (_mainwin->_onGpuInitWithScene) {
      _mainwin->_execscene = std::make_shared<scenegraph::Scene>(_mainwin->_execsceneparams);
      _mainwin->_onGpuInitWithScene(drwev->GetTarget(), _mainwin->_execscene);
    }
    _mainwin->_dogpuinit = false;
    while (ork::opq::mainSerialQueue()->Process()) {
    }
  }
  auto ezapp = (OrkEzQtApp*)OrkEzQtAppBase::get();
  while (ezapp->_rthreadq->Process()) {
  }
  if (_mainwin->_onDraw) {
    drwev->GetTarget()->makeCurrentContext();
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
/////////////////////////////////////////////////
void EzViewport::DoSurfaceResize() {
  if (_mainwin->_onResize)
    _mainwin->_onResize(width(), height());
}
/////////////////////////////////////////////////
ui::HandlerResult EzViewport::DoOnUiEvent(ui::event_constptr_t ev) {
    printf("WTF...\n");
  if (_mainwin->_onUiEvent) {
    return _mainwin->_onUiEvent(ev);
  } else
    return ui::HandlerResult();
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool OrkEzQtApp::checkAppState(uint64_t singlebitmask) {
  uint64_t chk = _appstate.load() & singlebitmask;
  return chk == singlebitmask;
}
OrkEzQtAppBase* OrkEzQtAppBase::_staticapp = nullptr;
OrkEzQtAppBase* OrkEzQtAppBase::get() {
  return _staticapp;
}
///////////////////////////////////////////////////////////////////////////////
OrkEzQtAppBase::OrkEzQtAppBase(int& argc, char** argv)
    : QApplication(argc, argv) {
  _staticapp = this;
  _ezapp     = EzApp::get(argc, argv);
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzQtApp::enqueueOnRenderer(const void_lambda_t& l){
  _rthreadq->enqueue(l);
}
///////////////////////////////////////////////////////////////////////////////
OrkEzQtApp::OrkEzQtApp(int& argc, char** argv,const QtAppInitData& initdata)
    : OrkEzQtAppBase(argc, argv)
    , _initdata(initdata)
    , _updateThread("updatethread")
    , _mainWindow(0) {

  //////////////////////////////////////////////////////////

  _uicontext   = std::make_shared<ui::Context>();
  _update_data = std::make_shared<ui::UpdateData>();
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

  bool fullscreen = _initdata._fullscreen;

  _mainWindow->setGeometry(
      _initdata._left, //
      _initdata._top,
      _initdata._width,
       _initdata._height
      );

  _mainWindow->show();
  _mainWindow->raise(); // for MacOS

  //////////////////////////////////////
  // create leve gfxwindow
  //////////////////////////////////////
  _mainWindow->_gfxwin = new CQtWindow(nullptr);
  GfxEnv::GetRef().RegisterWinContext(_mainWindow->_gfxwin);
  //////////////////////////////////////
  //////////////////////////////////////
  _ezviewport                       = std::make_shared<EzViewport>(_mainWindow);
  _ezviewport->_uicontext           = _uicontext.get();
  _mainWindow->_gfxwin->mRootWidget = _ezviewport.get();
  _ezviewport->_topLayoutGroup      = _uicontext->makeTop<ui::LayoutGroup>("top-layoutgroup", 0, 0, _initdata._width, _initdata._height);
  _topLayoutGroup                   = _ezviewport->_topLayoutGroup;

  _mainWindow->_ctqt = new CTQT(_mainWindow->_gfxwin, _mainWindow);
  _mainWindow->_ctqt->Show();

  _mainWindow->_ctxw = _mainWindow->_ctqt->GetQWidget();
  _mainWindow->_ctxw->Enable();
  // gpvp->Init();

  _mainWindow->activateWindow();
  //auto screens = this->screens();
  //_mainWindow->windowHandle()->setScreen(screens[2]);

  /////////////////////////////////
  if( _initdata._fullscreen )
    _mainWindow->showFullScreen();
  else
    _mainWindow->show();
  /////////////////////////////////
  _mainWindow->_ctxw->activateWindow();
  _mainWindow->_ctxw->show();

  _mainWindow->setCentralWidget(_mainWindow->_ctxw);
  //////////////////////////////////////////////
  _mainWindow->_ctqt->pushRefreshPolicy(RefreshPolicyItem{EREFRESH_WHENDIRTY});
  /////////////////////////////////////////////
  auto handler = [this](opq::progressdata_ptr_t data) { //
    if (_ezviewport->_initstate.load() == 1) {
      _mainWindow->_ctqt->progressHandler(data);
    }
  };
  opq::setProgressHandler(handler);

  /////////////////////////////////////////////
  _updq  = ork::opq::updateSerialQueue();
  _conq  = ork::opq::concurrentQueue();
  _mainq = ork::opq::mainSerialQueue();
  _rthreadq = std::make_shared<opq::OperationsQueue>(0, "renderSerialQueue");
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
  printf( "OrkEzQtApp<%p> destructor - joining update thread...\n", this );
  joinUpdate();
  printf( "OrkEzQtApp<%p> destructor - joined update thread\n", this );
  printf( "OrkEzQtApp<%p> terminating drawable buffers..\n", this );
  DrawableBuffer::terminateAll();
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
  _ezviewport->_topLayoutGroup->_evhandler = cb;
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

filedevctx_ptr_t OrkEzQtApp::newFileDevContext(std::string uriproto, const file::Path& basepath) {
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
