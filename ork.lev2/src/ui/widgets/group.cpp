#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/profiling.inl>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/pri.h>

namespace ork { namespace ui {
static anchor::guide_ptr_t GUIDES_UNDER_MOUSE;
/////////////////////////////////////////////////////////////////////////
Group::Group(const std::string& name, int x, int y, int w, int h)
    : Widget(name, x, y, w, h) {
}
/////////////////////////////////////////////////////////////////////////
Group::~Group() {
}
/////////////////////////////////////////////////////////////////////////
size_t Group::numChildren() const {
  return _children.size();
}
/////////////////////////////////////////////////////////////////////////
void Group::visitHeirarchy(visit_fn_t vfn) {
  std::stack<Widget*> stk;
  stk.push(this);
  while (not stk.empty()) {
    auto w = stk.top();
    vfn(w);
    stk.pop();
    auto as_group = dynamic_cast<Group*>(this);
    if (as_group) {
      for (auto it : as_group->_children) {
        stk.push(it.get());
      }
    }
  }
}
/////////////////////////////////////////////////////////////////////////
void Group::dumpTopology(int depth) {
  std::string indent(depth * 2, ' ');
  printf("%s Group <%s>\n", indent.c_str(), _name.c_str());
  for (auto child : _children) {
    if (auto as_gr = dynamic_pointer_cast<Group>(child)) {
      as_gr->dumpTopology(depth + 1);
    } else {
      printf("%s%s Widget <%s>\n", indent.c_str(), indent.c_str(), child->_name.c_str());
    }
  }
}
/////////////////////////////////////////////////////////////////////////
void Group::addChild(widget_ptr_t w) {
  if (w->parent()) {
    w->parent()->removeChild(w);
  }
  _children.push_back(w);
  w->setParent(this);
  DoLayout();
}
/////////////////////////////////////////////////////////////////////////
void Group::removeChild(widget_ptr_t w) {
  _children.erase(
      std::remove_if(
          _children.begin(),
          _children.end(),
          [=](widget_ptr_t test) -> bool {
            // Do "some stuff", then return true if element should be removed.
            return test == w;
          }),
      _children.end());

  DoLayout();
}
/////////////////////////////////////////////////////////////////////////
void Group::removeChild(Widget* w) {
  _children.erase(
      std::remove_if(
          _children.begin(),
          _children.end(),
          [=](widget_ptr_t test) -> bool {
            // Do "some stuff", then return true if element should be removed.
            return test.get() == w;
          }),
      _children.end());

  DoLayout();
}
/////////////////////////////////////////////////////////////////////////
void Group::drawChildren(ui::drawevent_constptr_t drwev) {
  for (auto child : _children) {
    child->draw(drwev);
  }
}
/////////////////////////////////////////////////////////////////////////
void Group::_doOnResized() {
  if (0) {
    const auto& g = _geometry;
    printf("Group<%s>::OnResize x<%d> y<%d> w<%d> h<%d>\n", _name.c_str(), g._x, g._y, g._w, g._h);
  }
  for (auto& it : _children) {
    if (it->mSizeDirty)
      it->_doOnResized();
  }
}
/////////////////////////////////////////////////////////////////////////
void Group::DoLayout() {
  if (0) {
    const auto& g = _geometry;
    printf("Group<%s>::DoLayout x<%d> y<%d> w<%d> h<%d>\n", _name.c_str(), g._x, g._y, g._w, g._h);
  }
  for (auto& it : _children) {
    it->ReLayout();
  }
}
/////////////////////////////////////////////////////////////////////////
Widget* Group::doRouteUiEvent(event_constptr_t ev) {
  if (0) {
    printf("Group<%s>::doRouteUiEvent\n", _name.c_str());
  }
  //
  for (auto& child : _children) {
    bool inside = child->IsEventInside(ev);
    if (0) {
      printf("Group<%s>::doRouteUiEvent ch<%s> inside<%d>\n", _name.c_str(), child->_name.c_str(), int(inside));
    }
    if (inside) {
      auto child_target = child->routeUiEvent(ev);
      if (child_target and not child_target->_ignoreEvents) {
        if (0) {
          printf("  child_target<%s>\n", child_target->_name.c_str());
        }
        return child_target;
      }
    }
  }
  //
  if (IsEventInside(ev))
    return this;
  //
  return nullptr;
}
/////////////////////////////////////////////////////////////////////////
void Group::_doOnPreDestroy() {
  for (auto c : _children) {
    c->onPreDestroy();
  }
}
/////////////////////////////////////////////////////////////////////////
LayoutGroup::LayoutGroup(const std::string& name, int x, int y, int w, int h, int margin)
    : Group(name, x, y, w, h)
    , _margin(margin) {
  _clearColorStd = fvec4(0.0, 0.0, 0.0, 1);
  _clearColorGuide = fvec4(0.3, 0.0, 0.3, 1);
  _layout = std::make_shared<anchor::Layout>(this);
  _layout->_margin = _margin;  // Set layout's margin from constructor param
  _evrouter = [this](ui::event_constptr_t ev) -> ui::Widget* { //
    return doRouteUiEvent(ev);
  };
  _evhandler = [this](ui::event_constptr_t ev) -> ui::HandlerResult { //
    EASY_BLOCK("LayoutGroup::evh1", profiler::colors::Red);
    ui::HandlerResult result;
    bool was_handled = false;
    switch (ev->_eventcode) {
      case ui::EventCode::PUSH: 
      case ui::EventCode::RELEASE: 
      case ui::EventCode::BEGIN_DRAG: 
      case ui::EventCode::END_DRAG: 
      case ui::EventCode::DRAG: {
        result = LayoutGroup::OnUiEvent(ev);
        was_handled = (result.mHandler!=nullptr);
        break;
      }
      default: {
        break;
      }
    }
    if(was_handled)
      result.setHandled(this);
    return result;
  };
}
/////////////////////////////////////////////////////////////////////////
LayoutGroup::~LayoutGroup() {
}
/////////////////////////////////////////////////////////////////////////
void LayoutGroup::_doOnResized() {
}
/////////////////////////////////////////////////////////////////////////
void LayoutGroup::DoLayout() {
  // in this case, the layout is responsible
  // for laying out all children, recursively..
  // note that the layout will use the geometry of this group
  //  to compute the layout of all children
  // So it is expected that you set the size of this group
  //  either manually or driven indirectly through the resize
  //  of a parent..
  const auto& g = _geometry;
  if (0)
    printf(
        "LayoutGroup<%s>::DoLayout l<%p> x<%d> y<%d> w<%d> h<%d>\n", //
        _name.c_str(),
        (void*)_layout.get(),
        g._x,
        g._y,
        g._w,
        g._h);
  if (_layout){
    _layout->updateAll();
  }
  //
}
/////////////////////////////////////////////////////////////////////////
void LayoutGroup::DoDraw(drawevent_constptr_t drwev) {
  int x = _geometry._x;
  int y = _geometry._y;
  int w = _geometry._w;
  int h = _geometry._h;
  //printf("LayoutGroup<%s>::DoDraw xywh<%d %d %d %d> clear<%d>\n", _name.c_str(), x, y, w, h, int(_clear));
  if(_clear){
    Widget::_drawColoredBox(drwev,  _clearColorStd);
  }
  drawChildren(drwev);

  // Draw highlighted guide if one is under the mouse
  if (GUIDES_UNDER_MOUSE) {
    auto tgt    = drwev->GetTarget();
    auto fbi    = tgt->FBI();
    auto mtxi   = tgt->MTXI();
    auto primi  = tgt->PRI();
    auto defmtl = lev2::defaultUIMaterial();

    mtxi->PushUIMatrix();
    {
      // Get the guide's line in geometry space
      auto line = GUIDES_UNDER_MOUSE->line(anchor::Mode::Geometry);

      // Transform to root space (same as in guide detection)
      Widget* widget = GUIDES_UNDER_MOUSE->_layout->_widget;
      Widget* current = widget->parent();
      while (current && current->parent()) {
        auto geo = current->geometry();
        line._from.x += geo._x;
        line._from.y += geo._y;
        line._to.x += geo._x;
        line._to.y += geo._y;
        current = current->parent();
      }

      // Calculate the box around the guide based on its margin
      int margin = GUIDES_UNDER_MOUSE->_margin;
      int ix1, iy1, ix2, iy2;

      if (GUIDES_UNDER_MOUSE->isVertical()) {
        // Vertical guide - draw a vertical box
        ix1 = line._from.x - margin;
        ix2 = line._from.x + margin;
        iy1 = line._from.y;
        iy2 = line._to.y;
      } else {
        // Horizontal guide - draw a horizontal box
        ix1 = line._from.x;
        ix2 = line._to.x;
        iy1 = line._from.y - margin;
        iy2 = line._from.y + margin;
      }

      defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
      defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);
      tgt->PushModColor(_clearColorGuide);
      defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
      primi->RenderQuadAtZ(
          defmtl.get(),
          ix1, ix2,  // x0, x1
          iy1, iy2,  // y0, y1
          0.0f,      // z
          0.0f, 1.0f, // u0, u1
          0.0f, 1.0f  // v0, v1
      );
      tgt->PopModColor();
    }
    mtxi->PopUIMatrix();
  }
}
//////////////////////////////////////
anchor::layout_ptr_t LayoutGroup::layoutAndAddChild(widget_ptr_t w) {
  auto layout = _layout->childLayout(w.get());
  addChild(w);
  return layout;
}
//////////////////////////////////////
void LayoutGroup::removeChild(anchor::layout_ptr_t ch) {
  _layout->removeChild(ch);
  Group::removeChild(ch->_widget);
}
//////////////////////////////////////
void LayoutGroup::replaceChild(anchor::layout_ptr_t ch, layoutitem_ptr_t rep) {
  // DON'T call _layout->removeChild(rep->_layout) here!
  // That calls prune() which breaks ALL guide associations in the entire tree
  // rep->_layout will be garbage collected when rep goes out of scope

  // Find the actual parent group of the widget being replaced
  auto old_widget = ch->_widget;
  auto actual_parent = dynamic_cast<Group*>(old_widget->parent());

  if (actual_parent) {
    // Remove old widget from its actual parent (row-0 in hierarchical layouts)
    actual_parent->removeChild(old_widget);
    actual_parent->addChild(rep->_widget);
  } else {
    // Fallback: no parent found, operate on this group
    Group::removeChild(ch->_widget);
    Group::addChild(rep->_widget);
  }

  // Update layout-widget connection
  // ch retains all its anchoring, just points to new widget
  ch->_widget = rep->_widget.get();
  rep->_layout = ch;  // rep now uses ch's layout (discarding rep's original layout)
}
//////////////////////////////////////
const std::set<uiguide_ptr_t>& LayoutGroup::horizontalGuides() const {
  return _hguides;
}
//////////////////////////////////////
const std::set<uiguide_ptr_t>& LayoutGroup::verticalGuides() const {
  return _vguides;
}
namespace anchor{
  guide_ptr_t findGuidePairUnderMouse(const Layout* rootLayout, const fvec2& mousePos);
  void dragGuidePairH(guide_ptr_t guide, float deltaY);
  void dragGuidePairV(guide_ptr_t guide, float deltaX);
};
///////////////////////////////////////////////////////////
HandlerResult LayoutGroup::OnUiEvent(event_constptr_t ev) {
  // ev->mFilteredEvent.Reset();
  static int counter = 0;
  int count = counter++;
  //printf("LayoutGroup<%p>::OnUiEvent count<%d>\n", this, count);
  ui::HandlerResult result;
  bool was_handled = false;
  static int lastx = ev->miX;
  static int lasty = ev->miY;
  switch (ev->_eventcode) {
    case ui::EventCode::PUSH: {
      was_handled = true;
      break;
    }
    case ui::EventCode::RELEASE: {
      //_clearColor = fvec4(0, 0, 0, 1);
      was_handled = true;
      break;
    }
    case ui::EventCode::BEGIN_DRAG: {
      was_handled       = true;
      result.mHoldFocus = true;
      lastx = ev->miX;
      lasty = ev->miY;
      break;
    }
    case ui::EventCode::END_DRAG: {
      was_handled       = true;
      result.mHoldFocus = false;
      //GUIDES_UNDER_MOUSE = std::pair<anchor::guide_ptr_t, anchor::guide_ptr_t>(nullptr,nullptr);
      break;
    }
    case ui::EventCode::MOVE: {
      //_clearColor = fvec4(0.1,0.1,0.2, 1);
      break;
    }
    case ui::EventCode::DRAG: {
      result.mHoldFocus = true;
      was_handled       = true;
      auto g1 = GUIDES_UNDER_MOUSE;
      int dx           = ev->miX - lastx;
      int dy           = ev->miY - lasty;
      if(g1){
        if(not g1->_locked){
          //_clearColor = fvec4(0.2,0.2,0.3, 1);
          if(g1->isVertical()){
            dragGuidePairV(g1, dx);
            _layout->updateAll();
          }
          else if(g1->isHorizontal()){
            dragGuidePairH(g1, dy);
            _layout->updateAll();
          }
          else{
            //_clearColor = fvec4(1, 0, 0, 1.0);
            printf("BAD GUIDE PAIR\n");
          }
        }
      }
      lastx = ev->miX;
      lasty = ev->miY;
      break;
    }
    case ui::EventCode::MOUSE_LEAVE: 
    default: {
      _highlightGuides = false;
      break;
    }
  }
  if (was_handled)
    result.setHandled(this);
  return result;
}
/////////////////////////////////////////////////////////////////////////
Widget* LayoutGroup::doRouteUiEvent(event_constptr_t ev) {
  ///////////////////////////
  GUIDES_UNDER_MOUSE = anchor::findGuidePairUnderMouse(_layout.get(), fvec2(ev->miX, ev->miY));
  if(GUIDES_UNDER_MOUSE){
    _highlightGuides = true;
    return this;
  }
  ///////////////////////////
  _highlightGuides = false;
  for (auto& child : _children) {
    bool inside = child->IsEventInside(ev);
    if (inside) {
      auto child_target = child->routeUiEvent(ev);
      if (child_target and not child_target->_ignoreEvents) {
        //_clearColor = fvec4(0,0,0, 1.0);
        return child_target;
      }
    }
  }
  return this;
}
/////////////////////////////////////////////////////////////////////////
}} // namespace ork::ui
