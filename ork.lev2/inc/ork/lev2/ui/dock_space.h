////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/ui/dock_panel.h>
#include <ork/lev2/ui/tabs.h>

namespace ork::ui {

struct DockNode;
struct DockSpace;
using docknode_ptr_t  = std::shared_ptr<DockNode>;
using dockspace_ptr_t = std::shared_ptr<DockSpace>;

////////////////////////////////////////////////////////////////////
// Drop zone for a moveChild attach. LEFT/RIGHT/TOP/BOTTOM split the
//  target leaf; CENTER tab-inserts into it.
//  crc values match the LEFT/RIGHT/TOP/BOTTOM placement tokens.
////////////////////////////////////////////////////////////////////

enum class EDockZone : crc_enum_t {
  CrcEnum(LEFT),
  CrcEnum(RIGHT),
  CrcEnum(TOP),
  CrcEnum(BOTTOM),
  CrcEnum(CENTER),
};

////////////////////////////////////////////////////////////////////
// DockNode: a node in a DockSpace's dock tree.
//  - leaf     : a TabWidget hosting one or more DockPanels
//  - interior : two child DockNodes divided by a split guide
//  (formalizes the LayoutGroup::split() container pattern)
////////////////////////////////////////////////////////////////////

struct DockNode {
  bool isLeaf() const { return _tabs != nullptr; }

  anchor::layout_ptr_t _layout;      // this node's region in the dock tree

  // leaf
  tabwidget_ptr_t _tabs;             // hosts N DockPanels (tabs hidden while N<=1)

  // interior
  docknode_ptr_t _child_a;
  docknode_ptr_t _child_b;
  anchor::guide_ptr_t _split_guide;
};

////////////////////////////////////////////////////////////////////
// DockDragHint: a draw-only, input-transparent translucent rect drawn as an
//  overlay over the would-be dock region during a titlebar drag.
////////////////////////////////////////////////////////////////////

struct DockDragHint : public Widget {
  DockDragHint(const std::string& name);
  void DoDraw(drawevent_constptr_t drwev) override;
  Widget* doRouteUiEvent(event_constptr_t ev) override { return nullptr; }  // never intercepts
  fvec4 _color = fvec4(0.25f, 0.70f, 1.0f, 0.40f);  // translucent highlight
};
using dockdraghint_ptr_t = std::shared_ptr<DockDragHint>;

////////////////////////////////////////////////////////////////////
// Result of a zone hit-test: the leaf under the cursor, that leaf's target
//  panel (identifies it for moveChild) and the drop zone.
////////////////////////////////////////////////////////////////////

struct DockZoneHit {
  docknode_ptr_t _leaf;
  dockpanel_ptr_t _target;
  EDockZone _zone = EDockZone::CENTER;
  bool _valid = false;
};

////////////////////////////////////////////////////////////////////
// DockSpace: a LayoutGroup that owns a dock tree of DockNodes hosting
//  DockPanels. Reuses LayoutGroup::split() / guide-drag verbatim.
//
//  S0 declarative construction; S1 moveChild; S2 titlebar drag choreography.
////////////////////////////////////////////////////////////////////

struct DockSpace : public LayoutGroup {

  DockSpace(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0, int margin = 0);
  ~DockSpace();

  // Give split dividers a real grab band: claim PUSH/MOVE over a draggable guide
  // BEFORE child routing (else an adjacent leaf swallows it). See the .cpp.
  Widget* doRouteUiEvent(event_constptr_t ev) override;

  // Add a panel hosting 'content' to a leaf (defaults to the root leaf).
  dockpanel_ptr_t addPanel(
      widget_ptr_t content,
      const std::string& title,
      bool closeable    = false,
      dockpanel_ptr_t into_leaf_of = nullptr);

  // Split the leaf hosting 'target' and place a new panel hosting 'content'
  // into the freshly created region. Returns the new panel.
  dockpanel_ptr_t splitPanel(
      dockpanel_ptr_t target,
      anchor::ELayoutSplitPlacement placement,
      float proportion,
      widget_ptr_t content,
      const std::string& title,
      bool closeable = false,
      int margin     = -1);

