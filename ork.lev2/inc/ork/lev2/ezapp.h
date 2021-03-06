#pragma once

#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/varmap.inl>
#include <ork/lev2/init.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/file/file.h>
#include <ork/object/Object.h>
#include <ork/rtti/downcast.h>
#include <ork/reflect/properties/register.h>
#include <ork/application/application.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/gfx/scenegraph/scenegraph.h>

#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtCore/QTimer>
#include <QtWidgets/QApplication>
#include <ork/lev2/qtui/qtui.h>
#include <ork/lev2/qtui/qtui.hpp>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/context.h>

namespace ork::lev2 {
class EzApp;
class OrkEzQtApp;
using ezapp_ptr_t   = std::shared_ptr<EzApp>;
using qtezapp_ptr_t = std::shared_ptr<OrkEzQtApp>;
////////////////////////////////////////////////////////////////////////////////
struct QtAppInitData{
  bool _fullscreen = false;
  int _top = 100;
  int _left = 100;
  int _width = 1280;
  int _height = 720;
  std::string _monitor_id = "";
};
////////////////////////////////////////////////////////////////////////////////
struct QtAppInit {
  QtAppInit();
  QtAppInit(int argc, char** argv, const QtAppInitData& initdata);
  QtAppInit(int argc, char** argv);
  ~QtAppInit();
  int _argc = 0;
  std::string _arg;
  char* _argv   = nullptr;
  char** _argvp = nullptr;
  QtAppInitData _initdata;
  std::shared_ptr<StdFileSystemInitalizer> _fsinit;
};
////////////////////////////////////////////////////////////////////////////////
extern QtAppInit& qtinit();
extern QtAppInit& qtinit(int& argc, char** argv);
extern QtAppInit& qtinit(int& argc, char** argv,const QtAppInitData& initdata);
////////////////////////////////////////////////////////////////////////////////
class OrkEzQtAppBase : public QApplication {
  Q_OBJECT
public:
  OrkEzQtAppBase(int& argc, char** argv);
  ezapp_ptr_t _ezapp;
  static OrkEzQtAppBase* get();
  static OrkEzQtAppBase* _staticapp;
};
////////////////////////////////////////////////////////////////////////////////
class EzApp final : public ork::Application {
  RttiDeclareAbstract(EzApp, ork::Application);

public:
  static ezapp_ptr_t get(int& argc, char** argv);
  static ezapp_ptr_t get();
  ~EzApp();

private:
  EzApp(int& argc, char** argv);
  opq::TrackCurrent* _trackq;

  ork::opq::opq_ptr_t _mainq;
  ork::opq::opq_ptr_t _conq;
};
////////////////////////////////////////////////////////////////////////////////
class EzMainWin : public QMainWindow {
public:
  typedef std::function<void(ui::drawevent_constptr_t)> drawcb_t;
  typedef std::function<void(int w, int h)> onresizecb_t;

  typedef std::function<void(Context* ctx)> ongpuinit_t;
  typedef std::function<void(ui::updatedata_ptr_t upd)> onupdate_t;

  typedef std::function<void(Context* ctx, scenegraph::scene_ptr_t)> ongpuinitwitchscene_t;
  typedef std::function<void(ui::updatedata_ptr_t upd, scenegraph::scene_ptr_t)> onupdatewithscene_t;

  typedef std::function<ui::HandlerResult(ui::event_constptr_t ev)> onuieventcb_t;

  EzMainWin();
  ~EzMainWin();
  bool _dogpuinit                           = true;
  CQtWindow* _gfxwin                        = nullptr;
  CTQT* _ctqt                               = nullptr;
  QCtxWidget* _ctxw                         = nullptr;
  drawcb_t _onDraw                          = nullptr;
  onresizecb_t _onResize                    = nullptr;
  onuieventcb_t _onUiEvent                  = nullptr;
  ongpuinit_t _onGpuInit                    = nullptr;
  onupdate_t _onUpdate                      = nullptr;
  ongpuinitwitchscene_t _onGpuInitWithScene = nullptr;
  onupdatewithscene_t _onUpdateWithScene    = nullptr;
  scenegraph::scene_ptr_t _execscene;
  varmap::varmap_ptr_t _execsceneparams;
  ork::Timer _render_timer;
  double _render_prevtime        = 0;
  double _render_stats_timeaccum = 0;
  double _render_state_numiters  = 0.0;
};
///////////////////////////////////////////////////////////////////////////////
struct EzViewport : public ui::Viewport {
  EzViewport(EzMainWin* mainwin);
  void DoInit(ork::lev2::Context* pTARG) final;
  void DoDraw(ui::drawevent_constptr_t drwev) final;
  void DoSurfaceResize() final;
  ui::HandlerResult DoOnUiEvent(ui::event_constptr_t ev) final;
  EzMainWin* _mainwin;
  ui::layoutgroup_ptr_t _topLayoutGroup;
  std::atomic<int> _initstate;
};
////////////////////////////////////////////////////////////////////////////////
class OrkEzQtApp : public OrkEzQtAppBase {
  Q_OBJECT

public:
  ///////////////////////////////////
  OrkEzQtApp(int& argc, char** argv,const QtAppInitData& initdata);
  ~OrkEzQtApp();
  ///////////////////////////////////
  static qtezapp_ptr_t create();
  static qtezapp_ptr_t create(int argc, char** argv);
  static qtezapp_ptr_t create(int argc, char** argv, const QtAppInitData& initdata);
  static qtezapp_ptr_t createWithScene(varmap::varmap_ptr_t sceneparams);

  filedevctx_ptr_t newFileDevContext(std::string uriproto, const file::Path& basepath);

  void onDraw(EzMainWin::drawcb_t cb);
  void onResize(EzMainWin::onresizecb_t cb);
  void onGpuInit(EzMainWin::ongpuinit_t cb);
  void onUiEvent(EzMainWin::onuieventcb_t cb);
  void onUpdate(EzMainWin::onupdate_t cb);
  void setRefreshPolicy(RefreshPolicyItem policy);

  void onGpuInitWithScene(EzMainWin::ongpuinitwitchscene_t cb);
  void onUpdateWithScene(EzMainWin::onupdatewithscene_t cb);

  int runloop();
  void setSceneRunLoop(scenegraph::scene_ptr_t scene);

  void joinUpdate();
  bool checkAppState(uint64_t singlebitmask);
public slots:
  void OnTimer();

  void enqueueOnRenderer(const void_lambda_t& l);

  ///////////////////////////////////
public:
  QTimer mIdleTimer;
  QtAppInitData _initdata;
  EzMainWin* _mainWindow;
  std::map<std::string, filedevctx_ptr_t> _fdevctxmap;
  ork::Timer _update_timer;
  double _update_prevtime        = 0;
  double _update_timeaccumulator = 0;
  ork::Thread _updateThread;
  ork::opq::opq_ptr_t _mainq;
  ork::opq::opq_ptr_t _updq;
  ork::opq::opq_ptr_t _conq;
  varmap::VarMap _vars;
  std::atomic<uint64_t> _appstate;
  ui::updatedata_ptr_t _update_data;
  ui::context_ptr_t _uicontext;
  ui::layoutgroup_ptr_t _topLayoutGroup;
  std::shared_ptr<EzViewport> _ezviewport;
  ork::opq::opq_ptr_t _rthreadq;
};

} // namespace ork::lev2
