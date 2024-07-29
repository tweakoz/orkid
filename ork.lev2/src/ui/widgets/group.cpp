#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/profiling.inl>

namespace ork { namespace ui {
/////////////////////////////////////////////////////////////////////////
Group::Group(const std::string& name, int x, int y, int w, int h)
    : Widget(name, x, y, w, h) {
}
/////////////////////////////////////////////////////////////////////////
Group::~Group() {
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
LayoutGroup::LayoutGroup(const std::string& name, int x, int y, int w, int h)
    : Group(name, x, y, w, h) {
  _layout = std::make_shared<anchor::Layout>(this);
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
  _clear = true;
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
  if (_layout)
    _layout->updateAll();
  //
}
/////////////////////////////////////////////////////////////////////////
void LayoutGroup::DoDraw(drawevent_constptr_t drwev) {
  if (_clear) {
    auto context = drwev->GetTarget();
    auto FBI     = context->FBI();
    lev2::ViewportRect vrect;
    vrect._x = x();
    vrect._y = y();
    vrect._w = width();
    vrect._h = height();
    FBI->pushScissor(vrect);
    FBI->pushViewport(vrect);
    FBI->Clear(_clearColor, 1);
    FBI->popViewport();
    FBI->popScissor();
    //_clear = false;
  }
  drawChildren(drwev);
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
  _layout->removeChild(rep->_layout);
  Group::removeChild(ch->_widget);
  Group::addChild(rep->_widget);
  ch->_widget  = rep->_widget.get();
  rep->_layout = ch;
}
//////////////////////////////////////
void LayoutGroup::setClearColor(fvec4 clr) {
  _clearColor = clr;
}
//////////////////////////////////////
fvec4 LayoutGroup::clearColor() const {
  return _clearColor;
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
  std::pair<guide_ptr_t, guide_ptr_t> findGuidePairUnderMouse(const Layout* rootLayout, const fvec2& mousePos);
  void dragGuidePairH(const std::pair<guide_ptr_t, guide_ptr_t>& pair, float deltaY);
  void dragGuidePairV(const std::pair<guide_ptr_t, guide_ptr_t>& pair, float deltaX);
};
///////////////////////////////////////////////////////////
HandlerResult LayoutGroup::OnUiEvent(event_constptr_t ev) {
  // ev->mFilteredEvent.Reset();
  //printf("LayoutGroup<%p>::OnUiEvent _evhandlerset<%d>\n", this, int(_evhandler != nullptr));
  ui::HandlerResult result;
  bool was_handled = false;
  static std::pair<anchor::guide_ptr_t, anchor::guide_ptr_t> GUIDES_UNDER_MOUSE;
  static int lastx = ev->miX;
  static int lasty = ev->miY;
  switch (ev->_eventcode) {
    case ui::EventCode::PUSH: {
      was_handled = true;
      break;
    }
    case ui::EventCode::RELEASE: {
      _clearColor = fvec4(0, 0, 0, 1);
      was_handled = true;
      break;
    }
    case ui::EventCode::BEGIN_DRAG: {
      was_handled       = true;
      result.mHoldFocus = true;
      GUIDES_UNDER_MOUSE = anchor::findGuidePairUnderMouse(_layout.get(), fvec2(ev->miX, ev->miY));
      lastx = ev->miX;
      lasty = ev->miY;
      break;
    }
    case ui::EventCode::END_DRAG: {
      was_handled       = true;
      result.mHoldFocus = false;
      GUIDES_UNDER_MOUSE = std::pair<anchor::guide_ptr_t, anchor::guide_ptr_t>(nullptr,nullptr);
      break;
    }
    case ui::EventCode::MOVE: {
      _clearColor = fvec4(0.1,0.1,0.2, 1);
      break;
    }
    case ui::EventCode::DRAG: {
      result.mHoldFocus = true;
      was_handled       = true;
      auto g1 = GUIDES_UNDER_MOUSE.first;
      auto g2 = GUIDES_UNDER_MOUSE.second;
      int dx           = ev->miX - lastx;
      int dy           = ev->miY - lasty;
      if(g1 and g2){
        if((not g1->_locked) and (not g2->_locked)){
          _clearColor = fvec4(0.2,0.2,0.3, 1);
          if(g1->isVertical() && g2->isVertical()){
            dragGuidePairV(GUIDES_UNDER_MOUSE, dx);
            _layout->updateAll();
          }
          else if(g1->isHorizontal() && g2->isHorizontal()){
            dragGuidePairH(GUIDES_UNDER_MOUSE, dy);
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
    default: {
      break;
    }
  }
  if (was_handled)
    result.setHandled(this);
  return result;
}
/////////////////////////////////////////////////////////////////////////
Widget* LayoutGroup::doRouteUiEvent(event_constptr_t ev) {
  //
  for (auto& child : _children) {
    bool inside = child->IsEventInside(ev);
    if (inside) {
      auto child_target = child->routeUiEvent(ev);
      if (child_target and not child_target->_ignoreEvents) {
        _clearColor = fvec4(0,0,0, 1.0);
        return child_target;
      }
    }
  }
  //
  if (IsEventInside(ev)){
    _clearColor = fvec4(0.15,0.15,0.25, 1.0);
  }
  return this;
}
/////////////////////////////////////////////////////////////////////////
}} // namespace ork::ui
