#pragma once

#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/varmap.inl>
#include <ork/lev2/init.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/file/file.h>
#include <ork/object/Object.h>
#include <ork/rtti/downcast.h>
#include <ork/reflect/RegisterProperty.h>
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

namespace ork::lev2 {

class EzApp;

using ezapp_ptr_t = std::shared_ptr<EzApp>;

class EzApp : public ork::Application {
  RttiDeclareAbstract(EzApp, ork::Application);

public:
  static ezapp_ptr_t create(int& argc, char** argv);
  ~EzApp() final;

private:
  EzApp(int& argc, char** argv);
  opq::TrackCurrent* _trackq;

  ork::opq::opq_ptr_t _mainq;
  ork::opq::opq_ptr_t _conq;
};

struct UpdateData {
  double _dt      = 0.0;
  double _abstime = 0.0;
};
using updatedata_ptr_t = std::shared_ptr<UpdateData>;

class EzMainWin : public QMainWindow {

  // void resizeEvent(QResizeEvent* ev) final;
  // void paintEvent(QPaintEvent* ev) final;

public:
  typedef std::function<void(const ui::DrawEvent&)> drawcb_t;
  typedef std::function<void(int w, int h)> onresizecb_t;

  typedef std::function<void(Context* ctx)> ongpuinit_t;
  typedef std::function<void(updatedata_ptr_t upd)> onupdate_t;

  typedef std::function<void(Context* ctx, scenegraph::scene_ptr_t)> ongpuinitwitchscene_t;
  typedef std::function<void(updatedata_ptr_t upd, scenegraph::scene_ptr_t)> onupdatewithscene_t;

  typedef std::function<ui::HandlerResult(const ui::Event& ev)> onuieventcb_t;

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
  ork::Timer _render_timer;
  double _render_prevtime        = 0;
  double _render_stats_timeaccum = 0;
  double _render_state_numiters  = 0.0;
};

class OrkEzQtApp;
using qtezapp_ptr_t = std::shared_ptr<OrkEzQtApp>;

class OrkEzQtApp : public QApplication {
  Q_OBJECT

public:
  ///////////////////////////////////
  OrkEzQtApp(int& argc, char** argv);
  ~OrkEzQtApp() final;
  ///////////////////////////////////
  static qtezapp_ptr_t create();
  static qtezapp_ptr_t create(int argc, char** argv);
  static qtezapp_ptr_t createWithScene();

  filedevctxptr_t newFileDevContext(std::string uribase);

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

public slots:
  void OnTimer();

  ///////////////////////////////////
public:
  QTimer mIdleTimer;
  EzMainWin* _mainWindow;
  std::shared_ptr<EzApp> _ezapp;
  std::map<std::string, filedevctxptr_t> _fdevctxmap;
  ork::Timer _update_timer;
  double _update_prevtime        = 0;
  double _update_timeaccumulator = 0;
  ork::Thread _updateThread;
  bool _updatekill;
  ork::opq::opq_ptr_t _mainq;
  ork::opq::opq_ptr_t _updq;
  ork::opq::opq_ptr_t _conq;
  varmap::VarMap _vars;
  std::atomic<uint64_t> _appstate;
  updatedata_ptr_t _update_data;
};

} // namespace ork::lev2
