#!/usr/bin/env ork.python
################################################################################
# ork.uitest INJECT gate (U1) — offscreen + lockstep, deterministic.
#
# Exercises the synthetic-event surface end to end through a real ui::Context:
#   * ui.Event factories (make_key/make_pointer/make_wheel) + derived-code refusal
#   * app.injectUiEvent funnel (works with the GLFW pump dead)
#   * Event.code_name reverse lookup
#   * Context click-clock virtualization -> deterministic DOUBLECLICK on replay
#   * ork.uitest.play (Player + helpers) + THE inject->record round-trip (byte-equal)
#   * CONSUMER PROOF: cmd-] into a focused CodeView -> block indent (first headless chord)
#
# All sub-gates are driven from onUpdate keyed on updata.counter in lockstep mode:
# injection is SYNCHRONOUS on the update thread and in lockstep the render never
# overlaps onUpdate, so handleEvent runs race-free and the recorder's frame stamp
# is coherent (the round-trip is the proof of that rule).
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys

# Resolve ork.uitest / ork.ui from THIS checkout (ork.python otherwise binds `ork`
# to the primary checkout).
ROOT = os.path.abspath(__file__)
for _ in range(5):
    ROOT = os.path.dirname(ROOT)
sys.path.insert(0, os.path.join(ROOT, "obt.project", "scripts"))

from orkengine.core import vec4, CrcStringProxy
from orkengine import lev2

import ork.uitest as U
from ork.uitest import eventcodes
from ork.ui.code_editor import CodeView

tokens = CrcStringProxy()
ui = lev2.ui

W, H = 480, 320

################################################################################
# result accumulator

_RESULTS = []
def check(name, cond, detail=""):
    _RESULTS.append((name, bool(cond)))
    tag = "PASS" if cond else "FAIL"
    line = f"  [{tag}] {name}"
    if detail and not cond:
        line += f"   {detail}"
    print(line, flush=True)


################################################################################
# pre-app, no-GPU checks: factories + refusal (do not need a running context)

def factory_checks():
    print("=== factories / refusal ===", flush=True)
    ev = ui.Event.make_key(code=tokens.KEY_DOWN.hashed, keycode=70, ctrl=True)
    check("make_key stamps code/keycode/mods",
          int(ev.code) == int(tokens.KEY_DOWN.hashed) and ev.keycode == 70 and bool(ev.ctrl))
    check("Event.code_name reverse lookup", ev.code_name == "KEY_DOWN")

    p = ui.Event.make_pointer(code=tokens.PUSH.hashed, x=12, y=34,
                              screen_w=W, screen_h=H, left=True)
    check("make_pointer stamps full field set",
          p.x == 12 and p.y == 34 and bool(p.left)
          and int(p.screen_dim.x) == W and int(p.screen_dim.y) == H)
    check("make_pointer unit coords from x,y/screen_dim",
          abs(p.unit_pos.x - 12.0 / W) < 1e-5 and abs(p.unit_pos.y - 34.0 / H) < 1e-5)

    wv = ui.Event.make_wheel(dx=0, dy=1)
    check("make_wheel ×10 primary convention", wv.wheel_y == 10 and wv.wheel_x == 0)

    for nm in ("DOUBLECLICK", "BEGIN_DRAG", "END_DRAG", "MOUSE_ENTER", "MOUSE_LEAVE"):
        crc = getattr(tokens, nm).hashed
        raised = False
        try:
            ui.Event.make_pointer(code=crc, x=1, y=1, screen_w=W, screen_h=H)
        except ValueError as e:
            raised = nm in str(e)
        check(f"factory refuses derived {nm}", raised)


################################################################################
# the offscreen lockstep app

