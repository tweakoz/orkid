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
  std::shared_ptr<T> typedWidget() {
    return dynamic_pointer_cast<T>(_widget);
  }
  std::shared_ptr<LayoutItemBase> as_shared() const {
    auto shlitem = std::make_shared<ui::LayoutItemBase>();
    shlitem->_widget = _widget;
    shlitem->_layout = _layout;
    return shlitem;
  }
};

using layoutitem_ptr_t = std::shared_ptr<LayoutItemBase>;

struct LayoutGroup : public Group {

  LayoutGroup(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);
  ~LayoutGroup();

  //////////////////////////////////////
  template <typename T, typename... A> //
  LayoutItem<T> makeChild(A&&... args) {
    LayoutItem<T> rval;
    rval._widget = std::make_shared<T>(std::forward<A>(args)...);
    rval._layout = _layout->childLayout(rval._widget.get()); // HERE (name==2)
    addChild(rval._widget);
    return rval;
  }
  //////////////////////////////////////
  template <typename T, typename... A> //
  layoutitem_ptr_t makeChild2(A&&... args) {
    layoutitem_ptr_t rval;
    rval          = std::make_shared<LayoutItem<T>>();
    rval->_widget = std::make_shared<T>(std::forward<A>(args)...);
    rval->_layout = _layout->childLayout(rval->_widget.get());
    addChild(rval->_widget);
    return rval;
  }
  //////////////////////////////////////
  template <typename T, typename... A> //
  std::vector<LayoutItem<T>> makeWidgetsRC(std::vector<int> rccounts, A&&... args) {
    std::vector<LayoutItem<T>> layout_items;
    ui::anchor::guide_ptr_t gxa, gxb;
    ui::anchor::guide_ptr_t gya, gyb;
    int h = rccounts.size();
    for (int y = 0; y < h; y++) {
      float fya = float(y) / float(h);
      float fyb = float(y + 1) / float(h);
      if (y == 0) {
        gya          = _layout->proportionalHorizontalGuide(fya); // 23,27,31,35
        gya->_margin = _margin;
      } else {
        gya = gyb;
      }
      gyb          = _layout->proportionalHorizontalGuide(fyb); // 24,28,32,36
      gyb->_margin = _margin;
      _hguides.insert(gya);
      _hguides.insert(gyb);
      int w = rccounts[y];
      for (int x = 0; x < w; x++) {
        float fxa = float(x) / float(w);
        float fxb = float(x + 1) / float(w);
        if (x == 0) {
          gxa          = _layout->proportionalVerticalGuide(fxa); // 25,29,33,37
          gxa->_margin = _margin;
        } else {
          gxa = gxb;
        }
        gxb          = _layout->proportionalVerticalGuide(fxb); // 25,29,33,37
        gxb->_margin = _margin;
        _vguides.insert(gxa);
        _vguides.insert(gxb);
        auto name   = _name + FormatString("-ch-%d", (y * w + x));
        auto chitem = this->makeChild<T>(std::forward<A>(args)...);
        layout_items.push_back(chitem);
        chitem._layout->setMargin(_margin);
        chitem._layout->top()->anchorTo(gya);
        chitem._layout->left()->anchorTo(gxa);
        chitem._layout->bottom()->anchorTo(gyb);
        chitem._layout->right()->anchorTo(gxb);
      }
    }
    return layout_items; 
  }
  //////////////////////////////////////
  template <typename T, typename... A> //
  std::vector<LayoutItem<T>> makeGridOfWidgets(int w, int h, A&&... args) {
    std::vector<LayoutItem<T>> layout_items;
    ui::anchor::guide_ptr_t gxa, gxb;
    ui::anchor::guide_ptr_t gya, gyb;
    for (int x = 0; x < w; x++) {
      float fxa = float(x) / float(w);
      float fxb = float(x + 1) / float(w);
      if (x == 0) {
        gxa          = _layout->proportionalVerticalGuide(fxa); // 23,27,31,35
        gxa->_margin = _margin;
      } else {
        gxa = gxb;
      }
      gxb          = _layout->proportionalVerticalGuide(fxb); // 24,28,32,36
      gxb->_margin = _margin;
      _vguides.insert(gxa);
      _vguides.insert(gxb);
      for (int y = 0; y < h; y++) {
        float fya = float(y) / float(h);
        float fyb = float(y + 1) / float(h);
        if (y == 0) {
          gya          = _layout->proportionalHorizontalGuide(fya); // 25,29,33,37
          gya->_margin = _margin;
        } else {
          gya = gyb;
        }
        gyb          = _layout->proportionalHorizontalGuide(fyb); // 25,29,33,37
        gyb->_margin = _margin;
        _hguides.insert(gya);
        _hguides.insert(gyb);
        auto name   = _name + FormatString("-ch-%d", (y * w + x));
        auto chitem = this->makeChild<T>(std::forward<A>(args)...);
        layout_items.push_back(chitem);
        chitem._layout->setMargin(_margin);
        chitem._layout->top()->anchorTo(gya);
        chitem._layout->left()->anchorTo(gxa);
        chitem._layout->bottom()->anchorTo(gyb);
        chitem._layout->right()->anchorTo(gxb);
      }
    }
    return layout_items;
  }
  //////////////////////////////////////
  anchor::layout_ptr_t layoutAndAddChild(widget_ptr_t w);
  void removeChild(anchor::layout_ptr_t ch);
  void replaceChild(anchor::layout_ptr_t ch, layoutitem_ptr_t rep);
  void setClearColor(fvec4 clr);
  fvec4 clearColor() const;
  const std::set<uiguide_ptr_t>& horizontalGuides() const;
  const std::set<uiguide_ptr_t>& verticalGuides() const;
  HandlerResult OnUiEvent(event_constptr_t ev);
  //////////////////////////////////////
  anchor::layout_ptr_t _layout;

  int _margin = 2;
  bool _clear = true;
  fvec4 _clearColor;
  bool _lockLayout = false;
  Widget* doRouteUiEvent(event_constptr_t Ev) override;

private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;
  void _doOnResized() override;
  void DoLayout() override;
  std::set<uiguide_ptr_t> _hguides;
  std::set<uiguide_ptr_t> _vguides;
};

} // namespace ork::ui
