#!/usr/bin/env ork.python
################################################################################
# W5 gate: DflowEditor cross-window docking — REAL editor, offscreen.
#
#  Boots the actual DflowEditor (TWO voronoi sources -> a two-tab Graph column) and
#  drives the shipped cross-window-docking integration against its REAL DockManager
#  (the editor's own EditorDockGlue: the Graph node-editor column AND the property sheet
#  are transferable factory panels; the composed VIEWPORT is PINNED — no factory —
#  because it holds a one-shot Context-bound GPU seam). Subprocess-per-mode; ork.testing
#  VERDICT-BEFORE-TEARDOWN (multi-window teardown is a crash-risk zone; a post-verdict
#  SIGSEGV reads PASS_WITH_TEARDOWN_BUG, not FAIL).
#
#    --mode t : API TRANSFER of the FACTORY "Graph" node-editor column main->secondary
#               (the GPU-seam panel — a fresh TabsWidget hosting one PrimCanvas+NodeEditor
#               per binding is rebuilt in the destination window's context). Factory-
#               recreate oracle: Graph leaves main + lands in sec (structural); ONE fresh
#               node editor per binding is rebuilt (editor count == binding count) each
#               bound to the SAME per-binding document (node count preserved); the FOCUSED
#               tab (tab 1 of 2) is carried; the focused tab's selection survives; and the
#               destination framebuffer is LIT by the rebuilt canvas (empty before,
#               content-bearing after) — the graph RENDERS in the new window.
#    --mode u : Graph transfer THEN return-on-close — the GPU-seam node editors rebuild back
#               in MAIN (their glyph textures re-init against the main context) and re-render
#               LIT, with the per-binding documents intact (node count preserved).
#    --mode o : TEAR-OUT — an injected titlebar drag of the Graph column OUT of main (past
#               the window edge -> no registered window there -> TEAROUT) spawns a fresh
#               window (glue pump), factory-recreates the node editors into it, and the graph
#               RENDERS there.
#    --mode c : PINNED — drive an injected cross-window titlebar drag of the VIEWPORT panel
#               (the sole remaining pinned panel: a one-shot Context-bound GPU seam, no
#               factory). Rect overrides map the cursor out of main and INTO a registered
#               secondary (a would-be FOREIGN target). The pinning predicate must keep it
#               LOCAL: NO transfer occurs, the viewport stays in main, and the secondary shows
#               NO foreign drop hint. The driver also greps the child's ORKID_DOCK_TRACE for
#               the [pinned:Viewport] classification.
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

# TWO sources -> a two-tab Graph column, so the FOCUSED-tab carry is proven with >1 tab.
SRC = ["voronoi", "voronoi"]

W, H     = 480, 360               # main (editor) window
SEC_RECT = (W + 120, 0, 420, 340) # secondary window screen rect (disjoint from main)

SETTLE     = 80    # frames for the (composed terrain) scene to bake + settle before gate ops
POST       = 16    # frames to let a post-transfer render land before re-capture
CLOSE_WAIT = 22    # frames to let requestClose -> onClosed -> return -> render land
GAP        = 3     # frames between injected drag steps

TMP = os.path.join(os.environ.get("TMPDIR", "/tmp"), "dflowedit_xwin")

LIT_THRESH        = 24     # per-channel brightness that counts a pixel as "lit"
SEC_EMPTY_MAX     = 2000   # lit pixels tolerated in the EMPTY secondary (bg only)
SEC_CONTENT_MIN   = 12000  # lit pixels proving the rebuilt Graph column renders content
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
# gate app — the REAL DflowEditor, subclassed only to drive a per-mode script from
# the main/GPU-thread post-frame hook (where dock mutations are render-safe).
################################################################################

from ork.editor.dflowedit import DflowEditor


