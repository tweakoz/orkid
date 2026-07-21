################################################################################
# ork/ui/dock_manager.py — python-side cross-window DockPanel transfer.
#
#   DockManager orchestrates moving a logical panel between the DockSpaces of
#   different OS windows (a "main" window + N secondary windows). Transfer v1 is
#   FACTORY-RECREATE at commit (owner-ratified): live widget migration across
#   ui::Contexts is FORBIDDEN because GPU seams are one-shot Context-bound
#   (RtGroup._parentTarget, PrimCanvas gpuInit guard). A transfer therefore:
#     1. snapshots the source panel's carry-state (save_state -> dict),
#     2. DockSpace.removePanel() on the source dock (releases the source
#        panel+content; python refcount then rules its lifetime),
#     3. re-runs the panel's registered FACTORY against the destination dock to
#        build FRESH content+panel there,
#     4. moveChild()'s the fresh panel to the requested zone vs a target, and
#     5. restore_state() re-applies the snapshot onto the fresh content.
#
#   Division of labor mirrors the persistence ruling (dock_layout.py): the C++
#   ui:: side exposes only primitives (addPanel / removePanel / moveChild); the
#   transfer POLICY (state carry, factory recreate, return-on-close) lives here.
#
# ---------------------------------------------------------------------------
# Factory contract
# ---------------------------------------------------------------------------
#   factory(dock, window) -> DockPanel
#     Builds the panel's content and hosts it in 'dock' (typically by calling
#     dock.addPanel(uiclass=..., args=..., title=..., closeable=...)), and
#     RETURNS the resulting ui.DockPanel. 'window' is the object the destination
#     dock was attached with (an OrkEzApp for "main", an EzSecondaryWin for a
#     secondary) so a factory needing per-window GPU wiring can reach it. The
#     manager stamps panel.name = panel_id after the factory returns, so the
#     DockPanel identity (== dock_layout.py's key) is always the panel_id
#     regardless of the display title the factory chose.
#
#   save_state(panel) -> dict        (optional; None => no carry-state)
#   restore_state(panel, dict)       (optional; applied on the FRESH panel)
#     panel is the ui.DockPanel; panel.child is its content widget.
#
# ---------------------------------------------------------------------------
# Return-on-close (documented ordering: EzSecondaryWin.onClosed)
# ---------------------------------------------------------------------------
#   Closing an attached secondary window (via win.requestClose(), the OS "X", or
#   app teardown) transfers ALL of that window's still-hosted, factory-known
#   panels back to the MAIN dock (carry-state preserved), then fires the optional
#   per-window close observer. This is wired through win.onClosed, which the C++
#   side fires during PHASE 1 of EzSecondaryWin close — while the app + main
#   window + the closing window's own widget tree are ALL still alive (the GPU
#   context is not torn down until phase 2, and the widget tree survives until the
#   window object itself is destructed). onClosed is thus the point where the
#   factory may run against the main dock and removePanel may run against the
#   secondary dock. The C++ side fires onClosed EXACTLY once (idempotence guard),
#   so the return runs once. onClosed is preferred over requestClose-interception
#   because it ALSO handles an OS-initiated close (window "X") uniformly.
#
#   Safety note: win.onClosed is a std::function capturing a python callable, and
#   the run loop's _cleanupClosedSecondaryWindows() destroys the window with the
#   GIL RELEASED. The pyext callback setters wrap the callable in gil_safe_pyobj
#   (custom deleter reacquires the GIL before Py_DECREF), so that destruction is
#   safe. Returned panels land in the main dock's default leaf (zone is not
#   preserved across a return — v1).
################################################################################

from orkengine.core import CrcStringProxy

