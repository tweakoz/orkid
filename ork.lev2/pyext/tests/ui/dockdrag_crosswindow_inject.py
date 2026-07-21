#!/usr/bin/env ork.python
################################################################################
# W4 gate: cross-window DockPanel drag choreography + tear-out.
#
#  Proves the full cross-window drag: a titlebar drag whose cursor leaves the
#  SOURCE window is COMPUTED against a registry of every window's screen rect
#  (ui::DockCoordinator) and either (a) targets ANOTHER window's dock at a zone
#  (live foreign hint on the TARGET window) then transfers on release, or (b)
#  tears the panel out onto empty desktop into a NEW secondary window.
#
#  The OS delivers every button-held pointer event to the source window (implicit
#  capture) — GLFW streams source-window coords that go NEGATIVE / beyond-bounds
#  once the cursor leaves. So the drag is injected into the SOURCE window with END
#  coordinates whose source-local values map (via each window's KNOWN DISJOINT
#  rect override) beyond the source into the target rect — exactly how the OS
#  behaves. Drags are stepped FRAME-BY-FRAME (PUSH, in-source DRAG, foreign DRAG,
#  HOLD, RELEASE) so the mid-drag foreign hint renders on the target window and
#  can be captured before release (a single all-in-one drag would raise+drop the
#  hint inside one frame, never rendered).
#
#  Coordinate contract (see dock_coordinator.h): _macosUseHIDPI is false in the
#  shipped build, so ui-root == screen points, scale 1.0 — the mapping is a pure
#  translation by each window's screen origin, and the disjoint OVERRIDE rects are
#  given directly in that space.
#
#  Subprocess-per-mode, lockstep+offscreen (freerun=False), ork.testing
#  VERDICT-BEFORE-TEARDOWN. Stateful oracle (as W2+W3): the mover carries a
#  post-creation magenta that differs from its gray factory default, so magenta in
#  the destination proves restore ran; no magenta in the source proves it left.
#
#    --mode a : XFER main->sec1 LEFT; mid-drag foreign hint visible on sec1;
#               after release mover in sec1 + gone from main (double-run determinism).
#    --mode b : XFER sec1->main LEFT (reverse); same oracles + main foreign hint.
#    --mode c : XFER sec1->sec2 LEFT (three windows registered).
#    --mode d : TEAROUT main->empty desktop -> NEW secondary hosts the mover;
#               close it -> mover returns to main (closed_count==1, rc==0).
#    --mode e : CANCEL main->sec1 nozone (inside sec1 but off its dock) -> byte-equal
#               to baseline in BOTH windows, no hint/overlay left behind.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys, argparse, subprocess

ROOT = os.path.abspath(__file__)
for _ in range(5):
    ROOT = os.path.dirname(ROOT)
sys.path.insert(0, os.path.join(ROOT, "obt.project", "scripts"))

from orkengine.core import vec4, CrcStringProxy
from orkengine import lev2
from ork.testing import verdict, read_verdict, PASS, PASS_WITH_TEARDOWN_BUG
from ork.ui.dock_manager import DockManager
import ork.uitest as U

W, H = 400, 300

# Disjoint window screen rects (ui-root units == screen points, scale 1). A drag
# out of one window maps into another purely by these translations.
RECTS = {
    "main": (0, 0, W, H),
    "sec1": (W + 100, 0, W, H),          # (500,0,400,300)
    "sec2": (W + 100, H + 100, W, H),    # (500,400,400,300)
    "tearout1": (2 * (W + 100), 0, W, H),  # (1000,0,400,300) — where a torn-out window lands
}

