#!/usr/bin/env ork.python
################################################################################
# W1 gate: multi-window SUBSTRATE proof for cross-window docking.
#
#  Before docking DockPanels between OS windows can be built, three things must
#  be REAL and DETERMINISTIC under --offscreen/lockstep:
#    (a) a secondary OS window can be CREATED offscreen (no visible window, no
#        swapchain abort) and its pixels CAPTURED in a gate,
#    (b) UI events can be INJECTED into a secondary window (and land there, NOT
#        in the main window), and
#    (c) a secondary window can be CLOSED cleanly (onClosed fires exactly once,
#        process exits rc=0).
#
#  Subprocess-per-mode (one engine boot each) like the dockspace gates, but each
#  child speaks the ork.testing VERDICT-BEFORE-TEARDOWN protocol: the pass/fail is
#  emitted (flushed) BEFORE teardown, because the TWO-window teardown is a known
#  crash-risk zone — a teardown SIGSEGV after a green verdict must be distinguishable
#  from a real test failure (read_verdict -> PASS_WITH_TEARDOWN_BUG, not FAIL).
#
#    --mode render : lockstep app + offscreen secondary hosting a deterministic
#                    colored Box tree; capture BOTH main ctx.FBI and the secondary
#                    win.gfx_context.FBI after settle; assert secondary non-black +
#                    expected color. (driver double-runs it -> byte-equal determinism)
#    --mode inject : inject a full U.drag + a held PUSH into the SECONDARY (an
#                    EvTestBox); capture-delta proves the interaction landed in the
#                    secondary and NOT in the main window.
#    --mode close  : requestClose + pump cleanup; onClosed observed fired exactly
#                    once (incl. after dropping the last python ref); clean rc=0.
#
#  Determinism note: injection is driven from onUpdate; in lockstep the render
#  (main thread) never overlaps onUpdate, and the serial secondary pump runs AFTER
#  the main runloop iter — so an event injected in onUpdate(frame N) is applied
#  before frame N's secondary render. (Verified by the inject-delta capture.)
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys, argparse, subprocess

ROOT = os.path.abspath(__file__)
for _ in range(5):
    ROOT = os.path.dirname(ROOT)
sys.path.insert(0, os.path.join(ROOT, "obt.project", "scripts"))

from orkengine.core import vec4
from orkengine import lev2
import ork.uitest as U
from ork.testing import verdict, read_verdict, PASS, PASS_WITH_TEARDOWN_BUG

W, H = 220, 160
MAIN_COLOR = vec4(0.60, 0.18, 0.18, 1.0)   # main window bg (distinct red)
SEC_COLOR  = vec4(0.12, 0.68, 0.30, 1.0)   # secondary window bg (distinct green)
EV_COLOR   = vec4(0.20, 0.55, 0.75, 1.0)   # EvTestBox normal color (inject mode)

SETTLE = 12          # frames after ready before first action / capture
POST_INJECT = 6      # frames to let the post-injection render land before re-capture

TMP = os.path.join(os.environ.get("TMPDIR", "/tmp"), "secwin_offscreen")


################################################################################
# is_ready-polled capture of one FBI's main RTG -> an RGB numpy array (the dock
# gate idiom, factored so main + secondary use the identical path).
################################################################################

class Capturer:
    def __init__(self):
        self._buf = None
        self._fut = None
        self._inflight = False
        self._armed = False
        self.result = None

    def arm(self):
        self._armed = True
        self._inflight = False
        self._buf = None
        self._fut = None
        self.result = None

    @property
    def ready(self):
        return self.result is not None

    def tick(self, ctx):
        # called from the OWNING window's onGpuPostFrame (primary cmd buffer active).
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
    from PIL import Image
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    Image.fromarray(arr).save(path)


def _diff(a, b):
    import numpy
    if a is None or b is None or a.shape != b.shape:
        return -1, -1
    d = numpy.abs(a.astype(numpy.int16) - b.astype(numpy.int16))
    return int(numpy.count_nonzero(d.max(axis=2) > 0)), int(d.max())


################################################################################
# App — one instance per child process, parameterized by mode.
################################################################################