################################################################################
# W4 cross-window drag choreography (C++ ui::DockCoordinator)
# ---------------------------------------------------------------------------
#   The C++ DockCoordinator owns the window registry + the cross-window drag
#   hit-test (screen-cursor computation, foreign-target resolve, tear-out). Two
#   commit callbacks flow back into THIS manager:
#
#     transfer(panel_id, src_key, dst_key, target_panel_id, zone)
#       -> self.transfer(...)  — the SAME factory-recreate used everywhere else.
#          Fires from the C++ deferred-mutation queue (post-dispatch), on the
#          UPDATE thread, so removePanel/moveChild apply immediately and the
#          remove-before-rebuild ordering holds (dock_space.cpp endPanelDrag).
#
#     tear_out(panel_id, src_key, screen_x, screen_y)
#       -> record a pending request only. createSecondaryWindow is MAIN-thread
#          (GLFW/Cocoa) work, but the callback fires on the update thread — so the
#          app must call pump_tearouts(app) from a render-thread hook (onGpuPostFrame)
#          to actually build the window + transfer the panel into it.
#
#   attach() registers each window's DockSpace with the coordinator (window_key
#   strings shared); close/detach unregisters. The coordinator is a process-global
#   singleton — v1 assumes ONE DockManager per process (its callbacks are global).
################################################################################

class DockManagerError(Exception):
  """Raised for every self-defend failure: unknown panel_id, unregistered
  window, missing factory, absent source panel. Never a silent no-op."""
  pass

################################################################################

class _FactoryReg:
  def __init__(self, panel_id, title, factory, save_state, restore_state, closeable):
    self.panel_id      = panel_id
    self.title         = title
    self.factory       = factory
    self.save_state    = save_state
    self.restore_state = restore_state
    self.closeable     = closeable

class _WinReg:
  def __init__(self, key, window, dock, is_main):
    self.key      = key
    self.window   = window
    self.dock     = dock
    self.is_main  = is_main
    self.on_closed = None   # optional user close-observer (called after return-on-close)

################################################################################

