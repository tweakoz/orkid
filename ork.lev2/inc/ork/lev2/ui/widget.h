////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/util/fsm.h>
#include <ork/lev2/ui/event.h>
#include <functional>

namespace ork::ui {


struct IWidgetEventFilter {
  IWidgetEventFilter(Widget& w);
  virtual ~IWidgetEventFilter() {}
  
  void Filter(event_constptr_t Ev);

  virtual void DoFilter(event_constptr_t Ev) = 0;

  bool mShiftDown  = false;
  bool mCtrlDown   = false;
  bool mMetaDown   = false;
  bool mAltDown    = false;
  bool mLeftDown   = false;
  bool mMiddleDown = false;
  bool mRightDown  = false;
  bool mCapsDown   = false;
  bool mBut0Down   = false;
  bool mBut1Down   = false;
  bool mBut2Down   = false;
  int mLastKeyCode = 0;

  Widget& mWidget;
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
  DeclareAbstractX(Widget, ork::Object);

  friend struct Group;

public:
  static const int keycode_shift = 16777248; // Qt::Key_Shift;
  static const int keycode_cmd   = 16777249;
  static const int keycode_alt   = 16777251;
  static const int keycode_ctrl  = 16777250;

  Widget(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);
  ~Widget();

  void gpuInit(lev2::Context* pTARG);

  template <typename T> std::shared_ptr<T> pushEventFilter() {
    auto filter = std::make_shared<T>(*this);
    _eventfilterstack.push(filter);
    return filter;
  }
  void popEventFilter() {
    _eventfilterstack.pop();
  }

  const std::string& GetName(void) const {
    return _name;
  }
  void SetName(const std::string& name) {
    _name = name;
  }

  lev2::Context* GetTarget(void) const {
    return _target;
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
  void setX(int X) {
    SetPos(X, y());
  }
  void setY(int Y) {
    SetPos(x(), Y);
  }
  void SetX2(int X2) {
    SetSize((X2 - x()), height());
  }
  void SetY2(int Y2) {
    SetSize(x(), (Y2 - y()));
  }
  void setW(int W) {
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
  virtual void draw(ui::drawevent_constptr_t drwev);

  bool IsKeyDepressed(int ch);
  bool IsHotKeyDepressed(const char* pact);
  bool IsHotKeyDepressed(const HotKey& hk);

  HandlerResult handleUiEvent(event_constptr_t Ev);

  bool IsEventInside(event_constptr_t Ev) const;

  void SetDirty();
  bool IsDirty() const {
    return _dirty;
  }
  Group* parent() const {
    return _parent;
  }

  Group* root() const;

  // CWidgetFlags &GetFlagsRef( void ) { return mWidgetFlags; }

  void onPreDestroy();
  
  virtual void _doOnResized(void);
  virtual void _doOnParentChanged(Group* parent) {}
  virtual void _doOnPreDestroy() {}
  HandlerResult OnUiEvent(event_constptr_t Ev);

  bool hasMouseFocus() const;
  void setParent(Group* p);

  Widget* routeUiEvent(event_constptr_t Ev);

  float logicalWidth() const;
  float logicalHeight() const;
  float logicalX() const;
  float logicalY() const;

  Rect geometry() const {
    return _geometry;
  }
  void setGeometry(Rect geo);

  bool _needsinit        = true;
  bool _dirty            = true;
  bool mSizeDirty        = true;
  bool mPosDirty         = true;
  Group* _parent         = nullptr;
  lev2::Context* _target = nullptr;
  Context* _uicontext    = nullptr;
  evrouter_t _evrouter   = nullptr;
  evhandler_t _evhandler = nullptr;
  bool _ignoreEvents = false;

  std::string _name;
  uint64_t _userID = 0;
  drawevent_constptr_t _drawEvent;
  Rect _geometry;
  std::stack<eventfilter_ptr_t> _eventfilterstack;
  Rect _prevGeometry;
  varmap::VarMap _uservars;

  virtual Widget* doRouteUiEvent(event_constptr_t Ev);

private:
  friend struct ui::Context;
  virtual void _doGpuInit(lev2::Context* pTARG) {
  }
  virtual void DoDraw(ui::drawevent_constptr_t drwev) = 0;
  virtual HandlerResult DoOnUiEvent(event_constptr_t Ev);
  void ReLayout();
  virtual void DoLayout() {
  }

};

} // namespace ork::ui
