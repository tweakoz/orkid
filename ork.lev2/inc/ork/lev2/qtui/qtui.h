////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/orkconfig.h>

#pragma once

#if defined(ORK_CONFIG_QT)

///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/gfxenv.h>
///////////////////////////////////////////////////////////////////////////////

extern int QtTest(int argc, char** argv);
#define register

///////////////////////////////////////////////////////////////////////////////
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaObject>
#include <QtCore/QSize>
#include <QtCore/QTimer>
#include <QtCore/qdatastream.h>
#include <QtCore/qmetatype.h>
#include <QtGui/QMouseEvent>
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include <QtGui/QResizeEvent>
#include <QtGui/QShowEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QOpenGLWidget>
#include <QtWidgets/QSplashScreen>
#include <QtWidgets/QWidget>

#undef register

#include <ork/lev2/ui/event.h>

#include <ork/lev2/gfx/ctxbase.h>

///////////////////////////////////////////////////////////////////////////////

struct SmtFinger;
typedef struct SmtFinger MtFinger;

namespace ork {

std::string TypeIdNameStrip(const char* name);
std::string TypeIdName(const std::type_info* ti);

namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

QPoint logicalMousePos();

class CTQT;

class QCtxWidget : public QWidget {
  friend class CTQT;

public:
  void Enable() {
    mbEnabled = true;
  }
  bool IsEnabled() const {
    return mbEnabled;
  }

  bool event(QEvent* event) final;

  void focusInEvent(QFocusEvent* event) final;
  void focusOutEvent(QFocusEvent* event) final;
  void showEvent(QShowEvent* event) final;

  void mouseMoveEvent(QMouseEvent* event) final;
  void mousePressEvent(QMouseEvent* event) final;
  void mouseReleaseEvent(QMouseEvent* event) final;
  void mouseDoubleClickEvent(QMouseEvent* event) final;

  void keyPressEvent(QKeyEvent* event) final;
  void keyReleaseEvent(QKeyEvent* event) final;
  void wheelEvent(QWheelEvent* event) final;
  void resizeEvent(QResizeEvent* event) final;
  void paintEvent(QPaintEvent* event) final;

  void MouseEventCommon(QMouseEvent* event);

  ui::event_constptr_t uievent() const;
  ui::event_ptr_t uievent();
  Context* Target() const;
  Window* GetWindow() const;
  bool AlwaysRun() const;

  QPaintEngine* paintEngine() const final {
    return nullptr;
  }

  QCtxWidget(CTQT* pctxbase, QWidget* parent);
  ~QCtxWidget();

protected:
  QTimer mQtTimer;
  ork::Timer _pushTimer;
  bool mbSignalConnected;
  CTQT* mpCtxBase;
  bool mbEnabled;
  int miWidth;
  int miHeight;

private:
  void SendOrkUiEvent();
  ui::Widget* _evstealwidget = nullptr;
};

class CTQT : public CTXBASE {
  friend class QCtxWidget;

  bool mbAlwaysRun;
  QCtxWidget* mpQtWidget;
  int mix, miy, miw, mih;
  QWidget* mParent;
  int mDrawLock;

  void SlotRepaint() final;

public:
  void Show() final;
  void Hide() final;

  void _setRefreshPolicy(RefreshPolicyItem epolicy) final;

  QTimer& Timer() const;
  CTQT(Window* pwin, QWidget* parent = 0);
  ~CTQT();

  void Resize(int X, int Y, int W, int H);
  void SetParent(QWidget* pw);
  void SetAlwaysRun(bool brun) {
    mbAlwaysRun = brun;
  }
  void* winId() const {
    return (void*)mpQtWidget->winId();
  }
  QCtxWidget* GetQWidget() const {
    return mpQtWidget;
  }
  QWidget* GetParent() const {
    return mParent;
  }

  fvec2 MapCoordToGlobal(const fvec2& v) const override;
};

///////////////////////////////////////////////////////////////////////////////

class CQtWindow : public ork::lev2::Window {
public:
  bool mbinit;
  ui::Widget* mRootWidget;

  CQtWindow(ui::Widget* root_widget);
  ~CQtWindow();

  virtual void Draw(void);
  // virtual void Show( void );
  virtual void GotFocus(void);
  virtual void LostFocus(void);
  virtual void Hide(void) {
  }
  /*virtual*/ void OnShow();
};

///////////////////////////////////////////////////////////////////////////////

} // namespace lev2
} // namespace ork

///////////////////////////////////////////////////////////////////////////////

#endif // ORK_CONFIG_QT
