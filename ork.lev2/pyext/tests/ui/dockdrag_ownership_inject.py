#!/usr/bin/env ork.python
################################################################################
# W5-fix gate: point-OWNERSHIP (occlusion) cross-window drag classification.
#
#  Covers BUG-A (stuck local drop hint) + BUG-B (drop over ANOTHER app's window
#  must equal drop over empty desktop = TEAR-OUT), plus the true-z-order tie-break
#  between our OWN overlapping windows. Rect containment alone is occlusion-blind:
#  a screen point geometrically inside OUR window but under a foreign OS window
#  (e.g. the terminal) would classify LOCAL (stuck tile + local move) instead of
#  TEAR-OUT. DockCoordinator::setPointOwnershipOverride is the pluggable test seam
#  that simulates that occlusion (and, for our own overlapping windows, the actual
#  top-most window at a point). Override -> "" means "the top-most window here is
#  NOT ours".
#
#  Offscreen + lockstep (freerun=False), subprocess-per-mode, ork.testing
#  VERDICT-BEFORE-TEARDOWN. Screen space == ui-root (scale 1, _macosUseHIDPI
#  false) so the disjoint/overlapping OVERRIDE rects are given directly.
#
#    --mode occl : PUSH mover titlebar -> DRAG to a LOCAL edge zone (tile UP) ->
#                  DRAG to coords INSIDE main's rect but marked NOT-OURS by the
#                  ownership override (terminal overlap) -> capture: local tile is
#                  GONE (BUG-A) -> RELEASE -> TEAR-OUT happened, mover LEFT main,
#                  NOT a local move (BUG-B).
#    --mode zorder: main + two FULLY-OVERLAPPING secondaries (sec1, sec2). The
#                  registration-order heuristic would pick sec2 (newest); the
#                  ownership override names sec1 the top-most -> the mover must
#                  land in sec1, proving z-order override wins.
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

RECTS = {
    "main":     (0, 0, W, H),
    "sec1":     (W + 100, 0, W, H),          # (500,0,400,300)  — zorder: overlaps sec2
    "sec2":     (W + 100, 0, W, H),          # (500,0,400,300)  — SAME rect as sec1 (full overlap)
    "tearout1": (2 * (W + 100), 0, W, H),    # where a torn-out window lands
}

FACTORY_DEFAULT = vec4(0.15, 0.15, 0.15, 1.0)   # fresh (un-restored) mover box
STATE_COLOR     = vec4(0.85, 0.15, 0.80, 1.0)   # carried post-creation state (magenta)
MAIN_ANCHOR     = vec4(0.15, 0.45, 0.55, 1.0)   # teal
SEC_ANCHOR      = vec4(0.50, 0.45, 0.12, 1.0)   # olive
SEC2_ANCHOR     = vec4(0.30, 0.20, 0.55, 1.0)   # purple

# "terminal" band over the MAIN window (screen coords). A point here is INSIDE
# main's rect but the override reports it as NOT ours (occluded).
NOT_OURS = (280, 120, 400, 220)  # x0,y0,x1,y1

SETTLE       = 14
GAP          = 3
POST         = 8
POST_TEAROUT = 16

MAG_PRESENT_MIN = 800
MAG_ABSENT_MAX  = 8
TILE_MIN        = 400    # blue-increase pixels proving the LOCAL drop tile is drawn
TILE_ABSENT_MAX = 60     # tolerance for "no local tile"

TMP = os.path.join(os.environ.get("TMPDIR", "/tmp"), "xwinowner")


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
    R = arr[..., 0].astype(numpy.int16); G = arr[..., 1].astype(numpy.int16); B = arr[..., 2].astype(numpy.int16)
    return int(((R > 150) & (B > 120) & (G < 100)).sum())

def _count_blue_increase(mid, base):
    import numpy
    if mid is None or base is None:
        return -1
    return int(((mid[..., 2].astype(numpy.int16) - base[..., 2].astype(numpy.int16)) > 30).sum())


