#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/ui/surface.h>
#include <ork/profiling.inl>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/pri.h>

namespace ork { namespace ui {
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
void Group::addChild(widget_ptr_t w, bool relayout) {
  if (w->parent()) {
    w->parent()->removeChild(w);
  }
  _children.push_back(w);
  w->setParent(this);
  _onChildrenChanged();  // Notify subclasses
  if (relayout) {
    DoLayout();
  }
}
/////////////////////////////////////////////////////////////////////////
void Group::removeChild(widget_ptr_t w, bool relayout) {
  _children.erase(
      std::remove_if(
          _children.begin(),
          _children.end(),
          [=](widget_ptr_t test) -> bool {
            // Do "some stuff", then return true if element should be removed.
            return test == w;
          }),
      _children.end());

  _onChildrenChanged();  // Notify subclasses
  if (relayout) {
    DoLayout();
  }
}
/////////////////////////////////////////////////////////////////////////
void Group::removeChild(Widget* w, bool relayout) {
  _children.erase(
      std::remove_if(
          _children.begin(),
          _children.end(),
          [=](widget_ptr_t test) -> bool {
            // Do "some stuff", then return true if element should be removed.
            return test.get() == w;
          }),
      _children.end());

  _onChildrenChanged();  // Notify subclasses
  if (relayout) {
    DoLayout();
  }
}
/////////////////////////////////////////////////////////////////////////
widget_ptr_t Group::findChildPtr(const Widget* w) const {
  for (const auto& child : _children) {
    if (child.get() == w) {
      return child;
    }
  }
  return nullptr;
}
/////////////////////////////////////////////////////////////////////////
void Group::_doOnParentChanged(Group* parent) {
  if (_propagate_on_parent_change && _uicontext) {
    std::function<void(Group*)> propagate = [&](Group* g) {
      for (auto& child : g->_children) {
        child->_uicontext = _uicontext;
        if (auto child_group = dynamic_cast<Group*>(child.get())) {
          propagate(child_group);
        }
      }
    };
    propagate(this);
  }
}
/////////////////////////////////////////////////////////////////////////
void Group::drawChildren(ui::drawevent_constptr_t drwev) {
  for (auto child : _children) {
    if(child->_enable){
      child->draw(drwev);
    }
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
void Group::setMargin(int margin) {
  _margin = margin;
  DoLayout();
}
int Group::margin() const {
  return _margin;
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
    : Group(name, x, y, w, h) {
  _margin = margin;  // Set margin (inherited from Group)
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
      case ui::EventCode::MOUSE_ENTER: 
      case ui::EventCode::MOUSE_LEAVE: 
      case ui::EventCode::MOVE: 
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
  _animtimer.Start();

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
  // Position overlay if present
  if (_overlay_widget) {
    _positionOverlay();
  }
  //
}
//////////////////////////////////////
void LayoutGroup::_positionOverlay() {
  if (!_overlay_widget)
    return;

  // Get this LayoutGroup's geometry
  int x = _geometry._x;
  int y = _geometry._y;
  int w = _geometry._w;
  int h = _geometry._h;

  // Apply 10% margin on all sides
  float margin_percent = 0.0125f;
  int margin_w = w * margin_percent;
  int margin_h = h * margin_percent;

  int overlay_x = x + margin_w;
  int overlay_y = y + margin_h;
  int overlay_w = w - (2 * margin_w);  // 80% of width
  int overlay_h = h - (2 * margin_h);  // 80% of height

  _overlay_widget->SetRect(overlay_x, overlay_y, overlay_w, overlay_h);
}
/////////////////////////////////////////////////////////////////////////
void LayoutGroup::DoDraw(drawevent_constptr_t drwev) {
  auto tgt    = drwev->GetTarget();
  auto fbi    = tgt->FBI();
  auto mtxi   = tgt->MTXI();
  auto primi  = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();

  int x = _geometry._x;
  int y = _geometry._y;
  int w = _geometry._w;
  int h = _geometry._h;

  //printf("LayoutGroup<%s>::DoDraw xywh<%d %d %d %d> clear<%d>\n", _name.c_str(), x, y, w, h, int(_clear));
  if(_clear){
    Widget::_drawColoredBox(drwev,  _clearColorStd);
  }
  drawChildren(drwev);

  if(_overlay_widget){
    _overlay_widget->_enable = _overlay_enabled;
  }
  if (_overlay_widget && _overlay_enabled) {
    _overlay_widget->draw(drwev);
  }

  // Draw highlighted guide if one is under the mouse
  // Only render if the guide belongs to this LayoutGroup's hierarchy

  auto hlguide = _guide_highlite;
  if(hlguide==nullptr){
    hlguide = _guide_being_dragged;
  }
  if (hlguide) {
    // Check if the guide's widget is a descendant of this LayoutGroup
    Widget* guide_widget = hlguide->_layout->_widget;
    bool is_descendant = false;
    Widget* check = guide_widget;
    while (check) {
      if (check == this) {
        is_descendant = true;
        break;
      }
      check = check->parent();
    }

    if (is_descendant) {
      // Get the guide's line in geometry space
      auto line = hlguide->line(anchor::Mode::Geometry);

      // Transform to root space (same as in guide detection)
      // Stop at Surface boundaries since they render to their own coordinate system
      Widget* widget = hlguide->_layout->_widget;
      Widget* current = widget->parent();
      while (current && current->parent()) {
        // Stop if we hit a Surface - it's the root of its own coordinate system
        if (dynamic_cast<Surface*>(current)) {
          break;
        }
        auto geo = current->geometry();
        line._from.x += geo._x;
        line._from.y += geo._y;
        line._to.x += geo._x;
        line._to.y += geo._y;
        current = current->parent();
      }

      // Calculate the box around the guide based on its margin
      int margin = hlguide->_margin;
      int ix1, iy1, ix2, iy2;

      if (hlguide->isVertical()) {
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

      // Use viewport-based UI matrix since coordinates are in root/window space
      mtxi->PushUIMatrix();
      {
        defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
        defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);

        // Modulate color when actively dragging for visual feedback

        float modcolor = 1.0;
        if(hlguide == _guide_being_dragged){
          modcolor = 0.8+sinf(_animtimer.SecsSinceStart()*PI2*3.0)*0.2;
        }
        auto guide_color = hlguide //
                         ? (_clearColorGuide*modcolor) //
                         : _clearColorGuide;

        tgt->PushModColor(guide_color);
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
    } // end if (is_descendant)
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
layoutitem_ptr_t LayoutGroup::split(anchor::layout_ptr_t target_layout,
                                    float proportion,
                                    anchor::ELayoutSplitPlacement placement,
                                    int margin) {

  // Determine if this is a horizontal or vertical split based on placement
  bool is_horizontal_split = (placement == anchor::ELayoutSplitPlacement::TOP ||
                               placement == anchor::ELayoutSplitPlacement::BOTTOM);

  /////////////////////
  // PLAN:
  //. capture the EXACT guides that target_layout is currently anchored to (top/bottom/left/right)
  //. create a new LayoutGroup "container"
  //. remove target_layout as direct child of its parent layout (this->_layout)
  //. remove target_widget from this LayoutGroup's _children (widget hierarchy)
  //. add the container as child of this LayoutGroup (both widget and layout hierarchy)
  //. anchor container's layout to the EXACT SAME guides that target_layout was using
  //.   (container becomes drop-in replacement for target_layout in parent's guide topology)
  //. add target_widget as child of container (widget hierarchy)
  //. add target_layout as child of container's layout (layout hierarchy)
  //. create a new layout as child of container's layout (for new widget)
  //. create a horizontal guide on container's layout at 'proportion'
  //. anchor target_layout and new_layout to container's edges and split guide according to 'half'
  //.   (both layouts span full width of container, split vertically by the guide)
  //. return the new layoutgroup (container) to the caller
  //.  the caller is responsible for creating the new widget and assigning it to the new layout/layoutgroup
  /////////////////////

  //printf("=== DUMP at START of splitVertical ===\n");
  //dumpLayoutHierarchy();

  auto parent_layout = target_layout->_parent;
  OrkAssert(parent_layout != nullptr);

  // Inherit margin from top layout group if not specified
  if (margin == -1) {
    margin = _margin;
    //printf("split() inheriting margin from LayoutGroup: %d\n", margin);
  } else {
    //printf("split() using explicit margin: %d\n", margin);
  }

  // Get the guides that target is currently anchored to
  auto target_top_guide = target_layout->_top->_relative;
  auto target_bottom_guide = target_layout->_bottom->_relative;
  auto target_left_guide = target_layout->_left->_relative;
  auto target_right_guide = target_layout->_right->_relative;

  // Create a container LayoutGroup that spans the target's current bounds
  // This container will own the horizontal split guide, limiting its span (T-junction)
  auto container_name = _name + "-split-container";
  auto container = std::make_shared<LayoutGroup>(container_name, 0, 0, 0, 0, margin);

  container->_clear = false;  // Don't draw background (like makeWidgetsRC row containers)
  container->_ignoreEvents = true;  // Don't intercept events - let root LayoutGroup handle guide dragging
  container->_layout->setMargin(0);  // Container layout has no margin, guides have the margin

  // Find the actual LayoutGroup that owns the target widget
  // This might not be 'this' if target was already split before
  auto parent_widget = parent_layout->_widget;
  OrkAssert(parent_widget != nullptr);
  auto parent_group = dynamic_cast<LayoutGroup*>(parent_widget);
  OrkAssert(parent_group != nullptr);

  // Find the target widget in the parent group's _children and get its shared_ptr
  widget_ptr_t target_widget_ptr;
  for (auto& child : parent_group->_children) {
    if (child.get() == target_layout->_widget) {
      target_widget_ptr = child;
      break;
    }
  }
  OrkAssert(target_widget_ptr != nullptr);

  // Remove target layout from parent's child layouts
  auto& parent_children = parent_layout->_childlayouts;
  parent_children.erase(
    std::remove(parent_children.begin(), parent_children.end(), target_layout),
    parent_children.end()
  );

  // Remove target widget from parent group (but keep the shared_ptr alive, no relayout)
  parent_group->Group::removeChild(target_widget_ptr, false);

  // Add container to parent group (without triggering DoLayout yet - state is incomplete)
  parent_group->addChild(container, false);

  // Add container's layout to parent layout's children (layout hierarchy)
  auto container_layout = container->_layout;
  parent_layout->_childlayouts.push_back(container_layout);
  container_layout->_parent = parent_layout;

  // Anchor container to the target's original guides (spans same area as target did)
  container_layout->top()->anchorTo(target_top_guide);
  container_layout->bottom()->anchorTo(target_bottom_guide);
  container_layout->left()->anchorTo(target_left_guide);
  container_layout->right()->anchorTo(target_right_guide);

  // Add target widget to container's children (directly, without DoLayout)
  // and re-parent target_layout to container_layout
  container->_children.push_back(target_widget_ptr);
  target_widget_ptr->setParent(container.get());
  target_layout->_parent = container_layout.get();
  container_layout->_childlayouts.push_back(target_layout);

  // Create a new layout for the new widget (widget will be assigned by binding layer)
  auto new_layout = container_layout->childLayout(nullptr);  // widget is nullptr for now

  // Create the split guide on the container's layout (horizontal or vertical depending on placement)
  // This guide will span across the container creating a T-junction
  anchor::guide_ptr_t split_guide;
  if (is_horizontal_split) {
    split_guide = container_layout->proportionalHorizontalGuide(proportion);
    container->_hguides.insert(split_guide);
    parent_group->_hguides.insert(split_guide);
  } else {
    split_guide = container_layout->proportionalVerticalGuide(proportion);
    container->_vguides.insert(split_guide);
    parent_group->_vguides.insert(split_guide);
  }
  split_guide->_locked = false;  // Explicitly unlock for dragging
  split_guide->_margin = margin;

  // Set margins
  target_layout->setMargin(margin);
  new_layout->setMargin(margin);

  // Anchor both layouts within the container based on placement
  switch (placement) {
    case anchor::ELayoutSplitPlacement::BOTTOM:
      // New layout goes in bottom half (horizontal split)
      new_layout->top()->anchorTo(split_guide);
      new_layout->bottom()->anchorTo(container_layout->bottom());
      new_layout->left()->anchorTo(container_layout->left());
      new_layout->right()->anchorTo(container_layout->right());
      // Target layout goes in top half
      target_layout->top()->anchorTo(container_layout->top());
      target_layout->bottom()->anchorTo(split_guide);
      target_layout->left()->anchorTo(container_layout->left());
      target_layout->right()->anchorTo(container_layout->right());
      break;

    case anchor::ELayoutSplitPlacement::TOP:
      // New layout goes in top half (horizontal split)
      new_layout->top()->anchorTo(container_layout->top());
      new_layout->bottom()->anchorTo(split_guide);
      new_layout->left()->anchorTo(container_layout->left());
      new_layout->right()->anchorTo(container_layout->right());
      // Target layout goes in bottom half
      target_layout->top()->anchorTo(split_guide);
      target_layout->bottom()->anchorTo(container_layout->bottom());
      target_layout->left()->anchorTo(container_layout->left());
      target_layout->right()->anchorTo(container_layout->right());
      break;

    case anchor::ELayoutSplitPlacement::RIGHT:
      // New layout goes in right half (vertical split)
      new_layout->left()->anchorTo(split_guide);
      new_layout->right()->anchorTo(container_layout->right());
      new_layout->top()->anchorTo(container_layout->top());
      new_layout->bottom()->anchorTo(container_layout->bottom());
      // Target layout goes in left half
      target_layout->left()->anchorTo(container_layout->left());
      target_layout->right()->anchorTo(split_guide);
      target_layout->top()->anchorTo(container_layout->top());
      target_layout->bottom()->anchorTo(container_layout->bottom());
      break;

    case anchor::ELayoutSplitPlacement::LEFT:
      // New layout goes in left half (vertical split)
      new_layout->left()->anchorTo(container_layout->left());
      new_layout->right()->anchorTo(split_guide);
      new_layout->top()->anchorTo(container_layout->top());
      new_layout->bottom()->anchorTo(container_layout->bottom());
      // Target layout goes in right half
      target_layout->left()->anchorTo(split_guide);
      target_layout->right()->anchorTo(container_layout->right());
      target_layout->top()->anchorTo(container_layout->top());
      target_layout->bottom()->anchorTo(container_layout->bottom());
      break;
  }

  // Don't update layouts yet - the widget hasn't been assigned
  // The binding layer will assign the widget and then update

  //printf("=== DUMP at end of splitVertical ===\n");
  //dumpLayoutHierarchy();

  // Create and return a LayoutItem containing the container and new_layout
  auto result = std::make_shared<ui::LayoutItemBase>();
  result->_widget = container;
  result->_layout = new_layout;
  return result;
}
//////////////////////////////////////
const std::set<uiguide_ptr_t>& LayoutGroup::horizontalGuides() const {
  return _hguides;
}
//////////////////////////////////////
const std::set<uiguide_ptr_t>& LayoutGroup::verticalGuides() const {
  return _vguides;
}
//////////////////////////////////////

anchor::guide_ptr_t LayoutGroup::findGuideBetween(anchor::layout_ptr_t layout_a, anchor::layout_ptr_t layout_b) {
  anchor::guide_ptr_t found_guide = nullptr;

  // First check if layouts share an edge guide directly
  if (layout_a->_right && layout_b->_left && layout_a->_right.get() == layout_b->_left.get()) {
    return layout_a->_right;  // Side by side (a on left, b on right)
  }
  if (layout_a->_left && layout_b->_right && layout_a->_left.get() == layout_b->_right.get()) {
    return layout_a->_left;  // Side by side (a on right, b on left)
  }
  if (layout_a->_bottom && layout_b->_top && layout_a->_bottom.get() == layout_b->_top.get()) {
    return layout_a->_bottom;  // Stacked (a on top, b on bottom)
  }
  if (layout_a->_top && layout_b->_bottom && layout_a->_top.get() == layout_b->_bottom.get()) {
    return layout_a->_top;  // Stacked (a on bottom, b on top)
  }

  // Search horizontal guides (looking for dividers only, not shared edges)
  for (auto& guide : _hguides) {
    anchor::Guide* layout_a_guide = nullptr;
    anchor::Guide* layout_b_guide = nullptr;

    for (auto* assoc : guide->_associates) {
      if (assoc->_layout == layout_a.get()) layout_a_guide = assoc;
      if (assoc->_layout == layout_b.get()) layout_b_guide = assoc;
    }

    if (layout_a_guide && layout_b_guide) {
      // Check if this is a divider (opposite sides) or shared edge (same side)
      bool is_divider = false;
      if ((layout_a_guide->_edge == anchor::Edge::Top && layout_b_guide->_edge == anchor::Edge::Bottom) ||
          (layout_a_guide->_edge == anchor::Edge::Bottom && layout_b_guide->_edge == anchor::Edge::Top)) {
        is_divider = true;
      }

      if (is_divider) {
        if (found_guide != nullptr) {
          return nullptr;  // Ambiguous - multiple guides
        }
        found_guide = guide;
      }
    }
  }

  // Search vertical guides (looking for dividers only, not shared edges)
  for (auto& guide : _vguides) {
    anchor::Guide* layout_a_guide = nullptr;
    anchor::Guide* layout_b_guide = nullptr;

    for (auto* assoc : guide->_associates) {
      if (assoc->_layout == layout_a.get()) {
        layout_a_guide = assoc;
      }
      if (assoc->_layout == layout_b.get()) {
        layout_b_guide = assoc;
      }
    }

    if (layout_a_guide && layout_b_guide) {
      // Check if this is a divider (opposite sides) or shared edge (same side)
      bool is_divider = false;
      if ((layout_a_guide->_edge == anchor::Edge::Left && layout_b_guide->_edge == anchor::Edge::Right) ||
          (layout_a_guide->_edge == anchor::Edge::Right && layout_b_guide->_edge == anchor::Edge::Left)) {
        is_divider = true;
      }

      if (is_divider) {
        if (found_guide != nullptr) {
          return nullptr;  // Ambiguous - multiple guides
        }
        found_guide = guide;
      }
    }
  }

  return found_guide;
}
//////////////////////////////////////
//////////////////////////////////////
void LayoutGroup::dumpLayoutHierarchy() {
  printf("\n");
  printf("================================================================================\n");
  printf("  LAYOUT HIERARCHY DUMP\n");
  printf("  LayoutGroup: %s <%p>\n", _name.c_str(), (void*)this);
  printf("================================================================================\n\n");

  std::unordered_set<void*> visited_guides;
  std::unordered_map<void*, int> guide_ids;
  int next_guide_id = 1;

  // Helper to get/assign guide ID
  auto get_guide_id = [&](void* ptr) -> int {
    if (guide_ids.find(ptr) == guide_ids.end()) {
      guide_ids[ptr] = next_guide_id++;
    }
    return guide_ids[ptr];
  };

  // Helper to dump a guide
  auto dump_guide = [&](const char* label, anchor::guide_ptr_t guide, int indent, bool show_associates = true) {
    if (!guide) return;

    std::string ind(indent * 2, ' ');
    void* guide_ptr = (void*)guide.get();
    int gid = get_guide_id(guide_ptr);

    const char* type_str = "UNKNOWN";
    if (guide->_type == anchor::GuideType::PROPORTIONAL) type_str = "PROPORTIONAL";
    else if (guide->_type == anchor::GuideType::FIXED) type_str = "FIXED";

    const char* edge_str = "UNKNOWN";
    if (guide->_edge == anchor::Edge::Top) edge_str = "Top";
    else if (guide->_edge == anchor::Edge::Left) edge_str = "Left";
    else if (guide->_edge == anchor::Edge::Bottom) edge_str = "Bottom";
    else if (guide->_edge == anchor::Edge::Right) edge_str = "Right";
    else if (guide->_edge == anchor::Edge::CustomVertical) edge_str = "CustomVert";
    else if (guide->_edge == anchor::Edge::CustomHorizontal) edge_str = "CustomHorz";

    printf("%s%-8s [G%d] <%p>\n", ind.c_str(), label, gid, guide_ptr);
    printf("%s          edge=%-12s type=%-12s prop=%.3f fixed=%d locked=%d margin=%d\n",
           ind.c_str(), edge_str, type_str, guide->_proportion, guide->_fixed, guide->_locked, guide->_margin);

    // Show associates if not already visited
    if (show_associates && visited_guides.find(guide_ptr) == visited_guides.end()) {
      visited_guides.insert(guide_ptr);
      if (!guide->_associates.empty()) {
        printf("%s          associates (%zu):\n", ind.c_str(), guide->_associates.size());
        for (auto* assoc : guide->_associates) {
          int assoc_id = get_guide_id((void*)assoc);
          printf("%s            -> [G%d] <%p> @ Layout <%p>\n",
                 ind.c_str(), assoc_id, (void*)assoc, (void*)assoc->_layout);
        }
      }
    }
    printf("\n");
  };

  // Helper to dump a layout
  std::function<void(anchor::layout_ptr_t, int)> dump_layout;
  dump_layout = [&](anchor::layout_ptr_t layout, int indent) {
    if (!layout) return;

    std::string ind(indent * 2, ' ');
    const char* widget_name = layout->_widget ? layout->_widget->_name.c_str() : "NULL";

    printf("%s┌─ LAYOUT <%p>\n", ind.c_str(), (void*)layout.get());
    printf("%s│  widget: %s <%p>\n", ind.c_str(), widget_name, (void*)layout->_widget);
    printf("%s│  margin: %d  locked: %d\n", ind.c_str(), layout->_margin, layout->_locked);
    printf("%s│\n", ind.c_str());

    // Dump edge guides
    printf("%s│  Edge Guides:\n", ind.c_str());
    dump_guide("top", layout->_top, indent + 1, false);
    dump_guide("left", layout->_left, indent + 1, false);
    dump_guide("bottom", layout->_bottom, indent + 1, false);
    dump_guide("right", layout->_right, indent + 1, false);
    if (layout->_centerH) dump_guide("centerH", layout->_centerH, indent + 1, false);
    if (layout->_centerV) dump_guide("centerV", layout->_centerV, indent + 1, false);

    // Dump custom guides
    if (!layout->_customguides.empty()) {
      printf("%s│  Custom Guides: %zu\n", ind.c_str(), layout->_customguides.size());
      for (auto& guide : layout->_customguides) {
        dump_guide("custom", guide, indent + 1);
      }
    }

    printf("%s└─────────────────────────────────────────\n\n", ind.c_str());

    // Dump child layouts
    for (auto& child : layout->_childlayouts) {
      dump_layout(child, indent + 1);
    }
  };

  // Dump main layout hierarchy
  printf("LAYOUT TREE:\n");
  printf("────────────────────────────────────────────────────────────────────────────────\n\n");
  dump_layout(_layout, 0);

  // Dump horizontal guides collection
  printf("\n");
  printf("HORIZONTAL GUIDES COLLECTION (_hguides): %zu\n", _hguides.size());
  printf("────────────────────────────────────────────────────────────────────────────────\n");
  for (auto& guide : _hguides) {
    dump_guide("", guide, 0);
  }

  // Dump vertical guides collection
  printf("\n");
  printf("VERTICAL GUIDES COLLECTION (_vguides): %zu\n", _vguides.size());
  printf("────────────────────────────────────────────────────────────────────────────────\n");
  for (auto& guide : _vguides) {
    dump_guide("", guide, 0);
  }

  printf("\n");
  printf("================================================================================\n");
  printf("  END LAYOUT HIERARCHY DUMP\n");
  printf("================================================================================\n\n");
  fflush(stdout);
}
//////////////////////////////////////
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
  //printf("LayoutGroup<%s>::OnUiEvent eventcode<%d>\n", _name.c_str(), int(ev->_eventcode));
  ui::HandlerResult result;
  bool was_handled = false;
  static int lastx = ev->miX;
  static int lasty = ev->miY;
  switch (ev->_eventcode) {
    case ui::EventCode::PUSH: {
      was_handled = true;
      _guide_being_dragged = anchor::findGuidePairUnderMouse(_layout.get(), fvec2(ev->miX, ev->miY));
      _guide_highlite = nullptr;
      break;
    }
    case ui::EventCode::RELEASE: {
      //_clearColor = fvec4(0, 0, 0, 1);
      was_handled = true;
      _guide_being_dragged = nullptr;
      _guide_highlite = nullptr;
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
      _guide_being_dragged  = nullptr;
      //_guide_being_dragged = std::pair<anchor::guide_ptr_t, anchor::guide_ptr_t>(nullptr,nullptr);
      break;
    }
    case ui::EventCode::MOVE: {
      //_clearColor = fvec4(0.1,0.1,0.2, 1);
      _guide_highlite = anchor::findGuidePairUnderMouse(_layout.get(), fvec2(ev->miX, ev->miY));
      //printf("LayoutGroup<%s>::OnUiEvent MOVE mx<%d> my<%d> _guide_highlite<%p>\n", _name.c_str(), ev->miX, ev->miY, (void*)_guide_highlite.get());
      break;
    }
    case ui::EventCode::DRAG: {
      result.mHoldFocus = true;
      was_handled       = true;
      auto g1 = _guide_being_dragged;
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
      _guide_being_dragged = nullptr;
      _guide_highlite = nullptr;
      break;
    }
  }
  if (was_handled)
    result.setHandled(this);
  return result;
}
/////////////////////////////////////////////////////////////////////////
Widget* LayoutGroup::doRouteUiEvent(event_constptr_t ev) {
  //printf("LayoutGroup<%s>::doRouteUiEvent _ignoreEvents<%d>\n", _name.c_str(), int(_ignoreEvents));

  // If ignoreEvents, skip LayoutGroup-specific handling but still route to children
  if (_ignoreEvents) {
    // Route to children (same logic as below, but skip all LayoutGroup handling)
    for (size_t i = 0; i < _children.size(); i++) {
      size_t idx = _children.size() - 1 - i;
      auto child = _children[idx];
      if (child->IsEventInside(ev)) {
        auto child_target = child->routeUiEvent(ev);
        if (child_target && !child_target->_ignoreEvents) {
          return child_target;
        }
      }
    }
    return nullptr;  // Don't return 'this' - we ignore events
  }

  ///////////////////////////
  // Check for overlay toggle hotkey
  ///////////////////////////
  if (ev->_eventcode == ui::EventCode::KEY_DOWN) {
    if (ev->miKeyCode == '~' || ev->miKeyCode == '`') {
      if (_overlay_widget) {
        _overlay_enabled = !_overlay_enabled;
        SetDirty();
        //printf("KC\n");
        return this;  // Consume event
      }
    }
  }

  ///////////////////////////
  // If overlay is enabled, route events to it first (modal behavior)
  ///////////////////////////
  if (_overlay_widget && _overlay_enabled) {
    auto result = _overlay_widget->routeUiEvent(ev);
    if (result) {
      //printf("OVL\n");
      _guide_highlite = nullptr;
      return result;  // Overlay handled the event
    }
  }

  ///////////////////////////
  // Only search for a new guide if we're not already dragging one
  // Once grabbed, the guide stays grabbed until RELEASE or END_DRAG
  if(_guide_being_dragged){
    //printf("GBG\n");
    _guide_highlite = nullptr;
    return this;
  }
  ///////////////////////////
  Widget* target_widget = nullptr;
  size_t num_children = _children.size();
  for( size_t i=0; i<num_children; i++ ){
    size_t idx = num_children - 1 - i;
    auto child = _children[idx];
    bool inside = child->IsEventInside(ev);
    if (inside) {
      auto child_target = child->routeUiEvent(ev);
      if(child_target and child_target->_ignoreEvents) continue;
      if (child_target and child_target!=_overlay_widget.get()) {
        //printf("CHILD <%p:%s>\n", (void*) child_target, child_target->_name.c_str());
        //_clearColor = fvec4(0,0,0, 1.0);
        _guide_highlite = nullptr;
        return child_target;
      }
    }
  }
  //printf("FALLTHRU\n");
  return this;
}
/////////////////////////////////////////////////////////////////////////
}} // namespace ork::ui
