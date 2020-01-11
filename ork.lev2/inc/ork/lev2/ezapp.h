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
  static std::shared_ptr<EzApp> create(int argc, char** argv);
  ~EzApp() final;

private:
  EzApp(int argc, char** argv);
  ork::Thread _updateThread;
  bool _updatekill;
  opq::TrackCurrent* _trackq;
};

class EzMainWin : public QMainWindow {

  // void resizeEvent(QResizeEvent* ev) final;
  // void paintEvent(QPaintEvent* ev) final;

public:
  typedef std::function<void(const ui::DrawEvent&)> drawcb_t;
  typedef std::function<void(int w, int h)> onresizecb_t;
  typedef std::function<void(Context* ctx)> ongpuinit_t;
  typedef std::function<ui::HandlerResult(const ui::Event& ev)> onuieventcb_t;

  EzMainWin();
  ~EzMainWin();
  bool _dogpuinit          = true;
  CTQT* _ctqt              = nullptr;
  QCtxWidget* _ctxw        = nullptr;
  drawcb_t _onDraw         = nullptr;
  onresizecb_t _onResize   = nullptr;
  onuieventcb_t _onUiEvent = nullptr;
  ongpuinit_t _onGpuInit   = nullptr;
};

class OrkEzQtApp : public QApplication {
  Q_OBJECT

  OrkEzQtApp(int& argc, char** argv);

public:
  ///////////////////////////////////
  ///////////////////////////////////
  static std::shared_ptr<OrkEzQtApp> create(int argc, char** argv);

  void onDraw(EzMainWin::drawcb_t cb);
  void onResize(EzMainWin::onresizecb_t cb);
  void onGpuInit(EzMainWin::ongpuinit_t cb);
  void onUiEvent(EzMainWin::onuieventcb_t cb);

public slots:
  void OnTimer();

  ///////////////////////////////////
public:
public:
  QTimer mIdleTimer;
  EzMainWin* _mainWindow;
  std::shared_ptr<EzApp> _ezapp;
};

} // namespace ork::lev2
