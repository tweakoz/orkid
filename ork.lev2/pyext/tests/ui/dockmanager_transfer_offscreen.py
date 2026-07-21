#!/usr/bin/env ork.python
################################################################################
# W2+W3 gate: cross-window DockPanel transfer via FACTORY-RECREATE (DockManager).
#
#  Proves the owner-ratified v1 transfer: a panel is moved between the DockSpaces
#  of DIFFERENT OS windows by (1) snapshotting its carry-state, (2) removePanel on
#  the source dock, (3) re-running its factory to build FRESH content in the
#  destination dock, (4) moveChild to the requested zone, (5) restore_state onto
#  the fresh content. The stateful oracle: the mover box carries a post-creation
#  color (magenta) that DIFFERS from its factory default (gray) — so magenta in
#  the DESTINATION render proves restore ran (not just a bare re-factory), and NO
#  magenta in the SOURCE render proves the panel truly left.
#
#  Subprocess-per-mode (one engine boot each), lockstep+offscreen (freerun=False
#  forces offscreen), ork.testing VERDICT-BEFORE-TEARDOWN (two/three-window
#  teardown is a crash-risk zone; a post-verdict teardown SIGSEGV must read as
#  PASS_WITH_TEARDOWN_BUG, not FAIL).
#
#    --mode a : MAIN->SEC   transfer; dual capture: magenta ABSENT from main,
#               PRESENT (restored) in secondary. (driver double-runs -> byte-equal)
#    --mode b : SEC->MAIN   reverse; same oracles.
#    --mode c : SEC->SEC    two secondaries; transfer between them; both assert.
#    --mode d : RETURN-ON-CLOSE  transfer main->sec, close the secondary, the
#               mover (state carried) returns to the MAIN dock; onClosed count==1,
#               clean rc=0.
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

W, H = 280, 200

FACTORY_DEFAULT = vec4(0.15, 0.15, 0.15, 1.0)   # a fresh (un-restored) mover box
STATE_COLOR     = vec4(0.85, 0.15, 0.80, 1.0)   # the carried post-creation state (magenta)
MAIN_ANCHOR     = vec4(0.15, 0.45, 0.55, 1.0)   # teal   (never magenta)
SEC_ANCHOR      = vec4(0.50, 0.45, 0.12, 1.0)   # olive  (never magenta)
SEC2_ANCHOR     = vec4(0.30, 0.20, 0.55, 1.0)   # purple (R<150 -> never magenta)

SETTLE      = 14     # frames after ready before first capture / transfer
POST        = 8      # frames to let a post-transfer render land before re-capture
CLOSE_WAIT  = 10     # frames to let requestClose -> onClosed -> return -> render land

TMP = os.path.join(os.environ.get("TMPDIR", "/tmp"), "dockmgr_transfer")

MAG_PRESENT_MIN = 800   # magenta pixel count that proves the restored mover is rendered
MAG_ABSENT_MAX  = 8     # tolerance for "no magenta" (mover truly gone)

# per-mode plan: which secondaries exist, and the transfer src/dst window keys.
CFG = {
    "a": dict(secondaries=["sec1"],          src="main", dst="sec1"),
    "b": dict(secondaries=["sec1"],          src="sec1", dst="main"),
    "c": dict(secondaries=["sec1", "sec2"],  src="sec1", dst="sec2"),
    "d": dict(secondaries=["sec1"],          src="main", dst="sec1"),
}


