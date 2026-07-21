#!/usr/bin/env ork.python
################################################################################
# W5 gate: TerrainEditor cross-window docking — REAL editor, offscreen.
#
#  Boots the actual TerrainEditor (voronoi, preview 256) and drives the shipped
#  cross-window-docking integration against its REAL DockManager (the editor's
#  own EditorDockGlue: the property sheet is the transferable factory panel; the
#  viewport + node-editor column are PINNED — no factory — because they hold
#  one-shot Context-bound GPU seams). Subprocess-per-mode; ork.testing
#  VERDICT-BEFORE-TEARDOWN (multi-window teardown is a crash-risk zone; a
#  post-verdict SIGSEGV reads PASS_WITH_TEARDOWN_BUG, not FAIL).
#
#    --mode a : API TRANSFER of the FACTORY propsheet main->secondary. Dual capture:
#               the secondary is EMPTY before, and after the transfer its framebuffer
#               is LIT by the rebuilt+restored property sheet (content renders, not
#               just a titlebar). Structural: propsheet leaves main, lands in sec.
#    --mode b : same transfer, then close the secondary -> return-on-close carries the
#               property sheet back to the MAIN dock (structural + main re-renders).
#    --mode c : PINNED — drive an injected cross-window titlebar drag of the VIEWPORT
#               panel (rect overrides map the cursor out of main and INTO a registered
#               secondary, i.e. a would-be FOREIGN target). The pinning predicate must
#               keep it LOCAL: NO transfer occurs, the viewport stays in main, and the
#               secondary shows NO foreign drop hint. The driver also greps the child's
#               ORKID_DOCK_TRACE for the [pinned:Viewport] classification.
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

TERR_KW = dict(extent_m=512.0, dsl_kwargs={"amplitude": 60.0}, preview_dim=256, chunk=128)

W, H     = 480, 360               # main (editor) window
SEC_RECT = (W + 120, 0, 420, 340) # secondary window screen rect (disjoint from main)

SETTLE     = 70    # frames for the terrain scene to bake + settle before gate ops
POST       = 16    # frames to let a post-transfer render land before re-capture
CLOSE_WAIT = 22    # frames to let requestClose -> onClosed -> return -> render land
GAP        = 3     # frames between injected drag steps

TMP = os.path.join(os.environ.get("TMPDIR", "/tmp"), "terredit_xwin")

LIT_THRESH        = 24     # per-channel brightness that counts a pixel as "lit"
SEC_EMPTY_MAX     = 2000   # lit pixels tolerated in the EMPTY secondary (bg only)
SEC_CONTENT_MIN   = 12000  # lit pixels proving the rebuilt property sheet renders content
MAIN_RETURN_MIN   = 12000  # lit pixels proving main re-renders after the return


################################################################################
# async framebuffer capture (is_ready-polled; the W1/W2 idiom)
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

def _lit_count(arr):
    import numpy
    if arr is None:
        return -1
    return int((arr.max(axis=2) > LIT_THRESH).sum())


################################################################################
# gate app — the REAL TerrainEditor, subclassed only to drive a per-mode script
# from the main/GPU-thread post-frame hook (where dock mutations are render-safe).
################################################################################

from ork.editor.terrainedit import TerrainEditor


