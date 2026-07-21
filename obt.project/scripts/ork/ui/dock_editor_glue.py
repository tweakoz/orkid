################################################################################
# ork/ui/dock_editor_glue.py — shared cross-window-docking wiring for the desktop
# editors (terrainedit + dflowedit). Keeps each editor's W5 diff DECLARATIVE: the
# editor factors its non-viewport panel construction into factory functions and
# registers them here; the manager lifecycle (attach, tear-out window builder,
# per-frame pump, Shift+L-with-secondaries) all lives in this one place so the two
# editors share it byte-for-byte.
#
# Pinning (W5): panels WITHOUT a registered factory (a SceneGraphViewport, a
# node-editor PrimCanvas — both hold one-shot Context-bound GPU seams that cannot be
# rebuilt in a foreign window's context) are automatically PINNED by DockManager's
# transferable predicate: they classify LOCAL-only in a cross-window drag (no
# foreign hint, no tear-out) and behave exactly as before. Only the panels handed to
# register() here are transferable / tearable.
#
# Tear-out window contract (editor defaults): decorated + resizable, NOT floating /
# always-on-top, titled by the panel's registered title, sized to the source panel's
# current size clamped to >= 400x300.
#
# Shift+L with secondaries (documented ordering): FIRST close every secondary dock
# window (return-on-close transfers its factory-known panels back to main), THEN —
# once all secondaries have returned — reset the MAIN layout to default. The two
# phases are driven across frames by pump_reset() so the main reset never runs while
# a returned panel is still missing from the main dock (which load_layout refuses).
#
# W6 multi-window persistence (schema v2): the editor's persisted app-state doc grows
# an OPTIONAL 'windows' list — one entry per OPEN secondary DOCK window at save time:
#   {"window_key": str, "rect": {"x","y","w","h"} (optional), "layout": <save_layout form>}
# add_windows_to_state() appends it (ONLY when >=1 secondary is open, so a single-window
# session stays byte-identical to v1). queue_windows_from_state() parses it at boot and
# defers recreation to the render/main-thread pump() (createSecondaryWindow is GLFW/Cocoa
# main-thread work — the SAME dispatch seam as tear-out). Recreation reuses the tear-out
# window builder, transfers each member panel out of main (they boot in main via factories),
# then load_layout()s the window's saved tree onto the now-live panels. A member panel whose
# factory no longer exists is skipped with ONE loud line (it stays in main), never a boot raise.
# Geometry is clamped (>= _MIN_WIN_W x _MIN_WIN_H; insane/absent position -> default placement).
# A Shift+L reset drops the recreation queue AND closes every secondary, so the next save
# writes no 'windows' list -> the reset session boots single-window.
################################################################################

from orkengine import lev2
from ork.ui.dock_manager import DockManager
from ork.ui.dock_layout import save_layout, load_layout, panel_ids

_MIN_TEAROUT_W = 400
_MIN_TEAROUT_H = 300

# W6 recreation geometry sanity (deliverable 4). Monitors-changed staleness stance: CLAMP,
# don't chase displays — a persisted rect is honored as-is if sane, else default placement;
# we never re-query the monitor topology to relocate a window onto a now-present display.
_MIN_WIN_W   = 200
_MIN_WIN_H   = 150
_DEFAULT_WIN = (120, 120, 600, 420)   # x,y,w,h default placement (absent / insane rect)
_SANE_POS_LO = -16000
_SANE_POS_HI = 32000

################################################################################