################################################################################
# factory + carry-state (module-level so every mode shares them)
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

        # factories
        self.mover_factory  = _box_factory("Mover", FACTORY_DEFAULT, closeable=True)
        self.mgr = DockManager()
        self.mgr.register_factory("mover", "Mover", self.mover_factory,
                                  save_state=save_box, restore_state=restore_box, closeable=True)
        self.mgr.register_factory("main_anchor", "MainAnchor", _box_factory("MainAnchor", MAIN_ANCHOR))
        self.mgr.register_factory("sec_anchor",  "SecAnchor",  _box_factory("SecAnchor",  SEC_ANCHOR))
        self.mgr.register_factory("sec2_anchor", "Sec2Anchor", _box_factory("Sec2Anchor", SEC2_ANCHOR))

        self.mgr.attach("main", self.ezapp, self.main_dock)

        # build the MAIN dock's initial panels
        self._probe = None
        if mode in ("a", "d"):
            mover = self._place(self.main_dock, self.ezapp, self.mover_factory, "mover")
            main_anchor = self._place(self.main_dock, self.ezapp,
                                      _box_factory("MainAnchor", MAIN_ANCHOR), "main_anchor",
                                      target=mover, zone="RIGHT")
            mover.child.color = STATE_COLOR             # establish the carried state
            self._probe = main_anchor.child
        else:  # b, c : main hosts only the anchor; the mover starts in a secondary
            main_anchor = self._place(self.main_dock, self.ezapp,
                                      _box_factory("MainAnchor", MAIN_ANCHOR), "main_anchor")
            self._probe = main_anchor.child
        self._main_lg.setRect(0, 0, W, H)
        self.main_dock.updateLayout()

        # windows / capturers ("main" ticks via the App.onGpuPostFrame method below)
        self.caps = {"main": Capturer(), "sec1": Capturer(), "sec2": Capturer()}
        self._secs = {}          # label -> EzSecondaryWin (live)
        self._docks = {"main": self.main_dock}

        # state machine
        self.frame = 0; self.ready = False; self.ready_frame = 0; self.phase = 0
        self.t0 = 0
        self.before = {}; self.after = {}; self.mid = {}
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

    def _sec_closed(self):
        self.closed_count += 1
        print(f"[{self.mode}] onClosed observed (count={self.closed_count})", flush=True)

    def onGpuPostFrame(self, ctx):
        # MAIN window post-frame (primary cmd buffer active): tick the main capturer.
        self.caps["main"].tick(ctx)

    ############################################################

    def onGpuInit(self, ctx):
        for label in self.cfg["secondaries"]:
            win = self.ezapp.createSecondaryWindow(
                width=W, height=H, x=60, y=60, title=f"w2_{label}", decorated=False)
            uic = win.ui_context
            root = lev2.ui.LayoutGroup.create(f"{label}_lg")
            root.setRect(0, 0, W, H)
            uic.top = root
            root.margin = 0
            dock = root.makeChild(fill=True, margin=0, uiclass=lev2.ui.DockSpace, args=[f"{label}dock"]).widget
            dock.clear = False
            win.onGpuPostFrame = (lambda l: (lambda c: self.caps[l].tick(c)))(label)

            # per-mode initial panels in this secondary
            if self.mode == "a" and label == "sec1":
                self._place(dock, win, _box_factory("SecAnchor", SEC_ANCHOR), "sec_anchor")
            elif self.mode == "b" and label == "sec1":
                sa = self._place(dock, win, _box_factory("SecAnchor", SEC_ANCHOR), "sec_anchor")
                mover = self._place(dock, win, self.mover_factory, "mover", target=sa, zone="RIGHT")
                mover.child.color = STATE_COLOR
            elif self.mode == "c" and label == "sec1":
                sa = self._place(dock, win, _box_factory("SecAnchor", SEC_ANCHOR), "sec_anchor")
                mover = self._place(dock, win, self.mover_factory, "mover", target=sa, zone="RIGHT")
                mover.child.color = STATE_COLOR
            elif self.mode == "c" and label == "sec2":
                self._place(dock, win, _box_factory("Sec2Anchor", SEC2_ANCHOR), "sec2_anchor")
            # mode d sec1 starts EMPTY (mover arrives via transfer)

            root.setRect(0, 0, W, H)
            dock.updateLayout()

            on_closed = self._sec_closed if (self.mode == "d") else None
            self.mgr.attach(label, win, dock, on_closed=on_closed)
            self._secs[label] = win
            self._docks[label] = dock

    ############################################################

    def _emit(self, ok, detail):
        if self._verdict_emitted:
            return
        self.rc = verdict(ok, detail)   # TESTVERDICT line, flushed, BEFORE teardown
        self._verdict_emitted = True
        # disarm teardown re-entry: the manager wired win.onClosed on every attached
        # secondary; without this, ~EzSecondaryWin would call back into python during
        # the (post-verdict) teardown.
        for w in self._secs.values():
            try:
                w.onClosed = None
            except Exception:
                pass
        self.ezapp.signalExit()

    def onUpdate(self, updata):
        self.frame = int(updata.counter)
        # keep live secondaries re-rendering so their capturers can complete
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
        if self.mode == "d":
            self._upd_d(rel)
        else:
            self._upd_transfer(rel)

    ############################################################
    # a/b/c : capture src+dst before -> transfer -> capture src+dst after -> assert
    ############################################################
    def _upd_transfer(self, rel):
        src, dst = self.cfg["src"], self.cfg["dst"]
        if self.phase == 0:
            if rel >= SETTLE:
                self.caps[src].arm(); self.caps[dst].arm()
                self.phase = 1
        elif self.phase == 1:
            if self.caps[src].ready and self.caps[dst].ready:
                self.before[src] = self.caps[src].result
                self.before[dst] = self.caps[dst].result
                # do the transfer (target = the dst anchor; zone RIGHT split)
                target = {"a": "sec_anchor", "b": "main_anchor", "c": "sec2_anchor"}[self.mode]
                self.mgr.transfer("mover", src, dst, target_panel_id=target, zone="RIGHT")
                self.t0 = rel
                self.phase = 2
        elif self.phase == 2:
            if rel >= self.t0 + POST:
                self.caps[src].arm(); self.caps[dst].arm()
                self.phase = 3
        elif self.phase == 3:
            if self.caps[src].ready and self.caps[dst].ready:
                self.after[src] = self.caps[src].result
                self.after[dst] = self.caps[dst].result
                self.phase = 4
                self._assert_transfer(src, dst)

    def _assert_transfer(self, src, dst):
        sb = _count_magenta(self.before[src]); sa = _count_magenta(self.after[src])
        db = _count_magenta(self.before[dst]); da = _count_magenta(self.after[dst])
        _save_png(self.before[src], os.path.join(TMP, f"{self.tag}_{src}_before.png"))
        _save_png(self.after[src],  os.path.join(TMP, f"{self.tag}_{src}_after.png"))
        _save_png(self.before[dst], os.path.join(TMP, f"{self.tag}_{dst}_before.png"))
        _save_png(self.after[dst],  os.path.join(TMP, f"{self.tag}_{dst}_after.png"))
        src_had     = sb > MAG_PRESENT_MIN       # mover was in the source
        src_gone    = sa <= MAG_ABSENT_MAX       # mover left the source
        dst_absent0 = db <= MAG_ABSENT_MAX       # mover was NOT in the dest before
        dst_here    = da > MAG_PRESENT_MIN       # mover arrived + RESTORED (magenta) in dest
        ok = src_had and src_gone and dst_absent0 and dst_here
        print(f"[{self.mode}] src({src}) magenta before={sb} after={sa} | "
              f"dst({dst}) magenta before={db} after={da} | "
              f"src_had={src_had} src_gone={src_gone} dst_absent0={dst_absent0} dst_here={dst_here}",
              flush=True)
        self._emit(ok, f"src_gone={src_gone} dst_here={dst_here}")

    ############################################################
    # d : main->sec transfer, then close sec -> mover returns to main.
    ############################################################
    def _upd_d(self, rel):
        if self.phase == 0:
            if rel >= SETTLE:
                self.caps["main"].arm(); self.caps["sec1"].arm()
                self.phase = 1
        elif self.phase == 1:
            if self.caps["main"].ready and self.caps["sec1"].ready:
                self.before["main"] = self.caps["main"].result   # main has the mover (magenta)
                self.mgr.transfer("mover", "main", "sec1")        # no target -> sec default leaf
                self.t0 = rel
                self.phase = 2
        elif self.phase == 2:
            if rel >= self.t0 + POST:
                self.caps["main"].arm(); self.caps["sec1"].arm()
                self.phase = 3
        elif self.phase == 3:
            if self.caps["main"].ready and self.caps["sec1"].ready:
                self.mid["main"] = self.caps["main"].result       # main lost the mover
                self.mid["sec1"] = self.caps["sec1"].result       # sec has it (magenta)
                # close the secondary -> return-on-close transfers the mover back to main
                self._secs["sec1"].requestClose()
                del self._secs["sec1"]                            # stop markDirty-ing it
                self.t0 = rel
                self.phase = 4
        elif self.phase == 4:
            if rel >= self.t0 + CLOSE_WAIT:
                self.caps["main"].arm()
                self.phase = 5
        elif self.phase == 5:
            if self.caps["main"].ready:
                self.after["main"] = self.caps["main"].result     # main has it again (restored)
                self.phase = 6
                self._assert_d()

    def _assert_d(self):
        mb = _count_magenta(self.before["main"])
        mm = _count_magenta(self.mid["main"])
        sm = _count_magenta(self.mid["sec1"])
        ma = _count_magenta(self.after["main"])
        _save_png(self.before["main"], os.path.join(TMP, f"{self.tag}_main_before.png"))
        _save_png(self.mid["main"],    os.path.join(TMP, f"{self.tag}_main_mid.png"))
        _save_png(self.mid["sec1"],    os.path.join(TMP, f"{self.tag}_sec1_mid.png"))
        _save_png(self.after["main"],  os.path.join(TMP, f"{self.tag}_main_after.png"))
        started_in_main = mb > MAG_PRESENT_MIN
        left_main       = mm <= MAG_ABSENT_MAX
        went_to_sec     = sm > MAG_PRESENT_MIN
        returned        = ma > MAG_PRESENT_MIN
        fired_once      = (self.closed_count == 1)
        ok = started_in_main and left_main and went_to_sec and returned and fired_once
        print(f"[d] main magenta before={mb} mid={mm} after={ma} | sec1 mid={sm} | "
              f"started={started_in_main} left={left_main} went={went_to_sec} "
              f"returned={returned} closed_count={self.closed_count}", flush=True)
        self._emit(ok, f"returned={returned} closed_once={fired_once}")


