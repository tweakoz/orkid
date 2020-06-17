#pragma once

#include <ork/util/fsm.h>
#include <ork/lev2/ui/event.h>
#include <functional>

namespace ork::ui {

struct IWidgetEventFilter {
  IWidgetEventFilter(Widget& w);
  void Filter(event_constptr_t Ev);

  virtual void DoFilter(event_constptr_t Ev) = 0;

  Widget& mWidget;
  bool mShiftDown;
  bool mCtrlDown;
  bool mMetaDown;
  bool mAltDown;
  bool mLeftDown;
  bool mMiddleDown;
  bool mRightDown;
  bool mCapsDown;
  bool mBut0Down;
  bool mBut1Down;
  bool mBut2Down;
  int mLastKeyCode;
  std::vector<int> mKeySequence;
  Timer mKeyTimer;
  Timer mDoubleTimer;
  Timer mMoveTimer;
};
struct Apple3ButtonMouseEmulationFilter : public IWidgetEventFilter {
  Apple3ButtonMouseEmulationFilter(Widget& w)
      : IWidgetEventFilter(w) {
  }
  void DoFilter(event_constptr_t Ev);
};
struct NopEventFilter : public IWidgetEventFilter {
  NopEventFilter(Widget& w)
      : IWidgetEventFilter(w) {
  }
  void DoFilter(event_constptr_t Ev);
};

struct Widget : public ork::Object {
  RttiDeclareAbstract(Widget, ork::Object);

  friend struct Group;

public:
  static const int keycode_shift = 16777248; // Qt::Key_Shift;
  static const int keycode_cmd   = 16777249;
  static const int keycode_alt   = 16777251;
  static const int keycode_ctrl  = 16777250;

  Widget(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);
  ~Widget();

  void Init(lev2::Context* pTARG);

  template <typename T> std::shared_ptr<T> pushEventFilter() {
    auto filter = std::make_shared<T>(*this);
    _eventfilterstack.push(filter);
    return filter;
  }
  void popEventFilter() {
    _eventfilterstack.pop();
  }

  const std::string& GetName(void) const {
    return msName;
  }
  void SetName(const std::string& name) {
    msName = name;
  }

  lev2::Context* GetTarget(void) const {
    return mpTarget;
  }

  int x(void) const {
    return _geometry._x;
  }
  int y(void) const {
    return _geometry._y;
  }
  int x2(void) const {
    return _geometry.x2();
  }
  int y2(void) const {
    return _geometry.y2();
  }
  int width(void) const {
    return _geometry._w;
  }
  int height(void) const {
    return _geometry._h;
  }
  void SetX(int X) {
    SetPos(X, y());
  }
  void SetY(int Y) {
    SetPos(x(), Y);
  }
  void SetX2(int X2) {
    SetSize((X2 - x()), height());
  }
  void SetY2(int Y2) {
    SetSize(x(), (Y2 - y()));
  }
  void SetW(int W) {
    SetSize(W, height());
  }
  void SetH(int H) {
    SetSize(width(), H);
  }

  void LocalToRoot(int lx, int ly, int& rx, int& ry) const;
  void RootToLocal(int rx, int ry, int& lx, int& ly) const;

  void SetPos(int iX, int iY);
  void SetSize(int iW, int iH);
  void SetRect(int iX, int iY, int iW, int iH);

  virtual void Show(void) {
  }
  virtual void Hide(void) {
  }

  void ExtDraw(lev2::Context* pTARG);
  virtual void Draw(ui::drawevent_constptr_t drwev);

  void GotKeyboardFocus(void) {
    mbKeyboardFocus = true;
  }
  void LostKeyboardFocus(void) {
    mbKeyboardFocus = false;
  }
  bool HasKeyboardFocus(void) const {
    return mbKeyboardFocus;
  }

  bool IsKeyDepressed(int ch);
  bool IsHotKeyDepressed(const char* pact);
  bool IsHotKeyDepressed(const HotKey& hk);

  HandlerResult handleUiEvent(event_constptr_t Ev);

  bool IsEventInside(event_constptr_t Ev) const;

  void SetDirty();
  bool IsDirty() const {
    return mDirty;
  }
  Group* parent() const {
    return mParent;
  }
  Group* root() const;

  // CWidgetFlags &GetFlagsRef( void ) { return mWidgetFlags; }

  virtual void OnResize(void);
  HandlerResult OnUiEvent(event_constptr_t Ev);

  bool hasMouseFocus() const;
  void SetParent(Group* p) {
    mParent = p;
  }

  Widget* routeUiEvent(event_constptr_t Ev);

  float logicalWidth() const;
  float logicalHeight() const;
  float logicalX() const;
  float logicalY() const;

  Rect geometry() const {
    return _geometry;
  }
  void setGeometry(Rect geo);

  Context* _uicontext = nullptr;

protected:
  bool mbInit;
  bool mbKeyboardFocus;
  Rect _geometry;
  Rect _prevGeometry;
  std::string msName;
  lev2::Context* mpTarget;
  drawevent_constptr_t _drawEvent;
  bool mDirty;
  bool mSizeDirty;
  bool mPosDirty;
  Group* mParent;
  std::stack<eventfilter_ptr_t> _eventfilterstack;

private:
  friend struct ui::Context;
  virtual void DoInit(lev2::Context* pTARG) {
  }
  virtual void DoDraw(ui::drawevent_constptr_t drwev) = 0;
  virtual HandlerResult DoOnUiEvent(event_constptr_t Ev);
  void ReLayout();
  virtual void DoLayout() {
  }
  virtual Widget* doRouteUiEvent(event_constptr_t Ev);
};

} // namespace ork::ui
