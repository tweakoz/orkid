////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orktypes.h>
#include <ork/lev2/lev2_types.h>
#include <ork/lev2/ui/enum.h>
#include <memory>

///////////////////////////////////////////////////////////////////////////////

namespace ork {
class HotKey;

///////////////////////////////////////////////////////////////////////////////

namespace ui {

struct Context;
struct Widget;
struct Group;
struct Surface;
struct Panel;
struct SplitPanel;
struct Viewport;
struct Coordinate;
struct Event;
struct DrawEvent;
struct MultiTouchPoint;
struct IWidgetEventFilter;
struct LayoutGroup;

using context_ptr_t     = std::shared_ptr<Context>;
using widget_ptr_t      = std::shared_ptr<Widget>;
using widget_weakptr_t  = std::weak_ptr<Widget>;
using group_ptr_t       = std::shared_ptr<Group>;
using layoutgroup_ptr_t = std::shared_ptr<LayoutGroup>;
using surface_ptr_t     = std::shared_ptr<Surface>;
using splitpanel_ptr_t  = std::shared_ptr<SplitPanel>;
using panel_ptr_t       = std::shared_ptr<Panel>;
using viewport_ptr_t    = std::shared_ptr<Viewport>;
using eventfilter_ptr_t = std::shared_ptr<IWidgetEventFilter>;
using event_ptr_t       = std::shared_ptr<Event>;
using event_constptr_t  = std::shared_ptr<const Event>;

////////////////////////////////////////////////////////////////////////////////

struct Rect {
  Rect();
  Rect(int x, int y, int w, int h);
  SRect asSRect() const;
  void reset();
  bool isPointInside(int x, int y) const;
  int x2() const;
  int y2() const;
  int center_x() const;
  int center_y() const;
  void moveCenter(int x, int y);
  void moveTop(int y);
  void moveLeft(int x);
  void moveBottom(int y);
  void moveRight(int x);
  void setTop(int y);
  void setLeft(int x);
  void setBottom(int y);
  void setRight(int x);
  int _x, _y, _w, _h;
};

namespace anchor {
struct Layout;
struct Guide;
using guide_ptr_t       = std::shared_ptr<Guide>;
using guide_constptr_t  = std::shared_ptr<const Guide>;
using layout_ptr_t      = std::shared_ptr<Layout>;
using layout_constptr_t = std::shared_ptr<const Layout>;
} // namespace anchor

///////////////////////////////////////////////////////////////////////////////

} // namespace ui
} // namespace ork

///////////////////////////////////////////////////////////////////////////////