################################################################################
# child entry
################################################################################

def run_child(args):
    app = App(args.mode, args.tag)
    app.ezapp.mainThreadLoop()   # teardown runs here, AFTER the verdict line
    sys.exit(app.rc)


################################################################################
# driver
################################################################################

def _spawn(mode, tag, timeout=120):
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

    # (a) MAIN->SEC + determinism (double-run byte-equal of the dest capture)
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

    # (b) SEC->MAIN
    rcb, outb = _spawn("b", "b")
    if not _pass(rcb, outb):
        problems.append(f"(b) verdict={read_verdict(outb, rcb)}")

    # (c) SEC->SEC
    rcc, outc = _spawn("c", "c")
    if not _pass(rcc, outc):
        problems.append(f"(c) verdict={read_verdict(outc, rcc)}")

    # (d) RETURN-ON-CLOSE (requires a clean rc=0 like W1's close mode)
    rcd, outd = _spawn("d", "d")
    if not _pass(rcd, outd):
        problems.append(f"(d) verdict={read_verdict(outd, rcd)}")
    if rcd != 0:
        problems.append(f"(d) return-on-close did not exit rc=0 (rc={rcd})")

    if problems:
        print("=== W2+W3 dockmanager transfer gate FAILED ===", flush=True)
        for p in problems:
            print("  - " + p, flush=True)
        sys.exit(1)
    print("=== W2+W3 dockmanager transfer gate PASSED ===", flush=True)
    sys.exit(0)


################################################################################

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mode", choices=["a", "b", "c", "d"], default=None)
    ap.add_argument("--tag", default="x")
    args = ap.parse_args()
    if args.mode:
        run_child(args)
    driver()


if __name__ == "__main__":
    main()
