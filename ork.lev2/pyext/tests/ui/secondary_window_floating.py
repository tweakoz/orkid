#!/usr/bin/env ork.python
###############################################################################
# secondary_window_floating — committed gate for the FLOATING (always-on-top) secondary window
# option + the WINDOW-PER-FIELD expression-editor registry (owner order 2026-07-19: "expression
# secondary window needs to float on top" + "can we have multiple expression editors open
# simultaneously?").
#
# HEADLESS + NO WINDOWS by construction: this gate exercises the PYTHON PLUMBING + the engine-free
# registry/placement logic. It never creates a real OS secondary window (that would pop a window
# and needs a live GPU/display), so it is safe for CI. What it proves:
#
#   FLOATING (the C++ / pyext plumbing):
#     (1) EzSecondaryWinConfig.floating round-trips (default False, settable True) — the field the
#         `floating=` createSecondaryWindow kwarg sets.
#     (2) The created-window bindings are PRESENT: EzSecondaryWin exposes a read/write `floating`
#         property (backed by glfwGetWindowAttrib/glfwSetWindowAttrib post-create) and a
#         `focusWindow()` method.
#     (3) ExprEditorWindow OPTS IN: it requests floating=True from createSecondaryWindow (spied at
#         the API boundary — no window built).
#   The LIVE-window attrib round-trip + the actual visual float above the main window are
#   WINDOWED, hence OWNER-VERIFY (see pyext/tests/ui/secondary_window.py --aot).
#
#   MULTI-WINDOW REGISTRY (ExprEditorRegistry, engine-free with fake windows):
#     (4) two fields -> two windows at DISTINCT, viewport-clear placements (real cascade placement).
#     (5) re-opening a field with a live window FOCUSES it (no third window; count stays 2).
#     (6) different NODE, same field name -> a concurrent window (distinct registry key).
#     (7) APPLY isolation: applying in one window leaves another window's buffer/dirty untouched.
#     (8) CLOSE isolation + no leaks: closing one window drops ONLY its entry; the other stays live.
#     (9) editor teardown (closeAll) tears every window down -> zero leaked windows.
#
# ork.python only (orkengine.core first). Verdict line: SECONDARY_WINDOW_FLOATING_RESULT=PASS|FAIL.
###############################################################################

import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys

import orkengine.core  # noqa: F401  (law: core before any lev2 / ork.ui import)
from orkengine import lev2

from ork.ui.expr_detail_editor import (
    ExprEditorController, ExprEditorWindow, ExprEditorRegistry)
from ork.ui.expr_window_placement import compute_expr_window_rect, CASCADE_STEP

_FAILS = []


def _check(name, ok):
    if not ok:
        _FAILS.append(name)
    print("  %s : %s" % ("ok  " if ok else "FAIL", name), flush=True)


def _intersects(a, b):
    ax, ay, aw, ah = a; bx, by, bw, bh = b
    return not (ax + aw <= bx or bx + bw <= ax or ay + ah <= by or by + bh <= ay)


###############################################################################
# (1)(2) FLOATING plumbing: config round-trip + created-window bindings present
###############################################################################
def _sec_floating_plumbing():
    print("\n[1] EzSecondaryWinConfig.floating round-trip", flush=True)
    cfg = lev2.EzSecondaryWinConfig()
    _check("config.floating defaults False", cfg.floating is False)
    cfg.floating = True
    _check("config.floating round-trips True (the floating= kwarg field)", cfg.floating is True)
    cfg.floating = False
    _check("config.floating settable back to False", cfg.floating is False)
    # the popup convenience factory is floating by design (tool palette)
    pop = lev2.EzSecondaryWinConfig.popup(0, 0, 200, 200)
    _check("config popup() is floating", pop.floating is True)

    print("\n[2] created-window bindings present (floating property + focusWindow)", flush=True)
    _check("EzSecondaryWin.floating property bound (read/write, glfwSetWindowAttrib post-create)",
           isinstance(getattr(lev2.EzSecondaryWin, "floating", None), property))
    _check("EzSecondaryWin.focusWindow() method bound",
           callable(getattr(lev2.EzSecondaryWin, "focusWindow", None)))


###############################################################################
# (3) ExprEditorWindow opts in: requests floating=True from createSecondaryWindow
###############################################################################
class _StopInit(Exception):
    pass


class _FloatSpy:
    """A fake ezapp whose createSecondaryWindow records the kwargs then short-circuits the rest of
    ExprEditorWindow.__init__ (we only need to prove the floating opt-in — no UI/GPU built)."""

    def __init__(self):
        self.kwargs = None

    def createSecondaryWindow(self, **kwargs):
        self.kwargs = kwargs
        raise _StopInit()


def _sec_expr_optin():
    print("\n[3] ExprEditorWindow requests floating=True", flush=True)
    spy = _FloatSpy()
    try:
        ExprEditorWindow(spy, None, "force_x", "particles.force", "a*2",
                         (100, 100, 400, 500), on_apply=lambda s: None)
    except _StopInit:
        pass
    _check("createSecondaryWindow called with floating=True",
           spy.kwargs is not None and spy.kwargs.get("floating") is True)
    _check("createSecondaryWindow still decorated + resizable (a real editor window)",
           spy.kwargs.get("decorated") is True and spy.kwargs.get("resizable") is True)