class App:
    def __init__(self, mode, sec_out=None, main_out=None):
        self.mode = mode
        self.sec_out = sec_out
        self.main_out = main_out

        self.ezapp = lev2.OrkEzApp.create(self, width=W, height=H,
                                          freerun=False, target_ups=60.0, target_fps=60.0)
        self.ezapp.topWidget.enableUiDraw()
        lg = self.ezapp.topLayoutGroup
        lg.margin = 0
        main_cls = lev2.ui.EvTestBox if mode == "inject" else lev2.ui.Box
        main_args = ["mainbox", EV_COLOR] if mode == "inject" else ["mainbox", MAIN_COLOR]
        self.mainbox = lg.makeChild(fill=True, margin=0, uiclass=main_cls, args=main_args).widget
        lg.setRect(0, 0, W, H)

        self.win = None
        self.secbox = None
        self.frame = 0
        self.ready = False
        self.ready_frame = 0
        self.phase = 0
        self.inject_frame = 0

        self.cap_main = Capturer()
        self.cap_sec = Capturer()

        # baseline / after arrays (inject mode)
        self.sec_before = self.sec_after = None
        self.main_before = self.main_after = None

        # close mode
        self.closed_count = 0
        self.ref_dropped = False
        self.close_frame = 0

        self._verdict_emitted = False
        self.rc = 0

    ############################################################

    def onGpuInit(self, ctx):
        self.win = self.ezapp.createSecondaryWindow(
            width=W, height=H, x=60, y=60, title="w1_secondary", decorated=False)
        uic = self.win.ui_context
        root = lev2.ui.LayoutGroup.create("sec_lg")
        root.setRect(0, 0, W, H)
        uic.top = root
        root.margin = 0
        if self.mode == "inject":
            self.secbox = root.makeChild(uiclass=lev2.ui.EvTestBox, args=["secbox", EV_COLOR], fill=True).widget
        else:
            self.secbox = root.makeChild(uiclass=lev2.ui.Box, args=["secbox", SEC_COLOR], fill=True).widget
        self.win.onGpuPostFrame = self._sec_postframe
        if self.mode == "close":
            self.win.onClosed = self._on_closed

    def _sec_postframe(self, ctx):
        self.cap_sec.tick(ctx)

    def onGpuPostFrame(self, ctx):
        # MAIN window post-frame (primary cmd buffer active): capture the main FBI here.
        self.cap_main.tick(ctx)

    def _on_closed(self):
        self.closed_count += 1
        print(f"[close] onClosed fired (count={self.closed_count})", flush=True)

    ############################################################

    def _emit(self, ok, detail):
        if self._verdict_emitted:
            return
        self.rc = verdict(ok, detail)   # prints TESTVERDICT line, flushed, BEFORE teardown
        self._verdict_emitted = True
        self.ezapp.signalExit()

    def onUpdate(self, updata):
        self.frame = int(updata.counter)
        if not self.ready:
            if self.mainbox.width > 0:
                self.ready = True
                self.ready_frame = self.frame
            return
        rel = self.frame - self.ready_frame
        getattr(self, "_upd_" + self.mode)(rel)

    ############################################################
    # RENDER: settle, capture both, assert secondary non-black + expected color.
    ############################################################
    def _upd_render(self, rel):
        if self.phase == 0:
            if rel >= SETTLE:
                self.cap_main.arm()
                self.cap_sec.arm()
                self.phase = 1
        elif self.phase == 1:
            if self.cap_main.ready and self.cap_sec.ready:
                self.phase = 2
                import numpy
                sec = self.cap_sec.result
                main = self.cap_main.result
                _save_png(sec, self.sec_out)
                _save_png(main, self.main_out)
                sec_max = int(sec.max())
                sec_mean = float(sec.mean())
                # expected: green box dominates -> G channel is the max channel
                gmean = float(sec[..., 1].mean())
                rmean = float(sec[..., 0].mean())
                bmean = float(sec[..., 2].mean())
                nonblack = sec_max > 0 and sec_mean > 2.0
                green_dominant = gmean > rmean and gmean > bmean
                # two INDEPENDENT contexts: secondary (green) must differ from main (red)
                distinct = (sec.shape == main.shape) and (_diff(sec, main)[0] > 0)
                ok = nonblack and green_dominant and distinct
                print(f"[render] sec max={sec_max} mean={sec_mean:.2f} "
                      f"RGBmean=({rmean:.1f},{gmean:.1f},{bmean:.1f}) "
                      f"nonblack={nonblack} green_dominant={green_dominant} distinct_from_main={distinct}",
                      flush=True)
                self._emit(ok, f"sec_mean={sec_mean:.2f} green_dom={green_dominant} distinct={distinct}")

    ############################################################
    # INJECT: baseline capture -> inject into SECONDARY -> after capture ->
    #  assert secondary CHANGED and main UNCHANGED.
    ############################################################
    def _upd_inject(self, rel):
        if self.phase == 0:
            if rel >= SETTLE:
                self.cap_main.arm()
                self.cap_sec.arm()
                self.phase = 1
        elif self.phase == 1:
            if self.cap_main.ready and self.cap_sec.ready:
                self.sec_before = self.cap_sec.result
                self.main_before = self.cap_main.result
                cx, cy = W // 2, H // 2
                # (1) a full composed U.drag (PUSH + interpolated DRAGs + RELEASE) into the
                #     SECONDARY: exercises the Context PUSH->BEGIN_DRAG/DRAG/END_DRAG synthesis
                #     + _evdragtarget capture through the secondary injection path + the RELEASE.
                U.drag(self.ezapp, cx - 40, cy, cx + 40, cy, W, H, steps=6, window=self.win)
                # (2) a held PUSH (no matching release) leaves the EvTestBox in a non-normal
                #     color state -> a PERSISTENT, deterministic delta to capture.
                U.push(self.ezapp, cx, cy, W, H, window=self.win)
                self.inject_frame = rel
                self.phase = 2
        elif self.phase == 2:
            if rel >= self.inject_frame + POST_INJECT:
                self.cap_main.arm()
                self.cap_sec.arm()
                self.phase = 3
        elif self.phase == 3:
            if self.cap_main.ready and self.cap_sec.ready:
                self.sec_after = self.cap_sec.result
                self.main_after = self.cap_main.result
                _save_png(self.sec_before, os.path.join(TMP, "inject_sec_before.png"))
                _save_png(self.sec_after, os.path.join(TMP, "inject_sec_after.png"))
                _save_png(self.main_before, os.path.join(TMP, "inject_main_before.png"))
                _save_png(self.main_after, os.path.join(TMP, "inject_main_after.png"))
                sec_diff, sec_max = _diff(self.sec_before, self.sec_after)
                main_diff, main_max = _diff(self.main_before, self.main_after)
                self.phase = 4
                landed_in_sec = sec_diff > 0
                not_in_main = main_diff == 0
                ok = landed_in_sec and not_in_main
                print(f"[inject] sec_delta_px={sec_diff} (max={sec_max})  "
                      f"main_delta_px={main_diff} (max={main_max})  "
                      f"landed_in_sec={landed_in_sec} not_in_main={not_in_main}", flush=True)
                self._emit(ok, f"sec_delta={sec_diff} main_delta={main_diff}")

    ############################################################
    # CLOSE: requestClose -> pump cleanup -> onClosed fired ONCE (incl. after
    #  dropping the last python ref) -> window fully removed -> clean rc=0.
    ############################################################
    def _upd_close(self, rel):
        if self.phase == 0:
            if rel >= SETTLE:
                self.win.requestClose()
                self.close_frame = rel
                self.phase = 1
        elif self.phase == 1:
            # two-phase close + cleanup completes within a couple frames
            if rel >= self.close_frame + 4:
                self.phase = 2
                count_after_close = self.closed_count
                removed = (len(self.ezapp.secondaryWindows) == 0)
                fully_closed = bool(self.win.should_close)
                # drop the last python ref and pump once more: a double-fire from the
                # destructor path would bump closed_count past 1 (the idempotence check).
                self.win = None
                self.secbox = None
                self.ref_dropped = True
                self.drop_frame = rel
                self.phase = 3
                self._c1 = count_after_close
                self._removed = removed
                self._fully = fully_closed
        elif self.phase == 3:
            if rel >= self.drop_frame + 3:
                self.phase = 4
                count_final = self.closed_count
                fired_once = (self._c1 == 1) and (count_final == 1)
                ok = fired_once and self._removed and self._fully
                print(f"[close] closed_count_at_cleanup={self._c1} final={count_final} "
                      f"removed_from_app={self._removed} should_close={self._fully} "
                      f"fired_exactly_once={fired_once}", flush=True)
                self._emit(ok, f"fired_once={fired_once} count={count_final}")