class EditorDockGlue:

  def __init__(self, app, dock, app_name):
    self.app      = app
    self.dock     = dock
    self.app_name = app_name
    self.mgr      = DockManager()
    self.mgr.attach("main", app.ezapp, dock)
    self.mgr.set_tearout_builder(self._build_tearout_window)
    self._tearout_counter = 0
    self._reset_pending   = False   # Shift+L requested
    self._reset_closing   = False   # phase 1 (close secondaries) issued
    self._pending_recreate = []     # W6: window entries queued at boot for main-thread recreation

  ##############################################################################
  # factory registration (transferable == has a factory)
  ##############################################################################

  def register(self, panel_id, title, factory, save_state=None, restore_state=None, closeable=True):
    self.mgr.register_factory(panel_id, title, factory,
                              save_state=save_state, restore_state=restore_state,
                              closeable=closeable)

  ##############################################################################
  # per-frame pumps (call from a MAIN/GPU-thread hook — onGpuPostFrame)
  ##############################################################################

  def pump(self):
    """Realize any pending boot-time window recreations (W6) + tear-outs. MUST run on
    the main/GPU thread (createSecondaryWindow is GLFW/Cocoa main-thread work) — the
    editors call this from onGpuPostFrame, per the DockManager contract. Recreation drains
    FIRST so a persisted arrangement is whole before any live tear-out lands on top of it."""
    try:
      self._pump_recreate()
      self.mgr.pump_tearouts(self.app.ezapp)
    except Exception as e:
      print(f"[{self.app_name}] pump EXC: {e}", flush=True)

  ##############################################################################
  # Shift+L with secondaries (two-phase, driven across frames)
  ##############################################################################

  def request_reset(self):
    """Shift+L pressed (already gated on text-focus by the editor). Idempotent while a
    reset is in flight. Drops any un-realized W6 recreation queue so a reset session boots
    single-window (the persisted 'windows' list is then cleared on the next save)."""
    self._reset_pending = True
    self._pending_recreate = []

  def pump_reset(self, reset_fn):
    """Drive the two-phase Shift+L reset. Call every main/GPU-thread frame tick. Phase
    1: request-close every secondary (return-on-close carries its panels back to main).
    Phase 2: once no secondary remains registered, run reset_fn (the editor's default
    main-layout restore). No-op when no reset is pending."""
    if not self._reset_pending:
      return
    if not self._reset_closing:
      self._close_all_secondaries()
      self._reset_closing = True
    if self.secondary_count() == 0:
      self._reset_pending = False
      self._reset_closing = False
      reset_fn()

  def secondary_count(self):
    return sum(1 for r in self.mgr._windows.values() if not r.is_main)

  def _close_all_secondaries(self):
    for key in [k for k, r in self.mgr._windows.items() if not r.is_main]:
      reg = self.mgr._windows.get(key)
      if reg is None:
        continue
      try:
        if hasattr(reg.window, "requestClose"):
          reg.window.requestClose()   # -> EzSecondaryWin.onClosed -> return-on-close
      except Exception as e:
        print(f"[{self.app_name}] secondary close failed for {key!r}: {e}", flush=True)

  ##############################################################################
  # editor tear-out window builder
  ##############################################################################

  def _build_tearout_window(self, app, panel_id, src_key, sx, sy):
    reg   = self.mgr._factories.get(panel_id)
    title = reg.title if reg else panel_id
    w, h  = self._panel_size(src_key, panel_id)
    self._tearout_counter += 1
    key = f"{self.app_name}_tearout_{self._tearout_counter}"
    win, dock = self._make_window(app, key, title, w, h, sx, sy)
    return key, win, dock

  def _make_window(self, app, key, title, w, h, x, y):
    """Shared secondary-window construction (tear-out + W6 boot recreation): a decorated,
    resizable window hosting an EMPTY DockSpace. Returns (win, dock). MAIN-thread only."""
    win = app.createSecondaryWindow(width=w, height=h, x=int(x), y=int(y),
                                    title=title, decorated=True, resizable=True,
                                    floating=False)
    uic  = win.ui_context
    root = lev2.ui.LayoutGroup.create(key + "_lg")
    root.setRect(0, 0, w, h)
    uic.top     = root
    root.margin = 0
    dock = root.makeChild(fill=True, margin=0, uiclass=lev2.ui.DockSpace,
                          args=[key + "dock"]).widget
    dock.clear = False
    root.setRect(0, 0, w, h)
    dock.updateLayout()
    return win, dock

  def _panel_size(self, src_key, panel_id):
    """Source panel's current size, clamped to the sensible tear-out minimum."""
    try:
      reg = self.mgr._windows.get(src_key)
      if reg is not None:
        for p in reg.dock.allPanels():
          if p.name == panel_id:
            return (max(_MIN_TEAROUT_W, int(p.width)),
                    max(_MIN_TEAROUT_H, int(p.height)))
    except Exception:
      pass
    return _MIN_TEAROUT_W, _MIN_TEAROUT_H

  ##############################################################################
  # W6 multi-window persistence: capture (save side) + recreation (boot side)
  ##############################################################################

  def add_windows_to_state(self, doc):
    """SAVE side: append the OPEN secondary dock windows to the app-state 'doc' (schema v2).
    Byte-compat: when NO secondary is open the doc is returned UNCHANGED (no 'windows' key),
    so a single-window session persists exactly as v1. Entries are window_key-sorted for
    determinism. Called by the editor's _saveSession, on the SAME trigger the main layout
    persists on (session exit / explicit save)."""
    entries = []
    for key, reg in self.mgr._windows.items():
      if reg.is_main:
        continue
      entry = {"window_key": key, "layout": save_layout(reg.dock)}
      rect = self._window_rect(reg.window)
      if rect is not None:
        entry["rect"] = {"x": rect[0], "y": rect[1], "w": rect[2], "h": rect[3]}
      entries.append(entry)
    if entries:
      entries.sort(key=lambda e: e["window_key"])
      doc["windows"] = entries
    return doc

  @staticmethod
  def _window_rect(win):
    """(x,y,w,h) via screenRect, or None when the platform cannot report window position
    (offscreen / Wayland / closed) — the recreation then uses default placement."""
    try:
      r = win.screenRect()
    except Exception:
      r = None
    if r is None:
      return None
    return (int(r[0]), int(r[1]), int(r[2]), int(r[3]))

  def queue_windows_from_state(self, raw):
    """BOOT side: parse the 'windows' list out of a persisted app-state doc and QUEUE each
    entry for recreation. Recreation itself is deferred to pump() (render/main thread) because
    createSecondaryWindow is GLFW/Cocoa main-thread-only work. Tolerant: a malformed / v1 (no
    'windows') doc simply queues nothing -> single-window boot."""
    import json
    doc = raw
    if isinstance(raw, (str, bytes)):
      try:
        doc = json.loads(raw)
      except Exception as e:
        print(f"[{self.app_name}] windows-state parse failed ({e}); single-window boot", flush=True)
        return
    wins = doc.get("windows") if isinstance(doc, dict) else None
    if wins:
      self._pending_recreate.extend(wins)

  def _pump_recreate(self):
    """Drain the boot-time recreation queue on the render/main thread (called from pump())."""
    if not self._pending_recreate:
      return
    pending = self._pending_recreate
    self._pending_recreate = []
    for entry in pending:
      try:
        self._recreate_window(entry)
      except Exception as e:
        import traceback
        traceback.print_exc()
        print(f"[{self.app_name}] window recreate failed for "
              f"{entry.get('window_key')!r}: {e}", flush=True)

  def _recreate_window(self, entry):
    """Recreate ONE persisted secondary dock window: build the window at its (clamped)
    geometry via the shared tear-out builder, attach it (wires return-on-close), transfer each
    member panel out of MAIN (they boot in main via factories), then load_layout the saved tree
    onto the now-live panels. A member without a registered factory is skipped LOUDLY (stays in
    main) — never a boot raise."""
    key    = entry.get("window_key") or self._next_recreate_key()
    layout = entry.get("layout") or {}
    ids    = panel_ids(layout)
    x, y, w, h = self._recreate_rect(entry.get("rect"))
    title = self._recreate_title(ids)

    win, dock = self._make_window(self.app.ezapp, key, title, w, h, x, y)
    self.mgr.attach(key, win, dock, on_closed=None)

    for pid in ids:
      if pid in self.mgr._factories:
        try:
          self.mgr.transfer(pid, "main", key)
        except Exception as e:
          print(f"[{self.app_name}] recreate: transfer of {pid!r} -> {key!r} failed: {e}",
                flush=True)
      else:
        print(f"[{self.app_name}] recreate: panel {pid!r} has no registered factory — "
              f"left in main (window {key!r})", flush=True)

    try:
      load_layout(dock, layout)
      dock.updateLayout()
    except Exception as e:
      print(f"[{self.app_name}] recreate: load_layout for {key!r} skipped ({e})", flush=True)

    self._advance_counter_past(key)
    return win, dock

  def _recreate_rect(self, rect):
    """Clamp a persisted rect to sane bounds (deliverable 4): w/h >= minimum; absent or
    wildly out-of-range position -> default placement (do NOT chase displays)."""
    dx, dy, dw, dh = _DEFAULT_WIN
    x = rect.get("x") if isinstance(rect, dict) else None
    y = rect.get("y") if isinstance(rect, dict) else None
    w = rect.get("w") if isinstance(rect, dict) else None
    h = rect.get("h") if isinstance(rect, dict) else None
    w = max(_MIN_WIN_W, int(w)) if isinstance(w, (int, float)) else dw
    h = max(_MIN_WIN_H, int(h)) if isinstance(h, (int, float)) else dh
    if not isinstance(x, (int, float)) or not (_SANE_POS_LO <= x <= _SANE_POS_HI):
      x = dx
    if not isinstance(y, (int, float)) or not (_SANE_POS_LO <= y <= _SANE_POS_HI):
      y = dy
    return int(x), int(y), int(w), int(h)

  def _recreate_title(self, ids):
    for pid in ids:
      reg = self.mgr._factories.get(pid)
      if reg is not None:
        return reg.title
    return ids[0] if ids else "recreated"

  def _next_recreate_key(self):
    self._tearout_counter += 1
    return f"{self.app_name}_tearout_{self._tearout_counter}"

  def _advance_counter_past(self, key):
    """Keep future tear-out keys from colliding with a recreated {app}_tearout_N key."""
    prefix = f"{self.app_name}_tearout_"
    if key.startswith(prefix):
      try:
        n = int(key[len(prefix):])
        if n > self._tearout_counter:
          self._tearout_counter = n
      except ValueError:
        pass
