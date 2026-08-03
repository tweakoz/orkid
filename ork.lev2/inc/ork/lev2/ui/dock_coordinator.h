////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/dock_space.h>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>

namespace ork::ui {

struct DockCoordinator;
using dockcoordinator_ptr_t = std::shared_ptr<DockCoordinator>;

////////////////////////////////////////////////////////////////////
// A window's CURRENT screen rect, in SCREEN POINTS (glfwGetWindowPos +
//  logical window size). A returned rect with _w<=0 || _h<=0 signals
//  "position unavailable" (window closed, or a platform where window
//  position cannot be queried, e.g. Wayland) -> the coordinator degrades
//  that window out of cross-window targeting.
////////////////////////////////////////////////////////////////////
using dock_rect_provider_t = std::function<Rect()>;

// Transfer commit: (panel_id, src_key, dst_key, target_panel_id_or_empty, zone_string).
using dock_transfer_cb_t = std::function<void(std::string, std::string, std::string, std::string, std::string)>;
// Tear-out commit: (panel_id, src_key, screen_x, screen_y).
using dock_tearout_cb_t = std::function<void(std::string, std::string, int, int)>;
// Transferable predicate (W5): (panel_id) -> bool. A drag of a panel for which this
//  returns false is PINNED — classified LOCAL-only in resolveDrag: no foreign hint,
//  no tear-out, in-window drag behavior unchanged. Unset => every panel is
//  transferable (the pre-W5 behavior). Set from python (DockManager: a panel is
//  transferable IFF it has a registered factory).
using dock_transferable_pred_t = std::function<bool(std::string)>;

////////////////////////////////////////////////////////////////////
// Point-ownership (BUG-B occlusion awareness). Rect containment alone cannot
//  tell that ANOTHER application's window (e.g. the terminal) covers ours at a
//  screen point — so a point that is geometrically inside our rect but under a
//  foreign OS window must classify as if it were OUTSIDE every one of our
//  windows (tear-out). These resolve which of OUR windows (by key) owns the
//  TOPMOST OS window at a screen point:
//    ""  => the topmost window at the point is NOT ours (another app / desktop).
//    key => that OUR window is the actually-topmost one at the point (true
//           z-order between our OWN overlapping windows, replacing the v1
//           registration-order heuristic).
//  Two sources, override WINS:
//    * setPointOwnershipOverride — a test seam pluggable for gates.
//    * the native leg (mac): a per-entry Cocoa window-number provider +
//      _topmost_number_fn (the topmost OS window number at a screen point).
//  Unavailable (non-mac, unwired, or synthetic rect-override test mode) =>
//  degrade to the rect-only classification (the pre-BUG-B behavior).
using dock_point_owner_fn_t      = std::function<std::string(int, int)>;
using dock_topmost_number_fn_t   = std::function<int64_t(int, int)>;
using dock_win_number_provider_t = std::function<int64_t()>;

////////////////////////////////////////////////////////////////////
// Drag cursor feedback (F2a-lite). The ui:: layer names the intent; the glfw side
//  installs a seam that maps it to a cached glfwCreateStandardCursor on the drag
//  window (main-thread only — all drag code is main-thread). Values are the seam's
//  contract (glfw resolves the actual shapes); unset seam => no-op (offscreen gates,
//  non-glfw hosts). ARROW is the drag-idle restore.
////////////////////////////////////////////////////////////////////
enum class DragCursor : int {
  ARROW       = 0,
  RESIZE_ALL  = 1, // hovering a valid dock zone (local or foreign)
  HAND        = 2, // over empty desktop -> tear-out
  NOT_ALLOWED = 3, // no valid drop here (nozone / pinned-out / off any zone)
};
using dock_cursor_fn_t = std::function<void(int)>;

////////////////////////////////////////////////////////////////////
// Result of resolving a cross-window drag position against the window
//  registry. Computed each updatePanelDrag; consumed by endPanelDrag.
////////////////////////////////////////////////////////////////////

struct DockDragResolve {
  enum class Kind {
    LOCAL,          // cursor inside the source window (or source unregistered) -> local behavior
    FOREIGN_ZONE,   // cursor over a foreign registered window, over a valid drop zone
    FOREIGN_NOZONE, // cursor over a foreign registered window but not a valid zone
    TEAROUT,        // cursor outside every registered window -> tear-out into a new window
    UNSUPPORTED,    // source window position unavailable (degrade) -> behave single-window
  };
  Kind _kind = Kind::LOCAL;
  DockSpace* _foreign_dock = nullptr; // FOREIGN_*: the target window's dock
  std::string _foreign_key;           // FOREIGN_*: the target window key
  DockZoneHit _foreign_hit;           // FOREIGN_ZONE: hit in TARGET-local coords
  int _screen_x = 0;                  // FOREIGN_*/TEAROUT: screen cursor (points)
  int _screen_y = 0;
};

////////////////////////////////////////////////////////////////////
// DockCoordinator: process-global (lazily-created singleton) registry of every
//  window's DockSpace + screen-rect provider, plus the two python-set commit
//  callbacks (transfer / tear-out). The SOURCE DockSpace consults it during a
//  titlebar/tab drag to compute a cross-window target (the OS delivers all
//  button-held pointer events to the source window, so cross-window targeting is
//  COMPUTED, never observed on the target window until release).
//
//  Reachable from DockSpace via DockCoordinator::instance() so the ui:: layer
//  never depends on ezapp — the pyext side constructs the glfw-backed rect
//  providers (which CAN touch glfw) and stores them as plain std::functions, so
//  the coordinator itself stays glfw-free.
//
//  COORDINATE-SPACE CONTRACT (v1; the ONE place cross-window mapping is derived)
//  --------------------------------------------------------------------------
//  Three spaces are in play:
//    * ui-root space  : ui::Event::miX/miY, every widget geometry, LocalToRoot,
//                       zoneHitTest. Origin at the window top-left, y-down. A
//                       window's ui::Context top widget is SetRect(0,0,Wlog,Hlog)
//                       so ui-root == window-logical with the window origin (0,0).
//    * screen space   : glfwGetWindowPos + glfwGetWindowSize units — logical
//                       SCREEN POINTS on macOS (content-scale independent), origin
//                       at the desktop top-left. This is what a rect provider (and
//                       a rect override) returns.
//    * framebuffer px : glfwGetFramebufferSize — logical*contentScale. NOT used.
//
//  ui-root <-> screen needs a per-window scale = (ui-root units / screen point).
//  In the shipped engine build _macosUseHIDPI is forced false (ctx_glfw_osx.mm),
//  so fillEventCursor writes miX=int(xoffset) with NO *contentScale step: ui-root
//  == screen points and the scale is 1.0 for every window. The mapping is then a
//  pure translation by the window origin:
//
//    screen_cursor  = source_rect.origin + (rx, ry)       // source ui-root -> screen
//    target_ui_root = screen_cursor - target_rect.origin  // screen -> target ui-root
//
//  If a future build re-enables HIDPI event scaling, divide the event delta by the
//  source window's contentScale before the add and multiply by the target window's
//  contentScale after the subtract — the ONLY place that would change is here.
////////////////////////////////////////////////////////////////////

struct DockCoordinator {

