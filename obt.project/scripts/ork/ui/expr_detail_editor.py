################################################################################
# ork.ui.expr_detail_editor — the CodeView-backed EXPRESSION EDITOR hosted in a real OS
# SECONDARY WINDOW (E2.5 S7; owner refinement 2026-07-19: re-hosted from a cramped propsheet
# overlay into a resizable window placed clear of the viewport). A host opens it for an
# expression-typed row (editor.custom == "expr"): the row's SOURCE fills an editable CodeView
# with APPLY + CLOSE. One implementation serves every expr field (particles force / terrain
# expr_source / hypermesh selexpr).
#
# APPLY semantics: the window STAYS OPEN across applies so the owner iterates on an expression
# while watching the sim react — each apply validates + writes + rebakes through the host edit
# path; a loud, in-window error on an invalid/dishonest edit leaves the document untouched and
# the buffer preserved. CLOSE discards unapplied edits. A dirty marker ('*') rides the title.
#
# The engine-free ExprEditorController IS the tested contract (the S7 gate exercises apply /
# close / dirty headlessly); the ExprEditorWindow wraps it around a live secondary window
# (windowed look/size = owner-verify).
################################################################################

from orkengine.core import vec3, vec4
from orkengine import lev2
from ork.ui.code_editor import CodeView, CodeDocument

_ui = lev2.ui


################################################################################
# ExprEditorController + ExprEditorWindow — the SECONDARY-WINDOW expression editor
# (owner refinement 2026-07-19). One implementation serves every expr field (particles
# force / terrain expr_source / hypermesh selexpr). APPLY semantics: the window STAYS OPEN
# across applies so the owner iterates while watching the sim react. The engine-free
# controller (below) IS the tested contract; the window widget layer wraps it (owner-verify).
################################################################################


class ExprEditorController:
    """Engine-free APPLY/CLOSE/DIRTY state machine for the expression editor window. The widget
    layer feeds edits (setBuffer) and drives apply()/close(); the S7 gate exercises it headlessly.

      * APPLY = validate + write + rebake via on_apply(source) (raises LOUDLY on an invalid /
        dishonest edit — no write). On SUCCESS the buffer BECOMES the now-current applied source
        (dirty clears) and the window stays open (multiple applies per session). On FAILURE the
        error is recorded, the buffer is PRESERVED (fix + re-apply), the window stays open.
      * CLOSE discards unapplied edits — the document keeps the last APPLIED value.
      * DIRTY = the buffer differs from the last applied source (drives the title '*' marker)."""

    def __init__(self, field_label, context_name, initial_source, on_apply, on_close=None):
        self.field_label = field_label
        self.context_name = context_name
        self._applied = initial_source or ""     # last successfully-applied source
        self._buffer = self._applied             # current (possibly edited) buffer
        self._on_apply = on_apply                # (source) -> raises loudly on invalid (no write)
        self._on_close = on_close
        self.last_error = None
        self.closed = False

    @property
    def dirty(self):
        return self._buffer != self._applied

    @property
    def buffer(self):
        return self._buffer

    @property
    def applied(self):
        return self._applied

    def setBuffer(self, text):
        self._buffer = text or ""

    def title(self):
        """The window title with a dirty marker ('*') when the buffer differs from the applied
        source — e.g. 'force_x — particles.force *'."""
        return "%s — %s%s" % (self.field_label, self.context_name, " *" if self.dirty else "")

    def apply(self):
        """Validate + write + rebake the current buffer. Returns True on success (buffer becomes
        the applied source, dirty clears, window stays open), False on failure (error recorded,
        buffer preserved, window stays open)."""
        try:
            self._on_apply(self._buffer)
        except Exception as ex:
            self.last_error = "expression rejected: %s" % (ex,)
            return False
        self.last_error = None
        self._applied = self._buffer
        return True

    def close(self):
        """Dismiss — unapplied edits are DISCARDED (the document keeps the last applied value)."""
        if self.closed:
            return
        self.closed = True
        if self._on_close is not None:
            self._on_close()


