#!/usr/bin/env ork.python
################################################################################
# CodeView S2 (editing core) gate harness.
#
#   ork.lev2/pyext/tests/ui/code_edit.py --selftest        # headless edit-API battery
#   ork.lev2/pyext/tests/ui/code_edit.py                    # windowed (type into a file)
#   ork.lev2/pyext/tests/ui/code_edit.py --offscreen --capture /tmp/ce.png
#
# --selftest drives every public edit method (no GPU) with exact buffer-text /
# caret / selection assertions, undo/redo byte-exactness, read-only refusal, and a
# clipboard round-trip via whichever backend is live. The offscreen mode scripts a
# caret + selection and captures a PNG so the caret quad and selection bands are an
# eyeball observable for the coordinator / owner windowed pass.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import argparse

# Prepend THIS checkout's scripts dir so `ork.ui.code_editor` resolves from the
# same tree as this test (ork.python otherwise binds `ork` to the primary checkout).
ROOT = os.path.abspath(__file__)
for _ in range(5):
    ROOT = os.path.dirname(ROOT)
sys.path.insert(0, os.path.join(ROOT, "obt.project", "scripts"))

from ork.ui.code_editor import CodeView, CodeDocument

PY_FILE = os.path.join(ROOT, "obt.project/scripts/ork/hypergraph/assets/terrain/xxx.py")


################################################################################
# headless edit-API battery (no GPU): a plain object stands in for the canvas —
# construction + every edit op touch only CodeView/CodeDocument, never the canvas.

class _HeadlessCanvas:
    supersample = 0
    def markDirty(self):
        pass


class _KeyEv:
    """Minimal stand-in for the engine key event handleKeyDown reads (keycode +
    modifier flags) so the gate can exercise the real key-routing path headlessly."""
    def __init__(self, keycode, shift=False, super=False, ctrl=False, alt=False):
        self.keycode = keycode
        self.shift = shift
        self.super = super
        self.ctrl = ctrl
        self.alt = alt


def _view(text="", read_only=False, generated=False, language="python",
          w=420, h=220, adv=8.0, lh=18.0):
    if generated:
        doc = CodeDocument.generated("gen", text, language=language)
    else:
        doc = CodeDocument.from_string(text, language=language, read_only=read_only)
    cv = CodeView(_HeadlessCanvas(), document=doc)
    # inject the geometry gpuInit would supply so scroll-follow / caret math runs
    cv._adv_w, cv._line_h = adv, lh
    cv._last_w, cv._last_h = w, h
    cv._sync_scroll_metrics()
    return cv