  static dockcoordinator_ptr_t instance();

  // --- registry (window_key strings are shared with the python DockManager) ---
  //  win_number_provider (optional): the native Cocoa window number of this
  //  window, for the point-ownership z-order leg. nullptr => no native leg for
  //  this window (non-mac, or unwired).
  void registerDock(
      const std::string& window_key,
      DockSpace* dock,
      dock_rect_provider_t provider,
      dock_win_number_provider_t win_number_provider = nullptr);
  void unregisterDock(const std::string& window_key);

  // Test seam: force a window's screen rect (offscreen/hidden windows report
  // degenerate positions). Override WINS over the provider while set.
  void setWindowRectOverride(const std::string& window_key, int x, int y, int w, int h);
  void clearWindowRectOverride(const std::string& window_key);

  void setTransferCallback(dock_transfer_cb_t cb) { _transfer_cb = std::move(cb); }
  void setTearOutCallback(dock_tearout_cb_t cb) { _tearout_cb = std::move(cb); }
  void setTransferablePredicate(dock_transferable_pred_t p) { _transferable_pred = std::move(p); }

  // Drag cursor seam (F2a-lite). Installed once from the glfw side; DockSpace drives
  //  it from the drag lifecycle. No-op when unset (offscreen gates).
  void setDragCursorFn(dock_cursor_fn_t fn) { _cursor_fn = std::move(fn); }
  void setDragCursor(DragCursor kind) { if (_cursor_fn) _cursor_fn(int(kind)); }