################################################################################

class App:
    def __init__(self, mode, tag):
        self.mode = mode
        self.tag  = tag
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

        # main dock: mover (magenta) LEFT, an anchor RIGHT
        mover = self._place(self.main_dock, self.ezapp, self.mover_factory, "mover")
        main_anchor = self._place(self.main_dock, self.ezapp,
                                  _box_factory("MainAnchor", MAIN_ANCHOR), "main_anchor",
                                  target=mover, zone="RIGHT")
        mover.child.color = STATE_COLOR
        self._probe = main_anchor.child
        self._main_lg.setRect(0, 0, W, H)
        self.main_dock.updateLayout()

        # ownership override per mode
        self.coord.setPointOwnershipOverride(self._owner_occl if mode == "occl" else self._owner_zorder)

        self.caps  = {"main": Capturer()}
        self._secs = {}
        self._docks = {"main": self.main_dock}

        self.frame = 0; self.ready = False; self.ready_frame = 0; self.phase = 0
        self.t0 = 0
        self.cap = {}
        self.tearout_key = None
        self.tearout_built = False
        self.closed_count = 0
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

    @staticmethod
    def _names(dock):
        return sorted(p.name for p in dock.allPanels())

    def _tearout_closed(self):
        self.closed_count += 1

    # ownership overrides (screen coords) ----------------------------------

    def _owner_occl(self, sx, sy):
        x0, y0, x1, y1 = NOT_OURS
        if x0 <= sx <= x1 and y0 <= sy <= y1:
            return ""            # occluded by the "terminal" -> not ours
        if 0 <= sx < W and 0 <= sy < H:
            return "main"        # inside main, not occluded
        return ""                # outside all our windows

    def _owner_zorder(self, sx, sy):
        # sec1 and sec2 fully overlap; declare sec1 the top-most at the overlap.
        rx, ry, rw, rh = RECTS["sec1"]
        if rx <= sx < rx + rw and ry <= sy < ry + rh:
            return "sec1"
        if 0 <= sx < W and 0 <= sy < H:
            return "main"
        return ""

    ############################################################

    def onGpuInit(self, ctx):
        labels = [] if self.mode == "occl" else ["sec1", "sec2"]
        for label in labels:
            x, y, w, h = RECTS[label]
            win = self.ezapp.createSecondaryWindow(width=w, height=h, x=x, y=y,
                                                   title=f"own_{label}", decorated=False)
            uic = win.ui_context
            root = lev2.ui.LayoutGroup.create(f"{label}_lg")
            root.setRect(0, 0, w, h)
            uic.top = root
            root.margin = 0
            dock = root.makeChild(fill=True, margin=0, uiclass=lev2.ui.DockSpace, args=[f"{label}dock"]).widget
            dock.clear = False
            self.caps[label] = Capturer()
            win.onGpuPostFrame = (lambda l: (lambda c: self.caps[l].tick(c)))(label)
            if label == "sec1":
                self._place(dock, win, _box_factory("SecAnchor", SEC_ANCHOR), "sec_anchor")
            else:
                self._place(dock, win, _box_factory("Sec2Anchor", SEC2_ANCHOR), "sec2_anchor")
            root.setRect(0, 0, w, h)
            dock.updateLayout()
            self.mgr.attach(label, win, dock)
            self.coord.setWindowRectOverride(label, x, y, w, h)
            self._secs[label] = win
            self._docks[label] = dock

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
        self.tearout_built = True
        self._secs[key] = win
        self._docks[key] = dock
        print(f"[occl] tear-out window built key={key}", flush=True)
        return key, win, dock

    def _on_iter(self):
        try:
            self.mgr.pump_tearouts(self.ezapp)
        except Exception as e:
            print(f"[{self.mode}] pump_tearouts EXC: {e}", flush=True)

    ############################################################

    def onGpuPostFrame(self, ctx):
        self.caps["main"].tick(ctx)

    def _inject(self, code, x, y, window=None):
        if code == "push":
            U.push(self.ezapp, x, y, W, H, window=window)
        elif code == "drag":
            U.move(self.ezapp, x, y, W, H, button="left", window=window)
        elif code == "release":
            U.release(self.ezapp, x, y, W, H, window=window)

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
        if self.mode == "occl":
            self._upd_occl(rel)
        else:
            self._upd_zorder(rel)
        if rel > 1400:
            self._emit(False, "TIMEOUT")

    ############################################################
    # occl : local tile UP -> drag to NOT-OURS (tile must vanish) -> release = TEAROUT
    ############################################################

    LOCAL_ZONE = (300, 40)     # over main_anchor, override -> "main" (tile UP)
    OCCLUDED   = (330, 160)    # over main_anchor but inside NOT_OURS band -> "" (tear-out)

    def _upd_occl(self, rel):
        if self.phase == 0:
            if rel >= SETTLE:
                self.caps["main"].arm(); self.phase = 1
        elif self.phase == 1:
            if self.caps["main"].ready:
                self.cap["base"] = self.caps["main"].result
                self._inject("push", 100, 20)
                self.t0 = rel; self.phase = 2
        elif self.phase == 2:
            if rel >= self.t0 + GAP:
                self._inject("drag", *self.LOCAL_ZONE)   # local edge zone -> tile UP
                self.caps["main"].arm()
                self.t0 = rel; self.phase = 3
        elif self.phase == 3:
            if self.caps["main"].ready:
                self.cap["tile_up"] = self.caps["main"].result
                self._inject("drag", *self.OCCLUDED)     # inside main rect but NOT ours
                self.caps["main"].arm()
                self.t0 = rel; self.phase = 4
        elif self.phase == 4:
            if self.caps["main"].ready:
                self.cap["occluded"] = self.caps["main"].result
                self.names_pre = self._names(self.main_dock)
                self._inject("release", *self.OCCLUDED)  # commit
                self.t0 = rel; self.phase = 5
        elif self.phase == 5:
            if rel >= self.t0 + POST_TEAROUT:
                self.phase = 6
                self._assert_occl()

    def _assert_occl(self):
        base   = self.cap["base"]
        up     = _count_blue_increase(self.cap["tile_up"], base)
        occl   = _count_blue_increase(self.cap["occluded"], base)
        _save_png(base,               os.path.join(TMP, f"{self.tag}_base.png"))
        _save_png(self.cap["tile_up"],   os.path.join(TMP, f"{self.tag}_tile_up.png"))
        _save_png(self.cap["occluded"],  os.path.join(TMP, f"{self.tag}_occluded.png"))
        names_post = self._names(self.main_dock)
        # BUG-A: local tile drawn over the local zone, then GONE once the cursor is
        #        over the not-ours point.
        tile_shown  = up > TILE_MIN
        tile_hidden = occl <= TILE_ABSENT_MAX
        # BUG-B: release over not-ours == tear-out (mover LEFT main + a new window),
        #        NOT a local move (mover would stay in main).
        mover_left  = "mover" not in names_post
        torn_out    = self.tearout_built
        ok = tile_shown and tile_hidden and mover_left and torn_out
        print(f"[occl] tile_up_blue={up} occluded_blue={occl} | names {self.names_pre}->{names_post} | "
              f"tile_shown={tile_shown} tile_hidden(A)={tile_hidden} mover_left(B)={mover_left} "
              f"torn_out(B)={torn_out}", flush=True)
        self._emit(ok, f"tile_hidden={tile_hidden} mover_left={mover_left} torn_out={torn_out}")

    ############################################################
    # zorder : drag mover into the sec1/sec2 overlap; override names sec1 top-most
    ############################################################

    OVERLAP = (540, 150)   # screen point inside BOTH sec1 and sec2

    def _upd_zorder(self, rel):
        tx, ty = self.OVERLAP  # screen == main-local (main origin 0,0)
        if self.phase == 0:
            if rel >= SETTLE and "sec1" in self.caps and "sec2" in self.caps:
                self.phase = 1
        elif self.phase == 1:
            self._inject("push", 100, 20)
            self.t0 = rel; self.phase = 2
        elif self.phase == 2:
            if rel >= self.t0 + GAP:
                self._inject("drag", 150, 150)
                self.t0 = rel; self.phase = 3
        elif self.phase == 3:
            if rel >= self.t0 + GAP:
                self._inject("drag", tx, ty)   # into the overlap
                self.t0 = rel; self.phase = 4
        elif self.phase == 4:
            if rel >= self.t0 + GAP:
                self._inject("release", tx, ty)
                self.t0 = rel; self.phase = 5
        elif self.phase == 5:
            if rel >= self.t0 + POST_TEAROUT:
                self.caps["sec1"].arm(); self.caps["sec2"].arm()
                self.phase = 6
        elif self.phase == 6:
            if self.caps["sec1"].ready and self.caps["sec2"].ready:
                self.cap["sec1"] = self.caps["sec1"].result
                self.cap["sec2"] = self.caps["sec2"].result
                self.phase = 7
                self._assert_zorder()

    def _assert_zorder(self):
        s1 = _count_magenta(self.cap["sec1"])
        s2 = _count_magenta(self.cap["sec2"])
        _save_png(self.cap["sec1"], os.path.join(TMP, f"{self.tag}_sec1.png"))
        _save_png(self.cap["sec2"], os.path.join(TMP, f"{self.tag}_sec2.png"))
        n1 = self._names(self._docks["sec1"]); n2 = self._names(self._docks["sec2"])
        landed_sec1 = ("mover" in n1) and (s1 > MAG_PRESENT_MIN)
        not_in_sec2 = ("mover" not in n2) and (s2 <= MAG_ABSENT_MAX)
        ok = landed_sec1 and not_in_sec2
        print(f"[zorder] sec1 names={n1} mag={s1} | sec2 names={n2} mag={s2} | "
              f"landed_sec1={landed_sec1} not_in_sec2={not_in_sec2}", flush=True)
        self._emit(ok, f"landed_sec1={landed_sec1} not_in_sec2={not_in_sec2}")