class DockManager:

  def __init__(self):
    self._factories = {}     # panel_id  -> _FactoryReg
    self._windows   = {}     # window_key -> _WinReg
    self._main_key  = None   # key of the window with no onClosed (the OrkEzApp)
    self._tokens    = CrcStringProxy()

    # --- W4 cross-window drag wiring ---
    from orkengine import lev2
    self._coord            = lev2.ui.DockCoordinator.instance()
    self._pending_tearouts = []      # (panel_id, src_key, sx, sy) queued off the update thread
    self._tearout_builder  = None    # fn(app, panel_id, src_key, sx, sy) -> (window_key, win, dock)
    self._tearout_size     = (400, 300)
    self._tearout_counter  = 0
    self._tearout_on_closed = None   # optional user close-observer for torn-out windows
    self._coord.setTransferCallback(self._on_coord_transfer)
    self._coord.setTearOutCallback(self._on_coord_tearout)
    # W5 pinning: a panel is transferable IFF it has a registered factory. The C++
    # coordinator consults this every resolveDrag and classifies a NON-transferable
    # (pinned) panel LOCAL-only — no foreign hint, no tear-out, in-window drag
    # unchanged. Panels with a one-shot Context-bound GPU seam (a viewport, a
    # node-editor canvas) are pinned simply by never registering a factory for them.
    self._coord.setTransferablePredicate(self._is_transferable)

  ##############################################################################
  # registration
  ##############################################################################

  def register_factory(self, panel_id, title, factory,
                       save_state=None, restore_state=None, closeable=True):
    if panel_id in self._factories:
      raise DockManagerError(f"register_factory: panel_id already registered: {panel_id!r}")
    self._factories[panel_id] = _FactoryReg(
        panel_id, title, factory, save_state, restore_state, closeable)

  def attach(self, window_key, app_or_win, dock, on_closed=None):
    """Register a window's DockSpace under 'window_key'. The window with NO
    onClosed attribute (the OrkEzApp) is recorded as the main / return-on-close
    target; a window WITH onClosed (an EzSecondaryWin) gets its onClosed wired to
    return-on-close. 'on_closed' (optional) is a user close-observer invoked once
    AFTER the return-on-close transfers complete (lets a caller/gate count
    closes, since the manager owns win.onClosed)."""
    is_main = not hasattr(app_or_win, "onClosed")
    reg = _WinReg(window_key, app_or_win, dock, is_main)
    reg.on_closed = on_closed
    self._windows[window_key] = reg
    # register the window's DockSpace + a glfw-backed screen-rect provider with the
    # C++ coordinator (window_key shared) so a drag out of any window can target it.
    self._coord.registerDock(window_key, dock, app_or_win)
    if is_main:
      self._main_key = window_key
    else:
      app_or_win.onClosed = lambda k=window_key: self._on_window_closed(k)

  ##############################################################################
  # lookup helpers (self-defending)
  ##############################################################################

  def _winreg(self, window_key):
    reg = self._windows.get(window_key)
    if reg is None:
      raise DockManagerError(f"unregistered window key: {window_key!r} "
                             f"(known: {sorted(self._windows.keys())})")
    return reg

  def _factory(self, panel_id):
    reg = self._factories.get(panel_id)
    if reg is None:
      raise DockManagerError(f"unknown / unregistered panel_id: {panel_id!r} "
                             f"(known: {sorted(self._factories.keys())})")
    return reg

  def _is_transferable(self, panel_id):
    """W5 pinning predicate (set on the C++ coordinator). Transferable IFF a factory
    is registered — a factory is what lets the panel be rebuilt in a destination dock,
    so it is exactly the set of panels that CAN cross a window boundary."""
    return panel_id in self._factories

  @staticmethod
  def _find_panel(dock, panel_id):
    for p in dock.allPanels():
      if p.name == panel_id:
        return p
    return None

  def _zone_token(self, zone):
    z = zone.upper() if isinstance(zone, str) else zone
    if z not in ("LEFT", "RIGHT", "TOP", "BOTTOM", "CENTER"):
      raise DockManagerError(f"invalid dock zone: {zone!r}")
    return getattr(self._tokens, z)

  ##############################################################################
  # the transfer
  ##############################################################################

  def transfer(self, panel_id, src_key, dst_key, target_panel_id=None, zone="CENTER"):
    """Factory-recreate 'panel_id' from window 'src_key' into window 'dst_key'.
    Must be called OUTSIDE ui event dispatch (from onUpdate, a post-dispatch
    action, or onClosed): removePanel/moveChild apply immediately there, so the
    remove-then-rebuild ordering is honored (during dispatch those defer while
    the factory's addPanel would apply immediately — an inversion). Works
    main->sec, sec->main, sec->sec."""
    reg     = self._factory(panel_id)
    src     = self._winreg(src_key)
    dst     = self._winreg(dst_key)

    panel = self._find_panel(src.dock, panel_id)
    if panel is None:
      raise DockManagerError(
          f"transfer: panel {panel_id!r} is not hosted in source window {src_key!r}")

    # 1. snapshot carry-state off the source (live) panel
    state = reg.save_state(panel) if reg.save_state else None

    # 2. release the source panel + content (DockSpace holds no ref afterward)
    src.dock.removePanel(panel)

    # 3. factory-build FRESH content + panel in the destination dock
    new_panel = reg.factory(dst.dock, dst.window)
    if new_panel is None:
      raise DockManagerError(
          f"transfer: factory for {panel_id!r} returned no panel")
    new_panel.name = panel_id   # stamp stable identity (dock_layout.py's key)

    # 4. place it relative to a target (else the factory's default placement stands)
    if target_panel_id is not None:
      target = self._find_panel(dst.dock, target_panel_id)
      if target is None:
        raise DockManagerError(
            f"transfer: target panel {target_panel_id!r} not in dest window {dst_key!r}")
      dst.dock.moveChild(panel=new_panel, to=target, zone=self._zone_token(zone))

    dst.dock.updateLayout()

    # 5. re-apply carry-state onto the fresh content
    if reg.restore_state and state is not None:
      reg.restore_state(new_panel, state)

    # dirty the destination window so a dirty-flag secondary re-renders the arrival
    if not dst.is_main and hasattr(dst.window, "markDirty"):
      dst.window.markDirty()

    return new_panel

  ##############################################################################
  # return-on-close
  ##############################################################################

  def _on_window_closed(self, window_key):
    # Fires once (C++ onClosed idempotence). Transfer every factory-known panel
    # still hosted in the closing window back to the main dock, then drop the
    # window from the registry and notify the optional user observer.
    reg = self._windows.get(window_key)
    if reg is None:
      return   # already handled (defensive)
    if self._main_key is None:
      raise DockManagerError(
          "return-on-close: no main window attached to receive returned panels")

    returning = [p.name for p in reg.dock.allPanels() if p.name in self._factories]
    for pid in returning:
      self.transfer(pid, window_key, self._main_key)

    user_cb = reg.on_closed
    del self._windows[window_key]
    self._coord.unregisterDock(window_key)
    if user_cb:
      user_cb()

  ##############################################################################
  # W4 cross-window drag: coordinator commit callbacks + tear-out
  ##############################################################################

  def _on_coord_transfer(self, panel_id, src_key, dst_key, target_panel_id, zone):
    """C++ transfer-commit callback (fires post-dispatch on the update thread).
    Routes to the same factory-recreate transfer used everywhere else."""
    # belt+braces (W5): the pinning predicate should already have kept a factory-less
    # panel LOCAL, but if a commit still arrives for one (predicate raced or unset),
    # log ONE loud line and no-op — NEVER raise back through the pyext callback boundary.
    if panel_id not in self._factories:
      print(f"[dockmanager] DECLINED cross-window transfer of {panel_id!r}: no registered "
            f"factory (pinned/unknown) — no-op", flush=True)
      return
    target = target_panel_id if target_panel_id else None
    self.transfer(panel_id, src_key, dst_key, target_panel_id=target, zone=zone or "CENTER")

  def _on_coord_tearout(self, panel_id, src_key, screen_x, screen_y):
    """C++ tear-out-commit callback (fires post-dispatch on the update thread).
    Records a request ONLY — window creation is main-thread work done in
    pump_tearouts()."""
    # belt+braces (W5): same graceful decline as transfer — a pinned/factory-less panel
    # can never be torn out; log once and no-op instead of queuing an unbuildable request.
    if panel_id not in self._factories:
      print(f"[dockmanager] DECLINED tear-out of {panel_id!r}: no registered factory "
            f"(pinned/unknown) — no-op", flush=True)
      return
    self._pending_tearouts.append((panel_id, src_key, int(screen_x), int(screen_y)))

  def set_tearout_builder(self, fn):
    """fn(app, panel_id, src_key, screen_x, screen_y) -> (window_key, win, dock).
    Builds a fresh secondary window hosting an EMPTY DockSpace (the manager then
    attaches it + transfers the panel in). None => use the default builder."""
    self._tearout_builder = fn

  def set_tearout_size(self, width, height):
    self._tearout_size = (int(width), int(height))

  def set_tearout_close_observer(self, fn):
    """Optional observer invoked once (after return-on-close completes) when a
    torn-out window closes — lets a caller/gate count tear-out-window closes."""
    self._tearout_on_closed = fn

  def pump_tearouts(self, app):
    """Process tear-out requests recorded since the last pump. MUST be called on
    the render/main thread (e.g. from onGpuPostFrame): createSecondaryWindow is
    GLFW/Cocoa main-thread-only work. Returns the number of windows torn out."""
    if not self._pending_tearouts:
      return 0
    pending = self._pending_tearouts
    self._pending_tearouts = []
    for (panel_id, src_key, sx, sy) in pending:
      self._do_tearout(app, panel_id, src_key, sx, sy)
    return len(pending)

  def _new_tearout_key(self):
    self._tearout_counter += 1
    return f"tearout_{self._tearout_counter}"

  def _do_tearout(self, app, panel_id, src_key, sx, sy):
    builder = self._tearout_builder or self._default_tearout_builder
    result = builder(app, panel_id, src_key, sx, sy)
    if result is None:
      raise DockManagerError("tear-out builder returned no (window_key, win, dock)")
    window_key, win, dock = result
    self.attach(window_key, win, dock, on_closed=self._tearout_on_closed)  # register + wire return-on-close
    self.transfer(panel_id, src_key, window_key)   # factory-recreate the panel into the new dock

  def _default_tearout_builder(self, app, panel_id, src_key, sx, sy):
    from orkengine import lev2
    w, h = self._tearout_size
    key = self._new_tearout_key()
    win = app.createSecondaryWindow(width=w, height=h, x=int(sx), y=int(sy),
                                    title=key, decorated=True)
    uic = win.ui_context
    root = lev2.ui.LayoutGroup.create(key + "_lg")
    root.setRect(0, 0, w, h)
    uic.top = root
    root.margin = 0
    dock = root.makeChild(fill=True, margin=0, uiclass=lev2.ui.DockSpace, args=[key + "dock"]).widget
    dock.clear = False
    root.setRect(0, 0, w, h)
    dock.updateLayout()
    return key, win, dock