class Gate(TerrainEditor):

    def __init__(self, mode, tag):
        self.gate_mode = mode
        self.gate_tag  = tag
        self._tokens   = CrcStringProxy()
        super().__init__("voronoi", **TERR_KW, offscreen=True)
        self._persist_layout = False        # never touch the on-disk session slot

        # gate state
        self._gframe = 0
        self._gphase = 0
        self._gt0    = 0
        self._sec_win  = None
        self._sec_dock = None
        self._sec_cap  = Capturer()
        self._main_cap = Capturer()
        self._before   = {}
        self._after    = {}
        self._closed_count = 0
        self._verdict_emitted = False
        self.rc = 0

    @property
    def mgr(self):
        return self._dock_glue.mgr

    ############################################################

    def onGpuPostFrame(self, ctx):
        super().onGpuPostFrame(ctx)         # editor capture + glue.pump (tear-out realize)
        self._main_cap.tick(ctx)
        if self._sec_win is not None:
            try:
                self._sec_win.markDirty()   # keep the secondary re-rendering for its capturer
            except Exception:
                pass
        self._gframe += 1
        try:
            self._gate_tick(ctx)
        except Exception as e:
            import traceback
            traceback.print_exc()
            self._emit(False, f"gate exception: {e}")

    ############################################################

    def _make_secondary(self):
        x, y, w, h = SEC_RECT
        win = self.ezapp.createSecondaryWindow(width=w, height=h, x=x, y=y,
                                               title="w5sec", decorated=False)
        uic  = win.ui_context
        root = lev2.ui.LayoutGroup.create("w5sec_lg")
        root.setRect(0, 0, w, h)
        uic.top     = root
        root.margin = 0
        dock = root.makeChild(fill=True, margin=0, uiclass=lev2.ui.DockSpace,
                              args=["w5secdock"]).widget
        dock.clear = False
        root.setRect(0, 0, w, h)
        dock.updateLayout()
        win.onGpuPostFrame = lambda c: self._sec_cap.tick(c)
        self.mgr.attach("sec1", win, dock, on_closed=self._sec_closed)
        self._sec_win  = win
        self._sec_dock = dock

    def _sec_closed(self):
        self._closed_count += 1
        print(f"[{self.gate_mode}] secondary onClosed observed (count={self._closed_count})",
              flush=True)

    def _populate_propsheet(self):
        # bind the Terrain Parameters model so the property sheet has visible rows to
        # render (a fresh editor boot shows an empty node-property sheet until selection).
        self.params_model.refresh()
        self.propsheet.model = self.params_model
        self.propsheet.rebuild()

    @staticmethod
    def _names(dock):
        return sorted(p.name for p in dock.allPanels())

    ############################################################

    def _emit(self, ok, detail):
        if self._verdict_emitted:
            return
        self.rc = verdict(ok, detail)
        self._verdict_emitted = True
        # disarm teardown re-entry: the manager wired win.onClosed on the secondary.
        if self._sec_win is not None:
            try:
                self._sec_win.onClosed = None
            except Exception:
                pass
        self.ezapp.signalExit()

    ############################################################
    # per-mode script (main/GPU thread; dock mutations are render-sequential here)
    ############################################################

    def _gate_tick(self, ctx):
        if self._verdict_emitted:
            return
        if self.gate_mode in ("a", "b"):
            self._tick_transfer(ctx)
        else:
            self._tick_pinned(ctx)
        if self._gframe > 1500:
            self._emit(False, "TIMEOUT")

    ############################################################
    # a/b : populate + capture empty sec -> transfer -> capture sec lit -> [b: return]
    ############################################################

    def _tick_transfer(self, ctx):
        ph = self._gphase
        if ph == 0:
            if self._gframe >= SETTLE:
                self._make_secondary()
                self._populate_propsheet()
                self._gphase = 1
                self._gt0 = self._gframe
        elif ph == 1:
            if self._gframe >= self._gt0 + POST:
                self._sec_cap.arm()
                self._gphase = 2
        elif ph == 2:
            if self._sec_cap.ready:
                self._before["sec"] = self._sec_cap.result
                # STRUCTURAL pre-state
                self._before["main_names"] = self._names(self.dock)
                self._before["sec_names"]  = self._names(self._sec_dock)
                # THE transfer (factory-recreate the property sheet into the secondary)
                self.mgr.transfer("propsheet", "main", "sec1")
                self._gphase = 3
                self._gt0 = self._gframe
        elif ph == 3:
            if self._gframe >= self._gt0 + POST:
                self._sec_cap.arm()
                self._gphase = 4
        elif ph == 4:
            if self._sec_cap.ready:
                self._after["sec"] = self._sec_cap.result
                self._after["main_names"] = self._names(self.dock)
                self._after["sec_names"]  = self._names(self._sec_dock)
                if self.gate_mode == "a":
                    self._assert_transfer()
                else:
                    # mode b: close the secondary -> return-on-close carries it back
                    self._sec_win.requestClose()
                    self._gphase = 5
                    self._gt0 = self._gframe
        elif ph == 5:
            if self._closed_count == 1 and self._gframe >= self._gt0 + CLOSE_WAIT:
                self._main_cap.arm()
                self._gphase = 6
        elif ph == 6:
            if self._main_cap.ready:
                self._after["main"] = self._main_cap.result
                self._assert_return()

    def _assert_transfer(self):
        eb = _lit_count(self._before["sec"])
        ea = _lit_count(self._after["sec"])
        _save_png(self._before["sec"], os.path.join(TMP, f"{self.gate_tag}_sec_before.png"))
        _save_png(self._after["sec"],  os.path.join(TMP, f"{self.gate_tag}_sec_after.png"))
        left_main   = "propsheet" not in self._after["main_names"]
        landed_sec  = "propsheet" in self._after["sec_names"]
        was_in_main = "propsheet" in self._before["main_names"]
        sec_empty0  = eb <= SEC_EMPTY_MAX
        sec_content = ea >= SEC_CONTENT_MIN
        ok = was_in_main and left_main and landed_sec and sec_empty0 and sec_content
        print(f"[a] main {self._before['main_names']}->{self._after['main_names']} | "
              f"sec {self._before['sec_names']}->{self._after['sec_names']} | "
              f"sec lit before={eb} after={ea} | was_in_main={was_in_main} left_main={left_main} "
              f"landed_sec={landed_sec} sec_empty0={sec_empty0} sec_content={sec_content}", flush=True)
        self._emit(ok, f"left_main={left_main} landed_sec={landed_sec} content={sec_content}")

    def _assert_return(self):
        ma = _lit_count(self._after["main"])
        _save_png(self._after["main"], os.path.join(TMP, f"{self.gate_tag}_main_return.png"))
        # _after["main_names"] (from ph4) proves propsheet had LEFT main into the sec; a
        # FRESH query now proves the return-on-close carried it back.
        left_main_names = self._after["main_names"]         # ph4: propsheet in sec, gone from main
        now_names       = self._names(self.dock)            # post-return
        left_for_sec = "propsheet" not in left_main_names
        back_in_main = "propsheet" in now_names
        main_lit     = ma >= MAIN_RETURN_MIN
        closed_once  = self._closed_count == 1
        ok = left_for_sec and back_in_main and main_lit and closed_once
        print(f"[b] main {left_main_names}(post-transfer) -> {now_names}(post-return) | "
              f"main lit={ma} | left_for_sec={left_for_sec} back_in_main={back_in_main} "
              f"main_lit={main_lit} closed_once={closed_once}", flush=True)
        self._emit(ok, f"left_for_sec={left_for_sec} back_in_main={back_in_main} closed_once={closed_once}")

    ############################################################
    # c : PINNED viewport drag out of main -> stays LOCAL (no transfer / no hint)
    ############################################################

    def _tick_pinned(self, ctx):
        ph = self._gphase
        coord = lev2.ui.DockCoordinator.instance()
        if ph == 0:
            if self._gframe >= SETTLE:
                self._make_secondary()
                # disjoint valid rects: a NON-pinned panel dragged here WOULD go FOREIGN;
                # the pinning predicate is what forces LOCAL.
                coord.setWindowRectOverride("main", 0, 0, W, H)
                coord.setWindowRectOverride("sec1", *SEC_RECT)
                self._before["main_names"] = self._names(self.dock)
                self._gphase = 1
                self._gt0 = self._gframe
        elif ph == 1:
            if self._gframe >= self._gt0 + POST:
                self._drag_viewport_out()
                self._gphase = 2
                self._gt0 = self._gframe
        elif ph == 2:
            if self._gframe >= self._gt0 + (GAP * 4 + POST):
                self._assert_pinned()

    def _drag_viewport_out(self):
        import ork.uitest as U
        top = self.ezapp.topWidget
        vp  = self.viewport_dock
        # grab the viewport titlebar (root space), drag the cursor beyond main's right
        # edge so it maps (via the overrides) INTO the registered secondary.
        x0, y0 = vp.localToRoot(24, vp.titlebar_height // 2)
        interm = (W - 8, y0)
        target = (W + 200, 160)     # main-local coords past the right edge -> sec1 screen
        U.push(self.ezapp, x0, y0, top.width, top.height)
        U.move(self.ezapp, interm[0], interm[1], top.width, top.height, button="left")
        U.move(self.ezapp, target[0], target[1], top.width, top.height, button="left")
        U.move(self.ezapp, target[0], target[1], top.width, top.height, button="left")
        U.release(self.ezapp, target[0], target[1], top.width, top.height)

    def _assert_pinned(self):
        main_names = self._names(self.dock)
        sec_names  = self._names(self._sec_dock)
        vp_in_main   = "Viewport" in main_names
        vp_not_sec   = "Viewport" not in sec_names
        no_transfer  = (main_names == self._before["main_names"])
        sec_overlay  = bool(self._sec_win.ui_context.hasOverlays())
        main_overlay = bool(self.ezapp.uicontext.hasOverlays())
        ok = vp_in_main and vp_not_sec and no_transfer and (not sec_overlay) and (not main_overlay)
        print(f"[c] main {self._before['main_names']}->{main_names} | sec {sec_names} | "
              f"vp_in_main={vp_in_main} vp_not_sec={vp_not_sec} no_transfer={no_transfer} "
              f"sec_overlay={sec_overlay} main_overlay={main_overlay}", flush=True)
        self._emit(ok, f"vp_in_main={vp_in_main} no_transfer={no_transfer} no_hint={not sec_overlay}")


################################################################################
# child entry
################################################################################

def run_child(args):
    gate = Gate(args.mode, args.tag)
    gate.ezapp.mainThreadLoop()          # teardown runs here, AFTER the verdict line
    sys.exit(gate.rc)


################################################################################
# driver
################################################################################

def _spawn(mode, tag, extra_env=None, timeout=300):
    env = dict(os.environ)
    if extra_env:
        env.update(extra_env)
    cmd = ["ork.python", os.path.abspath(__file__), "--mode", mode, "--tag", tag]
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, env=env)
    sys.stdout.write(p.stdout); sys.stderr.write(p.stderr)
    return p.returncode, (p.stdout or "") + (p.stderr or "")

