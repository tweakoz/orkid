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

struct GridParams {
  int _rows       = 1;
  int _cols       = 1;
  int _margin     = 4;
  std::vector<float> _h_proportions;
  std::vector<float> _v_proportions;
};
using gridparams_ptr_t = std::shared_ptr<GridParams>;
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

    // Step 1: Create horizontal guides for row boundaries (on root layout)
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

    // Step 2: For each row, create vertical guides and cells directly on root layout
    // Each row can have different column counts, so vertical guides are per-row
    // but they're created on the root layout and span only their row's vertical range
    for (int y = 0; y < h; y++) {
      int w = rccounts[y];

      // Create vertical guides for this row on the root layout
      // These guides span from hguides[y] to hguides[y+1]
      std::vector<ui::anchor::guide_ptr_t> row_vguides;

      for (int x = 0; x <= w; x++) {
        ui::anchor::guide_ptr_t guide;

        if (x == 0) {
          // Use root layout's left edge guide
          guide = _layout->left();
        } else if (x == w) {
          // Use root layout's right edge guide
          guide = _layout->right();
        } else {
          // Create interior vertical guide on root layout
          // Use newProportionalVerticalGuide to ensure unique per-row guides
          float fx = float(x) / float(w);
          guide = _layout->newProportionalVerticalGuide(fx);
          guide->_margin = _margin;
          // Set extent guides so vertical guide only spans this row
          guide->_extentMin = hguides[y];      // top of row
          guide->_extentMax = hguides[y + 1];  // bottom of row
          // Set constraint group so guides only constrain within same row
          guide->_constraintGroup = uint64_t(1) << y;
          _vguides.insert(guide);
        }

        row_vguides.push_back(guide);
      }

      // Create cells directly as children of this LayoutGroup
      for (int x = 0; x < w; x++) {
        auto name = _name + FormatString("-ch-%d", layout_items.size());
        auto chitem = this->makeChild<T>(std::forward<A>(args)...);
        layout_items.push_back(chitem);

        // Set margin on cell layout
        chitem._layout->setMargin(_margin);
        // Anchor to horizontal guides for row bounds
        chitem._layout->top()->anchorTo(hguides[y]);
        chitem._layout->bottom()->anchorTo(hguides[y + 1]);
        // Anchor to vertical guides for column bounds
        chitem._layout->left()->anchorTo(row_vguides[x]);
        chitem._layout->right()->anchorTo(row_vguides[x + 1]);
      }
    }

    // Update layout geometry
    _layout->updateAll();

    return layout_items;
  }
  //////////////////////////////////////
  // Overload with h_splits for custom column proportions per row
  // h_splits[row] contains split points for that row (w-1 values for w columns)
  template <typename T, typename... A>
  std::vector<LayoutItem<T>> makeWidgetsRCWithSplits(
      std::vector<int> rccounts,
      const std::vector<std::vector<float>>& h_splits,
      A&&... args) {
    std::vector<LayoutItem<T>> layout_items;
    int h = rccounts.size();

    // Step 1: Create horizontal guides for row boundaries (on root layout)
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

    // Step 2: For each row, create vertical guides and cells directly on root layout
    for (int y = 0; y < h; y++) {
      int w = rccounts[y];

      // Get splits for this row (if provided)
      const std::vector<float>* row_splits = nullptr;
      if (y < (int)h_splits.size() && !h_splits[y].empty()) {
        row_splits = &h_splits[y];
      }

      // Create vertical guides for this row on the root layout
      std::vector<ui::anchor::guide_ptr_t> row_vguides;

      for (int x = 0; x <= w; x++) {
        ui::anchor::guide_ptr_t guide;

        if (x == 0) {
          guide = _layout->left();
        } else if (x == w) {
          guide = _layout->right();
        } else {
          // Use h_splits if provided, otherwise divide evenly
          // Use newProportionalVerticalGuide to ensure unique per-row guides
          float fx;
          if (row_splits && (x - 1) < (int)row_splits->size()) {
            fx = (*row_splits)[x - 1];
          } else {
            fx = float(x) / float(w);
          }
          guide = _layout->newProportionalVerticalGuide(fx);
          guide->_margin = _margin;
          guide->_extentMin = hguides[y];
          guide->_extentMax = hguides[y + 1];
          guide->_constraintGroup = uint64_t(1) << y;
          _vguides.insert(guide);
        }

        row_vguides.push_back(guide);
      }

      // Create cells directly as children of this LayoutGroup
      for (int x = 0; x < w; x++) {
        auto name = _name + FormatString("-ch-%d", layout_items.size());
        auto chitem = this->makeChild<T>(std::forward<A>(args)...);
        layout_items.push_back(chitem);

        chitem._layout->setMargin(_margin);
        chitem._layout->top()->anchorTo(hguides[y]);
        chitem._layout->bottom()->anchorTo(hguides[y + 1]);
        chitem._layout->left()->anchorTo(row_vguides[x]);
        chitem._layout->right()->anchorTo(row_vguides[x + 1]);
      }
    }

    _layout->updateAll();

    return layout_items;
  }
  //////////////////////////////////////
  template <typename T, typename... A> //
  std::vector<LayoutItem<T>> makeGridOfWidgets(gridparams_ptr_t params, A&&... args) {
    std::vector<LayoutItem<T>> layout_items;
    int w = params->_cols;
    int h = params->_rows;
    // Create all vertical guides first (shared across all rows)
    std::vector<ui::anchor::guide_ptr_t> vguides;
    size_t num_hprops = params->_h_proportions.size();
    size_t num_vprops = params->_v_proportions.size();
    //printf("makeGridOfWidgets w<%d> h<%d> num_hprops<%zu> num_vprops<%zu>\n", w, h, num_hprops, num_vprops);
    OrkAssert((num_hprops == 0) or (num_hprops == (size_t(w)-1)));
    OrkAssert((num_vprops == 0) or (num_vprops == (size_t(h)-1)));
    for (int x = 0; x <= w; x++) {
      float fx = float(x) / float(w);
      if(num_hprops){
        if( x==0 ) {
          fx = 0.0f;
        }
        else if( x<w ) {
          fx = params->_h_proportions[x-1];
        }
        else if ( x==w ) {
          fx = 1.0f;
        }
      }
      //printf("  vguide x<%d> fx<%f>\n", x, fx);
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
      if(num_vprops){
        if( y==0 )
          fy = 0.0f;
        else if( y<h )
          fy = params->_v_proportions[y-1];
        else if ( y==h )
          fy = 1.0f;
      }
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