################################################################################
# child entry
################################################################################

def run_child(args):
    app = App(args.mode, sec_out=args.sec_out, main_out=args.main_out)
    app.ezapp.mainThreadLoop()   # teardown (2-window) runs here, AFTER the verdict line
    # If we reach here the teardown survived; exit with the verdict's intended rc.
    sys.exit(app.rc)


################################################################################
# driver
################################################################################

def _spawn(mode, extra=None, timeout=90):
    cmd = ["ork.python", os.path.abspath(__file__), "--mode", mode] + (extra or [])
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    sys.stdout.write(p.stdout)
    sys.stderr.write(p.stderr)
    return p.returncode, (p.stdout or "") + (p.stderr or "")


def _pass(rc, out):
    return read_verdict(out, rc) in (PASS, PASS_WITH_TEARDOWN_BUG)


def _pngeq(a, b):
    import numpy
    from PIL import Image
    ia = numpy.asarray(Image.open(a).convert("RGB"), dtype=numpy.int16)
    ib = numpy.asarray(Image.open(b).convert("RGB"), dtype=numpy.int16)
    if ia.shape != ib.shape:
        return False, -1, -1
    d = numpy.abs(ia - ib)
    return (int(d.max()) == 0), int(numpy.count_nonzero(d.max(axis=2) > 0)), int(d.max())