def selftest():
    results = []
    def check(name, cond, detail=""):
        results.append((name, bool(cond), detail))
        print(f"  [{'PASS' if cond else 'FAIL'}] {name}"
              + (f"   {detail}" if (detail and not cond) else ""))

    import ork.ui.code_editor as _ce_mod
    print("=== S2 edit-API selftest ===")
    print(f"    [module] ork.ui.code_editor <- {_ce_mod.__file__}")

    # G1 insert_text at caret -> exact buffer
    cv = _view("abcd")
    cv.set_caret(0, 2)
    cv.insert_text("XY")
    check("G1 insert_text exact buffer", cv.document.text == "abXYcd",
          repr(cv.document.text))
    check("G1b caret after insert", cv.caret == (0, 4), str(cv.caret))

    # G2 single-char inserts coalesce into ONE undo group
    cv = _view("")
    for ch in "hello":
        cv.insert_text(ch)
    check("G2a typed run buffer", cv.document.text == "hello", repr(cv.document.text))
    cv.undo()
    check("G2b one undo removes whole coalesced run", cv.document.text == "",
          repr(cv.document.text))
    cv.redo()
    check("G2c redo restores run", cv.document.text == "hello", repr(cv.document.text))

    # G3 newline_with_autoindent copies leading whitespace
    cv = _view("    foo = 1")
    cv.set_caret(0, len("    foo = 1"))
    cv.newline_with_autoindent()
    check("G3 autoindent copies indent", cv.document.text == "    foo = 1\n    ",
          repr(cv.document.text))
    check("G3b caret at indent end", cv.caret == (1, 4), str(cv.caret))

    # G4 newline adds a level after a python ':'
    cv = _view("  if x:", language="python")
    cv.set_caret(0, len("  if x:"))
    cv.newline_with_autoindent()
    check("G4 autoindent +1 level after ':'", cv.document.text == "  if x:\n    ",
          repr(cv.document.text))

    # G5 tab inserts spaces to the next tab stop (indent_width=2)
    cv = _view("x")
    cv.set_caret(0, 1)
    cv.insert_tab()
    check("G5 tab -> spaces to tab stop", cv.document.text == "x ",
          repr(cv.document.text))          # col 1 -> next stop col 2 == 1 space
    cv2 = _view("")
    cv2.insert_tab()
    check("G5b tab at col0 -> indent_width spaces", cv2.document.text == "  ",
          repr(cv2.document.text))

    # G6 backspace within a line
    cv = _view("abcd")
    cv.set_caret(0, 3)
    cv.backspace()
    check("G6 backspace within line", cv.document.text == "abd", repr(cv.document.text))

    # G7 backspace at col0 joins the previous line
    cv = _view("ab\ncd")
    cv.set_caret(1, 0)
    cv.backspace()
    check("G7 backspace joins lines", cv.document.text == "abcd", repr(cv.document.text))
    check("G7b caret at join", cv.caret == (0, 2), str(cv.caret))

    # G8 del_forward within a line
    cv = _view("abcd")
    cv.set_caret(0, 1)
    cv.del_forward()
    check("G8 del_forward within line", cv.document.text == "acd", repr(cv.document.text))

    # G9 del_forward at EOL joins the next line
    cv = _view("ab\ncd")
    cv.set_caret(0, 2)
    cv.del_forward()
    check("G9 del_forward joins next line", cv.document.text == "abcd",
          repr(cv.document.text))

    # G10 delete_range multi-line exact
    cv = _view("line0\nline1\nline2")
    cv.delete_range((0, 3), (2, 2))
    check("G10 delete_range multi-line", cv.document.text == "linne2",
          repr(cv.document.text))

    # G11 set_caret clamps + current_line follows
    cv = _view("ab\ncd")
    cv.set_caret(9, 9)
    check("G11 set_caret clamps to doc end", cv.caret == (1, 2), str(cv.caret))
    check("G11b current_line follows caret", cv.current_line == 1, str(cv.current_line))

    # G12 move_caret across line boundaries
    cv = _view("ab\ncd")
    cv.set_caret(0, 2)
    cv.move_caret("right")
    check("G12 right at EOL -> next line start", cv.caret == (1, 0), str(cv.caret))
    cv.move_caret("left")
    check("G12b left at BOL -> prev line end", cv.caret == (0, 2), str(cv.caret))

    # G13 move up/down preserves the goal column
    cv = _view("longline__\nx\nlongline__")
    cv.set_caret(0, 8)
    cv.move_caret("down")                    # onto short line -> col clamps to 1
    check("G13 down clamps to short line", cv.caret == (1, 1), str(cv.caret))
    cv.move_caret("down")                    # back to long line -> goal col restored
    check("G13b goal column restored", cv.caret == (2, 8), str(cv.caret))

    # G14 select_range + selected_text
    cv = _view("hello world")
    cv.select_range((0, 6), (0, 11))
    check("G14 selected_text exact", cv.selected_text() == "world",
          repr(cv.selected_text()))
    check("G14b has_selection", cv.has_selection())

    # G15 double-click word select
    cv = _view("alpha beta gamma")
    cv.select_word_at(0, 8)                   # inside 'beta'
    check("G15 word-select bounds", cv.selection == ((0, 6), (0, 10)), str(cv.selection))
    check("G15b word-select text", cv.selected_text() == "beta", repr(cv.selected_text()))

    # G16 typing over a selection replaces it (single undo unit)
    cv = _view("hello world")
    cv.select_range((0, 6), (0, 11))
    cv.insert_text("there")
    check("G16 insert replaces selection", cv.document.text == "hello there",
          repr(cv.document.text))
    cv.undo()
    check("G16b undo restores replaced text", cv.document.text == "hello world",
          repr(cv.document.text))

    # G17 undo restores byte-exact text + caret
    cv = _view("abc")
    before_text, before_caret = cv.document.text, cv.caret
    cv.set_caret(0, 3)
    cv.insert_text("Z")
    cv.undo()
    check("G17 undo byte-exact text", cv.document.text == before_text,
          repr(cv.document.text))
    check("G17b undo restores caret", cv.caret == (0, 3), str(cv.caret))

    # G18 redo restores byte-exact text + caret
    cv.redo()
    check("G18 redo byte-exact text", cv.document.text == "abcZ", repr(cv.document.text))
    check("G18b redo restores caret", cv.caret == (0, 4), str(cv.caret))

    # G19 read-only / generated buffers refuse edits loudly (False + unchanged)
    gen = _view("shader = 1", generated=True)
    gtext = gen.document.text
    r = gen.insert_text("x")
    check("G19 generated edit refused (False)", r is False, str(r))
    check("G19b generated text unchanged", gen.document.text == gtext,
          repr(gen.document.text))
    check("G19c refusal sets loud status", "refused" in gen.status, gen.status)
    ro = _view("locked", read_only=True)
    check("G19d read-only backspace refused", ro.backspace() is False)

    # G20 clipboard copy/paste round-trip (backend reported by caller)
    cv = _view("copy me please")
    cv.select_range((0, 0), (0, 7))
    cv.copy()
    cv.set_caret(0, len(cv.document.line(0)))
    cv.paste()
    check("G20 clipboard round-trip", cv.document.text == "copy me pleasecopy me",
          repr(cv.document.text))

    # G21 cut removes the selection and loads the clipboard
    cv = _view("aXXXb")
    cv.select_range((0, 1), (0, 4))
    cv.cut()
    check("G21 cut removes selection", cv.document.text == "ab", repr(cv.document.text))
    cv.set_caret(0, 2)
    cv.paste()
    check("G21b cut text pastes back", cv.document.text == "abXXX",
          repr(cv.document.text))

    # G22 scroll-follow after a far set_caret (quote first_visible_line)
    cv = _view("\n".join(f"row{i}" for i in range(300)))
    cv.set_caret(200, 0)
    fvl = cv.first_visible_line
    vis = cv._visible_line_count()
    check("G22 far caret is scrolled into view", fvl <= 200 <= fvl + vis,
          f"first_visible_line={fvl} visible={vis} caret_line=200")
    print(f"    [scroll-follow] first_visible_line={fvl} visible_lines={vis} caret_line=200")

    # G23 selection_spans_for_line reports per-line band spans (multi-line)
    cv = _view("aaaa\nbbbb\ncccc")
    cv.select_range((0, 2), (2, 2))
    s0 = cv.selection_spans_for_line(0)
    s1 = cv.selection_spans_for_line(1)
    s2 = cv.selection_spans_for_line(2)
    check("G23 selection span line0 (start->EOL+nl)", s0 == (2, 5), str(s0))
    check("G23b selection span line1 (full+nl)", s1 == (0, 5), str(s1))
    check("G23c selection span line2 (BOL->end)", s2 == (0, 2), str(s2))

    # G24 caret_screen_pos math after set_caret (pixel of the caret quad)
    cv = _view("abcdef")
    cv.set_caret(0, 3)
    gx = cv._gutter_width()
    x, y = cv.caret_screen_pos()
    from ork.ui.code_editor import CONTENT_PAD_L, HEADER_H
    check("G24 caret x pixel", abs(x - (gx + CONTENT_PAD_L + 3 * 8.0)) < 0.01, f"x={x}")
    check("G24b caret y pixel", abs(y - HEADER_H) < 0.01, f"y={y}")

    # G25 multi-line indent -> exact buffer (indent_width=2 spaces per non-blank line)
    cv = _view("a\nb\nc")
    cv.select_range((0, 0), (2, 1))
    cv.indent_selection()
    check("G25 multi-line indent exact", cv.document.text == "  a\n  b\n  c",
          repr(cv.document.text))

    # G26 dedent exact: a line with fewer than indent_width leading spaces (" baz")
    # and an empty line inside the block are both handled sanely
    cv = _view("    foo\n  bar\n\n baz")
    cv.select_range((0, 0), (3, 4))
    cv.dedent_selection()
    check("G26 dedent short-line + empty-line exact",
          cv.document.text == "  foo\nbar\n\nbaz", repr(cv.document.text))

    # G27 no-selection op works on the caret line only; caret tracks the shift
    cv = _view("x\ny\nz")
    cv.set_caret(1, 0)
    cv.indent_selection()
    check("G27 caret-line indent exact", cv.document.text == "x\n  y\nz",
          repr(cv.document.text))
    check("G27b caret shifts with line", cv.caret == (1, 2), str(cv.caret))
    check("G27c no selection created", not cv.has_selection())

    # G28 selection + caret track the per-line deltas; repeated op keeps the block
    cv = _view("aaaa\nbbbb\ncccc")
    cv.select_range((0, 1), (2, 3))
    cv.indent_selection()
    check("G28 selection tracks op", cv.selection == ((0, 3), (2, 5)),
          str(cv.selection))
    check("G28b caret tracks op", cv.caret == (2, 5), str(cv.caret))
    cv.indent_selection()
    check("G28c repeated op keeps logical selection",
          cv.selection == ((0, 5), (2, 7)), str(cv.selection))

    # G29 single-undo round-trip: ONE record, byte-exact text + selection + caret
    cv = _view("p\nq\nr")
    cv.select_range((0, 0), (2, 1))
    before_text, before_sel, before_caret = cv.document.text, cv.selection, cv.caret
    n0 = len(cv._undo)
    cv.indent_selection()
    check("G29 block op is ONE undo record", len(cv._undo) == n0 + 1,
          f"{n0}->{len(cv._undo)}")
    cv.undo()
    check("G29b undo text byte-exact", cv.document.text == before_text,
          repr(cv.document.text))
    check("G29c undo restores selection", cv.selection == before_sel,
          str(cv.selection))
    check("G29d undo restores caret", cv.caret == before_caret, str(cv.caret))

    # G30 redo re-applies text + the shifted selection + caret
    cv.redo()
    check("G30 redo text", cv.document.text == "  p\n  q\n  r", repr(cv.document.text))
    check("G30b redo restores selection", cv.selection == ((0, 2), (2, 3)),
          str(cv.selection))
    check("G30c redo restores caret", cv.caret == (2, 3), str(cv.caret))

    # G31 read-only + generated buffers refuse block ops loudly (False + unchanged)
    ro = _view("  a\n  b", read_only=True)
    rotext = ro.document.text
    check("G31 indent refused on read-only", ro.indent_selection() is False)
    check("G31b dedent refused on read-only", ro.dedent_selection() is False)
    check("G31c read-only buffer unchanged", ro.document.text == rotext,
          repr(ro.document.text))
    check("G31d refusal sets loud status", "refused" in ro.status, ro.status)
    gen = _view("  a\n  b", generated=True)
    check("G31e indent refused on generated", gen.indent_selection() is False)

    # G32 TAB with a MULTI-LINE selection routes to indent_selection (via key path)
    cv = _view("a\nb")
    cv.select_range((0, 0), (1, 1))
    cv.handleKeyDown(_KeyEv(258))
    check("G32 TAB multi-line selection indents", cv.document.text == "  a\n  b",
          repr(cv.document.text))

    # G33 TAB single-line/no-selection unchanged: tab-stop insert / replace selection
    cv = _view("x")
    cv.set_caret(0, 1)
    cv.handleKeyDown(_KeyEv(258))
    check("G33 TAB no-selection -> tab-stop spaces", cv.document.text == "x ",
          repr(cv.document.text))
    cv = _view("abcd")
    cv.select_range((0, 1), (0, 3))
    cv.handleKeyDown(_KeyEv(258))
    check("G33b TAB single-line selection replaces", cv.document.text == "a d",
          repr(cv.document.text))

    # G34 SHIFT-TAB with a multi-line selection routes to dedent_selection
    cv = _view("  a\n  b")
    cv.select_range((0, 0), (1, 1))
    cv.handleKeyDown(_KeyEv(258, shift=True))
    check("G34 SHIFT-TAB multi-line dedents", cv.document.text == "a\nb",
          repr(cv.document.text))

    # G35 cmd-] / cmd-[ chords indent / dedent the block (via key path)
    cv = _view("a\nb")
    cv.select_range((0, 0), (1, 1))
    cv.handleKeyDown(_KeyEv(93, super=True))
    check("G35 cmd-] indents", cv.document.text == "  a\n  b", repr(cv.document.text))
    cv.handleKeyDown(_KeyEv(91, super=True))
    check("G35b cmd-[ dedents", cv.document.text == "a\nb", repr(cv.document.text))

    npass = sum(1 for _, ok, _ in results if ok)
    ntot = len(results)
    print(f"=== S2 selftest {npass}/{ntot} "
          f"{'PASSED' if npass == ntot else 'FAILED'} ===")
    return npass == ntot