################################################################################

def run_child(args):
    app = App(args.mode, args.tag)
    app.ezapp.mainThreadLoop(on_iter=app._on_iter)
    sys.exit(app.rc)


def _spawn(mode, tag, timeout=180):
    cmd = ["ork.python", os.path.abspath(__file__), "--mode", mode, "--tag", tag]
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    sys.stdout.write(p.stdout); sys.stderr.write(p.stderr)
    return p.returncode, (p.stdout or "") + (p.stderr or "")

def _pass(rc, out):
    return read_verdict(out, rc) in (PASS, PASS_WITH_TEARDOWN_BUG)


def driver():
    os.makedirs(TMP, exist_ok=True)
    problems = []

    rco, outo = _spawn("occl", "occl")
    if not _pass(rco, outo):
        problems.append(f"(occl) A+B verdict={read_verdict(outo, rco)}")

    rcz, outz = _spawn("zorder", "zorder")
    if not _pass(rcz, outz):
        problems.append(f"(zorder) verdict={read_verdict(outz, rcz)}")

    if problems:
        print("=== dock ownership gate FAILED ===", flush=True)
        for p in problems:
            print("  - " + p, flush=True)
        sys.exit(1)
    print("=== dock ownership gate PASSED ===", flush=True)
    sys.exit(0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mode", choices=["occl", "zorder"], default=None)
    ap.add_argument("--tag", default="x")
    args = ap.parse_args()
    if args.mode:
        run_child(args)
    driver()


if __name__ == "__main__":
    main()