class ExprEditorWindow:
    """The expression editor hosted in a real resizable OS SECONDARY WINDOW at an opening
    placement chosen to NOT cover the viewport (expr_window_placement). The window FLOATS above
    the main editor window (a tool palette). The CodeView fills the window; a title bar shows
    'field — context' with a dirty marker; Apply + Close buttons drive the controller (Apply keeps
    the window open across commits, Close dismisses + discards). The windowed look/size is
    owner-verify; the controller is gate-tested.

    Lifecycle: one window PER FIELD, tracked by the host's ExprEditorRegistry (keyed by
    (node, field)). Re-opening a field with a live window raises it (focusWindow) rather than
    duplicating; on_closed drops the registry entry — so no window leaks."""

    _TITLE_BG = vec4(0.16, 0.16, 0.22, 1.0)

    def __init__(self, ezapp, ctx, field_label, context_name, initial_source, rect,
                 on_apply, on_closed=None):
        self.ezapp = ezapp
        self._on_closed = on_closed
        self.controller = ExprEditorController(field_label, context_name, initial_source, on_apply)
        x, y, w, h = (int(v) for v in rect)
        # floating=True keeps the editor above the main editor window (a tool palette). NOTE:
        # GLFW_FLOATING is SYSTEM-WIDE always-on-top (not parent-relative) — it floats above every
        # window, the accepted idiom for tool palettes on GLFW's supported platforms.
        self.win = ezapp.createSecondaryWindow(
            width=w, height=h, x=x, y=y,
            title=self.controller.title(), decorated=True, resizable=True, floating=True)
        uic = self.win.ui_context
        root = _ui.LayoutGroup.create("exprwin_lg")
        root.setRect(0, 0, self.win.width, self.win.height)
        uic.top = root
        root.margin = 4
        vpack = root.makeChild(uiclass=_ui.VerticalPack, args=["exprwin_vpack"], fill=True).widget

        self.title_box = vpack.makeChild(
            uiclass=_ui.LabelBox, args=["exprwin_title", self._TITLE_BG, self.controller.title()])

        doc = CodeDocument.from_string(
            initial_source or "", provenance=("memory", "%s expr" % field_label),
            language="python", read_only=False)
        self.canvas = vpack.makeChild(uiclass=_ui.PrimCanvas, args=["exprwin_code"])
        self.canvas.bg_color = vec4(0.12, 0.12, 0.16, 1.0)
        self.canvas.draw_background = True
        self.code_view = CodeView(self.canvas, document=doc, font_id="i16")
        vpack.fill_widget = self.canvas

        actions = vpack.makeChild(uiclass=_ui.HorizontalPack, args=["exprwin_actions"])
        actions.uniform = True
        actions.margin = 2
        self.apply_btn = actions.makeChild(uiclass=_ui.Button, args=["Apply", vec3(0.3, 0.5, 0.3)])
        self.apply_btn.onPressed = lambda w: self.applyEdit()
        self.close_btn = actions.makeChild(uiclass=_ui.Button, args=["Close", vec3(0.5, 0.3, 0.3)])
        self.close_btn.onPressed = lambda w: self.closeWindow()

        # init the CodeView on the SECONDARY window's OWN gpu context (its render thread).
        self.win.onGpuInit = lambda gctx: self.code_view.gpuInit(gctx)
        self.win.onClosed = self._onWindowClosed

    # -- source access (parity with ExprDetailEditor; the live buffer) ----------

    @property
    def source(self):
        d = self.code_view.document
        return d.text if d is not None else self.controller.buffer

    def _syncTitle(self):
        self.controller.setBuffer(self.source)
        self.title_box.text = self.controller.title()

    # -- apply / close ----------------------------------------------------------

    def applyEdit(self):
        """Apply the current buffer (validate + write + rebake). On success the window stays open
        with the buffer now the applied source; on failure the loud error shows in the CodeView
        status and the window stays open with the buffer preserved."""
        self.controller.setBuffer(self.source)
        ok = self.controller.apply()
        if not ok:
            self.code_view._status = self.controller.last_error or "expression rejected"
        self.title_box.text = self.controller.title()
        return ok

    def focusWindow(self):
        """Raise + focus this (already-open) window — the registry calls this when a field that
        already has a live window is re-opened, so no duplicate window is created."""
        self.win.focusWindow()

    def closeWindow(self):
        """Close the window — unapplied edits are discarded (document keeps the applied value)."""
        self.controller.close()
        self.win.requestClose()

    def _onWindowClosed(self):
        # the OS window went away (X button or requestClose): ensure the controller is closed +
        # notify the host so it drops its per-field registry entry (no leaks).
        self.controller.close()
        if self._on_closed is not None:
            self._on_closed()


################################################################################
# ExprEditorRegistry — WINDOW-PER-FIELD registry (owner refinement 2026-07-19: multiple
# expression editors open simultaneously). Keyed by (node, field): opening a field that already
# has a live window FOCUSES/raises it (no duplicate); different fields (same or other nodes) open
# CONCURRENT windows, each with its OWN controller/apply/close/dirty state. Closing one window
# never affects the others; a window drops its registry entry on close (no leaks). closeAll()
# tears every open window down (editor teardown).
#
# Engine-free by construction: it takes a `place(open_count) -> rect` callable (the host's live
# viewport-avoiding + cascade placement) and, per open, a `build(rect, on_closed) -> window`
# factory. The window contract is minimal: `.focusWindow()` and `.closeWindow()`. So the whole
# lifecycle is headlessly testable with fakes.
#
# SEMANTICS (v1, documented): a window's last-applied baseline updates only on ITS OWN
# open/apply. An external change to the same field while a window is open leaves that window's
# dirty state as-is (stale-baseline accepted for v1).
################################################################################


class ExprEditorRegistry:
    """Tracks the live expression editor windows, one per (node, field). See module note above."""

    def __init__(self, place):
        self._place = place            # (open_count) -> opening rect (host: live placement + cascade)
        self._windows = {}             # (node_key, field_key) -> window

    @property
    def count(self):
        return len(self._windows)

    def keys(self):
        return list(self._windows.keys())

    def has(self, node_key, field_key):
        return (node_key, field_key) in self._windows

    def windowFor(self, node_key, field_key):
        return self._windows.get((node_key, field_key))

    def open(self, node_key, field_key, build):
        """Open (or focus) the window for (node_key, field_key). If a live window already exists it
        is raised via focusWindow() and returned (no duplicate). Otherwise `build(rect, on_closed)`
        constructs a new window at the cascaded placement; on close it drops its registry entry."""
        rk = (node_key, field_key)
        existing = self._windows.get(rk)
        if existing is not None:
            existing.focusWindow()
            return existing
        rect = self._place(len(self._windows))

        def _on_closed(_rk=rk):
            self._windows.pop(_rk, None)

        win = build(rect, _on_closed)
        self._windows[rk] = win
        return win

    def close(self, node_key, field_key):
        """Close a single window (if live). Its on_closed drops the registry entry."""
        win = self._windows.get((node_key, field_key))
        if win is not None:
            win.closeWindow()

    def closeAll(self):
        """Tear down EVERY open window (editor teardown) — no leaks."""
        for win in list(self._windows.values()):
            try:
                win.closeWindow()
            except Exception:
                pass
        self._windows.clear()
