#pragma once

#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/init.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/file/file.h>
#include <ork/object/Object.h>
#include <ork/rtti/downcast.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/application/application.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/viewport.h>

#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtCore/QTimer>
#include <QtWidgets/QApplication>
#include <ork/lev2/qtui/qtui.h>
#include <ork/lev2/qtui/qtui.hpp>

namespace ork::lev2 {

class EzApp : public ork::Application {
  RttiDeclareAbstract(EzApp, ork::Application);

public:
  static std::shared_ptr<EzApp> create(int& argc, char** argv);
  ~EzApp() final;

private:
  EzApp(int& argc, char** argv);
  opq::TrackCurrent* _trackq;
};

struct UpdateData {
  double _dt = 0.0;
  double _abstime = 0.0;
};

class EzMainWin : public QMainWindow {

  // void resizeEvent(QResizeEvent* ev) final;
  // void paintEvent(QPaintEvent* ev) final;

public:
  typedef std::function<void(const ui::DrawEvent&)> drawcb_t;
  typedef std::function<void(int w, int h)> onresizecb_t;
  typedef std::function<void(Context* ctx)> ongpuinit_t;
  typedef std::function<void(UpdateData upd)> onupdate_t;
  typedef std::function<ui::HandlerResult(const ui::Event& ev)> onuieventcb_t;

  EzMainWin();
  ~EzMainWin();
  bool _dogpuinit          = true;
  CQtWindow* _gfxwin       = nullptr;
  CTQT* _ctqt              = nullptr;
  QCtxWidget* _ctxw        = nullptr;
  drawcb_t _onDraw         = nullptr;
  onresizecb_t _onResize   = nullptr;
  onuieventcb_t _onUiEvent = nullptr;
  ongpuinit_t _onGpuInit   = nullptr;
  onupdate_t _onUpdate = nullptr;

};

class OrkEzQtApp : public QApplication {
  Q_OBJECT

  OrkEzQtApp(int& argc, char** argv);

public:
  ///////////////////////////////////
  ~OrkEzQtApp() final;
  ///////////////////////////////////
  static std::shared_ptr<OrkEzQtApp> create(int& argc, char** argv);

  filedevctxptr_t newFileDevContext(std::string uribase);

  void onDraw(EzMainWin::drawcb_t cb);
  void onResize(EzMainWin::onresizecb_t cb);
  void onGpuInit(EzMainWin::ongpuinit_t cb);
  void onUiEvent(EzMainWin::onuieventcb_t cb);
  void onUpdate(EzMainWin::onupdate_t cb);
  void setRefreshPolicy(RefreshPolicyItem policy);

  int runloop();

public slots:
  void OnTimer();

  ///////////////////////////////////
public:
public:
  QTimer mIdleTimer;
  EzMainWin* _mainWindow;
  std::shared_ptr<EzApp> _ezapp;
  std::map<std::string, filedevctxptr_t> _fdevctxmap;
  ork::Timer _update_timer;
  double _update_prevtime = 0;
  double _update_timeaccumulator = 0;
  ork::Thread _updateThread;
  bool _updatekill;
};

} // namespace ork::lev2