  // Per-drag-session reset of the _traceResolve dedup so EVERY drag logs its first
  //  classification (else a later drag with an identical classification prints
  //  nothing — the DOCKTRACE tooling gap). Called at beginPanelDrag.
  void resetTraceDedup() { _traced_once = false; _last_trace_detail.clear(); }
  // BUG-B point-ownership. Override WINS over the native leg (test seam; a gate
  //  simulates another app's window occluding ours). fn(screen_x, screen_y) ->
  //  window_key or "" (not ours). Unset => native leg / rect-only.
  void setPointOwnershipOverride(dock_point_owner_fn_t fn) { _point_owner_override = std::move(fn); }
  // Native leg: the Cocoa window number of the topmost OS window at a screen
  //  point (set once from the glfw/ezapp side so the ui:: layer stays glfw-free).
  void setTopmostWindowNumberResolver(dock_topmost_number_fn_t fn) { _topmost_number_fn = std::move(fn); }

  // The source DockSpace calls this from updatePanelDrag.
  DockDragResolve resolveDrag(DockSpace* source, int rx, int ry);

  std::string keyForDock(DockSpace* dock) const;

  void invokeTransfer(
      const std::string& panel_id,
      const std::string& src_key,
      const std::string& dst_key,
      const std::string& target_panel_id,
      const std::string& zone);
  void invokeTearOut(const std::string& panel_id, const std::string& src_key, int screen_x, int screen_y);

  struct Entry {
    DockSpace* _dock = nullptr;
    dock_rect_provider_t _provider;
    dock_win_number_provider_t _win_number_provider; // native z-order leg (mac); may be null
    bool _has_override = false;
    Rect _override = Rect(0, 0, 0, 0);
  };

  // Resolve a window's screen rect. Returns false (degrade) when unavailable.
  bool _resolveRect(const Entry& e, Rect& out) const;
  bool _resolveRect(const std::string& window_key, Rect& out) const;

  // true if ANY window has a rect override set — signals synthetic (offscreen
  //  gate) test mode, in which the native ownership leg (which queries REAL OS
  //  windows) must NOT run, else it would contradict the synthetic rects.
  bool _anyRectOverride() const;
  // Which of OUR windows (key) owns the topmost OS window at a screen point.
  //  nullopt => ownership UNAVAILABLE (degrade to rect-only). "" => not ours
  //  (tear-out). key => that window is the actually-topmost one there.
  std::optional<std::string> _resolveOwner(int sx, int sy) const;

  // ORKID_DOCK_TRACE=1 diagnostic (owner live-test): one log line per resolveDrag
  //  classification CHANGE (kind + foreign key + reason + the window rects used).
  //  The owner's live test is the first exercise of the glfw rect providers on
  //  VISIBLE windows — if targeting is off, this trace is the diagnostic.
  void _traceResolve(const DockDragResolve& res, const std::string& src_key, const std::string& reason);

  std::map<std::string, Entry> _entries; // window_key -> entry
  std::vector<std::string> _order;       // registration order (newest last)
  dock_transfer_cb_t _transfer_cb;
  dock_tearout_cb_t _tearout_cb;
  dock_transferable_pred_t _transferable_pred; // W5 pinning; unset => all transferable
  dock_point_owner_fn_t _point_owner_override; // BUG-B test seam; wins over native
  dock_topmost_number_fn_t _topmost_number_fn; // BUG-B native leg (mac); unset => none
  dock_cursor_fn_t _cursor_fn;                 // F2a-lite drag cursor seam (glfw-installed)
  bool _degrade_logged = false;
  int _trace_enabled = -1;               // -1 = unread, 0/1 cached from ORKID_DOCK_TRACE
  bool _traced_once = false;
  std::string _last_trace_detail;        // dedup key: only log on classification change
};

} // namespace ork::ui
