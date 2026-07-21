#!/usr/bin/env ork.python
################################################################################
# CodeView demo / gate harness.
#
# Windowed by default (like sibling ui tests): a two-up split of two REAL files
# embedded in DockablePanels (proves the dock-embed path) — a Python/DSL terrain
# asset on the left, a shadlang .fxv2 on the right. Highlighting, gutter, and the
# themed scrollbars are the eyeball observable.
#
#   ork.lev2/pyext/tests/ui/code_view.py                 # windowed two-up
#   ork.lev2/pyext/tests/ui/code_view.py --offscreen --capture /tmp/cv.png
#   ork.lev2/pyext/tests/ui/code_view.py --file <path> [--language py|shadlang]
#   ork.lev2/pyext/tests/ui/code_view.py --selftest      # headless highlighter checks
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import argparse

# repo root = five levels up from <root>/ork.lev2/pyext/tests/ui/code_view.py.
# Prepend THIS checkout's scripts dir so `ork.ui.code_editor` resolves from the
# same tree as this test (ork.python otherwise binds `ork` to the primary
# checkout). Harmless (redundant) once the branch is integrated.
ROOT = os.path.abspath(__file__)
for _ in range(5):
    ROOT = os.path.dirname(ROOT)
sys.path.insert(0, os.path.join(ROOT, "obt.project", "scripts"))

from orkengine.core import vec4, CrcStringProxy
from orkengine import lev2

from ork.ui.code_editor import (
    CodeView, CodeDocument, PythonHighlighter, ShadlangHighlighter, DEFAULT_CLASS)

tokens = CrcStringProxy()

################################################################################
# repo-relative real assets
PY_FILE  = os.path.join(ROOT, "obt.project/scripts/ork/hypergraph/assets/terrain/xxx.py")
FXV2_FILE = os.path.join(ROOT, "ork.data/platform_lev2/shaders/fxv2/terrain.fxv2")

BROKEN_PY = (
    "import os\n"
    "def broken(  # unbalanced paren, unterminated below\n"
    "    x = 'unterminated string\n"
    "    y = T.lpf(h, cutoff=64)\n"
    "class Half:\n"
    "    def m(self):\n"
    "        return  42 + \n"
)


################################################################################
# headless highlighter unit checks (mechanical gate; no GPU)

def _classes_on_line(runs, line_idx):
    return runs[line_idx]

def _has(runs, line_idx, substr_start_col, substr_end_col, cls, doc):
    for (c0, c1, k) in runs[line_idx]:
        if k == cls and c0 <= substr_start_col and c1 >= substr_end_col:
            return True
    return False

def selftest():
    ok = True
    def check(name, cond):
        nonlocal ok
        print(f"  [{'PASS' if cond else 'FAIL'}] {name}")
        ok = ok and cond

    print("=== highlighter selftest ===")

    # --- Python + DSL ---
    py = "@decorator\ndef foo(h):\n    return T.lpf(h, cutoff=64)\n"
    doc = CodeDocument.from_string(py, language="python")
    runs = PythonHighlighter().highlight(doc)
    line2 = doc.line(2)                  # "    return T.lpf(h, cutoff=64)"
    t_col = line2.index("T")
    lpf_col = line2.index("lpf")
    num_col = line2.index("64")
    # T -> dsl namespace, lpf -> dsl verb, 64 -> number, return -> keyword
    check("decorator '@decorator' colored decorator",
          any(k == "decorator" for (_, _, k) in runs[0]))
    check("'def' -> keyword", any(k == "keyword" for (_, _, k) in runs[1]))
    check("'foo' (after def) -> definition",
          any(k == "definition" for (_, _, k) in runs[1]))
    check("'return' -> keyword", any(k == "keyword" for (_, _, k) in runs[2]))
    check("namespace 'T' -> dsl",
          any(k == "dsl" and c0 <= t_col < c1 for (c0, c1, k) in runs[2]))
    check("verb 'lpf' -> dsl",
          any(k == "dsl" and c0 <= lpf_col < c1 for (c0, c1, k) in runs[2]))
    check("'64' -> number",
          any(k == "number" and c0 <= num_col < c1 for (c0, c1, k) in runs[2]))

    # string + comment
    py2 = "x = 'hello'  # a comment\n"
    d2 = CodeDocument.from_string(py2, language="python")
    r2 = PythonHighlighter().highlight(d2)
    check("single-quoted string -> string", any(k == "string" for (_, _, k) in r2[0]))
    check("'# comment' -> comment", any(k == "comment" for (_, _, k) in r2[0]))

    # tokenize-error resilience: must NOT raise, must return per-line list
    dbroken = CodeDocument.from_string(BROKEN_PY, language="python")
    try:
        rb = PythonHighlighter().highlight(dbroken)
        check("broken python does not crash highlighter",
              isinstance(rb, list) and len(rb) == dbroken.line_count)
        # the valid prefix should still color 'import'/'def'
        check("broken python still colors valid prefix keywords",
              any(k == "keyword" for line in rb for (_, _, k) in line))
    except Exception as e:
        check(f"broken python does not crash highlighter ({e})", False)

    # --- shadlang ---
    fx = ("// a comment\n"
          "vertex_shader vs_mono {\n"
          "  vec4 pos = position * 2.0;\n"
          '  import "skintools.i2";\n'
          "}\n")
    dfx = CodeDocument.from_string(fx, language="shadlang")
    rfx = ShadlangHighlighter().highlight(dfx)
    check("shadlang '// comment' -> comment", any(k == "comment" for (_, _, k) in rfx[0]))
    check("shadlang 'vertex_shader' -> keyword", any(k == "keyword" for (_, _, k) in rfx[1]))
    l2 = dfx.line(2)
    vec4_col = l2.index("vec4")
    check("shadlang 'vec4' -> type",
          any(k == "type" and c0 <= vec4_col < c1 for (c0, c1, k) in rfx[2]))
    check("shadlang '2.0' -> number", any(k == "number" for (_, _, k) in rfx[2]))
    check("shadlang string \"skintools.i2\" -> string",
          any(k == "string" for (_, _, k) in rfx[3]))
    check("shadlang punctuation present -> punct",
          any(k == "punct" for line in rfx for (_, _, k) in line))

    print(f"=== selftest {'PASSED' if ok else 'FAILED'} ===")
    return ok