  // THE primitive: move 'panel' out of its current leaf and attach it relative
  //  to 'target's leaf per 'zone' (CENTER = tab-insert; edges = zone split).
  //  Detach collapses an emptied source leaf (unsplit). When invoked during
  //  event dispatch it rides the context deferred-mutation queue; a direct call
  //  outside dispatch applies immediately.
  void moveChild(dockpanel_ptr_t panel, dockpanel_ptr_t target, EDockZone zone);

  // Remove 'panel' from its leaf WITHOUT re-hosting: exactly moveChild's
  //  source-side half (detach + collapse the emptied source leaf), so after this
  //  returns (or its deferred application) the DockSpace references neither the
  //  panel nor its content — the panel's lifetime is then ruled by its remaining
  //  (e.g. python) owners. Deferred-mutation aware like moveChild. The primitive
  //  a python-side cross-window factory-recreate transfer removes the source
  //  panel with.
  void removePanel(dockpanel_ptr_t panel);

  // Cursor (root-space) -> (leaf, target panel, zone). Outer edge bands of a
  // leaf select LEFT/RIGHT/TOP/BOTTOM; the interior selects CENTER (tab-insert).
  DockZoneHit zoneHitTest(int rx, int ry) const;
  // Root-space rect of the region a drop in 'hit' would occupy (drag-hint rect).
  Rect zoneRect(const DockZoneHit& hit) const;

  // Titlebar drag session (driven by DockPanel titlebar BEGIN/DRAG/END_DRAG, or
  // directly by tests). update consults the DockCoordinator so a cursor dragged
  // out of this window can target ANOTHER window's dock (foreign hint) or empty
  // desktop (tear-out); when the cursor stays local it pushes/repositions this
  // dock's own drag-hint overlay. end routes the commit: local moveChild, foreign
  // transfer (deferred), tear-out (deferred), or cancel — hints cleaned up on
  // every path.
  void beginPanelDrag(dockpanel_ptr_t panel);
  void updatePanelDrag(int rx, int ry);
  // canceled: the drag died mid-flight (a CANCELED END_DRAG from the Context, a
  //  re-entrant begin) -> FULL teardown with NO commit (hint pop, foreign-hint
  //  clear, state reset, cursor restore). Default false = a genuine release.
  void endPanelDrag(int rx, int ry, bool canceled = false);
  bool dragActive() const { return _drag_active; }
  // Panel currently being dragged (valid while dragActive) — the DockCoordinator
  // consults it for the W5 transferable/pinning classification.
  dockpanel_ptr_t dragPanel() const { return _drag_panel; }

  // Cross-window drop hint: shown on THIS dock when it is the drop target of a
  // drag ORIGINATING in another window. Driven by the source dock's
  // updatePanelDrag via the DockCoordinator; thin reuse of the DockDragHint
  // overlay machinery on this dock's own ui::Context.
  void showForeignDropHint(const DockZoneHit& hit);
  void hideForeignDropHint();

  // outer band = a dock zone; interior = CENTER (tab-insert)
  static constexpr float _kEdgeBandFraction = 0.25f;

  // Drag-hit grab band (px, each side of the divider) for EVERY split guide a
  // DockSpace creates — decoupled from the thin visual margin so runtime-docked
  // splitters are actually resizable (rides anchor::Guide::_hit_margin).
  static constexpr int _kGuideGrabBand = 6;

  // --- persistence support (python-side JSON; no reflection here) ---
  // Set the proportion of the divider between two panels' leaves (clamped).
  void setSplitProportion(dockpanel_ptr_t pa, dockpanel_ptr_t pb, float proportion);
  // Make 'panel' the active tab of its leaf.
  void activatePanel(dockpanel_ptr_t panel);
  // Move 'panel' to position 'index' in its leaf's tab order.
  void reorderPanel(dockpanel_ptr_t panel, int index);
  // Every hosted DockPanel across all leaves.
  std::vector<dockpanel_ptr_t> allPanels() const;

