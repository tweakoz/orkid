////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/ui/dock_space.h>
#include <ork/lev2/ui/dock_coordinator.h>
#include <ork/lev2/ui/context.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/pri.h>

namespace ork::ui {

namespace anchor {
// defined in anchor_guide.cpp; returns the nearest UNLOCKED draggable guide within
// its (hit-margin-widened) band under the cursor, in the same root-space coords
// LayoutGroup::OnUiEvent uses — nullptr if none.
guide_ptr_t findGuidePairUnderMouse(const Layout* rootLayout, const fvec2& mousePos);
} // namespace anchor

/////////////////////////////////////////////////////////////////////////
static std::string _zoneToString(EDockZone z) {
  switch (z) {
    case EDockZone::LEFT:   return "LEFT";
    case EDockZone::RIGHT:  return "RIGHT";
    case EDockZone::TOP:    return "TOP";
    case EDockZone::BOTTOM: return "BOTTOM";
    default:                return "CENTER";
  }
}

/////////////////////////////////////////////////////////////////////////
DockDragHint::DockDragHint(const std::string& name)
    : Widget(name, 0, 0, 0, 0) {
  _ignoreEvents = true;
}
/////////////////////////////////////////////////////////////////////////
void DockDragHint::DoDraw(drawevent_constptr_t drwev) {
  if (not _enable)
    return;
  auto tgt    = drwev->GetTarget();
  auto mtxi   = tgt->MTXI();
  auto primi  = tgt->PRI();
  auto defmtl = lev2::defaultUIMaterial();
  int ixr, iyr;
  LocalToRoot(0, 0, ixr, iyr);
  mtxi->PushUIMatrix();
  {
    defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
    defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);
    tgt->PushModColor(_color);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
    primi->RenderQuadAtZ(
        defmtl.get(),
        ixr, ixr + _geometry._w,
        iyr, iyr + _geometry._h,
        0.0f,
        0.0f, 1.0f,
        0.0f, 1.0f);
    tgt->PopModColor();
    defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::OFF);
  }
  mtxi->PopUIMatrix();
}

