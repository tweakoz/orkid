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
struct LabelBox;
struct TextBox;
struct TabWidget;
struct ImageView;
struct LineEdit;
struct Button;
struct Checkbox;
struct IntSlider;
struct FloatSlider;
struct ComboBox;
struct DrawEvent;
struct MultiTouchPoint;
struct IWidgetEventFilter;
struct LayoutGroup;
struct HandlerResult;
struct VerticalPack;
struct HorizontalPack;
struct HorizontalSplit;
struct VerticalSplit;
struct DynaGrid;
struct GraphView;
struct LoggerGroup;
struct SdfShape;
struct DockablePanel;

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
using labelbox_ptr_t    = std::shared_ptr<LabelBox>;
using textbox_ptr_t     = std::shared_ptr<TextBox>;
using imgview_ptr_t     = std::shared_ptr<ImageView>;
using lineedit_ptr_t    = std::shared_ptr<LineEdit>;
using button_ptr_t      = std::shared_ptr<Button>;
using checkbox_ptr_t    = std::shared_ptr<Checkbox>;
using intslider_ptr_t   = std::shared_ptr<IntSlider>;
using floatslider_ptr_t = std::shared_ptr<FloatSlider>;
using combobox_ptr_t    = std::shared_ptr<ComboBox>;
using vpack_ptr_t       = std::shared_ptr<VerticalPack>;
using hpack_ptr_t       = std::shared_ptr<HorizontalPack>;
using hsplit_ptr_t      = std::shared_ptr<HorizontalSplit>;
using vsplit_ptr_t      = std::shared_ptr<VerticalSplit>;
using dynagrid_ptr_t    = std::shared_ptr<DynaGrid>;
using graphview_ptr_t   = std::shared_ptr<GraphView>;
using loggergroup_ptr_t = std::shared_ptr<LoggerGroup>;
using loggergroup_wkptr_t = std::weak_ptr<LoggerGroup>;
using sdfshape_ptr_t    = std::shared_ptr<SdfShape>;
using dockablepanel_ptr_t = std::shared_ptr<DockablePanel>;
////////////////////////////////////////////////////////////////////////////////

using evrouter_t  = std::function<Widget*(event_constptr_t ev)>;
using evhandler_t = std::function<HandlerResult(event_constptr_t ev)>;

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