  docknode_ptr_t rootNode() const { return _root_node; }
  int numPanels() const;

private:
  // Configure a freshly created leaf TabWidget: hidden tabs (page mode while 1),
  // no background, explicit (reorderable, persisted) order + tab-drag callbacks
  // that feed the drag session.
  void _configureLeafTabs(tabwidget_ptr_t tabs);
  Rect _leafRootRect(docknode_ptr_t leaf) const;
  // Locate the leaf DockNode whose TabWidget currently hosts 'panel'.
  docknode_ptr_t _leafHosting(dockpanel_ptr_t panel) const;
  dockpanel_ptr_t _wrapAndHost(docknode_ptr_t leaf, widget_ptr_t content, const std::string& title, bool closeable);
  // Split 'target_leaf's region and return a fresh empty leaf in the new region.
  docknode_ptr_t _makeSplitLeaf(docknode_ptr_t target_leaf, anchor::ELayoutSplitPlacement placement, float proportion, int margin);
  // Host an EXISTING panel in a leaf's TabWidget (tab-insert) + refresh tab bar.
  void _hostPanel(docknode_ptr_t leaf, dockpanel_ptr_t panel);
  void _refreshLeaf(docknode_ptr_t leaf);
  // Drop an emptied leaf and collapse its split container (promote the sibling).
  void _collapseLeaf(docknode_ptr_t leaf);
  // Source-side half of moveChild/removePanel: detach 'panel' from its hosting
  // leaf (severing the parent link) and collapse that leaf if it emptied. Does
  // NOT re-host or re-layout; the caller owns any subsequent attach + updateAll.
  void _detachFromLeaf(dockpanel_ptr_t panel);
  void _doMoveChild(dockpanel_ptr_t panel, dockpanel_ptr_t target, EDockZone zone);
  void _doRemovePanel(dockpanel_ptr_t panel);
  // Local (single-window) half of updatePanelDrag: hit-test this dock's own leaves
  // and drive this dock's own drag-hint overlay (the pre-W4 behavior).
  void _updateLocalDrag(int rx, int ry);
  // Explicit teardown of the LOCAL drop tile + its pending target, used by every
  // non-local classification (foreign / tear-out) so a cursor that leaves a valid
  // local zone never leaves a stale tile or _pending_target behind (BUG-A).
  void _hideLocalHintClearPending();
  // Manage the hint shown on a FOREIGN dock while this dock's drag targets it.
  void _setForeignHint(DockSpace* dock, const DockZoneHit& hit);
  void _clearForeignHint();

  docknode_ptr_t _root_node;
  std::vector<docknode_ptr_t> _leaves;   // flat leaf index (interior tree is S2/S3 drag work)
  int _panel_counter = 0;
  // Visual gap (px) for runtime (drag-created) splits — matches the editors'
  // declared split margin so runtime and declared dividers look identical.
  int _split_margin = 2;

  // Drag SESSION state — downstream CONSUMER bookkeeping, not a lifecycle
  // authority. The drag lifecycle (when a drag begins/ends/cancels, and the
  // guarantee that every capture clear emits END_DRAG) is owned by the
  // ui::Context drag-capture HFSM (context.h, F2); this widget only REACTS to
  // the BEGIN_DRAG / END_DRAG(canceled) events that machine emits, tracking
  // which panel/hint/commit-route the current session is about. Deliberately
  // plain flags, NOT a second fsm (owner adjudication 2026-07-23: converting
  // event-consumer bookkeeping would formalize nothing the machine doesn't
  // already guarantee) — keep it that way unless this code grows lifecycle
  // decisions of its own, which belong in the Context machine instead.
  bool _drag_active = false;
  dockpanel_ptr_t _drag_panel;
  dockdraghint_ptr_t _drag_hint;
  dockpanel_ptr_t _pending_target;
  EDockZone _pending_zone = EDockZone::CENTER;
  bool _has_pending = false;

  // cross-window drag state (W4). _drag_commit is recomputed each updatePanelDrag
  // and consumed by endPanelDrag to route the commit.
  enum class DragCommit { LOCAL, FOREIGN, TEAROUT, CANCEL };
  DragCommit _drag_commit = DragCommit::LOCAL;
  DockSpace* _active_foreign_dock = nullptr;   // foreign dock currently showing a hint (during a drag)
  std::string _pending_foreign_key;            // dst window key (FOREIGN commit)
  std::string _pending_foreign_target;         // target panel name, or "" (FOREIGN commit)
  EDockZone _pending_foreign_zone = EDockZone::CENTER;
  int _tearout_screen_x = 0;                   // TEAROUT commit: drop point (screen points)
  int _tearout_screen_y = 0;

  // the foreign-target drop hint drawn on THIS dock (as another window's target)
  dockdraghint_ptr_t _foreign_hint;
};

} // namespace ork::ui