/////////////////////////////////////////////////////////////////////////
DockSpace::DockSpace(const std::string& name, int x, int y, int w, int h, int margin)
    : LayoutGroup(name, x, y, w, h, margin) {
}
/////////////////////////////////////////////////////////////////////////
DockSpace::~DockSpace() {
}
/////////////////////////////////////////////////////////////////////////
// Splitter grab band. LayoutGroup::doRouteUiEvent is child-first with a
// LayoutGroup-fallthrough, so the effective grab band is only the visual gap
// (~2px) — the _hit_margin widening never fires for a PUSH that lands over a
// child leaf. For PUSH/MOVE only, if a draggable guide is under the cursor
// within its band, claim the event for this LayoutGroup up front; OnUiEvent
// re-finds the guide (grab on PUSH, hover-highlight on MOVE). DRAG/RELEASE are
// never re-routed — they ride Context::_evdragtarget capture from the PUSH, and
// BEGIN_DRAG is synthesized onto that captured target. _ignoreEvents delegates.
// Cost: a <=hit_margin edge strip adjacent to each divider is stolen from panel
// content / titlebars / tab bars — intended, standard splitter UX.
/////////////////////////////////////////////////////////////////////////
Widget* DockSpace::doRouteUiEvent(event_constptr_t ev) {
  if (not _ignoreEvents) {
    auto code = ev->_eventcode;
    if (code == EventCode::PUSH or code == EventCode::MOVE) {
      if (anchor::findGuidePairUnderMouse(_layout.get(), fvec2(ev->miX, ev->miY)))
        return this;
    }
  }
  return LayoutGroup::doRouteUiEvent(ev);
}
/////////////////////////////////////////////////////////////////////////
int DockSpace::numPanels() const {
  int n = 0;
  for (auto& leaf : _leaves)
    if (leaf->_tabs)
      n += leaf->_tabs->getTabCount();
  return n;
}
/////////////////////////////////////////////////////////////////////////
// Refresh a leaf's tab bar: hide tabs while a single panel is hosted, and make
// sure a valid tab is active after any add/remove.
/////////////////////////////////////////////////////////////////////////
void DockSpace::_refreshLeaf(docknode_ptr_t leaf) {
  int n = leaf->_tabs->getTabCount();
  leaf->_tabs->setShowTabs(n > 1);
  if (n > 0)
    leaf->_tabs->setActiveTab(0);
}
/////////////////////////////////////////////////////////////////////////
void DockSpace::_hostPanel(docknode_ptr_t leaf, dockpanel_ptr_t panel) {
  OrkAssertI(leaf && leaf->_tabs, "DockSpace: leaf has no TabWidget");
  leaf->_tabs->addChild(panel);   // auto-reparents out of any prior TabWidget
  int n = leaf->_tabs->getTabCount();
  leaf->_tabs->setShowTabs(n > 1);
  leaf->_tabs->setActiveTabByName(panel->GetName());
}
/////////////////////////////////////////////////////////////////////////
// Wrap 'content' in a DockPanel titlebar and host it in a leaf's TabWidget.
/////////////////////////////////////////////////////////////////////////
dockpanel_ptr_t DockSpace::_wrapAndHost(
    docknode_ptr_t leaf,
    widget_ptr_t content,
    const std::string& title,
    bool closeable) {
  // The panel's name is its stable id AND its tab label; default it to the
  // construction title so the tab layout is correct from the start (an empty
  // title falls back to an auto-generated unique name).
  auto name  = title.empty() ? (_name + FormatString("-panel%d", _panel_counter++)) : title;
  auto panel = std::make_shared<DockPanel>(name);
  panel->_title_override = title;
  panel->setChild(content);
  _hostPanel(leaf, panel);
  if (closeable)
    leaf->_tabs->setTabCloseable(panel, true);
  return panel;
}
/////////////////////////////////////////////////////////////////////////
docknode_ptr_t DockSpace::_leafHosting(dockpanel_ptr_t panel) const {
  for (auto& leaf : _leaves) {
    if (not leaf->_tabs)
      continue;
    for (auto& child : leaf->_tabs->_children) {
      if (child == panel)
        return leaf;
    }
  }
  return nullptr;
}
/////////////////////////////////////////////////////////////////////////
docknode_ptr_t DockSpace::_makeSplitLeaf(
    docknode_ptr_t target_leaf,
    anchor::ELayoutSplitPlacement placement,
    float proportion,
    int margin) {

  // Sole funnel for every DockSpace split (declared splitPanel + pyext dock.split
  // both reach here; _doMoveChild passes _split_margin directly). A negative
  // margin means "unspecified" — resolve it to the DockSpace split gap here,
  // BEFORE LayoutGroup::split() would fall back to the group's own margin (0),
  // so declared-default and runtime splits share one visual gap. Explicit
  // margins (editors, tests passing >=0) pass through untouched.
  if (margin < 0)
    margin = _split_margin;
  auto litem      = this->split(target_leaf->_layout, proportion, placement, margin, _kGuideGrabBand);
  auto container  = std::dynamic_pointer_cast<LayoutGroup>(litem->_widget);
  auto new_layout = litem->_layout;
  OrkAssertI(container != nullptr, "DockSpace: split did not yield a container");

  auto new_tabs = std::make_shared<TabWidget>(_name + FormatString("-leaf%d", int(_leaves.size())));
  _configureLeafTabs(new_tabs);
  new_layout->bindWidget(new_tabs);
  container->addChild(new_tabs, false);
  new_layout->updateAll();

  auto new_leaf     = std::make_shared<DockNode>();
  new_leaf->_layout = new_layout;
  new_leaf->_tabs   = new_tabs;
  _leaves.push_back(new_leaf);
  return new_leaf;
}
/////////////////////////////////////////////////////////////////////////
void DockSpace::_collapseLeaf(docknode_ptr_t leaf) {
  auto container_layout = leaf->_layout->_parent;
  if (not container_layout)
    return;
  // A leaf directly under the dock space root is the sole leaf — nothing to
  // collapse; keep it (empty) as the base.
  if (container_layout->_widget == this)
    return;

  anchor::layout_ptr_t sibling;
  for (auto& cl : container_layout->_childlayouts)
    if (cl.get() != leaf->_layout.get()) {
      sibling = cl;
      break;
    }
  OrkAssertI(sibling != nullptr, "DockSpace: emptied leaf has no sibling to promote");

  _leaves.erase(std::remove(_leaves.begin(), _leaves.end(), leaf), _leaves.end());
  unsplit(sibling);  // removes the emptied leaf + collapses the T-junction
}
/////////////////////////////////////////////////////////////////////////
dockpanel_ptr_t DockSpace::addPanel(
    widget_ptr_t content,
    const std::string& title,
    bool closeable,
    dockpanel_ptr_t into_leaf_of) {

  docknode_ptr_t leaf;

  if (not _root_node) {
    // first panel: create the root leaf as a full-bleed fill child
    auto tabs = std::make_shared<TabWidget>(_name + "-leaf0");
    _configureLeafTabs(tabs);
    auto layout = _layout->childLayout(tabs);
    layout->top()->anchorTo(_layout->top());
    layout->left()->anchorTo(_layout->left());
    layout->bottom()->anchorTo(_layout->bottom());
    layout->right()->anchorTo(_layout->right());
    layout->setMargin(_margin);
    addChild(tabs);
    _layout->updateAll();

    _root_node          = std::make_shared<DockNode>();
    _root_node->_layout = layout;
    _root_node->_tabs   = tabs;
    _leaves.push_back(_root_node);
    leaf = _root_node;
  } else if (into_leaf_of) {
    leaf = _leafHosting(into_leaf_of);
    OrkAssertI(leaf, "DockSpace::addPanel: into_leaf_of is not hosted by any leaf");
  } else {
    leaf = _leaves.front();
  }

  return _wrapAndHost(leaf, content, title, closeable);
}
/////////////////////////////////////////////////////////////////////////
dockpanel_ptr_t DockSpace::splitPanel(
    dockpanel_ptr_t target,
    anchor::ELayoutSplitPlacement placement,
    float proportion,
    widget_ptr_t content,
    const std::string& title,
    bool closeable,
    int margin) {

  auto leaf = _leafHosting(target);
  OrkAssertI(leaf, "DockSpace::splitPanel: target panel not hosted by any leaf");
  auto new_leaf = _makeSplitLeaf(leaf, placement, proportion, margin);
  return _wrapAndHost(new_leaf, content, title, closeable);
}
/////////////////////////////////////////////////////////////////////////
void DockSpace::moveChild(dockpanel_ptr_t panel, dockpanel_ptr_t target, EDockZone zone) {
  // During event dispatch, defer so no widget is destroyed mid-dispatch; a
  // direct (out-of-dispatch) call applies immediately.
  if (_uicontext && _uicontext->_dispatching) {
    auto self = this;
    _uicontext->enqueueDeferredMutation([self, panel, target, zone]() { //
      self->_doMoveChild(panel, target, zone);
    });
    return;
  }
  _doMoveChild(panel, target, zone);
}
/////////////////////////////////////////////////////////////////////////
void DockSpace::_detachFromLeaf(dockpanel_ptr_t panel) {
  auto source_leaf = _leafHosting(panel);
  OrkAssertI(source_leaf, "DockSpace: panel not hosted by any leaf");

  ////////////////////////////////////////
  // detach from the source leaf
  ////////////////////////////////////////
  source_leaf->_tabs->removeChild(panel);
  panel->_parent = nullptr;  // sever parent link before the source leaf may be destroyed
  _refreshLeaf(source_leaf);

  ////////////////////////////////////////
  // collapse the source leaf if it emptied
  ////////////////////////////////////////
  if (source_leaf->_tabs->getTabCount() == 0)
    _collapseLeaf(source_leaf);
}
/////////////////////////////////////////////////////////////////////////
void DockSpace::_doMoveChild(dockpanel_ptr_t panel, dockpanel_ptr_t target, EDockZone zone) {
  auto source_leaf = _leafHosting(panel);
  OrkAssertI(source_leaf, "moveChild: panel not hosted by any leaf");
  auto target_leaf0 = _leafHosting(target);
  OrkAssertI(target_leaf0, "moveChild: target not hosted by any leaf");

  // CENTER onto the panel's own leaf is a no-op.
  if (zone == EDockZone::CENTER && source_leaf == target_leaf0)
    return;

  ////////////////////////////////////////
  // detach from the source leaf (+ collapse if emptied)
  ////////////////////////////////////////
  _detachFromLeaf(panel);

  // re-resolve the target leaf (a collapse may have re-anchored layouts)
  auto target_leaf = _leafHosting(target);
  OrkAssertI(target_leaf, "moveChild: target leaf vanished after source collapse");

  ////////////////////////////////////////
  // attach to the target
  ////////////////////////////////////////
  if (zone == EDockZone::CENTER) {
    _hostPanel(target_leaf, panel);
  } else {
    anchor::ELayoutSplitPlacement placement = anchor::ELayoutSplitPlacement::RIGHT;
    switch (zone) {
      case EDockZone::LEFT:   placement = anchor::ELayoutSplitPlacement::LEFT;   break;
      case EDockZone::RIGHT:  placement = anchor::ELayoutSplitPlacement::RIGHT;  break;
      case EDockZone::TOP:    placement = anchor::ELayoutSplitPlacement::TOP;    break;
      case EDockZone::BOTTOM: placement = anchor::ELayoutSplitPlacement::BOTTOM; break;
      default: break;
    }
    auto new_leaf = _makeSplitLeaf(target_leaf, placement, 0.5f, _split_margin);
    _hostPanel(new_leaf, panel);
  }

  // Re-position the whole tree so re-parented leaves (and their contents) get a
  // full geometry + DoLayout cascade. In-__init__ callers get this free from the
  // first-frame layout; a runtime move (via the deferred queue) needs it
  // explicitly, or the re-parented widgets render with stale/zero geometry.
  _layout->updateAll();
}
/////////////////////////////////////////////////////////////////////////
void DockSpace::removePanel(dockpanel_ptr_t panel) {
  // Same deferral discipline as moveChild: during event dispatch defer so no
  // widget is destroyed mid-dispatch; a direct call applies immediately.
  if (_uicontext && _uicontext->_dispatching) {
    auto self = this;
    _uicontext->enqueueDeferredMutation([self, panel]() { //
      self->_doRemovePanel(panel);
    });
    return;
  }
  _doRemovePanel(panel);
}
/////////////////////////////////////////////////////////////////////////
void DockSpace::_doRemovePanel(dockpanel_ptr_t panel) {
  _detachFromLeaf(panel);
  // Re-position the surviving tree so leaves re-anchored by a source collapse
  // (and their contents) get a full geometry + DoLayout cascade — mirrors the
  // trailing updateAll of _doMoveChild.
  _layout->updateAll();
}
/////////////////////////////////////////////////////////////////////////
void DockSpace::setSplitProportion(dockpanel_ptr_t pa, dockpanel_ptr_t pb, float proportion) {
  auto la = _leafHosting(pa);
  auto lb = _leafHosting(pb);
  OrkAssertI(la && lb, "setSplitProportion: panel not hosted by any leaf");
  auto guide = _layout->findGuideBetween(la->_layout, lb->_layout);
  OrkAssertI(guide != nullptr, "setSplitProportion: no divider guide between the two leaves");
  // Propagate current geometry first so the guide's 32px anti-crossing clamp
  // works against real container dimensions (a restructure via moveChild only
  // updates the affected subtree, not the whole tree).
  _layout->updateAll();
  guide->setProportion(proportion);  // clamped
  _layout->updateAll();
}
/////////////////////////////////////////////////////////////////////////
void DockSpace::activatePanel(dockpanel_ptr_t panel) {
  auto leaf = _leafHosting(panel);
  OrkAssertI(leaf, "activatePanel: panel not hosted by any leaf");
  leaf->_tabs->setActiveTabByName(panel->GetName());
}
/////////////////////////////////////////////////////////////////////////
void DockSpace::reorderPanel(dockpanel_ptr_t panel, int index) {
  auto leaf = _leafHosting(panel);
  OrkAssertI(leaf, "reorderPanel: panel not hosted by any leaf");
  leaf->_tabs->reorderTab(panel, index);
}
/////////////////////////////////////////////////////////////////////////
void DockSpace::_configureLeafTabs(tabwidget_ptr_t tabs) {
  tabs->setShowTabs(false);
  tabs->_draw_background = false;
  tabs->setSortTabs(false);  // explicit, reorderable order (persisted via dock_layout)
  tabs->_onTabDetach = [this](widget_ptr_t tab, int rx, int ry) {
    if (auto p = std::dynamic_pointer_cast<DockPanel>(tab)) {
      beginPanelDrag(p);
      updatePanelDrag(rx, ry);
    }
  };
  tabs->_onTabDragMove   = [this](int rx, int ry) { updatePanelDrag(rx, ry); };
  tabs->_onTabDragCommit = [this](int rx, int ry, bool canceled) { endPanelDrag(rx, ry, canceled); };
}
/////////////////////////////////////////////////////////////////////////
std::vector<dockpanel_ptr_t> DockSpace::allPanels() const {
  std::vector<dockpanel_ptr_t> out;
  for (auto& leaf : _leaves) {
    if (not leaf->_tabs)
      continue;
    for (auto& ch : leaf->_tabs->_children)
      if (auto p = std::dynamic_pointer_cast<DockPanel>(ch))
        out.push_back(p);
  }
  return out;
}
/////////////////////////////////////////////////////////////////////////
Rect DockSpace::_leafRootRect(docknode_ptr_t leaf) const {
  int x = 0, y = 0;
  leaf->_tabs->LocalToRoot(0, 0, x, y);
  auto g = leaf->_tabs->geometry();
  return Rect(x, y, g._w, g._h);
}
/////////////////////////////////////////////////////////////////////////
DockZoneHit DockSpace::zoneHitTest(int rx, int ry) const {
  DockZoneHit hit;
  for (auto& leaf : _leaves) {
    if (not leaf->_tabs)
      continue;
    Rect r = _leafRootRect(leaf);
    if (rx < r._x || rx >= r._x + r._w || ry < r._y || ry >= r._y + r._h)
      continue;

    hit._leaf  = leaf;
    hit._valid = true;
    if (not leaf->_tabs->_children.empty())
      hit._target = std::dynamic_pointer_cast<DockPanel>(leaf->_tabs->_children[0]);

    // outer bands select an edge zone; the interior is CENTER. On a corner the
    // nearest edge wins.
    float fx = float(rx - r._x) / float(r._w);
    float fy = float(ry - r._y) / float(r._h);
    float dl = fx, dr = 1.0f - fx, dt = fy, db = 1.0f - fy;
    EDockZone zone = EDockZone::CENTER;
    float best = _kEdgeBandFraction;
    if (dl < best) { best = dl; zone = EDockZone::LEFT;   }
    if (dr < best) { best = dr; zone = EDockZone::RIGHT;  }
    if (dt < best) { best = dt; zone = EDockZone::TOP;    }
    if (db < best) { best = db; zone = EDockZone::BOTTOM; }
    hit._zone = zone;
    break;
  }
  return hit;
}
/////////////////////////////////////////////////////////////////////////
Rect DockSpace::zoneRect(const DockZoneHit& hit) const {
  if (not hit._valid || not hit._leaf)
    return Rect(0, 0, 0, 0);
  Rect r = _leafRootRect(hit._leaf);
  switch (hit._zone) {
    case EDockZone::LEFT:   return Rect(r._x, r._y, r._w / 2, r._h);
    case EDockZone::RIGHT:  return Rect(r._x + r._w / 2, r._y, r._w - r._w / 2, r._h);
    case EDockZone::TOP:    return Rect(r._x, r._y, r._w, r._h / 2);
    case EDockZone::BOTTOM: return Rect(r._x, r._y + r._h / 2, r._w, r._h - r._h / 2);
    default: break;
  }
  return r;  // CENTER
}
/////////////////////////////////////////////////////////////////////////
void DockSpace::beginPanelDrag(dockpanel_ptr_t panel) {
  if (not panel || not _uicontext)
    return;
  // Re-entry guard: a still-active drag here is a stale/orphaned session (a prior
  // drag that died without a clean end). Tear it down FIRST so we never stack a
  // second hint overlay over an orphaned one.
  if (_drag_active)
    endPanelDrag(0, 0, /*canceled*/ true);
  // Reset the DOCKTRACE dedup so this session logs its first classification.
  DockCoordinator::instance()->resetTraceDedup();
  _drag_active = true;
  _drag_panel  = panel;
  _has_pending = false;
  _drag_hint   = std::make_shared<DockDragHint>(_name + "-draghint");
  _drag_hint->_enable = false;
  _uicontext->pushOverlay(_drag_hint, 0, 0, 0, 0, /*dismiss_on_click_outside*/ false, nullptr, /*modal*/ false);
}
/////////////////////////////////////////////////////////////////////////
void DockSpace::_updateLocalDrag(int rx, int ry) {
  auto hit  = zoneHitTest(rx, ry);
  bool valid = hit._valid && (hit._target != nullptr);

  // reject a no-op self-drop (own leaf CENTER, or a lone panel onto its own edge)
  if (valid) {
    auto src_leaf = _leafHosting(_drag_panel);
    if (src_leaf == hit._leaf) {
      if (hit._zone == EDockZone::CENTER || hit._leaf->_tabs->getTabCount() <= 1)
        valid = false;
    }
  }

  if (valid) {
    Rect zr = zoneRect(hit);
    _drag_hint->_enable = true;
    _uicontext->repositionOverlay(_drag_hint, zr._x, zr._y, zr._w, zr._h);
    _pending_target = hit._target;
    _pending_zone   = hit._zone;
    _has_pending    = true;
  } else {
    _drag_hint->_enable = false;
    _has_pending        = false;
  }
}
/////////////////////////////////////////////////////////////////////////
void DockSpace::_hideLocalHintClearPending() {
  if (_drag_hint)
    _drag_hint->_enable = false;
  _has_pending    = false;
  _pending_target = nullptr;
}
/////////////////////////////////////////////////////////////////////////
void DockSpace::updatePanelDrag(int rx, int ry) {
  if (not _drag_active || not _uicontext)
    return;

  auto coord = DockCoordinator::instance();
  auto res   = coord->resolveDrag(this, rx, ry);
  using K    = DockDragResolve::Kind;

  // Explicit per-classification hint/pending state machine, re-applied on EVERY
  // update (not just on entry): whenever the cursor leaves a valid local zone —
  // into a foreign window, a foreign non-zone, empty desktop, or (BUG-B) over
  // another app's window occluding ours — the local drop tile and its
  // _pending_target/_has_pending are cleared, so no stale blue tile is left stuck
  // at the exit point (BUG-A).
  switch (res._kind) {
    case K::LOCAL:
    case K::UNSUPPORTED:
      // Cursor over THIS window, or cross-window info unavailable (single-window
      // degrade — Wayland / window bring-up). Foreign hint off; the local
      // hit-test owns the local tile + pending (it self-hides when off a zone).
      _clearForeignHint();
      _updateLocalDrag(rx, ry);
      _drag_commit = _has_pending ? DragCommit::LOCAL : DragCommit::CANCEL;
      break;
    case K::FOREIGN_ZONE:
      // Over another window's dock on a valid zone: local tile+pending OFF, foreign ON.
      _hideLocalHintClearPending();
      _setForeignHint(res._foreign_dock, res._foreign_hit);
      _pending_foreign_key    = res._foreign_key;
      _pending_foreign_target = res._foreign_hit._target ? res._foreign_hit._target->GetName() : std::string();
      _pending_foreign_zone   = res._foreign_hit._zone;
      _drag_commit            = DragCommit::FOREIGN;
      break;
    case K::FOREIGN_NOZONE:
      // Over another window but not a valid zone: BOTH hints OFF, pending cleared -> CANCEL.
      _hideLocalHintClearPending();
      _clearForeignHint();
      _drag_commit = DragCommit::CANCEL;
      break;
    case K::TEAROUT:
      // Outside every one of OUR windows — empty desktop OR occluded by another
      // app (BUG-B): BOTH hints OFF, pending cleared -> tear-out.
      _hideLocalHintClearPending();
      _clearForeignHint();
      _tearout_screen_x = res._screen_x;
      _tearout_screen_y = res._screen_y;
      _drag_commit      = DragCommit::TEAROUT;
      break;
  }

  // Drag cursor feedback (F2a-lite): the resolved commit names the drop affordance.
  DragCursor cur;
  switch (_drag_commit) {
    case DragCommit::LOCAL:
    case DragCommit::FOREIGN:  cur = DragCursor::RESIZE_ALL;  break; // a valid dock zone
    case DragCommit::TEAROUT:  cur = DragCursor::HAND;        break; // empty desktop
    case DragCommit::CANCEL:
    default:                   cur = DragCursor::NOT_ALLOWED; break; // no valid drop
  }
  coord->setDragCursor(cur);
}
/////////////////////////////////////////////////////////////////////////
void DockSpace::endPanelDrag(int rx, int ry, bool canceled) {
  if (not _drag_active)
    return;

  auto coord    = DockCoordinator::instance();
  auto self_ctx = _uicontext;

  // Canceled: the drag died mid-flight. FULL teardown, NO commit — un-wedges the
  // session (pop the hint overlay, clear any foreign hint, reset state, restore the
  // cursor) so the very next drag arms cleanly.
  if (canceled) {
    if (self_ctx && _drag_hint)
      self_ctx->removeOverlay(_drag_hint);
    _clearForeignHint();
    coord->setDragCursor(DragCursor::ARROW);
    _drag_active    = false;
    _drag_hint      = nullptr;
    _has_pending    = false;
    _drag_panel     = nullptr;
    _pending_target = nullptr;
    _drag_commit    = DragCommit::LOCAL;
    return;
  }

  updatePanelDrag(rx, ry);  // final resolve at the release point

  auto panel   = _drag_panel;

  switch (_drag_commit) {
    case DragCommit::LOCAL:
      if (_has_pending && _pending_target && panel)
        moveChild(panel, _pending_target, _pending_zone);  // defers when in dispatch
      break;
    case DragCommit::FOREIGN: {
      if (panel && not _pending_foreign_key.empty() && self_ctx) {
        std::string panel_id = panel->GetName();
        std::string src_key  = coord->keyForDock(this);
        std::string dst_key  = _pending_foreign_key;
        std::string target   = _pending_foreign_target;
        std::string zone     = _zoneToString(_pending_foreign_zone);
        // The transfer (python removePanel + factory-recreate + moveChild) must run
        // OUTSIDE ui event dispatch so removePanel/moveChild apply immediately and
        // the remove-before-rebuild ordering holds. endPanelDrag is on the dispatch
        // stack (titlebar END_DRAG), so route the commit through the source context
        // deferred-mutation queue — it runs post-dispatch (_dispatching==false).
        self_ctx->enqueueDeferredMutation([coord, panel_id, src_key, dst_key, target, zone]() {
          coord->invokeTransfer(panel_id, src_key, dst_key, target, zone);
        });
      }
      break;
    }
    case DragCommit::TEAROUT: {
      if (panel && self_ctx) {
        std::string panel_id = panel->GetName();
        std::string src_key  = coord->keyForDock(this);
        int sx = _tearout_screen_x, sy = _tearout_screen_y;
        // Same deferral discipline as FOREIGN. The python tear-out callback must
        // NOT create a window inline (that is main-thread work); it records a
        // request the app pumps on the render thread — see dock_manager.py.
        self_ctx->enqueueDeferredMutation([coord, panel_id, src_key, sx, sy]() {
          coord->invokeTearOut(panel_id, src_key, sx, sy);
        });
      }
      break;
    }
    case DragCommit::CANCEL:
    default:
      break;
  }

  // hint cleanup on EVERY path (local move, foreign, tear-out, cancel)
  if (self_ctx && _drag_hint)
    self_ctx->removeOverlay(_drag_hint);
  _clearForeignHint();
  coord->setDragCursor(DragCursor::ARROW);

  _drag_active    = false;
  _drag_hint      = nullptr;
  _has_pending    = false;
  _drag_panel     = nullptr;
  _pending_target = nullptr;
  _drag_commit    = DragCommit::LOCAL;
}
/////////////////////////////////////////////////////////////////////////
void DockSpace::showForeignDropHint(const DockZoneHit& hit) {
  if (not _uicontext)
    return;
  if (not _foreign_hint) {
    _foreign_hint         = std::make_shared<DockDragHint>(_name + "-foreignhint");
    _foreign_hint->_enable = false;
    // push at zero size (like the local hint); reposition sets the real rect and
    // avoids pushOverlay's overflow-flip on the initial push.
    _uicontext->pushOverlay(_foreign_hint, 0, 0, 0, 0, /*dismiss_on_click_outside*/ false, nullptr, /*modal*/ false);
  }
  Rect zr             = zoneRect(hit);
  _foreign_hint->_enable = true;
  _uicontext->repositionOverlay(_foreign_hint, zr._x, zr._y, zr._w, zr._h);
}
/////////////////////////////////////////////////////////////////////////
void DockSpace::hideForeignDropHint() {
  if (not _foreign_hint)
    return;
  _foreign_hint->_enable = false;
  if (_uicontext)
    _uicontext->removeOverlay(_foreign_hint);
  _foreign_hint = nullptr;
}
/////////////////////////////////////////////////////////////////////////
void DockSpace::_setForeignHint(DockSpace* dock, const DockZoneHit& hit) {
  if (_active_foreign_dock && _active_foreign_dock != dock)
    _active_foreign_dock->hideForeignDropHint();
  _active_foreign_dock = dock;
  if (dock)
    dock->showForeignDropHint(hit);
}
/////////////////////////////////////////////////////////////////////////
void DockSpace::_clearForeignHint() {
  if (_active_foreign_dock) {
    _active_foreign_dock->hideForeignDropHint();
    _active_foreign_dock = nullptr;
  }
}
/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