def driver():
    os.makedirs(TMP, exist_ok=True)
    problems = []

    ####################################
    # (a) RENDER + determinism (double-run byte-equal captures)
    ####################################
    S0 = os.path.join(TMP, "render_sec_run0.png"); M0 = os.path.join(TMP, "render_main_run0.png")
    S1 = os.path.join(TMP, "render_sec_run1.png"); M1 = os.path.join(TMP, "render_main_run1.png")
    rc0, out0 = _spawn("render", ["--sec-out", S0, "--main-out", M0])
    if not _pass(rc0, out0):
        problems.append(f"(a) render run0 verdict={read_verdict(out0, rc0)}")
    rc1, out1 = _spawn("render", ["--sec-out", S1, "--main-out", M1])
    if not _pass(rc1, out1):
        problems.append(f"(a) render run1 verdict={read_verdict(out1, rc1)}")
    if os.path.isfile(S0) and os.path.isfile(S1):
        eqs, ds, ms = _pngeq(S0, S1)
        print(f"[determinism] secondary byte_equal={eqs} differing={ds} maxdiff={ms}", flush=True)
        if not eqs:
            problems.append(f"(a) secondary capture NOT deterministic (differing={ds} maxdiff={ms})")
    else:
        problems.append("(a) render captures missing")
    if os.path.isfile(M0) and os.path.isfile(M1):
        eqm, dm, mm = _pngeq(M0, M1)
        print(f"[determinism] main byte_equal={eqm} differing={dm} maxdiff={mm}", flush=True)
        if not eqm:
            problems.append(f"(a) main capture NOT deterministic (differing={dm} maxdiff={mm})")

    ####################################
    # (b) INJECT: interaction lands in secondary, not main
    ####################################
    rcb, outb = _spawn("inject")
    if not _pass(rcb, outb):
        problems.append(f"(b) inject verdict={read_verdict(outb, rcb)}")

    ####################################
    # (c) CLOSE: onClosed once + clean exit
    ####################################
    rcc, outc = _spawn("close")
    if not _pass(rcc, outc):
        problems.append(f"(c) close verdict={read_verdict(outc, rcc)}")
    # (c) requires a CLEAN rc=0 (no teardown crash tolerated on the close path).
    if rcc != 0:
        problems.append(f"(c) close did not exit rc=0 (rc={rcc})")

    ####################################
    if problems:
        print("=== W1 secondary-window offscreen gate FAILED ===", flush=True)
        for p in problems:
            print("  - " + p, flush=True)
        sys.exit(1)
    print("=== W1 secondary-window offscreen gate PASSED ===", flush=True)
    sys.exit(0)


################################################################################

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mode", choices=["render", "inject", "close"], default=None)
    ap.add_argument("--sec-out", default=os.path.join(TMP, "sec.png"))
    ap.add_argument("--main-out", default=os.path.join(TMP, "main.png"))
    args = ap.parse_args()
    if args.mode:
        run_child(args)
    driver()


if __name__ == "__main__":
    main()