class InjectGate:
    def __init__(self):
        self.ezapp = lev2.OrkEzApp.create(
            self,
            width=W,
            height=H,
            freerun=False,       # lockstep / synchronous (forces offscreen)
            target_ups=60.0,
            target_fps=60.0)
        self.ezapp.topWidget.enableUiDraw()

        lg = self.ezapp.topLayoutGroup
        lg.margin = 4
        lg.clearColorStd = vec4(0.10, 0.10, 0.12, 1)

        # two side-by-side PrimCanvases: left = pointer target, right = CodeView host
        left_item = lg.makeChild(fill=True, margin=2,
                                 uiclass=lev2.ui.PrimCanvas, args=["target"])
        self.target = left_item.widget
        right_item = lg.split(layout=left_item.layout, proportion=0.5,
                              placement=tokens.RIGHT, margin=2,
                              uiclass=lev2.ui.PrimCanvas, args=["cvcanvas"])
        self.cv_canvas = right_item.widget

        # target widget records what its handler receives
        self._rec = []
        def _target_handler(ev):
            self._rec.append((ev.code_name, ev.x, ev.y))
            r = ui.HandlerResult()
            r.setHandler(self.target)
            return r
        self.target.onUiEvent = _target_handler

        self.codeview = None
        self.ready = False
        self.ready_counter = 0
        self._done_phases = set()
        self._finished = False
        # round-trip state
        self._rt_authored = None
        self._rt_recorder = None
        self._rt_player = None
        self._rt_c0 = None
        # doubleclick run streams
        self._dc_stream1 = None
        self._dc_stream2 = None

    ############################################################################

    def onGpuInit(self, ctx):
        self.codeview = CodeView(self.cv_canvas, font_id="i16")
        self.codeview.gpuInit(ctx)
        self.codeview.load_text("hello world\nsecond line\n",
                                provenance=("memory", "inject_gate.py"),
                                language="python", read_only=False)

    ############################################################################

    def _center(self, w):
        return (w.x + w.width // 2, w.y + w.height // 2)

    def _clear_rec(self):
        self._rec = []

    def _codes(self, name):
        return int(getattr(tokens, name).hashed)

    ############################################################################

    # deterministic virtual click-clock: 0.1 virtual-seconds per frame. Phases are
    # spaced so a fresh PUSH clears the 0.75s double-click re-arm and the paired
    # PUSH lands inside the 0.5s window (see _drive schedule).
    VIRTUAL_DT = 0.1

    def onUpdate(self, updata):
        counter = int(updata.counter)

        # virtualize the ui click clock every frame (independent of engine UPS /
        # wall clock) so DOUBLECLICK derivation is deterministic and reproducible.
        ctx = self.ezapp.uicontext
        if ctx is not None:
            ctx.virtual_time_enabled = True
            ctx.virtual_time = counter * self.VIRTUAL_DT

        if not self.ready:
            if self.target.width > 0 and self.cv_canvas.width > 0:
                self.ready = True
                self.ready_counter = counter
            else:
                if counter > 600:
                    check("layout ready within 600 frames", False)
                    self._finish()
                return

        rel = counter - self.ready_counter
        try:
            self._drive(rel, updata)
        except Exception as e:
            import traceback
            traceback.print_exc()
            check(f"phase rel={rel} raised: {e}", False)
            self._finish()

    ############################################################################

    def _run_once(self, key, fn):
        if key in self._done_phases:
            return
        self._done_phases.add(key)
        fn()

    def _drive(self, rel, updata):
        tx, ty = self._center(self.target)
        cvx, cvy = self._center(self.cv_canvas)

        # --- Phase A: PUSH/RELEASE reach the handler + focus acquisition ---
        if rel == 2:
            def A():
                self._clear_rec()
                U.move(self.ezapp, tx, ty, W, H)          # mouse focus onto target
                U.push(self.ezapp, tx, ty, W, H)
                U.release(self.ezapp, tx, ty, W, H)
                names = [c for (c, x, y) in self._rec]
                push_hit = any(c == "PUSH" and x == tx and y == ty for (c, x, y) in self._rec)
                rel_hit = any(c == "RELEASE" and x == tx and y == ty for (c, x, y) in self._rec)
                check("PUSH reaches target handler at right coords", push_hit,
                      str(self._rec))
                check("RELEASE reaches target handler at right coords", rel_hit,
                      str(self._rec))
                check("focus acquisition (mouse focus on target after MOVE)",
                      self.ezapp.uicontext.hasMouseFocus(self.target))
            self._run_once("A", A)

        # --- Phase B: raw drag -> Context derives BEGIN_DRAG/DRAG/END_DRAG ---
        elif rel == 14:
            def B():
                self._clear_rec()
                U.drag(self.ezapp, tx - 20, ty - 10, tx + 20, ty + 10, W, H, steps=4)
                names = [c for (c, x, y) in self._rec]
                check("drag: widget sees BEGIN_DRAG", "BEGIN_DRAG" in names, str(names))
                check("drag: widget sees DRAG", "DRAG" in names, str(names))
                check("release: widget sees END_DRAG", "END_DRAG" in names, str(names))
            self._run_once("B", B)

        # --- Phase C run1: two frame-paced PUSHes + virtual clock -> DOUBLECLICK ---
        # push1 gets a clean >0.5s gap (fresh single click); push2 lands 0.2s later
        # (inside the 0.5s window) and past the 0.75s re-arm -> DOUBLECLICK.
        elif rel == 26:
            self._run_once("C1a", lambda: (self._clear_rec(),
                                           U.push(self.ezapp, tx, ty, W, H)))
        elif rel == 28:
            def C1b():
                U.push(self.ezapp, tx, ty, W, H)
                self._dc_stream1 = [c for (c, x, y) in self._rec if c in ("PUSH", "DOUBLECLICK")]
                check("DOUBLECLICK derived from frame-paced PUSHes (run1)",
                      "DOUBLECLICK" in self._dc_stream1, str(self._dc_stream1))
            self._run_once("C1b", C1b)

        # --- Phase C run2 (well separated) -> identical stream ---
        elif rel == 42:
            self._run_once("C2a", lambda: (self._clear_rec(),
                                           U.push(self.ezapp, tx, ty, W, H)))
        elif rel == 44:
            def C2b():
                U.push(self.ezapp, tx, ty, W, H)
                self._dc_stream2 = [c for (c, x, y) in self._rec if c in ("PUSH", "DOUBLECLICK")]
                check("DOUBLECLICK replay stream identical across two runs",
                      self._dc_stream1 == self._dc_stream2 and "DOUBLECLICK" in (self._dc_stream2 or []),
                      f"{self._dc_stream1} != {self._dc_stream2}")
            self._run_once("C2b", C2b)

        # --- Phase D: KEY_DOWN -> isKeyDown True; KEY_UP -> False ---
        elif rel == 50:
            def D1():
                U.key_down(self.ezapp, 70)   # 'F'
                check("KEY_DOWN -> ctx.isKeyDown True",
                      self.ezapp.uicontext.isKeyDown(70))
            self._run_once("D1", D1)
        elif rel == 51:
            def D2():
                U.key_up(self.ezapp, 70)
                check("KEY_UP -> ctx.isKeyDown False",
                      not self.ezapp.uicontext.isKeyDown(70))
            self._run_once("D2", D2)

        # --- Phase E: THE inject->record round-trip (byte-equal) ---
        elif rel == 56:
            def E_setup():
                # authored session (built via the same factories the recorder reads)
                sess = U.Session(screen_dim=[W, H])
                sess.add_event(0, ui.Event.make_pointer(
                    code=self._codes("PUSH"), x=tx, y=ty, screen_w=W, screen_h=H, left=True))
                sess.add_event(1, ui.Event.make_pointer(
                    code=self._codes("RELEASE"), x=tx, y=ty, screen_w=W, screen_h=H))
                sess.add_event(2, ui.Event.make_pointer(
                    code=self._codes("MOVE"), x=tx + 8, y=ty + 4, screen_w=W, screen_h=H))
                sess.add_event(3, ui.Event.make_key(
                    code=self._codes("KEY_DOWN"), keycode=65, shift=True))
                sess.add_event(4, ui.Event.make_key(
                    code=self._codes("KEY_UP"), keycode=65, shift=True))
                sess.add_event(5, ui.Event.make_wheel(dx=0, dy=1))
                self._rt_authored = sess
                self._rt_recorder = U.Recorder(screen_dim=[W, H])
                self._rt_recorder.attach(self.ezapp)
                self._rt_player = U.Player(self.ezapp, sess, virtualize_clock=False)
                self._rt_c0 = None   # latched on the first driving frame -> rf starts at 0
            self._run_once("E_setup", E_setup)

        elif self._rt_player is not None and not self._rt_player.done and rel >= 56:
            # relative-frame shim so recorded frames match the authored 0..K frames.
            if self._rt_c0 is None:
                self._rt_c0 = int(updata.counter)
            rf = int(updata.counter) - self._rt_c0
            shim = _FrameShim(rf, updata.absolutetime)
            self._rt_recorder.on_update(shim)
            self._rt_player.on_update(shim)

        elif self._rt_recorder is not None and self._rt_player is not None \
                and self._rt_player.done and "E_check" not in self._done_phases:
            def E_check():
                recorded = self._rt_recorder.stop()   # detaches taps
                authored_bytes = self._rt_authored.to_bytes()
                recorded_bytes = recorded.to_bytes()
                check("round-trip: recorded session BYTE-EQUAL to authored",
                      recorded_bytes == authored_bytes,
                      f"\nauthored={authored_bytes!r}\nrecorded={recorded_bytes!r}")
            self._run_once("E_check", E_check)

        # --- Phase F: CONSUMER PROOF cmd-] -> CodeView block indent ---
        elif rel >= 72 and "F" not in self._done_phases:
            def F():
                before = self.codeview.document.line(0)
                # position the injection cursor over the CodeView, then the chord.
                U.move(self.ezapp, cvx, cvy, W, H)
                U.key_chord(self.ezapp, 93, mods={"super_": True})   # cmd-] -> indent
                after = self.codeview.document.line(0)
                check("cmd-] into focused CodeView indents line 0 (block indent)",
                      after == "  " + before,
                      f"before={before!r} after={after!r}")
            self._run_once("F", F)
            self._finish()

        elif rel > 700:
            check("gate completed before frame budget", False)
            self._finish()

    ############################################################################

    def _finish(self):
        if self._finished:
            return
        self._finished = True
        # detach a still-attached round-trip recorder (defensive)
        if self._rt_recorder is not None and "E_check" not in self._done_phases:
            try:
                self._rt_recorder.stop()
            except Exception:
                pass
        self.ezapp.signalExit()


class _FrameShim:
    __slots__ = ("counter", "absolutetime")
    def __init__(self, counter, absolutetime):
        self.counter = counter
        self.absolutetime = absolutetime


################################################################################

def main():
    factory_checks()
    app = InjectGate()
    app.ezapp.mainThreadLoop()

    print("=== SUMMARY ===", flush=True)
    npass = sum(1 for _, ok in _RESULTS if ok)
    ntot = len(_RESULTS)
    for name, ok in _RESULTS:
        if not ok:
            print(f"  FAILED: {name}", flush=True)
    print(f"uitest_inject: {npass}/{ntot} checks passed", flush=True)
    sys.exit(0 if (npass == ntot and ntot > 0) else 1)


if __name__ == "__main__":
    main()