FACTORY_DEFAULT = vec4(0.15, 0.15, 0.15, 1.0)   # a fresh (un-restored) mover box
STATE_COLOR     = vec4(0.85, 0.15, 0.80, 1.0)   # the carried post-creation state (magenta)
MAIN_ANCHOR     = vec4(0.15, 0.45, 0.55, 1.0)   # teal   (never magenta)
SEC_ANCHOR      = vec4(0.50, 0.45, 0.12, 1.0)   # olive  (never magenta)
SEC2_ANCHOR     = vec4(0.30, 0.20, 0.55, 1.0)   # purple (R<150 -> never magenta)

SETTLE       = 14    # frames after ready before first capture / first drag step
GAP          = 3     # frames between injected drag steps
POST         = 8     # frames to let a post-transfer render land before re-capture
POST_TEAROUT = 16    # tear-out needs the new window created (on_iter) + gpuInit + draw
CLOSE_WAIT   = 12    # frames to let requestClose -> onClosed -> return -> render land

SEC_MARGIN_E = 30    # mode e: inset sec1's dock so a drop in the window margin is a nozone

TMP = os.path.join(os.environ.get("TMPDIR", "/tmp"), "xwindrag")

MAG_PRESENT_MIN = 800    # magenta pixels proving the restored mover is rendered
MAG_ABSENT_MAX  = 8      # tolerance for "no magenta"
HINT_MIN        = 1500   # blue-increase pixels proving the foreign drop hint is drawn

# per-mode plan
CFG = {
    "a": dict(secs=["sec1"],          src="main", dst="sec1", target="sec_anchor"),
    "b": dict(secs=["sec1"],          src="sec1", dst="main", target="main_anchor"),
    "c": dict(secs=["sec1", "sec2"],  src="sec1", dst="sec2", target="sec2_anchor"),
    "d": dict(secs=[],                src="main", dst=None,   target=None),
    "e": dict(secs=["sec1"],          src="main", dst="sec1", target=None),
}


################################################################################
# factory + carry-state
################################################################################

def _box_factory(title, color, closeable=True):
    def f(dock, window):
        return dock.addPanel(uiclass=lev2.ui.Box, args=[title, color], title=title, closeable=closeable)
    return f

def save_box(panel):
    c = panel.child.color
    return {"color": (c.x, c.y, c.z, c.w)}

def restore_box(panel, state):
    panel.child.color = vec4(*state["color"])


################################################################################
# is_ready-polled capture of one FBI's main RTG -> RGB numpy array (W1 idiom).
################################################################################

class Capturer:
    def __init__(self):
        self._buf = None; self._fut = None
        self._inflight = False; self._armed = False
        self.result = None

    def arm(self):
        self._armed = True; self._inflight = False
        self._buf = None; self._fut = None; self.result = None

    @property
    def ready(self):
        return self.result is not None

    def tick(self, ctx):
        if not self._armed or self.result is not None:
            return
        if not self._inflight:
            self._buf = lev2.CaptureBuffer()
            self._fut = ctx.FBI.captureAsFormat(ctx.FBI.main_RTG.buffer(0), self._buf, "RGBA8")
            self._inflight = True
            return
        if self._fut is not None and not bool(self._fut.is_ready):
            return
        import numpy
        arr = numpy.array(self._buf, dtype=numpy.uint8).reshape(self._buf.height, self._buf.width, 4)
        self.result = arr[..., :3].copy()
        self._armed = False


def _save_png(arr, path):
    if arr is None:
        return
    from PIL import Image
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    Image.fromarray(arr).save(path)

def _count_magenta(arr):
    import numpy
    if arr is None:
        return -1
    R = arr[..., 0].astype(numpy.int16)
    G = arr[..., 1].astype(numpy.int16)
    B = arr[..., 2].astype(numpy.int16)
    mask = (R > 150) & (B > 120) & (G < 100)
    return int(mask.sum())

def _count_blue_increase(mid, base):
    # foreign drop hint = translucent blue quad -> B rises sharply where it lands.
    import numpy
    if mid is None or base is None:
        return -1
    Bm = mid[..., 2].astype(numpy.int16)
    Bb = base[..., 2].astype(numpy.int16)
    return int(((Bm - Bb) > 30).sum())

