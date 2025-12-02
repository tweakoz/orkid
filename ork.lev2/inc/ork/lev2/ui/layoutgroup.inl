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

  LayoutGroup(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0, int margin = 0);
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
    int h = rccounts.size();

    // Step 1: Create horizontal guides for row boundaries (on parent layout)
    std::vector<ui::anchor::guide_ptr_t> hguides;
    for (int y = 0; y <= h; y++) {
      float fy = float(y) / float(h);
      auto guide = _layout->proportionalHorizontalGuide(fy);
      hguides.push_back(guide);
      _hguides.insert(guide);

      bool is_edge = (y == 0) || (y == h);
      if (is_edge) {
        guide->_locked = true;
      }
    }

    // Step 2: For each row, create a row container with its own vertical guides
    for (int y = 0; y < h; y++) {
      int w = rccounts[y];

      // Create invisible row container with margin=0
      // The guides themselves have the margin, we don't want to compound it
      auto row_name = _name + FormatString("-row-%d", y);
      auto row_group = std::make_shared<LayoutGroup>(row_name, 0, 0, 0, 0, 0);
      row_group->_clear = false;  // Don't draw background
      row_group->_ignoreEvents = true;  // Don't intercept events - let root LayoutGroup handle guide dragging
      addChild(row_group);

      // Anchor row container to horizontal guides (spans full width)
      auto row_layout = row_group->_layout;
      row_layout->top()->anchorTo(hguides[y]);
      row_layout->bottom()->anchorTo(hguides[y + 1]);
      row_layout->left()->anchorTo(_layout->left());
      row_layout->right()->anchorTo(_layout->right());

      // Create vertical guides within this row's layout
      // Use parent's left/right edge guides instead of creating duplicates
      std::vector<ui::anchor::guide_ptr_t> row_vguides;

      for (int x = 0; x <= w; x++) {
        ui::anchor::guide_ptr_t guide;

        if (x == 0) {
          // Use parent's left edge guide
          guide = row_layout->left();
        } else if (x == w) {
          // Use parent's right edge guide
          guide = row_layout->right();
        } else {
          // Create interior guide with explicit margin from parent
          float fx = float(x) / float(w);
          guide = row_layout->proportionalVerticalGuide(fx);
          guide->_margin = _margin;  // Explicitly set margin from parent
          row_group->_vguides.insert(guide);
          _vguides.insert(guide);
        }

        row_vguides.push_back(guide);
      }

      // Create cells within this row
      for (int x = 0; x < w; x++) {
        auto name = _name + FormatString("-ch-%d", layout_items.size());
        auto chitem = row_group->makeChild<T>(std::forward<A>(args)...);
        layout_items.push_back(chitem);

        // Set margin on cell layout - this creates the spacing when edges anchor to guides
        chitem._layout->setMargin(_margin);
        chitem._layout->top()->anchorTo(row_layout->top());
        chitem._layout->bottom()->anchorTo(row_layout->bottom());
        chitem._layout->left()->anchorTo(row_vguides[x]);
        chitem._layout->right()->anchorTo(row_vguides[x + 1]);
      }

      // Update this row's layout geometry
      row_layout->updateAll();
    }

    // Update parent layout geometry
    _layout->updateAll();

    return layout_items;
  }
  //////////////////////////////////////
  template <typename T, typename... A> //
  std::vector<LayoutItem<T>> makeGridOfWidgets(int w, int h, A&&... args) {
    std::vector<LayoutItem<T>> layout_items;

    // Create all vertical guides first (shared across all rows)
    std::vector<ui::anchor::guide_ptr_t> vguides;
    for (int x = 0; x <= w; x++) {
      float fx = float(x) / float(w);
      auto guide = _layout->proportionalVerticalGuide(fx);
      // Margin inherited from layout
      vguides.push_back(guide);
      _vguides.insert(guide);
      bool first = (x == 0);
      bool last = (x == w);
      if(first or last){
        guide->_locked = true;
      }
    }

    // Create all horizontal guides (shared across all columns)
    std::vector<ui::anchor::guide_ptr_t> hguides;
    for (int y = 0; y <= h; y++) {
      float fy = float(y) / float(h);
      auto guide = _layout->proportionalHorizontalGuide(fy);
      // Margin inherited from layout
      hguides.push_back(guide);
      _hguides.insert(guide);
      bool first = (y == 0);
      bool last = (y == h);
      if(first or last){
        guide->_locked = true;  
      }
    }

    // Now create cells and anchor them to the appropriate guides
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        auto name   = _name + FormatString("-ch-%d", (y * w + x));
        auto chitem = this->makeChild<T>(std::forward<A>(args)...);
        layout_items.push_back(chitem);
        chitem._layout->setMargin(_margin);
        chitem._layout->top()->anchorTo(hguides[y]);
        chitem._layout->left()->anchorTo(vguides[x]);
        chitem._layout->bottom()->anchorTo(hguides[y + 1]);
        chitem._layout->right()->anchorTo(vguides[x + 1]);
      }
    }
    return layout_items;
  }
  //////////////////////////////////////
  layoutitem_ptr_t split(anchor::layout_ptr_t target_layout,
                         float proportion,
                         anchor::ELayoutSplitPlacement placement,
                         int margin);
  //////////////////////////////////////
  anchor::layout_ptr_t layoutAndAddChild(widget_ptr_t w);
  void removeChild(anchor::layout_ptr_t ch);
  void replaceChild(anchor::layout_ptr_t ch, layoutitem_ptr_t rep);
  const std::set<uiguide_ptr_t>& horizontalGuides() const;
  const std::set<uiguide_ptr_t>& verticalGuides() const;

  // Find guide between two layouts (returns nullptr if not found or ambiguous)
  anchor::guide_ptr_t findGuideBetween(anchor::layout_ptr_t layout_a, anchor::layout_ptr_t layout_b);

  // Comprehensive dump of layout hierarchy and guides
  void dumpLayoutHierarchy();

  HandlerResult OnUiEvent(event_constptr_t ev);

  anchor::layout_ptr_t _layout;

  bool _clear = true;
  fvec4 _clearColorStd;
  fvec4 _clearColorGuide;
  Timer _animtimer;

  anchor::guide_ptr_t _guide_being_dragged = nullptr;
  anchor::guide_ptr_t _guide_highlite = nullptr;

  Widget* doRouteUiEvent(event_constptr_t Ev) override;

  widget_ptr_t _overlay_widget = nullptr;  // Overlay widget (e.g., LoggerGroup)
  bool _overlay_enabled = false;           // Whether overlay is currently visible

private:
  void DoDraw(ui::drawevent_constptr_t drwev) override;
  void _doOnResized() override;
  void DoLayout() override;
  void _positionOverlay();  // Position overlay with 10% margin
  std::set<uiguide_ptr_t> _hguides;
  std::set<uiguide_ptr_t> _vguides;
};

} // namespace ork::ui