################################################################################
# windowed / offscreen app

class App:
    def __init__(self, args):
        self.args = args
        self.ezapp = lev2.OrkEzApp.create(
            self, width=args.width, height=args.height, offscreen=args.offscreen)
        self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
        self.ezapp.topWidget.enableUiDraw()

        lg = self.ezapp.topLayoutGroup
        lg.margin = 4
        lg.clearColorStd = vec4(0.10, 0.10, 0.12, 1)

        self.codeviews = []
        self._single = bool(args.file) or args.broken or args.generated

        if self._single:
            dock = lg.makeChild(fill=True, margin=2,
                                uiclass=lev2.ui.DockablePanel, args=["code_dock"]).widget
            dock.titlebar_color = vec4(0.18, 0.16, 0.22, 1)
            canvas = dock.createChild(uiclass=lev2.ui.PrimCanvas, args=["cv"])
            canvas.bg_color = vec4(0.075, 0.080, 0.098, 1)
            canvas.draw_background = True
            self.canvasL = canvas
            self.canvasR = None
        else:
            left_item = lg.makeChild(fill=True, margin=2,
                                     uiclass=lev2.ui.DockablePanel, args=["py_dock"])
            self.left_dock = left_item.widget
            self.left_dock.titlebar_color = vec4(0.18, 0.16, 0.22, 1)
            right_item = lg.split(layout=left_item.layout, proportion=0.5,
                                  placement=tokens.RIGHT, margin=2,
                                  uiclass=lev2.ui.DockablePanel, args=["fx_dock"])
            self.right_dock = right_item.widget
            self.right_dock.titlebar_color = vec4(0.16, 0.20, 0.24, 1)
            self.canvasL = self.left_dock.createChild(uiclass=lev2.ui.PrimCanvas, args=["cvL"])
            self.canvasR = self.right_dock.createChild(uiclass=lev2.ui.PrimCanvas, args=["cvR"])
            for c in (self.canvasL, self.canvasR):
                c.bg_color = vec4(0.075, 0.080, 0.098, 1)
                c.draw_background = True

        self.frame = 0
        self.captured = False
        self._applied = False
        self._applied_frame = 0
        self._cap_pending = False
        self._cap_inflight = False
        self._cap_async = None
        self._cap_buf = None

        import signal
        signal.signal(signal.SIGINT, lambda *a: self.ezapp.signalExit())

    def onGpuInit(self, ctx):
        cvL = CodeView(self.canvasL, font_id=self.args.font)
        cvL.gpuInit(ctx)
        self.codeviews.append(cvL)
        if self.args.broken:
            cvL.load_text(BROKEN_PY, provenance=("memory", "broken.py"),
                          language="python", read_only=True)
        elif self.args.generated:
            src = self.args.file or FXV2_FILE
            with open(src, "r") as f:
                text = f.read()
            cvL.load_generated(os.path.basename(src), text,
                               language=self.args.language)
        elif self.args.file:
            cvL.load_path(self.args.file, language=self.args.language)
        else:
            cvL.load_path(PY_FILE)

        if self.canvasR is not None:
            cvR = CodeView(self.canvasR, font_id=self.args.font)
            cvR.gpuInit(ctx)
            cvR.load_path(FXV2_FILE)
            self.codeviews.append(cvR)

    def _apply_actions(self):
        cv = self.codeviews[0]
        if self.args.current_line is not None:
            cv.current_line = self.args.current_line
        if self.args.highlight is not None:
            cv.highlight_range(self.args.highlight[0], self.args.highlight[1],
                               tag="node_src")
        if self.args.scroll is not None:
            cv.scroll_to_line(self.args.scroll)
        if self.args.hscroll is not None:
            cv.scroll_to_column(self.args.hscroll)
        if self.args.drag_vthumb is not None:
            cv.drag_thumb("v", self.args.drag_vthumb)
        if self.args.drag_hthumb is not None:
            cv.drag_thumb("h", self.args.drag_hthumb)
        print(f"[code_view] first_visible_line={cv.first_visible_line} "
              f"voff={cv.vscroll.offset:.1f} hoff={cv.hscroll.offset:.1f}", flush=True)

    def onUpdate(self, updinfo):
        for c in self.codeviews:
            c.canvas.markDirty()
        self.frame += 1
        # apply scroll/highlight only once the render thread has synced scroll
        # metrics (content_size>0) — otherwise clamp() would zero a scroll offset.
        if not self._applied and self.codeviews:
            cv = self.codeviews[0]
            if cv._last_h > 0 and cv.vscroll.content_size > 0:
                self._apply_actions()
                self._applied = True
                self._applied_frame = self.frame
        if self.args.offscreen and self._applied and not self.captured:
            if self.frame >= self._applied_frame + max(6, self.args.frames):
                self._cap_pending = True
            if self.frame > self._applied_frame + self.args.frames + 300:
                print("[code_view] capture timeout", flush=True)
                self.ezapp.signalExit()

    def onGpuPostFrame(self, ctx):
        if not self.args.offscreen or self.captured:
            return
        if getattr(self, "_cap_pending", False) and not self._cap_inflight:
            rtg = ctx.FBI.main_RTG
            self._cap_buf = lev2.CaptureBuffer()
            self._cap_async = ctx.FBI.captureAsFormat(rtg.buffer(0), self._cap_buf, "RGBA8")
            self._cap_inflight = True
        elif self._cap_inflight:
            if self._cap_async is None or bool(self._cap_async.is_ready):
                self._finish()

    def _finish(self):
        import numpy
        capbuf = self._cap_buf
        w, h = capbuf.width, capbuf.height
        arr = numpy.array(capbuf, dtype=numpy.uint8).reshape(h, w, 4)
        self.captured = True
        out = self.args.capture or "/tmp/code_view.png"
        try:
            from PIL import Image
            Image.fromarray(arr[..., :3]).save(out)
            print(f"[code_view] wrote {out} ({w}x{h})", flush=True)
        except Exception as e:
            print(f"[code_view] png save failed: {e}", flush=True)
        self.ezapp.signalExit()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--offscreen", action="store_true")
    ap.add_argument("--capture", default=None)
    ap.add_argument("--file", default=None)
    ap.add_argument("--language", default=None)
    ap.add_argument("--width", type=int, default=1100)
    ap.add_argument("--height", type=int, default=720)
    ap.add_argument("--font", default="i16")
    ap.add_argument("--scroll", type=int, default=None)
    ap.add_argument("--hscroll", type=int, default=None)
    ap.add_argument("--highlight", type=int, nargs=2, default=None, metavar=("A", "B"))
    ap.add_argument("--current-line", dest="current_line", type=int, default=None)
    ap.add_argument("--drag-vthumb", dest="drag_vthumb", type=float, default=None)
    ap.add_argument("--drag-hthumb", dest="drag_hthumb", type=float, default=None)
    ap.add_argument("--generated", action="store_true")
    ap.add_argument("--broken", action="store_true")
    ap.add_argument("--frames", type=int, default=24)
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args()

    if args.selftest:
        sys.exit(0 if selftest() else 1)

    app = App(args)
    app.ezapp.mainThreadLoop()
    sys.exit(0)


if __name__ == "__main__":
    main()