def _pass(rc, out):
    return read_verdict(out, rc) in (PASS, PASS_WITH_TEARDOWN_BUG)


def driver():
    os.makedirs(TMP, exist_ok=True)
    problems = []

    # (a) API transfer of the factory propsheet main->secondary, functional in dest
    rca, outa = _spawn("a", "a")
    if not _pass(rca, outa):
        problems.append(f"(a) transfer verdict={read_verdict(outa, rca)}")

    # (b) return-on-close -> propsheet back in main
    rcb, outb = _spawn("b", "b")
    if not _pass(rcb, outb):
        problems.append(f"(b) return verdict={read_verdict(outb, rcb)}")

    # (c) PINNED viewport drag stays LOCAL (structural in-child + trace grep here)
    rcc, outc = _spawn("c", "c", extra_env={"ORKID_DOCK_TRACE": "1"})
    if not _pass(rcc, outc):
        problems.append(f"(c) pinned verdict={read_verdict(outc, rcc)}")
    if "[pinned:Viewport]" not in outc:
        problems.append("(c) ORKID_DOCK_TRACE never classified the viewport drag as pinned")
    if "FOREIGN" in outc:
        problems.append("(c) a FOREIGN classification leaked for the pinned viewport drag")

    if problems:
        print("=== TerrainEditor cross-window gate FAILED ===", flush=True)
        for p in problems:
            print("  - " + p, flush=True)
        sys.exit(1)
    print("=== TerrainEditor cross-window gate PASSED ===", flush=True)
    sys.exit(0)


################################################################################

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mode", choices=["a", "b", "c"], default=None)
    ap.add_argument("--tag", default="x")
    args = ap.parse_args()
    if args.mode:
        run_child(args)
    driver()


if __name__ == "__main__":
    main()