def _arr_equal(a, b):
    import numpy
    if a is None or b is None or a.shape != b.shape:
        return False, -1
    d = numpy.abs(a.astype(numpy.int16) - b.astype(numpy.int16))
    return int(d.max()) == 0, int((d.max(axis=2) > 0).sum())


################################################################################
# App — one instance per child process, parameterized by mode.
################################################################################

class App:
    def __init__(self, mode, tag):
        self.mode = mode
        self.tag  = tag
        self.cfg  = CFG[mode]
        self._tokens = CrcStringProxy()

        self.ezapp = lev2.OrkEzApp.create(self, width=W, height=H,
                                          freerun=False, target_ups=60.0, target_fps=60.0)
        self.ezapp.topWidget.enableUiDraw()
        lg = self.ezapp.topLayoutGroup
        lg.margin = 0
        self.main_dock = lg.makeChild(fill=True, margin=0, uiclass=lev2.ui.DockSpace, args=["maindock"]).widget
        self.main_dock.clear = False
        self._main_lg = lg

        self.mover_factory = _box_factory("Mover", FACTORY_DEFAULT, closeable=True)
        self.mgr = DockManager()
        self.mgr.register_factory("mover", "Mover", self.mover_factory,
                                  save_state=save_box, restore_state=restore_box, closeable=True)
        self.mgr.register_factory("main_anchor", "MainAnchor", _box_factory("MainAnchor", MAIN_ANCHOR))
        self.mgr.register_factory("sec_anchor",  "SecAnchor",  _box_factory("SecAnchor",  SEC_ANCHOR))
        self.mgr.register_factory("sec2_anchor", "Sec2Anchor", _box_factory("Sec2Anchor", SEC2_ANCHOR))
        self.mgr.set_tearout_builder(self._build_tearout_window)
        self.mgr.set_tearout_close_observer(self._tearout_closed)

        self.coord = lev2.ui.DockCoordinator.instance()
        self.mgr.attach("main", self.ezapp, self.main_dock)
        self.coord.setWindowRectOverride("main", *RECTS["main"])

        # main dock initial panels
        self._probe = None
        if mode in ("a", "d", "e"):
            mover = self._place(self.main_dock, self.ezapp, self.mover_factory, "mover")
            main_anchor = self._place(self.main_dock, self.ezapp,
                                      _box_factory("MainAnchor", MAIN_ANCHOR), "main_anchor",
                                      target=mover, zone="RIGHT")
            mover.child.color = STATE_COLOR
            self._probe = main_anchor.child
        else:  # b, c : main hosts only the anchor; the mover starts in a secondary
            main_anchor = self._place(self.main_dock, self.ezapp,
                                      _box_factory("MainAnchor", MAIN_ANCHOR), "main_anchor")
            self._probe = main_anchor.child
        self._main_lg.setRect(0, 0, W, H)
        self.main_dock.updateLayout()

        self.caps  = {"main": Capturer()}
        self._secs = {}
        self._docks = {"main": self.main_dock}

        self.frame = 0; self.ready = False; self.ready_frame = 0; self.phase = 0
        self.t0 = 0
        self.before = {}; self.after = {}; self.mid = {}
        self.closed_count = 0
        self.tearout_key = None
        self.tearout_win_seen = 0
        self._verdict_emitted = False
        self.rc = 0

    ############################################################

    def _tok(self, zone):
        return getattr(self._tokens, zone)

    def _place(self, dock, win, factory, pid, target=None, zone=None):
        p = factory(dock, win)
        p.name = pid
        if target is not None:
            dock.moveChild(panel=p, to=target, zone=self._tok(zone))
        return p

    def _tearout_closed(self):
        self.closed_count += 1
        print(f"[{self.mode}] tear-out window onClosed observed (count={self.closed_count})", flush=True)

    def onGpuPostFrame(self, ctx):
        self.caps["main"].tick(ctx)

    def onGpuInit(self, ctx):
        for label in self.cfg["secs"]:
            x, y, w, h = RECTS[label]
            win = self.ezapp.createSecondaryWindow(width=w, height=h, x=x, y=y,
                                                   title=f"w4_{label}", decorated=False)
            uic = win.ui_context
            root = lev2.ui.LayoutGroup.create(f"{label}_lg")
            root.setRect(0, 0, w, h)
            uic.top = root
            root.margin = 0
            dock_margin = SEC_MARGIN_E if (self.mode == "e" and label == "sec1") else 0
            dock = root.makeChild(fill=True, margin=dock_margin,
                                  uiclass=lev2.ui.DockSpace, args=[f"{label}dock"]).widget
            dock.clear = False
            self.caps[label] = Capturer()
            win.onGpuPostFrame = (lambda l: (lambda c: self.caps[l].tick(c)))(label)

            if label == "sec1" and self.mode in ("a", "e"):
                self._place(dock, win, _box_factory("SecAnchor", SEC_ANCHOR), "sec_anchor")
            elif label == "sec1" and self.mode in ("b", "c"):
                sa = self._place(dock, win, _box_factory("SecAnchor", SEC_ANCHOR), "sec_anchor")
                mover = self._place(dock, win, self.mover_factory, "mover", target=sa, zone="RIGHT")
                mover.child.color = STATE_COLOR
            elif label == "sec2" and self.mode == "c":
                self._place(dock, win, _box_factory("Sec2Anchor", SEC2_ANCHOR), "sec2_anchor")

            root.setRect(0, 0, w, h)
            dock.updateLayout()
            self.mgr.attach(label, win, dock)
            self.coord.setWindowRectOverride(label, x, y, w, h)
            self._secs[label] = win
            self._docks[label] = dock

    # tear-out window builder (main thread; called from pump_tearouts via on_iter)
    def _build_tearout_window(self, app, panel_id, src_key, sx, sy):
        key = "tearout1"
        x, y, w, h = RECTS[key]
        win = app.createSecondaryWindow(width=w, height=h, x=x, y=y, title=key, decorated=False)
        uic = win.ui_context
        root = lev2.ui.LayoutGroup.create(key + "_lg")
        root.setRect(0, 0, w, h)
        uic.top = root
        root.margin = 0
        dock = root.makeChild(fill=True, margin=0, uiclass=lev2.ui.DockSpace, args=[key + "dock"]).widget
        dock.clear = False
        self.caps[key] = Capturer()
        win.onGpuPostFrame = (lambda c: self.caps[key].tick(c))
        root.setRect(0, 0, w, h)
        dock.updateLayout()
        self.coord.setWindowRectOverride(key, x, y, w, h)
        self.tearout_key = key
        self.tearout_win_seen = len(app.secondaryWindows) + 1
        self._secs[key] = win
        self._docks[key] = dock
        print(f"[d] tear-out window built key={key}", flush=True)
        return key, win, dock

    ############################################################

    def _on_iter(self):
        # main-thread, between frames — the safe place to create tear-out windows.
        try:
            self.mgr.pump_tearouts(self.ezapp)
        except Exception as e:
            print(f"[{self.mode}] pump_tearouts EXC: {e}", flush=True)

    ############################################################
    # coordinate mapping (all in the scale-1 screen == ui-root space)
    ############################################################

    def _src_local(self, src_key, screen_x, screen_y):
        sx, sy, _, _ = RECTS[src_key]
        return screen_x - sx, screen_y - sy

    def _dst_screen(self, dst_key, lx, ly):
        dx, dy, _, _ = RECTS[dst_key]
        return dx + lx, dy + ly

    def _coords(self):
        m = self.mode
        if m in ("a", "d", "e"):
            push, interm = (100, 20), (150, 150)
        else:  # b, c : mover is the RIGHT panel of sec1
            push, interm = (300, 20), (250, 150)
        if m == "a":
            target = self._src_local("main", *self._dst_screen("sec1", 40, 150))
        elif m == "b":
            target = self._src_local("sec1", *self._dst_screen("main", 40, 150))
        elif m == "c":
            target = self._src_local("sec1", *self._dst_screen("sec2", 40, 150))
        elif m == "d":
            target = self._src_local("main", 2000, 2000)          # empty desktop
        else:  # e : inside sec1 window but in its dock margin -> nozone
            target = self._src_local("main", *self._dst_screen("sec1", 5, 150))
        return push, interm, target

    def _inject(self, code, x, y):
        win = None if self.cfg["src"] == "main" else self._secs.get(self.cfg["src"])
        if code == "push":
            U.push(self.ezapp, x, y, W, H, window=win)
        elif code == "drag":
            U.move(self.ezapp, x, y, W, H, button="left", window=win)
        elif code == "release":
            U.release(self.ezapp, x, y, W, H, window=win)

    ############################################################

    def _emit(self, ok, detail):
        if self._verdict_emitted:
            return
        self.rc = verdict(ok, detail)
        self._verdict_emitted = True
        for w in self._secs.values():
            try:
                w.onClosed = None
            except Exception:
                pass
        self.ezapp.signalExit()

    def onUpdate(self, updata):
        self.frame = int(updata.counter)
        for w in self._secs.values():
            try:
                w.markDirty()
            except Exception:
                pass
        if not self.ready:
            if self._probe is not None and self._probe.width > 0:
                self.ready = True
                self.ready_frame = self.frame
            return
        rel = self.frame - self.ready_frame
        if self.mode in ("a", "b", "c"):
            self._upd_xfer(rel)
        elif self.mode == "d":
            self._upd_tearout(rel)
        else:
            self._upd_cancel(rel)

    ############################################################
    # a/b/c : baseline -> stepped drag into foreign zone (mid hint) -> release -> after
    ############################################################
    def _upd_xfer(self, rel):
        src, dst = self.cfg["src"], self.cfg["dst"]
        push, interm, target = self._coords()
        if self.phase == 0:
            if rel >= SETTLE:
                self.caps[src].arm(); self.caps[dst].arm()
                self.phase = 1
        elif self.phase == 1:
            if self.caps[src].ready and self.caps[dst].ready:
                self.before[src] = self.caps[src].result
                self.before[dst] = self.caps[dst].result
                self._inject("push", *push)
                self.t0 = rel; self.phase = 2
        elif self.phase == 2:
            if rel >= self.t0 + GAP:
                self._inject("drag", *interm)
                self.t0 = rel; self.phase = 3
        elif self.phase == 3:
            if rel >= self.t0 + GAP:
                self._inject("drag", *target)   # foreign zone -> hint raised on dst
                self.caps[dst].arm()            # capture the live foreign hint
                self.t0 = rel; self.phase = 4
        elif self.phase == 4:
            if self.caps[dst].ready:
                self.mid[dst] = self.caps[dst].result
                self.t0 = rel; self.phase = 5
        elif self.phase == 5:
            if rel >= self.t0 + GAP:
                self._inject("release", *target)  # commit: transfer enqueued+applied (deferred)
                self.t0 = rel; self.phase = 6
        elif self.phase == 6:
            if rel >= self.t0 + POST:
                self.caps[src].arm(); self.caps[dst].arm()
                self.phase = 7
        elif self.phase == 7:
            if self.caps[src].ready and self.caps[dst].ready:
                self.after[src] = self.caps[src].result
                self.after[dst] = self.caps[dst].result
                self.phase = 8
                self._assert_xfer(src, dst)

    def _assert_xfer(self, src, dst):
        sb = _count_magenta(self.before[src]); sa = _count_magenta(self.after[src])
        db = _count_magenta(self.before[dst]); da = _count_magenta(self.after[dst])
        hint = _count_blue_increase(self.mid[dst], self.before[dst])
        _save_png(self.before[src], os.path.join(TMP, f"{self.tag}_{src}_before.png"))
        _save_png(self.after[src],  os.path.join(TMP, f"{self.tag}_{src}_after.png"))
        _save_png(self.before[dst], os.path.join(TMP, f"{self.tag}_{dst}_before.png"))
        _save_png(self.mid[dst],    os.path.join(TMP, f"{self.tag}_{dst}_midhint.png"))
        _save_png(self.after[dst],  os.path.join(TMP, f"{self.tag}_{dst}_after.png"))
        src_had     = sb > MAG_PRESENT_MIN
        src_gone    = sa <= MAG_ABSENT_MAX
        dst_absent0 = db <= MAG_ABSENT_MAX
        dst_here    = da > MAG_PRESENT_MIN
        hint_ok     = hint > HINT_MIN
        ok = src_had and src_gone and dst_absent0 and dst_here and hint_ok
        print(f"[{self.mode}] src({src}) mag before={sb} after={sa} | dst({dst}) mag before={db} after={da} | "
              f"foreign_hint_blue={hint} | src_had={src_had} src_gone={src_gone} "
              f"dst_absent0={dst_absent0} dst_here={dst_here} hint_ok={hint_ok}", flush=True)
        self._emit(ok, f"src_gone={src_gone} dst_here={dst_here} hint={hint_ok}")

    ############################################################
    # d : tear-out onto empty desktop -> new window hosts the mover -> close -> return
    ############################################################
    def _upd_tearout(self, rel):
        push, interm, target = self._coords()
        if self.phase == 0:
            if rel >= SETTLE:
                self.caps["main"].arm(); self.phase = 1
        elif self.phase == 1:
            if self.caps["main"].ready:
                self.before["main"] = self.caps["main"].result   # main has the mover (magenta)
                self._inject("push", *push)
                self.t0 = rel; self.phase = 2
        elif self.phase == 2:
            if rel >= self.t0 + GAP:
                self._inject("drag", *interm)
                self.t0 = rel; self.phase = 3
        elif self.phase == 3:
            if rel >= self.t0 + GAP:
                self._inject("drag", *target)   # empty desktop
                self.t0 = rel; self.phase = 4
        elif self.phase == 4:
            if rel >= self.t0 + GAP:
                self._inject("release", *target)  # tear-out enqueued; pump (on_iter) builds the window
                self.t0 = rel; self.phase = 5
        elif self.phase == 5:
            # wait for the new window to exist + render, then capture it + main
            if rel >= self.t0 + POST_TEAROUT and self.tearout_key and self.tearout_key in self.caps:
                self.caps["main"].arm(); self.caps[self.tearout_key].arm()
                self.phase = 6
        elif self.phase == 6:
            if self.caps["main"].ready and self.caps[self.tearout_key].ready:
                self.mid["main"] = self.caps["main"].result
                self.mid["tearout"] = self.caps[self.tearout_key].result
                # close the torn-out window -> return-on-close moves the mover to main
                self._secs[self.tearout_key].requestClose()
                del self._secs[self.tearout_key]
                self.t0 = rel; self.phase = 7
        elif self.phase == 7:
            if rel >= self.t0 + CLOSE_WAIT:
                self.caps["main"].arm(); self.phase = 8
        elif self.phase == 8:
            if self.caps["main"].ready:
                self.after["main"] = self.caps["main"].result
                self.phase = 9
                self._assert_tearout()

    def _assert_tearout(self):
        mb = _count_magenta(self.before["main"])
        mm = _count_magenta(self.mid["main"])
        tm = _count_magenta(self.mid["tearout"])
        ma = _count_magenta(self.after["main"])
        _save_png(self.before["main"], os.path.join(TMP, f"{self.tag}_main_before.png"))
        _save_png(self.mid["main"],    os.path.join(TMP, f"{self.tag}_main_mid.png"))
        _save_png(self.mid["tearout"], os.path.join(TMP, f"{self.tag}_tearout.png"))
        _save_png(self.after["main"],  os.path.join(TMP, f"{self.tag}_main_after.png"))
        started = mb > MAG_PRESENT_MIN
        left    = mm <= MAG_ABSENT_MAX
        torn    = tm > MAG_PRESENT_MIN
        returned = ma > MAG_PRESENT_MIN
        new_win = self.tearout_win_seen > 0
        fired_once = self.closed_count == 1
        ok = started and left and torn and returned and new_win and fired_once
        print(f"[d] main mag before={mb} mid={mm} after={ma} | tearout mag={tm} | "
              f"started={started} left={left} torn={torn} returned={returned} "
              f"new_win={new_win} closed_count={self.closed_count}", flush=True)
        self._emit(ok, f"torn={torn} returned={returned} closed_once={fired_once}")

    ############################################################
    # e : cancel (drop inside a foreign window, off its dock) -> byte-equal baseline
    ############################################################
    def _upd_cancel(self, rel):
        push, interm, target = self._coords()
        if self.phase == 0:
            if rel >= SETTLE:
                self.caps["main"].arm(); self.caps["sec1"].arm()
                self.phase = 1
        elif self.phase == 1:
            if self.caps["main"].ready and self.caps["sec1"].ready:
                self.before["main"] = self.caps["main"].result
                self.before["sec1"] = self.caps["sec1"].result
                self._inject("push", *push)
                self.t0 = rel; self.phase = 2
        elif self.phase == 2:
            if rel >= self.t0 + GAP:
                self._inject("drag", *interm)
                self.t0 = rel; self.phase = 3
        elif self.phase == 3:
            if rel >= self.t0 + GAP:
                self._inject("drag", *target)   # inside sec1 window, off its dock (nozone)
                self.t0 = rel; self.phase = 4
        elif self.phase == 4:
            if rel >= self.t0 + GAP:
                self._inject("release", *target)  # cancel: no transfer
                self.t0 = rel; self.phase = 5
        elif self.phase == 5:
            if rel >= self.t0 + POST:
                self.caps["main"].arm(); self.caps["sec1"].arm()
                self.phase = 6
        elif self.phase == 6:
            if self.caps["main"].ready and self.caps["sec1"].ready:
                self.after["main"] = self.caps["main"].result
                self.after["sec1"] = self.caps["sec1"].result
                self.phase = 7
                self._assert_cancel()

    def _assert_cancel(self):
        eq_m, nd_m = _arr_equal(self.before["main"], self.after["main"])
        eq_s, nd_s = _arr_equal(self.before["sec1"], self.after["sec1"])
        _save_png(self.before["main"], os.path.join(TMP, f"{self.tag}_main_before.png"))
        _save_png(self.after["main"],  os.path.join(TMP, f"{self.tag}_main_after.png"))
        _save_png(self.before["sec1"], os.path.join(TMP, f"{self.tag}_sec1_before.png"))
        _save_png(self.after["sec1"],  os.path.join(TMP, f"{self.tag}_sec1_after.png"))
        main_ov = bool(self.ezapp.uicontext.hasOverlays())
        sec_ov  = bool(self._secs["sec1"].ui_context.hasOverlays())
        mover_stays = _count_magenta(self.after["main"]) > MAG_PRESENT_MIN
        ok = eq_m and eq_s and (not main_ov) and (not sec_ov) and mover_stays
        print(f"[e] main byte_equal={eq_m} (diff={nd_m}) | sec1 byte_equal={eq_s} (diff={nd_s}) | "
              f"main_overlay={main_ov} sec1_overlay={sec_ov} mover_stays={mover_stays}", flush=True)
        self._emit(ok, f"byteeq_main={eq_m} byteeq_sec={eq_s} no_overlay={not (main_ov or sec_ov)}")