###############################################################################
# (4)-(9) ExprEditorRegistry: window-per-field, focus-not-duplicate, isolation, teardown
###############################################################################
class _FakeExprWindow:
    """Minimal window standing in for a live ExprEditorWindow: carries its OWN controller (per-
    window apply/dirty state), counts focus, and drives its on_closed on close (so the registry
    drops the entry — the no-leak contract)."""

    def __init__(self, rect, on_closed, source):
        self.rect = rect
        self._on_closed = on_closed
        self.controller = ExprEditorController("f", "ctx", source, on_apply=lambda s: None)
        self.focus_count = 0
        self.closed = False

    def focusWindow(self):
        self.focus_count += 1

    def closeWindow(self):
        if self.closed:
            return
        self.closed = True
        if self._on_closed is not None:
            self._on_closed()


def _sec_registry():
    print("\n[4] ExprEditorRegistry multi-window lifecycle", flush=True)

    MAIN = (100, 100, 1200, 800)
    VP = (700, 100, 600, 800)          # viewport on the right half
    SCREEN = (0, 0, 2560, 1440)

    def place(count):
        rect, _s = compute_expr_window_rect(MAIN, VP, SCREEN, cascade_index=count)
        return rect

    built = []

    def build_for(source):
        def _b(rect, on_closed):
            w = _FakeExprWindow(rect, on_closed, source)
            built.append(w)
            return w
        return _b

    reg = ExprEditorRegistry(place=place)

    # (4) two fields on the same node -> two windows, distinct + viewport-clear placements
    reg.open("nodeA", "fx", build_for("ax"))
    reg.open("nodeA", "fy", build_for("ay"))
    w0, w1 = built[0], built[1]
    _check("two fields -> two windows (registry count 2)", reg.count == 2 and len(built) == 2)
    _check("the two placements DIFFER (cascade stagger)", w0.rect != w1.rect)
    _check("both placements clear the viewport",
           not _intersects(w0.rect, VP) and not _intersects(w1.rect, VP))

    # (5) re-open field fx (already live) -> focus, NO third window
    reg.open("nodeA", "fx", build_for("SHOULD-NOT-BUILD"))
    _check("re-open live field focuses (no duplicate; count still 2, 2 built)",
           reg.count == 2 and len(built) == 2)
    _check("focus call recorded on the existing window", w0.focus_count == 1)

    # (6) different NODE, same field name -> a concurrent window (distinct key)
    reg.open("nodeB", "fx", build_for("bx"))
    w2 = built[2]
    _check("different node + same field name -> concurrent window (count 3)",
           reg.count == 3 and len(built) == 3 and w2 is not w0)

    # (7) APPLY isolation: applying in window0 leaves window1 buffer/dirty untouched
    w0.controller.setBuffer("ax2")
    ok = w0.controller.apply()
    _check("apply in window0 succeeds + becomes its applied baseline",
           ok and w0.controller.applied == "ax2" and not w0.controller.dirty)
    _check("window1 buffer/dirty UNTOUCHED by window0 apply",
           w1.controller.buffer == "ay" and not w1.controller.dirty)

    # (8) CLOSE isolation + no leaks: closing window0 drops ONLY its entry
    reg.close("nodeA", "fx")
    _check("close window0 -> it is closed, registry drops only its entry (count 2)",
           w0.closed and reg.count == 2 and not w1.closed and not w2.closed)
    _check("closed key gone; surviving keys intact",
           ("nodeA", "fx") not in reg.keys()
           and ("nodeA", "fy") in reg.keys() and ("nodeB", "fx") in reg.keys())

    # (9) editor teardown: closeAll tears every remaining window down -> zero leaks
    reg.closeAll()
    _check("closeAll -> zero windows left (no leaks)", reg.count == 0)
    _check("every window is closed after teardown",
           w0.closed and w1.closed and w2.closed)


###############################################################################
# (10) placement cascade: staggered but still viewport-clear + on-screen
###############################################################################
def _sec_cascade():
    print("\n[10] placement cascade stagger", flush=True)
    MAIN = (100, 100, 1200, 800)
    VP = (700, 100, 600, 800)
    SCREEN = (0, 0, 2560, 1440)

    r0, s0 = compute_expr_window_rect(MAIN, VP, SCREEN, cascade_index=0)
    r1, s1 = compute_expr_window_rect(MAIN, VP, SCREEN, cascade_index=1)
    r2, s2 = compute_expr_window_rect(MAIN, VP, SCREEN, cascade_index=2)
    _check("cascade_index=0 unchanged base placement", s0 == "outside")
    _check("cascade shifts by a title-bar step (r1 offset from r0)",
           r1[0] == r0[0] + CASCADE_STEP and r1[1] == r0[1] + CASCADE_STEP)
    _check("successive cascades keep shifting (r2 != r1 != r0)", r2 != r1 and r1 != r0)
    for i, r in enumerate((r0, r1, r2)):
        _check("cascade[%d] stays viewport-clear" % i, not _intersects(r, VP))
        _check("cascade[%d] stays on-screen" % i,
               r[0] >= SCREEN[0] and r[1] >= SCREEN[1]
               and r[0] + r[2] <= SCREEN[0] + SCREEN[2]
               and r[1] + r[3] <= SCREEN[1] + SCREEN[3])


###############################################################################
def main():
    try:
        _sec_floating_plumbing()
        _sec_expr_optin()
        _sec_registry()
        _sec_cascade()
    except Exception:
        import traceback; traceback.print_exc()
        _FAILS.append("uncaught-exception")
    result = "PASS" if not _FAILS else "FAIL"
    print("\nSECONDARY_WINDOW_FLOATING_RESULT=%s" % result, flush=True)
    if _FAILS:
        print("FAILED: %s" % ", ".join(_FAILS), flush=True)
    sys.exit(0 if not _FAILS else 1)


if __name__ == "__main__":
    main()