################################################################################
# offscreen / windowed capture app — scripts a caret + selection so the caret quad
# and selection bands are a visual observable.

class App:
    def __init__(self, args):
        from orkengine.core import vec4
        from orkengine import lev2
        self.lev2 = lev2
        self.args = args
        self.ezapp = lev2.OrkEzApp.create(
            self, width=args.width, height=args.height, offscreen=args.offscreen)
        self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
        self.ezapp.topWidget.enableUiDraw()
        lg = self.ezapp.topLayoutGroup
        lg.margin = 4
        lg.clearColorStd = vec4(0.10, 0.10, 0.12, 1)
        dock = lg.makeChild(fill=True, margin=2,
                            uiclass=lev2.ui.DockablePanel, args=["code_dock"]).widget
        dock.titlebar_color = vec4(0.18, 0.16, 0.22, 1)
        canvas = dock.createChild(uiclass=lev2.ui.PrimCanvas, args=["cv"])
        canvas.bg_color = vec4(0.075, 0.080, 0.098, 1)
        canvas.draw_background = True
        self.canvas = canvas
        self.cv = None
        self.frame = 0
        self.captured = False
        self._applied = False
        self._applied_frame = 0
        self._cap_inflight = False
        self._cap_async = None
        self._cap_buf = None
        import signal
        signal.signal(signal.SIGINT, lambda *a: self.ezapp.signalExit())

    def onGpuInit(self, ctx):
        self.cv = CodeView(self.canvas, font_id=self.args.font)
        self.cv.gpuInit(ctx)
        # editing needs a mutable buffer -> load the real file text as editable.
        src = self.args.file or PY_FILE
        with open(src, "r") as f:
            self.cv.load_text(f.read(), provenance=("file", src),
                              language="python", read_only=False)

    def _apply(self):
        cv = self.cv
        cv.focus()
        cv.set_caret(self.args.caret_line, self.args.caret_col)
        if self.args.select is not None:
            a, b, c, d = self.args.select
            cv.select_range((a, b), (c, d))
        if self.args.block_op == "indent":
            cv.indent_selection()
        elif self.args.block_op == "dedent":
            cv.dedent_selection()
        cv._reset_blink()
        print(f"[code_edit] block_op={self.args.block_op} caret={cv.caret} "
              f"selection={cv.selection} first_visible_line={cv.first_visible_line} "
              f"focused={cv.focused}", flush=True)

    def onUpdate(self, updinfo):
        self.canvas.markDirty()
        self.frame += 1
        if not self._applied and self.cv is not None:
            if self.cv._last_h > 0 and self.cv.vscroll.content_size > 0:
                self._apply()
                self._applied = True
                self._applied_frame = self.frame
        if self.args.offscreen and self._applied and not self.captured:
            if self.frame >= self._applied_frame + 8:
                self._cap_pending = True
            if self.frame > self._applied_frame + 400:
                print("[code_edit] capture timeout", flush=True)
                self.ezapp.signalExit()

    def onGpuPostFrame(self, ctx):
        if not self.args.offscreen or self.captured:
            return
        if getattr(self, "_cap_pending", False) and not self._cap_inflight:
            rtg = ctx.FBI.main_RTG
            self._cap_buf = self.lev2.CaptureBuffer()
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
        out = self.args.capture or "/tmp/code_edit.png"
        try:
            from PIL import Image
            Image.fromarray(arr[..., :3]).save(out)
            print(f"[code_edit] wrote {out} ({w}x{h})", flush=True)
        except Exception as e:
            print(f"[code_edit] png save failed: {e}", flush=True)
        self.ezapp.signalExit()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--offscreen", action="store_true")
    ap.add_argument("--capture", default=None)
    ap.add_argument("--file", default=None)
    ap.add_argument("--width", type=int, default=1000)
    ap.add_argument("--height", type=int, default=680)
    ap.add_argument("--font", default="i16")
    ap.add_argument("--caret-line", dest="caret_line", type=int, default=6)
    ap.add_argument("--caret-col", dest="caret_col", type=int, default=4)
    ap.add_argument("--select", type=int, nargs=4, default=[4, 0, 6, 8],
                    metavar=("L0", "C0", "L1", "C1"))
    ap.add_argument("--block-op", dest="block_op",
                    choices=["none", "indent", "dedent"], default="none")
    args = ap.parse_args()

    if args.selftest:
        sys.exit(0 if selftest() else 1)

    app = App(args)
    app.ezapp.mainThreadLoop()
    sys.exit(0)


if __name__ == "__main__":
    main()