################################################################################
# child entry
################################################################################

def run_child(args):
    app = App(args.mode, args.tag)
    app.ezapp.mainThreadLoop(on_iter=app._on_iter)   # teardown runs here, AFTER the verdict line
    sys.exit(app.rc)


################################################################################
# driver
################################################################################

def _spawn(mode, tag, timeout=180):
    cmd = ["ork.python", os.path.abspath(__file__), "--mode", mode, "--tag", tag]
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    sys.stdout.write(p.stdout); sys.stderr.write(p.stderr)
    return p.returncode, (p.stdout or "") + (p.stderr or "")

def _pass(rc, out):
    return read_verdict(out, rc) in (PASS, PASS_WITH_TEARDOWN_BUG)

def _pngeq(a, b):
    import numpy
    from PIL import Image
    if not (os.path.isfile(a) and os.path.isfile(b)):
        return False, -1, -1
    ia = numpy.asarray(Image.open(a).convert("RGB"), dtype=numpy.int16)
    ib = numpy.asarray(Image.open(b).convert("RGB"), dtype=numpy.int16)
    if ia.shape != ib.shape:
        return False, -1, -1
    d = numpy.abs(ia - ib)
    return (int(d.max()) == 0), int(numpy.count_nonzero(d.max(axis=2) > 0)), int(d.max())