class Gate(DflowEditor):

    def __init__(self, mode, tag):
        self.gate_mode = mode
        self.gate_tag  = tag
        self._tokens   = CrcStringProxy()
        super().__init__(SRC, offscreen=True)
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
        self._nodes0    = 0       # focused-binding node count (doc-binding oracle)
        self._sel_node  = None    # carried node selection (focused tab)
        self._focus_tab = 1       # the tab we focus + expect carried across the transfer
        self._tearout_win  = None # torn-out window (mode o)
        self._tearout_dock = None
        self._tearout_cap  = None
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

    @staticmethod
    def _names(dock):
        return sorted(p.name for p in dock.allPanels())

    def _focus_and_select(self):
        # focus tab 1 (of 2) and seed a selection on ITS editor to prove focused-tab + per-tab
        # selection carry across the transfer. The focus poll (_onGpuUpdate) rebinds
        # self.node_editor to the focused editor before this runs (a full frame after the tab
        # switch), so self.node_editor IS the focused (tab-1) editor here.
        ne = self.node_editor
        nids = list(ne.model.nodes())
        self._nodes0 = len(nids)
        self._sel_node = nids[0] if nids else None
        if self._sel_node is not None:
            ne.sel_nodes = {self._sel_node}
            ne.mark_selection_changed()

    ############################################################

    def _emit(self, ok, detail):
        if self._verdict_emitted:
            return
        self.rc = verdict(ok, detail)
        self._verdict_emitted = True
        # disarm teardown re-entry: the manager wired win.onClosed on the secondary / torn-out
        # window (return-on-close would re-run the factory during teardown).
        for w in (self._sec_win, self._tearout_win):
            if w is not None:
                try:
                    w.onClosed = None
                    w.onGpuPostFrame = None
                except Exception:
                    pass
        self.ezapp.signalExit()

    ############################################################
    # per-mode script (main/GPU thread; dock mutations are render-sequential here)
    ############################################################

    def _gate_tick(self, ctx):
        if self._verdict_emitted:
            return
        if self.gate_mode == "t":
            self._tick_graph_transfer(ctx)
        elif self.gate_mode == "u":
            self._tick_graph_return(ctx)
        elif self.gate_mode == "o":
            self._tick_graph_tearout(ctx)
        else:
            self._tick_pinned(ctx)
        if self._gframe > 1500:
            self._emit(False, "TIMEOUT")

    ############################################################
    # t : the Graph node-editor column (the GPU-seam factory panel) transfers into a
    #     secondary — factory-recreate oracle: one fresh node editor per binding bound to the
    #     same docs, focused tab + selection carried, and the graph RENDERS in the destination.
    ############################################################

    def _tick_graph_transfer(self, ctx):
        ph = self._gphase
        if ph == 0:
            if self._gframe >= SETTLE:
                self._make_secondary()
                if self.tabs is not None:
                    self.tabs.setActiveTab(self._focus_tab)   # focus poll follows next frame
                self._gphase = 1
                self._gt0 = self._gframe
        elif ph == 1:
            # the focus poll has now rebound self.node_editor to the focused (tab-1) editor
            if self._gframe >= self._gt0 + GAP:
                self._focus_and_select()
                self._gphase = 2
                self._gt0 = self._gframe
        elif ph == 2:
            if self._gframe >= self._gt0 + POST:
                self._sec_cap.arm()          # capture the EMPTY secondary
                self._gphase = 3
        elif ph == 3:
            if self._sec_cap.ready:
                self._before["sec"] = self._sec_cap.result
                self._before["main_names"] = self._names(self.dock)
                self._before["sec_names"]  = self._names(self._sec_dock)
                self._before["focus"] = self._focused_index
                # THE transfer (factory-recreate the Graph node-editor column into the secondary)
                self.mgr.transfer("Graph", "main", "sec1")
                self._gphase = 4
                self._gt0 = self._gframe
        elif ph == 4:
            if self._gframe >= self._gt0 + POST:
                self._sec_cap.arm()          # capture the ARRIVED (rebuilt) canvas
                self._gphase = 5
        elif ph == 5:
            if self._sec_cap.ready:
                self._after["sec"] = self._sec_cap.result
                self._after["main_names"] = self._names(self.dock)
                self._after["sec_names"]  = self._names(self._sec_dock)
                self._assert_graph_transfer()

    def _assert_graph_transfer(self):
        eb = _lit_count(self._before["sec"])
        ea = _lit_count(self._after["sec"])
        _save_png(self._before["sec"], os.path.join(TMP, f"{self.gate_tag}_sec_before.png"))
        _save_png(self._after["sec"],  os.path.join(TMP, f"{self.gate_tag}_sec_after.png"))
        was_in_main = "Graph" in self._before["main_names"]
        left_main   = "Graph" not in self._after["main_names"]
        landed_sec  = "Graph" in self._after["sec_names"]
        sec_empty0  = eb <= SEC_EMPTY_MAX
        sec_content = ea >= SEC_CONTENT_MIN
        # factory-recreate oracle: ONE fresh node editor per binding, each bound to the SAME
        # per-binding document (node count preserved on the focused editor), focused tab + the
        # focused tab's selection carried across.
        editors_ok = (len(self._node_editors) == len(self._bindings)
                      and len(self._bindings) == len(SRC))
        ne = self.node_editor
        nodes_now = len(list(ne.model.nodes())) if ne is not None else -1
        nodes_ok  = (ne is not None and self._nodes0 > 0 and nodes_now == self._nodes0)
        focus_ok  = (self._focused_index == self._focus_tab)
        sel_ok    = (self._sel_node is None) or (ne is not None and self._sel_node in ne.sel_nodes)
        ok = (was_in_main and left_main and landed_sec and sec_empty0 and sec_content
              and editors_ok and nodes_ok and focus_ok and sel_ok)
        print(f"[t] main {self._before['main_names']}->{self._after['main_names']} | "
              f"sec {self._before['sec_names']}->{self._after['sec_names']} | "
              f"sec lit before={eb} after={ea} | was_in_main={was_in_main} left_main={left_main} "
              f"landed_sec={landed_sec} sec_empty0={sec_empty0} sec_content={sec_content} | "
              f"editors {len(self._node_editors)}/{len(self._bindings)} editors_ok={editors_ok} "
              f"nodes {self._nodes0}->{nodes_now} nodes_ok={nodes_ok} "
              f"focus {self._before['focus']}->{self._focused_index} focus_ok={focus_ok} "
              f"sel_ok={sel_ok}", flush=True)
        self._emit(ok, f"left_main={left_main} landed_sec={landed_sec} content={sec_content} "
                       f"editors_ok={editors_ok} nodes_ok={nodes_ok} focus_ok={focus_ok} "
                       f"sel_ok={sel_ok}")

    ############################################################
    # u : Graph transfer THEN return-on-close — the GPU-seam node editors rebuild back in MAIN
    #     (their glyph textures re-init against the main context) and re-render LIT.
    ############################################################

    def _tick_graph_return(self, ctx):
        ph = self._gphase
        if ph == 0:
            if self._gframe >= SETTLE:
                self._make_secondary()
                self._nodes0 = len(list(self.node_editor.model.nodes()))
                self._gphase = 1
                self._gt0 = self._gframe
        elif ph == 1:
            if self._gframe >= self._gt0 + POST:
                self.mgr.transfer("Graph", "main", "sec1")     # Graph -> secondary
                self._after["main_names"] = self._names(self.dock)   # proves it LEFT main
                self._gphase = 2
                self._gt0 = self._gframe
        elif ph == 2:
            if self._gframe >= self._gt0 + POST:
                self._sec_win.requestClose()   # -> onClosed -> return-on-close carries it back
                self._gphase = 3
                self._gt0 = self._gframe
        elif ph == 3:
            if self._closed_count == 1 and self._gframe >= self._gt0 + CLOSE_WAIT:
                self._main_cap.arm()
                self._gphase = 4
        elif ph == 4:
            if self._main_cap.ready:
                self._after["main"] = self._main_cap.result
                self._assert_graph_return()

    def _assert_graph_return(self):
        ma = _lit_count(self._after["main"])
        _save_png(self._after["main"], os.path.join(TMP, f"{self.gate_tag}_main_return.png"))
        left_for_sec = "Graph" not in self._after["main_names"]   # ph1: gone from main into sec
        now_names    = self._names(self.dock)                     # post-return
        back_in_main = "Graph" in now_names
        main_lit     = ma >= MAIN_RETURN_MIN
        closed_once  = self._closed_count == 1
        editors_ok   = (len(self._node_editors) == len(self._bindings))
        ne = self.node_editor
        nodes_now = len(list(ne.model.nodes())) if ne is not None else -1
        nodes_ok  = (ne is not None and self._nodes0 > 0 and nodes_now == self._nodes0)
        ok = (left_for_sec and back_in_main and main_lit and closed_once
              and editors_ok and nodes_ok)
        print(f"[u] main (post-transfer){self._after['main_names']} -> (post-return){now_names} | "
              f"main lit={ma} | left_for_sec={left_for_sec} back_in_main={back_in_main} "
              f"main_lit={main_lit} closed_once={closed_once} editors_ok={editors_ok} "
              f"nodes {self._nodes0}->{nodes_now} nodes_ok={nodes_ok}", flush=True)
        self._emit(ok, f"back_in_main={back_in_main} main_lit={main_lit} closed_once={closed_once} "
                       f"editors_ok={editors_ok} nodes_ok={nodes_ok}")

    ############################################################
    # o : TEAR-OUT — an injected titlebar drag of the Graph column OUT of main (past the window
    #     edge -> no registered window there -> TEAROUT) spawns a fresh window (glue pump),
    #     factory-recreates the node editors into it, and the graph RENDERS there.
    ############################################################

    def _tick_graph_tearout(self, ctx):
        ph = self._gphase
        coord = lev2.ui.DockCoordinator.instance()
        if ph == 0:
            if self._gframe >= SETTLE:
                # main's rect must be known so a drop PAST its right edge resolves TEAROUT
                # (outside all windows) rather than degrading to a single-window local move.
                coord.setWindowRectOverride("main", 0, 0, W, H)
                self._nodes0 = len(list(self.node_editor.model.nodes()))
                self._before["main_names"] = self._names(self.dock)
                self._gphase = 1
                self._gt0 = self._gframe
        elif ph == 1:
            if self._gframe >= self._gt0 + POST:
                self._drag_graph_out()
                self._gphase = 2
                self._gt0 = self._gframe
        elif ph == 2:
            # the tear-out is deferred: coordinator -> _pending_tearouts -> glue.pump (onGpuPost
            # Frame) builds the window + transfers Graph in. Poll for the new registered window.
            keys = [k for k, r in self.mgr._windows.items() if not r.is_main]
            if keys:
                reg = self.mgr._windows[keys[0]]
                self._tearout_win  = reg.window
                self._tearout_dock = reg.dock
                self._tearout_cap  = Capturer()
                reg.window.onGpuPostFrame = lambda c: self._tearout_cap.tick(c)
                self._gphase = 3
                self._gt0 = self._gframe
            elif self._gframe >= self._gt0 + 300:
                self._emit(False, "tear-out window never created")
        elif ph == 3:
            self._mark_tearout()
            if self._gframe >= self._gt0 + POST:
                self._tearout_cap.arm()
                self._gphase = 4
        elif ph == 4:
            self._mark_tearout()
            if self._tearout_cap.ready:
                self._assert_graph_tearout()

    def _mark_tearout(self):
        try:
            self._tearout_win.markDirty()   # keep the torn-out window re-rendering for its capturer
        except Exception:
            pass

    def _drag_graph_out(self):
        import ork.uitest as U
        top = self.ezapp.topWidget
        lp  = self.left_dock                 # the "Graph" DockPanel
        x0, y0 = lp.localToRoot(24, lp.titlebar_height // 2)
        interm = (W - 8, y0)
        target = (W + 240, 180)              # main-local coords past the right edge -> outside all
        U.push(self.ezapp, x0, y0, top.width, top.height)
        U.move(self.ezapp, interm[0], interm[1], top.width, top.height, button="left")
        U.move(self.ezapp, target[0], target[1], top.width, top.height, button="left")
        U.move(self.ezapp, target[0], target[1], top.width, top.height, button="left")
        U.release(self.ezapp, target[0], target[1], top.width, top.height)

    def _assert_graph_tearout(self):
        lit = _lit_count(self._tearout_cap.result)
        _save_png(self._tearout_cap.result, os.path.join(TMP, f"{self.gate_tag}_tearout.png"))
        main_names = self._names(self.dock)
        tear_names = self._names(self._tearout_dock)
        left_main    = "Graph" not in main_names
        in_tearout   = "Graph" in tear_names
        tear_content = lit >= SEC_CONTENT_MIN
        editors_ok   = (len(self._node_editors) == len(self._bindings))
        ne = self.node_editor
        nodes_now = len(list(ne.model.nodes())) if ne is not None else -1
        nodes_ok  = (ne is not None and self._nodes0 > 0 and nodes_now == self._nodes0)
        ok = left_main and in_tearout and tear_content and editors_ok and nodes_ok
        print(f"[o] main {self._before['main_names']}->{main_names} | tearout {tear_names} | "
              f"tearout lit={lit} left_main={left_main} in_tearout={in_tearout} content={tear_content} "
              f"| editors_ok={editors_ok} nodes {self._nodes0}->{nodes_now} nodes_ok={nodes_ok}",
              flush=True)
        self._emit(ok, f"left_main={left_main} in_tearout={in_tearout} content={tear_content} "
                       f"editors_ok={editors_ok} nodes_ok={nodes_ok}")

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
        # grab the viewport titlebar (root space), drag the cursor beyond main's right edge so
        # it maps (via the overrides) INTO the registered secondary.
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

    # (t) API transfer of the FACTORY Graph node-editor column -> secondary; the rebuilt canvas
    #     RENDERS there (content pixel oracle) with per-binding editors + focused tab + selection carried
    rct, outt = _spawn("t", "t")
    if not _pass(rct, outt):
        problems.append(f"(t) graph transfer verdict={read_verdict(outt, rct)}")

    # (u) Graph return-on-close -> the node editors rebuild back in main + re-render
    rcu, outu = _spawn("u", "u")
    if not _pass(rcu, outu):
        problems.append(f"(u) graph return verdict={read_verdict(outu, rcu)}")

    # (o) Graph TEAR-OUT via injected drag -> spawns a window, the graph renders there
    rco, outo = _spawn("o", "o")
    if not _pass(rco, outo):
        problems.append(f"(o) graph tear-out verdict={read_verdict(outo, rco)}")

    # (c) PINNED viewport drag stays LOCAL (structural in-child + trace grep here)
    rcc, outc = _spawn("c", "c", extra_env={"ORKID_DOCK_TRACE": "1"})
    if not _pass(rcc, outc):
        problems.append(f"(c) pinned verdict={read_verdict(outc, rcc)}")
    if "[pinned:Viewport]" not in outc:
        problems.append("(c) ORKID_DOCK_TRACE never classified the viewport drag as pinned")
    if "FOREIGN" in outc:
        problems.append("(c) a FOREIGN classification leaked for the pinned viewport drag")

    if problems:
        print("=== DflowEditor cross-window gate FAILED ===", flush=True)
        for p in problems:
            print("  - " + p, flush=True)
        sys.exit(1)
    print("=== DflowEditor cross-window gate PASSED ===", flush=True)
    sys.exit(0)


################################################################################

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mode", choices=["t", "u", "o", "c"], default=None)
    ap.add_argument("--tag", default="x")
    args = ap.parse_args()
    if args.mode:
        run_child(args)
    driver()


if __name__ == "__main__":
    main()
