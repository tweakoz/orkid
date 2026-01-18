////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/group.h>
#include <ork/lev2/gfx/rtgroup.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// ScrollMode: determines which axes can scroll
////////////////////////////////////////////////////////////////////

enum class ScrollMode {
  Y,   // vertical scrolling only (child width = container width)
  X,   // horizontal scrolling only (child height = container height)
  XY   // both axes can scroll (child uses its desired size)
};

////////////////////////////////////////////////////////////////////
// ScrollContainer: wraps a single child widget with scrolling
//
// Uses an RTGroup (render target) to render the child content at full size,
// then displays a scrolled portion using UV coordinates. This avoids
// coordinate transformation issues with nested scissors.
//
// The child is sized based on scroll mode:
//   Y mode:  child width = container width, height = child's desiredHeight (or container height if 0)
//   X mode:  child height = container height, width = child's desiredWidth (or container width if 0)
//   XY mode: both dimensions use child's desired size (or container size if 0)
//
// Mouse wheel scrolls the content. Content is clipped to container bounds.
////////////////////////////////////////////////////////////////////

struct ScrollContainer : public Group {
  ScrollContainer(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);
  ~ScrollContainer();

  // Child management
  void setChild(widget_ptr_t child);
  widget_ptr_t getChild() const { return _child; }

  // Scroll mode
  void setScrollMode(ScrollMode mode);
  ScrollMode scrollMode() const { return _mode; }

  // Scroll position
  int scrollOffsetX() const { return _scroll_offset_x; }
  int scrollOffsetY() const { return _scroll_offset_y; }
  void setScrollOffsetX(int offset);
  void setScrollOffsetY(int offset);
  void setScrollOffset(int x, int y);

  // Scroll to make a position visible
  void scrollToTop();
  void scrollToBottom();
  void scrollToLeft();
  void scrollToRight();

  // Configuration
  int _scroll_speed = 5;  // pixels per mouse wheel tick
  fvec4 _bg_color = fvec4(0.1f, 0.1f, 0.1f, 1.0f);
  bool _draw_background = true;
  bool _draw_scroll_indicator = true;  // macOS-style scroll indicator
  fvec4 _scroll_indicator_color = fvec4(1.0f, 1.0f, 1.0f, 0.5f);
  int _scroll_indicator_width = 6;  // pixels
  int _scroll_indicator_margin = 2;  // margin from edge
  float _scroll_indicator_fade_delay = 1.0f;  // seconds before fade starts
  float _scroll_indicator_fade_duration = 0.3f;  // fade duration in seconds

  // Content size (computed from child)
  int contentWidth() const;
  int contentHeight() const;

  // Max scroll values
  int maxScrollX() const;
  int maxScrollY() const;

  // Scroll adjustment for RootToLocal/LocalToRoot coordinate transformations
  int scrollAdjustX() const override { return _scroll_offset_x; }
  int scrollAdjustY() const override { return maxScrollY() - _scroll_offset_y; }

  // Mark content as needing repaint
  void markContentDirty() { _content_dirty = true; }

protected:
  void _doGpuInit(lev2::Context* ctx) override;
  void DoDraw(drawevent_constptr_t drwev) override;
  void DoLayout() override;
  void _doOnResized() override;
  Widget* doRouteUiEvent(event_constptr_t ev) override;
  HandlerResult DoOnUiEvent(event_constptr_t ev) override;

private:
  void _clampScrollOffset();
  void _layoutChild();
  void _checkChildSizeChanged();
  void _renderContentToRTG(drawevent_constptr_t drwev);
  void _drawScrollIndicator(drawevent_constptr_t drwev);

  widget_ptr_t _child;
  ScrollMode _mode = ScrollMode::Y;
  int _scroll_offset_x = 0;
  int _scroll_offset_y = 0;

  // Cached child desired size to detect changes
  int _cached_child_desired_w = 0;
  int _cached_child_desired_h = 0;

  // RTGroup for rendering child content
  lev2::rtgroup_ptr_t _rtgroup;
  bool _content_dirty = true;
  int _rtg_content_w = 0;
  int _rtg_content_h = 0;
  int _rtg_root_x = 0;  // Cached root offset for UV calculations
  int _rtg_root_y = 0;

  // Scroll indicator fade timing
  float _last_scroll_time = -10.0f;  // time of last scroll event (starts hidden)
};

using scroll_container_ptr_t = std::shared_ptr<ScrollContainer>;

} // namespace ork::ui