def driver():
    os.makedirs(TMP, exist_ok=True)
    problems = []

    # (a) XFER main->sec1 + determinism (double-run byte-equal of the dest capture)
    rca0, outa0 = _spawn("a", "a0")
    if not _pass(rca0, outa0):
        problems.append(f"(a) run0 verdict={read_verdict(outa0, rca0)}")
    rca1, outa1 = _spawn("a", "a1")
    if not _pass(rca1, outa1):
        problems.append(f"(a) run1 verdict={read_verdict(outa1, rca1)}")
    eq, ndiff, mx = _pngeq(os.path.join(TMP, "a0_sec1_after.png"),
                           os.path.join(TMP, "a1_sec1_after.png"))
    print(f"[determinism] (a) dest byte_equal={eq} differing={ndiff} maxdiff={mx}", flush=True)
    if not eq:
        problems.append(f"(a) dest capture NOT deterministic (differing={ndiff} maxdiff={mx})")

    # (b) XFER sec1->main
    rcb, outb = _spawn("b", "b")
    if not _pass(rcb, outb):
        problems.append(f"(b) verdict={read_verdict(outb, rcb)}")

    # (c) XFER sec1->sec2 (three windows)
    rcc, outc = _spawn("c", "c")
    if not _pass(rcc, outc):
        problems.append(f"(c) verdict={read_verdict(outc, rcc)}")

    # (d) TEAROUT + return-on-close (clean rc=0)
    rcd, outd = _spawn("d", "d")
    if not _pass(rcd, outd):
        problems.append(f"(d) verdict={read_verdict(outd, rcd)}")
    if rcd != 0:
        problems.append(f"(d) tear-out did not exit rc=0 (rc={rcd})")

    # (e) CANCEL (byte-equal both windows)
    rce, oute = _spawn("e", "e")
    if not _pass(rce, oute):
        problems.append(f"(e) verdict={read_verdict(oute, rce)}")

    if problems:
        print("=== W4 cross-window drag gate FAILED ===", flush=True)
        for p in problems:
            print("  - " + p, flush=True)
        sys.exit(1)
    print("=== W4 cross-window drag gate PASSED ===", flush=True)
    sys.exit(0)


################################################################################

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mode", choices=["a", "b", "c", "d", "e"], default=None)
    ap.add_argument("--tag", default="x")
    args = ap.parse_args()
    if args.mode:
        run_child(args)
    driver()


if __name__ == "__main__":
    main()
