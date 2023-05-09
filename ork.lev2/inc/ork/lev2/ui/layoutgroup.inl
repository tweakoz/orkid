////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/group.h>
#include <ork/lev2/ui/anchor.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// LayoutGroup :  collection of widgets which are layed out...
////////////////////////////////////////////////////////////////////

struct LayoutItemBase {

  inline void applyBounds(const anchor::Bounds& bounds) {
    if (bounds._top)
      _layout->top()->anchorTo(bounds._top);
    if (bounds._left)
      _layout->left()->anchorTo(bounds._left);
    if (bounds._bottom)
      _layout->bottom()->anchorTo(bounds._bottom);
    if (bounds._right)
      _layout->right()->anchorTo(bounds._right);
    _layout->setMargin(bounds._margin);
  }

  std::shared_ptr<Widget> _widget;
  anchor::layout_ptr_t _layout;
};

template <typename T> struct LayoutItem : public LayoutItemBase {
  std::shared_ptr<T> typedWidget(){
    return dynamic_pointer_cast<T>(_widget);
  }
};

using layoutitem_ptr_t = std::shared_ptr<LayoutItemBase>;

struct LayoutGroup : public Group {

  LayoutGroup(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);

  anchor::layout_ptr_t _layout;

  //////////////////////////////////////
  template <typename T, typename... A> //
  LayoutItem<T> makeChild(A&&... args) {
    LayoutItem<T> rval;
    rval._widget = std::make_shared<T>(std::forward<A>(args)...);
    rval._layout = _layout->childLayout(rval._widget.get());
    addChild(rval._widget);
    return rval;
  }
  //////////////////////////////////////
  template <typename T, typename... A> //
  std::vector<LayoutItem<T>> makeGridOfWidgets(int w, int h, A&&... args) {
    std::vector<LayoutItem<T>> widgets;
    for (int x = 0; x < w; x++) {
      float fxa = float(x) / float(w);
      float fxb = float(x + 1) / float(w);
      auto gxa  = _layout->proportionalVerticalGuide(fxa); // 23,27,31,35
      auto gxb  = _layout->proportionalVerticalGuide(fxb); // 24,28,32,36
      for (int y = 0; y < h; y++) {
        float fya   = float(y) / float(h);
        float fyb   = float(y + 1) / float(h);
        auto gya    = _layout->proportionalHorizontalGuide(fya); // 25,29,33,37
        auto gyb    = _layout->proportionalHorizontalGuide(fyb); // 26,30,34,38
        auto name   = _name + FormatString("-ch-%d", (y * w + x));
        auto chitem = this->makeChild<T>(std::forward<A>(args)...);
        widgets.push_back(chitem);
        chitem._layout->setMargin(2);
        chitem._layout->top()->anchorTo(gya);
        chitem._layout->left()->anchorTo(gxa);
        chitem._layout->bottom()->anchorTo(gyb);
        chitem._layout->right()->anchorTo(gxb);
      }
    }
    return widgets;
  }
  //////////////////////////////////////
  inline anchor::layout_ptr_t layoutAndAddChild(widget_ptr_t w) {
    auto layout = _layout->childLayout(w.get());
    addChild(w);
    return layout;
  }
  //////////////////////////////////////
  inline void removeChild(anchor::layout_ptr_t ch) {
    _layout->removeChild(ch);
    Group::removeChild(ch->_widget);
  }
  //////////////////////////////////////
  inline void replaceChild(anchor::layout_ptr_t ch, 
                           layoutitem_ptr_t rep) {
    _layout->removeChild(rep->_layout);
    Group::removeChild(ch->_widget);
    Group::addChild(rep->_widget);
    ch->_widget = rep->_widget.get();
    rep->_layout = ch;
  }
  //////////////////////////////////////
  inline void setClearColor(fvec4 clr) {
    _clearColor = clr;
  }
  //////////////////////////////////////
  inline fvec4 clearColor() const {
    return _clearColor;
  }
  //////////////////////////////////////
private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;
  void _doOnResized() override;
  void DoLayout() override;
  bool _clear = true;
  fvec4 _clearColor;
};

} // namespace ork::ui
