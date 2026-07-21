################################################################################
# code_editor - READ-ONLY GPU code viewer widget (PrimCanvas composite).
#
# CodeView renders a line-based document as a monospace glyph grid with syntax
# highlighting (Python + house DSLs via stdlib tokenize; shadlang via a lexer
# built at runtime FROM ork.data/grammars/shadlang.scanner), a right-aligned
# line-number gutter, current-line tint, region-highlight bands, and themed
# vertical + horizontal scrollbars (thumb drag + track paging + wheel).
#
# S2 SCOPE (this file): the editing core on top of the S1 viewer. API-FIRST — every
# edit is a public method on CodeView/CodeDocument (insert_text, backspace, del_forward,
# newline_with_autoindent, insert_tab, set_caret/move_caret/select_*, cut/copy/paste,
# undo/redo); the key handlers (handleKeyDown/Up) are thin, WIDGET-FOCUS-LOCAL wrappers.
# read-only / generated buffers refuse every mutator loudly (status + return False).
#   * CodeDocument holds a MUTABLE line list + pure text primitives (insert/delete/
#     get_range) that undo/redo replays; edits mutate lines then rehighlight()
#     (whole-buffer re-tokenize; files are small). Multi-line strings/comments already
#     handled by the whole-buffer highlighter.
#   * line_col_at(x,y) maps a screen point to a (line,col) doc position — the caret
#     placement query for a click.
#   * highlight_range()/clear_highlight() + scroll_to_range() are the node<->source
#     cross-workflow seams (highlight a node's source region; scroll it into view).
#
# Retained rendering: primitives rebuild only on load / scroll / resize / highlight
# change (idle settles to zero rebuilds). TextPrimitive is screen-fixed (ignores
# layer transforms), so every visible line is emitted in screen space each rebuild;
# only the visible window is emitted, so a multi-thousand-line file stays cheap.
#
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import io
import os
import keyword
import tokenize
from orkengine.core import vec2, vec4, mtx4, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

################################################################################
# Dark editor palette. Values follow the node_editor dark precedent; the scrollbar
# thumb mirrors ork::ui::ScrollController's StyleDatabase pill (white a=0.4, r=3).
# color-class keys are the contract between highlighters and the widget.
################################################################################

PALETTE = {
    "bg":           vec4(0.075, 0.080, 0.098, 1.0),
    "gutter_bg":    vec4(0.108, 0.114, 0.138, 1.0),
    "gutter_gen":   vec4(0.140, 0.110, 0.075, 1.0),   # generated-buffer gutter tint
    "header_bg":    vec4(0.055, 0.060, 0.075, 1.0),
    "text":         vec4(0.800, 0.820, 0.870, 1.0),   # default / identifiers / punctuation
    "gutter":       vec4(0.400, 0.420, 0.480, 1.0),   # line numbers (dimmer)
    "gutter_cur":   vec4(0.780, 0.800, 0.860, 1.0),   # current line number (brighter)
    "keyword":      vec4(0.905, 0.545, 0.740, 1.0),
    "string":       vec4(0.560, 0.800, 0.450, 1.0),
    "number":       vec4(0.860, 0.680, 0.360, 1.0),
    "comment":      vec4(0.430, 0.470, 0.535, 1.0),
    "decorator":    vec4(0.560, 0.760, 0.960, 1.0),
    "definition":   vec4(0.470, 0.830, 0.860, 1.0),
    "dsl":          vec4(0.980, 0.720, 0.300, 1.0),   # DSL namespaces + verbs (pops)
    "type":         vec4(0.450, 0.790, 0.910, 1.0),   # shadlang types
    "punct":        vec4(0.610, 0.640, 0.700, 1.0),   # shadlang punctuation
    "current_line": vec4(1.0, 1.0, 1.0, 0.050),
    "highlight":    vec4(0.300, 0.560, 1.000, 0.160), # default region band
    "selection":    vec4(0.230, 0.470, 0.860, 0.340), # S2 selection band (distinct from highlight)
    "caret":        vec4(0.960, 0.970, 0.560, 0.950), # S2 caret quad
    "header_txt":   vec4(0.720, 0.760, 0.820, 1.0),
    "badge_ro":     vec4(0.960, 0.620, 0.360, 1.0),   # read-only / generated badge
    "scroll_thumb": vec4(1.0, 1.0, 1.0, 0.400),
    "scroll_thumb_hot": vec4(1.0, 1.0, 1.0, 0.620),
    "scroll_track": vec4(1.0, 1.0, 1.0, 0.055),
}

# a run whose class is the default text color is covered by the base full-line pass,
# so it is never emitted as a colored overlay (halves text primitives + guarantees a
# plain-text fallback everywhere the highlighter yields nothing).
DEFAULT_CLASS = "text"

################################################################################
# DSL vocabulary provider (pluggable).
################################################################################

# Canonical single-letter DSL namespace roots (T=terrain, P=params/particles, S,
# R, L=loop, H, M=material, C=const/context). Families extend the vocabulary by
# passing their own provider callable to CodeView; this default also enriches the
# verb set at runtime from the engine's published reflection palette.
DSL_NAMESPACES = {"T", "P", "S", "R", "L", "H", "M", "C"}


def default_dsl_vocab():
    """(namespaces:set[str], verbs:set[str]).

    verbs are sourced at runtime from core.dataflow.moduleClasses() dsl.verb
    annotations (the reflection palette). Degrades to the static namespace roots if
    the engine is not initialized — never raises."""
    verbs = set()
    try:
        from orkengine.core import dataflow as _dflow
        for c in _dflow.moduleClasses():
            anns = c.get("annotations", {}) or {}
            v = anns.get("dsl.verb")
            if v:
                verbs.add(v)
    except Exception:
        pass
    return set(DSL_NAMESPACES), verbs


################################################################################
# Document model (read-only in S1; mutable-list shape so S2 edits bolt on).
################################################################################

class CodeDocument:
    """A line-based text buffer + provenance/read-only state.

    provenance is a (kind, label) tuple — ("file", path) | ("generated", name) |
    ("memory", label). Generated buffers are read-only-forever: generated shader
    code is regenerated, never hand-edited, and the widget surfaces that state.

    S1 exposes read accessors only. S2 mutates `_lines` then calls rehighlight();
    the whole-buffer highlighter already handles multi-line strings/comments."""

    def __init__(self, text="", provenance=("memory", "buffer"),
                 language=None, read_only=True, generated=False):
        self._lines = text.split("\n")
        self.provenance = provenance
        self.language = language
        self.read_only = read_only or generated
        self.generated = generated
        # per-line color runs: list[ list[(start_col, end_col, class_key)] ]
        self._runs = None

    # -- constructors ----------------------------------------------------------

    @classmethod
    def from_path(cls, path, language=None):
        with open(path, "r") as f:
            text = f.read()
        lang = language or language_for_path(path)
        return cls(text, provenance=("file", str(path)), language=lang, read_only=True)

    @classmethod
    def from_string(cls, text, provenance=("memory", "buffer"),
                    language=None, read_only=True):
        return cls(text, provenance=provenance, language=language, read_only=read_only)

    @classmethod
    def generated(cls, name, text, language=None):
        """A regenerated (never hand-edited) buffer — e.g. shader text pulled from
        the dslshadercache without touching disk. Read-only-forever."""
        return cls(text, provenance=("generated", name), language=language,
                   read_only=True, generated=True)

    # -- read accessors --------------------------------------------------------

    @property
    def line_count(self):
        return len(self._lines)

    @property
    def lines(self):
        return self._lines

    @property
    def text(self):
        return "\n".join(self._lines)

    def line(self, i):
        return self._lines[i] if 0 <= i < len(self._lines) else ""

    def max_line_length(self):
        return max((len(l) for l in self._lines), default=0)

    @property
    def title(self):
        kind, label = self.provenance
        if kind == "file":
            return os.path.basename(label)
        if kind == "generated":
            return "⟨gen⟩ " + label
        return label

    # -- highlight -------------------------------------------------------------

    def set_runs(self, runs):
        self._runs = runs

    def runs_for_line(self, i):
        if self._runs is None or not (0 <= i < len(self._runs)):
            return ()
        return self._runs[i]

    # -- mutation (S2; pure buffer ops — read-only gating lives on the widget) --
    #
    # Positions are (line, col) tuples. These NEVER enforce read_only (that is the
    # widget's loud-refusal duty); they are the raw primitives undo/redo replays.

    def clamp_pos(self, line, col):
        line = max(0, min(int(line), len(self._lines) - 1))
        col = max(0, min(int(col), len(self._lines[line])))
        return (line, col)

    def end_pos(self):
        last = len(self._lines) - 1
        return (last, len(self._lines[last]))

    def get_range(self, start, end):
        """Text in [start, end) (start<=end assumed, both clamped)."""
        (sl, sc) = self.clamp_pos(*start)
        (el, ec) = self.clamp_pos(*end)
        if (sl, sc) >= (el, ec):
            return ""
        if sl == el:
            return self._lines[sl][sc:ec]
        parts = [self._lines[sl][sc:]] + self._lines[sl + 1:el] + [self._lines[el][:ec]]
        return "\n".join(parts)

    def insert(self, line, col, text):
        """Insert `text` (possibly multi-line) at (line,col). Returns end position."""
        (line, col) = self.clamp_pos(line, col)
        if text == "":
            return (line, col)
        cur = self._lines[line]
        head, tail = cur[:col], cur[col:]
        parts = text.split("\n")
        if len(parts) == 1:
            self._lines[line] = head + text + tail
            return (line, col + len(text))
        block = [head + parts[0]] + parts[1:-1] + [parts[-1] + tail]
        self._lines[line:line + 1] = block
        return (line + len(parts) - 1, len(parts[-1]))

    def delete(self, start, end):
        """Delete [start, end) (start<=end). Returns the removed text."""
        (sl, sc) = self.clamp_pos(*start)
        (el, ec) = self.clamp_pos(*end)
        if (sl, sc) >= (el, ec):
            return ""
        removed = self.get_range((sl, sc), (el, ec))
        if sl == el:
            self._lines[sl] = self._lines[sl][:sc] + self._lines[sl][ec:]
        else:
            joined = self._lines[sl][:sc] + self._lines[el][ec:]
            self._lines[sl:el + 1] = [joined]
        return removed


def language_for_path(path):
    ext = os.path.splitext(str(path))[1].lower()
    if ext in (".fxv2", ".i2", ".glfx"):
        return "shadlang"
    return "python"


################################################################################
# Highlighters. Each returns per-line color runs; the widget maps class -> color.
# Robustness contract: NEVER raise on malformed input — degrade to plain text for
# the offending region (the widget's base pass shows plain text regardless).
################################################################################

class PythonHighlighter:
    """stdlib tokenize over the whole buffer, plus a DSL vocabulary overlay.

    Color classes: keyword, string, number, comment, decorator, definition, dsl.
    Everything else (identifiers, operators) stays DEFAULT_CLASS."""

    def __init__(self, vocab_provider=None):
        self._vocab_provider = vocab_provider or default_dsl_vocab
        self._vocab = None

    def _vocabulary(self):
        if self._vocab is None:
            try:
                self._vocab = self._vocab_provider()
            except Exception:
                self._vocab = (set(), set())
        return self._vocab

    def highlight(self, doc):
        lines = doc.lines
        runs = [[] for _ in lines]
        namespaces, verbs = self._vocabulary()
        src = doc.text
        prev1 = None            # (typ, string) of last meaningful token
        prev2 = None
        at_line_start = True
        decorate_chain = False

        def emit(srow, scol, erow, ecol, cls):
            # map a possibly multi-line token span to per-line column runs
            for row in range(srow, erow + 1):
                idx = row - 1
                if not (0 <= idx < len(lines)):
                    continue
                c0 = scol if row == srow else 0
                c1 = ecol if row == erow else len(lines[idx])
                if c1 > c0:
                    runs[idx].append((c0, c1, cls))

        try:
            tokgen = tokenize.generate_tokens(io.StringIO(src).readline)
            for tok in tokgen:
                ttype = tok.type
                tstr = tok.string
                name = tokenize.tok_name.get(ttype, "")
                if ttype in (tokenize.NEWLINE, tokenize.NL):
                    at_line_start = True
                    decorate_chain = False
                    prev2, prev1 = prev1, (name, tstr)
                    continue
                if ttype in (tokenize.INDENT, tokenize.DEDENT,
                             tokenize.ENCODING, tokenize.ENDMARKER):
                    continue

                cls = None
                if ttype == tokenize.COMMENT:
                    cls = "comment"
                elif name.startswith("STRING") or name.startswith("FSTRING"):
                    cls = "string"
                elif ttype == tokenize.NUMBER:
                    cls = "number"
                elif ttype == tokenize.OP and tstr == "@" and at_line_start:
                    cls = "decorator"
                    decorate_chain = True
                elif decorate_chain and (ttype == tokenize.NAME or tstr == "."):
                    cls = "decorator"
                elif ttype == tokenize.NAME:
                    decorate_chain = False
                    if keyword.iskeyword(tstr):
                        cls = "keyword"
                    elif prev1 is not None and prev1[1] in ("def", "class"):
                        cls = "definition"
                    elif tstr in namespaces:
                        cls = "dsl"
                    elif (prev1 is not None and prev1[1] == "."
                          and prev2 is not None and prev2[1] in namespaces):
                        cls = "dsl"                 # verb position: <namespace> . <verb>
                    elif tstr in verbs:
                        cls = "dsl"
                else:
                    decorate_chain = decorate_chain and tstr == "."

                if cls is not None and cls != DEFAULT_CLASS:
                    (sr, sc) = tok.start
                    (er, ec) = tok.end
                    emit(sr, sc, er, ec, cls)

                at_line_start = False
                prev2, prev1 = prev1, (name, tstr)
        except (tokenize.TokenError, IndentationError, SyntaxError, ValueError):
            # partial/invalid source: keep every run collected before the fault and
            # let the offending region render as plain text. Never crash.
            pass
        return runs


class ShadlangScanner:
    """Lexer built FROM ork.data/grammars/shadlang.scanner at runtime.

    The scanner file declares `TOKEN <| "regex" |>` rules in priority order. This
    mirrors its maximal-munch matching: at each position the longest match wins;
    ties break to the earliest-declared rule. QUOTED_STRING has an empty pattern in
    the grammar (filled by the C++ pegimpl) — we supply the standard double-quoted
    matcher so strings still color."""

    # GLSL data-type token names -> "type" color (the rest of KW_* -> "keyword").
    _TYPE_NAMES = {
        "KW_FLOAT", "KW_UINT", "KW_INT",
        "KW_VEC2", "KW_VEC3", "KW_VEC4",
        "KW_MAT2", "KW_MAT3", "KW_MAT4",
        "KW_SAMP1D", "KW_SAMP2D", "KW_SAMP3D", "KW_SAMPCUBE",
        "KW_SAMP1DARRAY", "KW_SAMP2DARRAY", "KW_SAMP3DARRAY",
        "KW_IVEC2", "KW_IVEC3", "KW_IVEC4",
        "KW_IMAT2", "KW_IMAT3", "KW_IMAT4",
        "KW_ISAMP1D", "KW_ISAMP2D", "KW_ISAMP3D",
        "KW_UVEC2", "KW_UVEC3", "KW_UVEC4",
        "KW_UMAT2", "KW_UMAT3", "KW_UMAT4",
        "KW_USAMP1D", "KW_USAMP2D", "KW_USAMP3D",
    }
    _COMMENTS = {"MULTI_LINE_COMMENT", "SINGLE_LINE_COMMENT"}
    _WS = {"WHITESPACE", "NEWLINE"}
    _NUMBERS = {"HEX_INTEGER", "INTEGER", "FLOATING_POINT"}

    _DEFAULT_SCANNER = "ork.data/grammars/shadlang.scanner"

    def __init__(self, scanner_path=None):
        import re
        self._re = re
        self._rules = []                          # ordered [(name, compiled)]
        self._load(scanner_path or self._resolve_default())
        # QUOTED_STRING is grammar-empty; supply the standard matcher (highest-but-
        # -after-comment priority is irrelevant — maximal-munch picks the longest).
        self._rules.append(("QUOTED_STRING", re.compile(r'"([^"\\]|\\.)*"')))

    def _resolve_default(self):
        try:
            root = os.environ.get("ORKID_WORKSPACE_DIR")
            if root:
                cand = os.path.join(root, self._DEFAULT_SCANNER)
                if os.path.exists(cand):
                    return cand
        except Exception:
            pass
        # walk up from this file to the repo root (…/orkid[-lanes/<lane>])
        here = os.path.dirname(os.path.abspath(__file__))
        for _ in range(12):
            cand = os.path.join(here, self._DEFAULT_SCANNER)
            if os.path.exists(cand):
                return cand
            here = os.path.dirname(here)
        return self._DEFAULT_SCANNER

    def _load(self, path):
        rule_re = self._re.compile(r'^\s*([A-Za-z_][A-Za-z0-9_]*)\s*<\|\s*"(.*)"\s*\|>')
        try:
            with open(path, "r") as f:
                lines = f.readlines()
        except OSError:
            lines = []
        for ln in lines:
            m = rule_re.match(ln)
            if not m:
                continue                          # macros / blanks / comments
            name, pattern = m.group(1), m.group(2)
            if pattern == "":
                continue                          # QUOTED_STRING placeholder
            try:
                self._rules.append((name, self._re.compile(pattern)))
            except self._re.error:
                continue                          # unmirror-able rule: skip, don't crash

    def classify(self, name):
        if name in self._COMMENTS:
            return "comment"
        if name in self._WS:
            return None
        if name in self._NUMBERS:
            return "number"
        if name == "QUOTED_STRING":
            return "string"
        if name == "IDENTIFIER":
            return DEFAULT_CLASS
        if name in self._TYPE_NAMES:
            return "type"
        if name.startswith("KW_"):
            return "keyword"
        return "punct"

    def scan(self, text):
        """[(start_offset, end_offset, class_key), ...] over the whole text."""
        out = []
        pos = 0
        N = len(text)
        rules = self._rules
        while pos < N:
            best_name = None
            best_end = pos
            for name, rx in rules:
                m = rx.match(text, pos)
                if m:
                    end = m.end()
                    if end > best_end:            # strictly-longer wins; ties keep
                        best_end = end            # the earlier-declared rule
                        best_name = name
            if best_name is None:
                pos += 1                          # unmatched char: skip as plain text
                continue
            cls = self.classify(best_name)
            if cls is not None:
                out.append((pos, best_end, cls))
            pos = best_end
        return out


class ShadlangHighlighter:
    """Wraps ShadlangScanner: whole-text scan mapped to per-line runs."""

    def __init__(self, scanner=None):
        self._scanner = scanner
        self._scanner_err = None

    def _get_scanner(self):
        if self._scanner is None and self._scanner_err is None:
            try:
                self._scanner = ShadlangScanner()
            except Exception as e:
                self._scanner_err = e
        return self._scanner

    def highlight(self, doc):
        lines = doc.lines
        runs = [[] for _ in lines]
        scanner = self._get_scanner()
        if scanner is None:
            return runs                           # degrade to plain text
        # single-line comments in the grammar consume a trailing newline; guarantee
        # one at EOF so a comment on the last line still matches.
        text = doc.text
        pad = 0
        if not text.endswith("\n"):
            text = text + "\n"
            pad = 1
        # line-start char offsets for O(1) offset->(row,col) mapping
        starts = []
        acc = 0
        for l in lines:
            starts.append(acc)
            acc += len(l) + 1
        total = acc                               # includes the synthetic pad newline
        try:
            spans = scanner.scan(text)
        except Exception:
            return runs                           # never crash on odd input
        import bisect
        for (s, e, cls) in spans:
            if pad and e > total - 1:
                e = total - 1
            if e <= s:
                continue
            srow = bisect.bisect_right(starts, s) - 1
            erow = bisect.bisect_right(starts, e - 1) - 1
            for row in range(srow, erow + 1):
                if not (0 <= row < len(lines)):
                    continue
                base = starts[row]
                c0 = (s - base) if row == srow else 0
                c1 = (e - base) if row == erow else len(lines[row])
                c0 = max(0, min(c0, len(lines[row])))
                c1 = max(0, min(c1, len(lines[row])))
                if c1 > c0:
                    runs[row].append((c0, c1, cls))
        return runs


def highlighter_for_language(language, vocab_provider=None):
    if language == "shadlang":
        return ShadlangHighlighter()
    return PythonHighlighter(vocab_provider=vocab_provider)


################################################################################
# Scroll axis — mirrors ork::ui::ScrollController (the object PropertySheet /
# Outliner own): plain offset/content/viewport state + wheel + clamp, extended
# with thumb geometry + drag/page math for an interactive themed scrollbar.
################################################################################

class _ScrollAxis:
    def __init__(self, speed=3):
        self.offset = 0.0
        self.content_size = 0.0
        self.viewport_size = 0.0
        self.speed = speed
        self.min_thumb = 24.0

    def max_scroll(self):
        return max(0.0, self.content_size - self.viewport_size)

    def clamp(self):
        self.offset = min(max(0.0, self.offset), self.max_scroll())

    def apply_wheel(self, delta_ticks, step):
        self.offset -= delta_ticks * self.speed * step
        self.clamp()

    def overflow(self):
        return self.content_size > self.viewport_size + 0.5

    def thumb(self, track_len):
        """(thumb_pos, thumb_len) along a track of `track_len` px."""
        if self.content_size <= 0:
            return (0.0, track_len)
        ratio = min(1.0, self.viewport_size / self.content_size)
        tlen = max(self.min_thumb, ratio * track_len)
        tlen = min(tlen, track_len)
        travel = track_len - tlen
        ms = self.max_scroll()
        pos = (self.offset / ms) * travel if ms > 0 else 0.0
        return (pos, tlen)

    def drag_by(self, mouse_delta, track_len):
        """Convert a thumb pixel motion into an offset delta (the mouse-drag path)."""
        _, tlen = self.thumb(track_len)
        travel = track_len - tlen
        ms = self.max_scroll()
        if travel > 0 and ms > 0:
            self.offset += mouse_delta * (ms / travel)
            self.clamp()

    def page(self, direction):
        self.offset += direction * self.viewport_size
        self.clamp()


################################################################################
# CodeView
################################################################################

# reserve enough vertical space for the header + scrollbars; grabbable thumbs are
# thicker than the C++ 6px fade indicator because S1 requires drag interaction.
HEADER_H       = 20.0
SCROLLBAR_THK  = 12.0
GUTTER_PAD_L   = 6.0
GUTTER_PAD_R   = 8.0
LINE_PAD       = 3.0
CONTENT_PAD_L  = 6.0


def _pos_advance(pos, text):
    """End position after `text` were inserted at `pos` (line,col)."""
    line, col = pos
    nl = text.count("\n")
    if nl == 0:
        return (line, col + len(text))
    return (line + nl, len(text) - (text.rindex("\n") + 1))


class _EditRecord:
    """One undoable replace: at `start`, `removed` text became `inserted` text.
    Carries the caret before/after so undo/redo restores it byte-for-caret, plus
    the selection anchor before/after: None (plain edits) clears the selection on
    undo/redo exactly as before, while block reindent stores the shifted anchor so
    undo/redo restore the whole selection, not just the caret."""

    __slots__ = ("start", "removed", "inserted", "caret_before", "caret_after",
                 "sel_before", "sel_after")

    def __init__(self, start, removed, inserted, caret_before, caret_after,
                 sel_before=None, sel_after=None):
        self.start = start
        self.removed = removed
        self.inserted = inserted
        self.caret_before = caret_before
        self.caret_after = caret_after
        self.sel_before = sel_before
        self.sel_after = sel_after

    def coalescable_with(self, start, inserted):
        """True if a single-char insert at `start` continues this insert run."""
        return (self.removed == ""
                and "\n" not in self.inserted
                and _pos_advance(self.start, self.inserted) == start
                and len(inserted) == 1 and inserted != "\n")


# GLFW physical keycode -> US-layout character. Physical codes only reach the
# widget (no Unicode/char-callback binding yet), so real typing is a US-keyboard
# approximation the owner windowed-verifies; gates drive the edit API directly.
_KEY_DIGITS_SHIFTED = {48: ")", 49: "!", 50: "@", 51: "#", 52: "$",
                       53: "%", 54: "^", 55: "&", 56: "*", 57: "("}
_KEY_PUNCT = {39: ("'", '"'), 44: (",", "<"), 45: ("-", "_"), 46: (".", ">"),
              47: ("/", "?"), 59: (";", ":"), 61: ("=", "+"), 91: ("[", "{"),
              92: ("\\", "|"), 93: ("]", "}"), 96: ("`", "~")}


class CodeView:
    """Read-only code viewer bound to a CodeDocument via a PrimCanvas."""

    def __init__(self, canvas, document=None, font_id="i16",
                 palette=None, vocab_provider=None, show_header=True,
                 scroll_speed=0.375, indent_width=2):
        self.canvas = canvas
        canvas.supersample = 3                    # proven-crisp SSAA
        self.palette = dict(PALETTE)
        if palette:
            self.palette.update(palette)
        self.font_id = font_id
        self.vocab_provider = vocab_provider
        self.show_header = show_header

        self.document = None
        self.current_line = 0                     # tinted line (S2 drives from caret)
        self._highlights = {}                     # tag -> (start_line, end_line, color)

        self.vscroll = _ScrollAxis(speed=scroll_speed)
        self.hscroll = _ScrollAxis(speed=scroll_speed)

        self._dirty = True
        self._gpu = False
        self._font = None
        self._adv_w = 8.0
        self._line_h = 18.0
        self._last_w = self._last_h = 0

        # interaction (S1 scrollbar drag + S2 caret/selection drag)
        self._drag_axis = None                    # "v" | "h" | None
        self._drag_start_mouse = 0.0
        self._last_mx = self._last_my = 0.0
        self._text_dragging = False               # left-drag selection in the text area

        # -- S2 editing state --------------------------------------------------
        self.indent_width = int(indent_width)
        self._caret = (0, 0)                      # (line, col) insertion point
        self._sel_anchor = None                   # (line, col) | None (selection = anchor..caret)
        self._goal_col = 0                        # sticky column for vertical caret moves
        self._focused = False
        self._blink_on = True
        self._blink_frame = 0
        self._blink_period = 30                   # frames per blink half-cycle (~2Hz @60fps)
        self._undo = []                           # list[_EditRecord] (bounded)
        self._redo = []
        self._undo_limit = 1000
        self._clip_fallback = ""                  # widget-local clipboard (headless / no window)
        self._status = ""                         # last loud-refusal / action message

        canvas.onPreRender = self._onPreRender
        canvas.onUiEvent = self._onUiEvent

        if document is not None:
            self.set_document(document)

    # -- host lifecycle --------------------------------------------------------

    def gpuInit(self, ctx):
        """API parity with NodeEditor.gpuInit (host calls from onGpuInit). CodeView
        needs no GPU textures; the font is fetched lazily on the render thread."""
        self._ctx = ctx

    def _gpuInit(self):
        self.lyr_bands  = self.canvas.createLayer("bands")    # tint + region bands
        self.lyr_text   = self.canvas.createLayer("text")     # code glyphs
        self.lyr_gutter = self.canvas.createLayer("gutter")   # opaque gutter/header chrome (masks scrolled code)
        self.lyr_scroll = self.canvas.createLayer("scroll")   # scrollbars (topmost)
        self.pip = self.canvas.pipelineSolid
        self._font = lev2.FontManager.fontForId(self.font_id)
        desc = self._font.description
        self._adv_w = float(desc.advance_width)
        self._line_h = float(desc.advance_height) + LINE_PAD
        self._gpu = True

    # -- loading ---------------------------------------------------------------

    def set_document(self, doc):
        self.document = doc
        if doc.language is None:
            doc.language = "python"
        self.rehighlight()
        self.vscroll.offset = 0.0
        self.hscroll.offset = 0.0
        self.current_line = 0
        self._highlights.clear()
        self._caret = (0, 0)
        self._sel_anchor = None
        self._goal_col = 0
        self._undo.clear()
        self._redo.clear()
        self._reset_blink()
        self.markDirty()

    def load_path(self, path, language=None):
        self.set_document(CodeDocument.from_path(path, language=language))

    def load_text(self, text, provenance=("memory", "buffer"),
                  language=None, read_only=True):
        self.set_document(CodeDocument.from_string(
            text, provenance=provenance, language=language, read_only=read_only))

    def load_generated(self, name, text, language=None):
        """Browse regenerated (read-only-forever) code — e.g. shader text from the
        dslshadercache — without touching disk. Surfaces the generated state."""
        self.set_document(CodeDocument.generated(name, text, language=language))

    def rehighlight(self):
        """(Re)compute color runs for the whole buffer. S2 calls this after edits."""
        doc = self.document
        if doc is None:
            return
        hl = highlighter_for_language(doc.language, self.vocab_provider)
        try:
            runs = hl.highlight(doc)
        except Exception:
            runs = [[] for _ in doc.lines]        # last-ditch: plain text, never crash
        doc.set_runs(runs)
        self.markDirty()

    # -- region mapping seams (node<->source cross-workflow) -------------------

    def highlight_range(self, start_line, end_line, tag="", color=None):
        """Tint a line range with a named band (the node->source direction). The tag
        lets a host replace/clear its own band without disturbing others."""
        col = color if color is not None else self.palette["highlight"]
        self._highlights[tag] = (int(start_line), int(end_line), col)
        self.markDirty()

    def clear_highlight(self, tag=None):
        if tag is None:
            self._highlights.clear()
        else:
            self._highlights.pop(tag, None)
        self.markDirty()

    def scroll_to_range(self, start_line, end_line=None):
        self.scroll_to_line(start_line)

    def line_col_at(self, local_x, local_y):
        """Map a canvas-local point to a (line, col) document position, or None if
        outside the text area. The exact query S2 consumes to place a caret."""
        if self.document is None:
            return None
        gx = self._gutter_width()
        top = self._content_top()
        if local_x < gx or local_y < top:
            return None
        if local_x > self._last_w - self._vbar_w() or local_y > self._last_h - self._hbar_h():
            return None
        # subtract CONTENT_PAD_L so the mapped column matches where the glyph +
        # caret actually render (S2 consumes this to place a caret from a click).
        col = int(round((local_x - gx - CONTENT_PAD_L + self.hscroll.offset) / self._adv_w))
        line = int((local_y - top + self.vscroll.offset) // self._line_h)
        line = max(0, min(line, self.document.line_count - 1))
        return (line, max(0, col))

    def _caret_from_point(self, local_x, local_y, clamp=False):
        """Map a point to a caret (line,col). clamp=True never returns None — it
        clamps to the document (the drag-select path can leave the text box)."""
        if self.document is None:
            return None
        if not clamp:
            return self.line_col_at(local_x, local_y)
        gx = self._gutter_width()
        top = self._content_top()
        col = int(round((local_x - gx - CONTENT_PAD_L + self.hscroll.offset) / self._adv_w))
        line = int((local_y - top + self.vscroll.offset) // self._line_h)
        line = max(0, min(line, self.document.line_count - 1))
        col = max(0, min(col, len(self.document.line(line))))
        return (line, col)

    # -- scrolling API ---------------------------------------------------------

    def scroll_to_line(self, line):
        self.vscroll.offset = float(line) * self._line_h
        self.vscroll.clamp()
        self.markDirty()

    def scroll_to_column(self, col):
        self.hscroll.offset = float(col) * self._adv_w
        self.hscroll.clamp()
        self.markDirty()

    @property
    def first_visible_line(self):
        return int(self.vscroll.offset // self._line_h) if self._line_h else 0

    @property
    def scroll_speed(self):
        return self.vscroll.speed

    @scroll_speed.setter
    def scroll_speed(self, value):
        # one knob, both axes: lines-per-wheel-tick vertically, 3-column units
        # horizontally (the h step already carries the x3 column factor).
        self.vscroll.speed = value
        self.hscroll.speed = value

    def drag_thumb(self, axis, pixels):
        """Programmatically drag a scrollbar thumb by `pixels` along `axis`
        ("v"|"h") — the SAME code path a mouse thumb-drag uses (gate observable)."""
        if axis == "v":
            self.vscroll.drag_by(pixels, self._vtrack_len())
        else:
            self.hscroll.drag_by(pixels, self._htrack_len())
        self.markDirty()

    def markDirty(self):
        self._dirty = True

    # -- geometry helpers ------------------------------------------------------

    def _content_top(self):
        return HEADER_H if self.show_header else 0.0

    def _gutter_width(self):
        doc = self.document
        digits = len(str(max(1, doc.line_count))) if doc else 3
        # +1 char: a one-character spacer column between the numbers and the content
        return GUTTER_PAD_L + (digits + 1) * self._adv_w + GUTTER_PAD_R

    def _vbar_w(self):
        return SCROLLBAR_THK                      # vertical bar always present (house behavior)

    def _hbar_h(self):
        return SCROLLBAR_THK if self.hscroll.overflow() else 0.0

    def _content_view_w(self):
        return self._last_w - self._gutter_width() - self._vbar_w()

    def _content_view_h(self):
        return self._last_h - self._content_top() - self._hbar_h()

    def _vtrack_len(self):
        return self._content_view_h()

    def _htrack_len(self):
        return self._content_view_w()

    def _sync_scroll_metrics(self):
        doc = self.document
        if doc is None:
            return
        self.vscroll.content_size = doc.line_count * self._line_h
        self.vscroll.viewport_size = self._content_view_h()
        self.hscroll.content_size = (doc.max_line_length() * self._adv_w) + CONTENT_PAD_L * 2
        self.hscroll.viewport_size = self._content_view_w()
        self.vscroll.clamp()
        self.hscroll.clamp()

    # -- pre-render ------------------------------------------------------------

    def _onPreRender(self):
        if not self._gpu:
            self._gpuInit()
        w, h = self.canvas.width, self.canvas.height
        if w < 2 or h < 2:
            return
        if (w, h) != (self._last_w, self._last_h):
            self._last_w, self._last_h = w, h
            self._dirty = True
        # caret blink: only while focused, so an unfocused view still settles to
        # zero rebuilds (S1 retained-render promise). A toggle forces one rebuild.
        if self._focused:
            self._blink_frame += 1
            if self._blink_frame >= self._blink_period:
                self._blink_frame = 0
                self._blink_on = not self._blink_on
                self._dirty = True
        if not self._dirty:
            return
        self._sync_scroll_metrics()
        self._rebuild()
        self._dirty = False
        self.canvas.markDirty()

    # -- primitive helpers -----------------------------------------------------

    def _quad(self, layer, x, y, w, h, color, radius=0.0):
        qp = lev2.ui.QuadPrimitive(pipeline=self.pip)
        qd = lev2.ui.QuadData()
        qd.setPosition(x, y)
        qd.setSize(w, h)
        qd.setColor(color)
        if radius > 0:
            qd.setCornerRadius(radius)
        qp.addQuad(qd)
        layer.addPrimitive(qp)

    # -- rebuild ---------------------------------------------------------------

    def _rebuild(self):
        for lyr in (self.lyr_bands, self.lyr_text, self.lyr_gutter, self.lyr_scroll):
            lyr.clear()
        doc = self.document
        if doc is None:
            return

        gx = self._gutter_width()
        top = self._content_top()
        adv = self._adv_w
        lh = self._line_h
        view_bottom = self._last_h - self._hbar_h()
        view_right = self._last_w - self._vbar_w()
        soff_y = self.vscroll.offset
        soff_x = self.hscroll.offset

        first = max(0, int(soff_y // lh))
        last = min(doc.line_count - 1, int((soff_y + (view_bottom - top)) // lh) + 1)

        # --- bands: current-line tint + region highlights (content area) ---
        content_w = view_right - gx
        for tag, (s0, s1, col) in self._highlights.items():
            for li in range(max(first, s0), min(last, s1) + 1):
                y = top + li * lh - soff_y
                self._quad(self.lyr_bands, gx, y, content_w, lh, col)
        if first <= self.current_line <= last:
            y = top + self.current_line * lh - soff_y
            self._quad(self.lyr_bands, gx, y, content_w, lh, self.palette["current_line"])

        # --- selection bands (per-line spans; distinct color from highlight) ---
        sel = self._sel_ordered()
        if sel is not None:
            selcol = self.palette["selection"]
            (s0l, _), (s1l, _) = sel
            for li in range(max(first, s0l), min(last, s1l) + 1):
                span = self.selection_spans_for_line(li)
                if span is None:
                    continue
                c0, c1 = span
                y = top + li * lh - soff_y
                x0 = gx + CONTENT_PAD_L + c0 * adv - soff_x
                w = max(0.0, (c1 - c0) * adv)
                if x0 < gx:                       # clip under the opaque gutter edge
                    w -= (gx - x0)
                    x0 = gx
                if w > 0:
                    self._quad(self.lyr_bands, x0, y, w, lh, selcol)

        # --- code text: base plain pass + colored overlays ---
        base_tp = lev2.ui.TextPrimitive(font=self._font, color=self.palette["text"])
        overlay_tps = {}                          # class_key -> TextPrimitive

        def overlay(cls):
            tp = overlay_tps.get(cls)
            if tp is None:
                tp = lev2.ui.TextPrimitive(font=self._font, color=self.palette[cls])
                overlay_tps[cls] = tp
            return tp

        for li in range(first, last + 1):
            text = doc.line(li)
            if not text:
                continue
            y = top + li * lh - soff_y
            x0 = gx + CONTENT_PAD_L - soff_x
            base_tp.addItem(text, vec2(x0, y))    # plain fallback for the whole line
            for (c0, c1, cls) in doc.runs_for_line(li):
                if cls == DEFAULT_CLASS or c1 <= c0:
                    continue
                sub = text[c0:c1]
                if sub.strip() == "" and cls not in ("string",):
                    continue
                overlay(cls).addItem(sub, vec2(x0 + c0 * adv, y))
        self.lyr_text.addPrimitive(base_tp)
        for tp in overlay_tps.values():
            self.lyr_text.addPrimitive(tp)

        # --- caret quad (on top of text; blinks while focused) ---
        if self._focused and self._blink_on and first <= self._caret[0] <= last:
            cx, cy = self.caret_screen_pos()
            if gx - 1.0 <= cx <= view_right:
                self._quad(self.lyr_text, cx, cy, max(1.5, adv * 0.14), lh,
                           self.palette["caret"])

        # --- gutter + header chrome (opaque, on top: masks h/v-scrolled code) ---
        self._quad(self.lyr_gutter, 0, top, gx, self._last_h - top,
                   self.palette["gutter_gen"] if doc.generated else self.palette["gutter_bg"])
        gnum_tp = lev2.ui.TextPrimitive(font=self._font, color=self.palette["gutter"])
        gcur_tp = lev2.ui.TextPrimitive(font=self._font, color=self.palette["gutter_cur"])
        digits = len(str(max(1, doc.line_count)))
        for li in range(first, last + 1):
            y = top + li * lh - soff_y
            # right-align via the x-offset ONLY (an rjust'd label ALSO advancing
            # spaces double-shifted short numbers into the content area)
            label = str(li + 1)
            tx = GUTTER_PAD_L + (digits - len(label)) * adv
            (gcur_tp if li == self.current_line else gnum_tp).addItem(label, vec2(tx, y))
        self.lyr_gutter.addPrimitive(gnum_tp)
        self.lyr_gutter.addPrimitive(gcur_tp)

        if self.show_header:
            self._quad(self.lyr_gutter, 0, 0, self._last_w, HEADER_H, self.palette["header_bg"])
            htp = lev2.ui.TextPrimitive(font=self._font, color=self.palette["header_txt"])
            htp.addItem(doc.title, vec2(GUTTER_PAD_L, 3.0))
            self.lyr_gutter.addPrimitive(htp)
            if doc.read_only:
                badge = "GENERATED - READ ONLY" if doc.generated else "READ ONLY"
                bw = len(badge) * adv + 12
                bx = self._last_w - bw - 6
                self._quad(self.lyr_gutter, bx, 2.0, bw, HEADER_H - 4, vec4(0, 0, 0, 0.35), radius=3.0)
                btp = lev2.ui.TextPrimitive(font=self._font, color=self.palette["badge_ro"])
                btp.addItem(badge, vec2(bx + 6, 3.0))
                self.lyr_gutter.addPrimitive(btp)

        # --- scrollbars (themed pill thumb + faint track) ---
        self._rebuild_scrollbars()

    def _rebuild_scrollbars(self):
        top = self._content_top()
        gx = self._gutter_width()
        thk = SCROLLBAR_THK
        thumb_col = self.palette["scroll_thumb_hot"] if self._drag_axis else self.palette["scroll_thumb"]
        track_col = self.palette["scroll_track"]
        r = 4.0
        # vertical (always present)
        vx = self._last_w - thk
        vtrack = self._vtrack_len()
        self._quad(self.lyr_scroll, vx + 2, top, thk - 4, vtrack, track_col, radius=r)
        if self.vscroll.overflow():
            pos, tlen = self.vscroll.thumb(vtrack)
            col = thumb_col if self._drag_axis == "v" else self.palette["scroll_thumb"]
            self._quad(self.lyr_scroll, vx + 2, top + pos, thk - 4, tlen, col, radius=r)
        # horizontal (on overflow)
        if self.hscroll.overflow():
            hy = self._last_h - thk
            htrack = self._htrack_len()
            self._quad(self.lyr_scroll, gx, hy + 2, htrack, thk - 4, track_col, radius=r)
            pos, tlen = self.hscroll.thumb(htrack)
            col = thumb_col if self._drag_axis == "h" else self.palette["scroll_thumb"]
            self._quad(self.lyr_scroll, gx + pos, hy + 2, tlen, thk - 4, col, radius=r)

    # -- event handling (scrollbar + wheel only; NO keys/caret in S1) ----------

    def _onUiEvent(self, ev):
        code = ev.code
        res = lev2.ui.HandlerResult()
        # keyboard (also reachable via a host that forwards to handleKeyDown) —
        # focus-local: the widget only acts on keys once it owns key focus.
        if code == tokens.KEY_DOWN.hashed or code == tokens.KEY_REPEAT.hashed:
            self.handleKeyDown(ev)
            return res
        if code == tokens.KEY_UP.hashed:
            self.handleKeyUp(ev)
            return res
        if code == tokens.GOT_KEYFOCUS.hashed:
            self.focus()
            return res
        if code == tokens.LOST_KEYFOCUS.hashed:
            self.blur()
            return res
        mx, my = self.canvas.rootToLocal(ev.x, ev.y)
        if code == tokens.MOUSEWHEEL.hashed:
            self._on_wheel(ev)
        elif code == tokens.DOUBLECLICK.hashed:
            pos = self.line_col_at(mx, my)
            if pos is not None:
                self.focus()
                self.select_word_at(*pos)
                res.setHandler(self.canvas)
        elif code == tokens.PUSH.hashed:
            if not self._on_push(mx, my):         # not a scrollbar hit -> text click
                if self._begin_text_click(mx, my, ev):
                    res.setHandler(self.canvas)
        elif code == tokens.DRAG.hashed:
            self._on_drag(mx, my)
        elif code == tokens.MOVE.hashed:
            if (self._drag_axis or self._text_dragging) and ev.left:
                self._on_drag(mx, my)
        elif code == tokens.RELEASE.hashed:
            self._drag_axis = None
            self._text_dragging = False
            self.markDirty()
        self._last_mx, self._last_my = mx, my
        return res

    def _begin_text_click(self, mx, my, ev):
        pos = self.line_col_at(mx, my)
        if pos is None:
            return False
        self.focus()
        self.set_caret(pos[0], pos[1], select=bool(ev.shift))
        self._text_dragging = True
        return True

    def _on_wheel(self, ev):
        wy = ev.wheel_y
        wx = ev.wheel_x
        if ev.shift and wx == 0:                  # shift+vertical wheel -> horizontal
            wx, wy = wy, 0
        if wy != 0:
            self.vscroll.apply_wheel(wy, self._line_h)
            self.markDirty()
        if wx != 0:
            self.hscroll.apply_wheel(wx, self._adv_w * 3.0)
            self.markDirty()

    def _on_push(self, mx, my):
        """Handle a scrollbar hit. Returns True if the push landed on a bar
        (so the caller leaves the text-click path alone)."""
        thk = SCROLLBAR_THK
        top = self._content_top()
        gx = self._gutter_width()
        # vertical bar hit
        vx = self._last_w - thk
        if mx >= vx and top <= my <= self._last_h - self._hbar_h():
            vtrack = self._vtrack_len()
            pos, tlen = self.vscroll.thumb(vtrack)
            ty = top + pos
            if ty <= my <= ty + tlen:
                self._drag_axis = "v"
                self._drag_start_mouse = my
            else:
                self.vscroll.page(1 if my > ty else -1)
            self.markDirty()
            return True
        # horizontal bar hit
        if self.hscroll.overflow():
            hy = self._last_h - thk
            if my >= hy and gx <= mx <= self._last_w - self._vbar_w():
                htrack = self._htrack_len()
                pos, tlen = self.hscroll.thumb(htrack)
                tx = gx + pos
                if tx <= mx <= tx + tlen:
                    self._drag_axis = "h"
                    self._drag_start_mouse = mx
                else:
                    self.hscroll.page(1 if mx > tx else -1)
                self.markDirty()
                return True
        return False

    def _on_drag(self, mx, my):
        if self._drag_axis == "v":
            self.vscroll.drag_by(my - self._drag_start_mouse, self._vtrack_len())
            self._drag_start_mouse = my
            self.markDirty()
        elif self._drag_axis == "h":
            self.hscroll.drag_by(mx - self._drag_start_mouse, self._htrack_len())
            self._drag_start_mouse = mx
            self.markDirty()
        elif self._text_dragging:
            pos = self._caret_from_point(mx, my, clamp=True)
            if pos is not None:
                self.set_caret(pos[0], pos[1], select=True)

    ############################################################################
    # S2 EDITING CORE
    #
    # API-FIRST: every edit is a public method here; the key handlers below are
    # thin wrappers. Gates drive these directly and capture the result headlessly.
    # Read-only / generated buffers refuse every mutator loudly (status + False).
    ############################################################################

    # -- caret + selection accessors -------------------------------------------

    @property
    def caret(self):
        return self._caret

    @property
    def status(self):
        return self._status

    @property
    def focused(self):
        return self._focused

    def has_selection(self):
        return self._sel_anchor is not None and self._sel_anchor != self._caret

    def _sel_ordered(self):
        """(start, end) with start<=end, or None when there is no selection."""
        if not self.has_selection():
            return None
        a, b = self._sel_anchor, self._caret
        return (a, b) if a <= b else (b, a)

    def _selection_is_multiline(self):
        sel = self._sel_ordered()
        return sel is not None and sel[0][0] != sel[1][0]

    @property
    def selection(self):
        return self._sel_ordered()

    def selected_text(self):
        sel = self._sel_ordered()
        return "" if sel is None else self.document.get_range(*sel)

    def clear_selection(self):
        if self._sel_anchor is not None:
            self._sel_anchor = None
            self.markDirty()

    # -- read-only gate + edit primitive ---------------------------------------

    def _refuse(self, what):
        """Loud refusal for a mutation on a read-only / generated buffer."""
        doc = self.document
        kind = "generated" if (doc and doc.generated) else "read-only"
        self._status = f"edit refused ({what}): {kind} buffer '{doc.title if doc else '?'}'"
        import sys
        print("[code_editor] " + self._status, file=sys.stderr, flush=True)
        return False

    def _editable(self, what):
        if self.document is None:
            return self._refuse(what)
        if self.document.read_only:
            return self._refuse(what)
        return True

    def _push_undo(self, start, removed, inserted, caret_before, caret_after,
                   coalesce, sel_before=None, sel_after=None):
        self._redo.clear()
        if coalesce and self._undo and self._undo[-1].coalescable_with(start, inserted):
            top = self._undo[-1]
            top.inserted += inserted
            top.caret_after = caret_after
            return
        self._undo.append(_EditRecord(start, removed, inserted,
                                      caret_before, caret_after,
                                      sel_before, sel_after))
        if len(self._undo) > self._undo_limit:
            self._undo.pop(0)

    def _edit(self, start, end, new_text, coalesce=False):
        """Replace [start,end) with new_text (caret -> end of insert). Caller has
        already passed the read-only gate. Records one undo step."""
        doc = self.document
        start = doc.clamp_pos(*start)
        end = doc.clamp_pos(*end)
        if start > end:
            start, end = end, start
        caret_before = self._caret
        removed = doc.delete(start, end) if start != end else ""
        new_end = doc.insert(start[0], start[1], new_text) if new_text else start
        self._push_undo(start, removed, new_text, caret_before, new_end, coalesce)
        self._sel_anchor = None
        self._caret = new_end
        self.rehighlight()
        self._commit_caret()
        return True

    # -- public edit API -------------------------------------------------------

    def insert_text(self, text):
        if not self._editable("insert"):
            return False
        if text == "" and not self.has_selection():
            return True
        sel = self._sel_ordered()
        if sel is not None:
            return self._edit(sel[0], sel[1], text, coalesce=False)
        coalesce = (len(text) == 1 and text != "\n")
        return self._edit(self._caret, self._caret, text, coalesce=coalesce)

    def newline_with_autoindent(self):
        if not self._editable("newline"):
            return False
        sel = self._sel_ordered()
        anchor = sel[0] if sel is not None else self._caret
        line_text = self.document.line(anchor[0])
        lead = line_text[:len(line_text) - len(line_text.lstrip(" \t"))]
        head = line_text[:anchor[1]].rstrip()
        if self.document.language == "python" and head.endswith(":"):
            lead += " " * self.indent_width
        start, end = (sel if sel is not None else (self._caret, self._caret))
        return self._edit(start, end, "\n" + lead, coalesce=False)

    def insert_tab(self):
        if not self._editable("tab"):
            return False
        sel = self._sel_ordered()
        col = (sel[0] if sel is not None else self._caret)[1]
        w = max(1, self.indent_width)
        spaces = w - (col % w) or w              # advance to the next tab stop
        return self.insert_text(" " * spaces)

    def backspace(self):
        if not self._editable("backspace"):
            return False
        if self.has_selection():
            return self._edit(*self._sel_ordered(), new_text="")
        line, col = self._caret
        if (line, col) == (0, 0):
            return True
        prev = (line, col - 1) if col > 0 else (line - 1, len(self.document.line(line - 1)))
        return self._edit(prev, self._caret, "")

    def del_forward(self):
        if not self._editable("delete"):
            return False
        if self.has_selection():
            return self._edit(*self._sel_ordered(), new_text="")
        line, col = self._caret
        if self._caret == self.document.end_pos():
            return True
        nxt = (line, col + 1) if col < len(self.document.line(line)) else (line + 1, 0)
        return self._edit(self._caret, nxt, "")

    def delete_range(self, start, end):
        if not self._editable("delete_range"):
            return False
        return self._edit(tuple(start), tuple(end), "")

    # -- block indent / dedent (one undo record; selection-preserving) ---------
    #
    # Operate on every line the selection spans, or the caret line with no
    # selection. Indent prepends `indent_width` spaces at col 0 of each non-blank
    # line (blank / whitespace-only lines are skipped — no trailing indent on empty
    # lines); dedent strips up to `indent_width` leading SPACE chars per line,
    # stopping at the first non-space so a leading tab is left as-found. The whole
    # block is ONE contiguous replace => one undo step; every caret/selection
    # endpoint shifts by its line's column delta so repeated ops keep the same
    # logical selection, and undo/redo restore the exact prior/after selection.

    @staticmethod
    def _reindent_line(text, sign, width):
        """(new_text, col_delta) for one line under an indent (sign>0) / dedent."""
        if sign > 0:
            if text.strip() == "":
                return text, 0
            return (" " * width) + text, width
        n = 0
        while n < width and n < len(text) and text[n] == " ":
            n += 1
        return text[n:], -n

    def _block_reindent(self, sign):
        what = "indent" if sign > 0 else "dedent"
        if not self._editable(what):
            return False
        doc = self.document
        width = max(1, self.indent_width)
        sel = self._sel_ordered()
        l0 = sel[0][0] if sel is not None else self._caret[0]
        l1 = sel[1][0] if sel is not None else self._caret[0]
        l0 = max(0, min(l0, doc.line_count - 1))
        l1 = max(0, min(l1, doc.line_count - 1))
        orig = [doc.line(li) for li in range(l0, l1 + 1)]
        new = []
        deltas = {}
        for i, li in enumerate(range(l0, l1 + 1)):
            nt, d = self._reindent_line(orig[i], sign, width)
            new.append(nt)
            deltas[li] = d
        removed = "\n".join(orig)
        inserted = "\n".join(new)
        if inserted == removed:
            return True                           # blank block / no leading spaces: nothing to shift

        def shift(pos):
            if pos is None:
                return None
            li, c = pos
            return (li, max(0, c + deltas[li])) if li in deltas else (li, c)

        start = (l0, 0)
        end = (l1, len(orig[-1]))
        caret_before = self._caret
        sel_before = self._sel_anchor
        caret_after = shift(self._caret)
        sel_after = shift(self._sel_anchor)
        doc.delete(start, end)
        doc.insert(l0, 0, inserted)
        self._push_undo(start, removed, inserted, caret_before, caret_after,
                        False, sel_before=sel_before, sel_after=sel_after)
        self._sel_anchor = doc.clamp_pos(*sel_after) if sel_after is not None else None
        self._caret = caret_after
        self.rehighlight()
        self._commit_caret()
        return True

    def indent_selection(self):
        return self._block_reindent(+1)

    def dedent_selection(self):
        return self._block_reindent(-1)

    # -- caret + selection movement --------------------------------------------

    def _commit_caret(self, update_goal=True):
        self._caret = self.document.clamp_pos(*self._caret)
        if update_goal:
            self._goal_col = self._caret[1]
        self.current_line = self._caret[0]
        self._reset_blink()
        self._ensure_caret_visible()
        self.markDirty()

    def _begin_move(self, select):
        """Manage the selection anchor at the start of a caret move."""
        if select:
            if self._sel_anchor is None:
                self._sel_anchor = self._caret
        else:
            self._sel_anchor = None

    def set_caret(self, line, col, select=False):
        self._begin_move(select)
        self._caret = self.document.clamp_pos(line, col)
        self._commit_caret()
        return True

    def select_range(self, start, end):
        self._sel_anchor = self.document.clamp_pos(*start)
        self._caret = self.document.clamp_pos(*end)
        self._commit_caret()
        return True

    def select_all(self):
        self._sel_anchor = (0, 0)
        self._caret = self.document.end_pos()
        self._commit_caret()
        return True

    def _word_bounds(self, line, col):
        text = self.document.line(line)
        n = len(text)
        if n == 0:
            return (0, 0)
        def is_word(c):
            return c.isalnum() or c == "_"
        i = min(col, n - 1)
        cls = is_word(text[i])
        s = i
        while s > 0 and is_word(text[s - 1]) == cls:
            s -= 1
        e = i
        while e < n and is_word(text[e]) == cls:
            e += 1
        return (s, e)

    def select_word_at(self, line, col):
        line, col = self.document.clamp_pos(line, col)
        s, e = self._word_bounds(line, col)
        self._sel_anchor = (line, s)
        self._caret = (line, e)
        self._commit_caret()
        return True

    def _word_left(self, line, col):
        if col == 0:
            return (line - 1, len(self.document.line(line - 1))) if line > 0 else (0, 0)
        text = self.document.line(line)
        i = col
        while i > 0 and not (text[i - 1].isalnum() or text[i - 1] == "_"):
            i -= 1
        while i > 0 and (text[i - 1].isalnum() or text[i - 1] == "_"):
            i -= 1
        return (line, i)

    def _word_right(self, line, col):
        text = self.document.line(line)
        n = len(text)
        if col >= n:
            return (line + 1, 0) if line < self.document.line_count - 1 else (line, n)
        i = col
        while i < n and not (text[i].isalnum() or text[i] == "_"):
            i += 1
        while i < n and (text[i].isalnum() or text[i] == "_"):
            i += 1
        return (line, i)

    def move_caret(self, direction, by_word=False, select=False):
        self._begin_move(select)
        line, col = self._caret
        doc = self.document
        vertical = direction in ("up", "down", "page_up", "page_down")
        if direction == "left":
            pos = self._word_left(line, col) if by_word else (
                (line, col - 1) if col > 0 else
                ((line - 1, len(doc.line(line - 1))) if line > 0 else (0, 0)))
        elif direction == "right":
            pos = self._word_right(line, col) if by_word else (
                (line, col + 1) if col < len(doc.line(line)) else
                ((line + 1, 0) if line < doc.line_count - 1 else (line, col)))
        elif direction == "home":
            lead = len(doc.line(line)) - len(doc.line(line).lstrip(" \t"))
            pos = (line, 0 if col <= lead else lead)      # smart-home: toggle indent<->0
        elif direction == "end":
            pos = (line, len(doc.line(line)))
        elif direction == "doc_home":
            pos = (0, 0)
        elif direction == "doc_end":
            pos = doc.end_pos()
        elif direction in ("up", "down"):
            nl = line + (1 if direction == "down" else -1)
            pos = (nl, self._goal_col)
        elif direction in ("page_up", "page_down"):
            step = max(1, self._visible_line_count())
            nl = line + (step if direction == "page_down" else -step)
            pos = (nl, self._goal_col)
        else:
            pos = (line, col)
        self._caret = doc.clamp_pos(*pos)
        self._commit_caret(update_goal=not vertical)
        return True

    # -- clipboard (engine GLFW clipboard, widget-local fallback in headless) ---

    def _clip_set(self, text):
        self._clip_fallback = text
        ctx = getattr(self, "_ctx", None)
        if ctx is not None:
            try:
                ctx.setClipboardText(text)
            except Exception:
                pass

    def _clip_get(self):
        ctx = getattr(self, "_ctx", None)
        if ctx is not None:
            try:
                t = ctx.getClipboardText()
                if t:
                    return t
            except Exception:
                pass
        return self._clip_fallback

    def copy(self):
        text = self.selected_text()
        if text:
            self._clip_set(text)
            self._status = f"copied {len(text)} chars"
        return text

    def cut(self):
        if not self.has_selection():
            return False
        if not self._editable("cut"):
            return False
        self._clip_set(self.selected_text())
        return self._edit(*self._sel_ordered(), new_text="")

    def paste(self):
        if not self._editable("paste"):
            return False
        text = self._clip_get()
        if text == "":
            return False
        return self.insert_text(text)

    # -- undo / redo -----------------------------------------------------------

    def undo(self):
        if not self._undo:
            return False
        rec = self._undo.pop()
        self._redo.append(rec)
        doc = self.document
        ins_end = _pos_advance(rec.start, rec.inserted)
        if rec.inserted:
            doc.delete(rec.start, ins_end)
        if rec.removed:
            doc.insert(rec.start[0], rec.start[1], rec.removed)
        self._sel_anchor = rec.sel_before
        self._caret = doc.clamp_pos(*rec.caret_before)
        self.rehighlight()
        self._commit_caret()
        return True

    def redo(self):
        if not self._redo:
            return False
        rec = self._redo.pop()
        self._undo.append(rec)
        doc = self.document
        rem_end = _pos_advance(rec.start, rec.removed)
        if rec.removed:
            doc.delete(rec.start, rem_end)
        if rec.inserted:
            doc.insert(rec.start[0], rec.start[1], rec.inserted)
        self._sel_anchor = rec.sel_after
        self._caret = doc.clamp_pos(*rec.caret_after)
        self.rehighlight()
        self._commit_caret()
        return True

    # -- focus + blink ---------------------------------------------------------

    def focus(self):
        self._focused = True
        self._reset_blink()
        self.markDirty()

    def blur(self):
        self._focused = False
        self._blink_on = False
        self.markDirty()

    def _reset_blink(self):
        self._blink_on = True
        self._blink_frame = 0
        self._dirty = True

    # -- caret/selection geometry (pure; gate-observable without GPU) -----------

    def _visible_line_count(self):
        vh = self._content_view_h()
        return max(1, int(vh // self._line_h)) if (self._line_h and vh > 0) else 1

    def caret_screen_pos(self):
        """(x, y) pixel of the caret's top-left in canvas-local space."""
        gx = self._gutter_width()
        top = self._content_top()
        x = gx + CONTENT_PAD_L + self._caret[1] * self._adv_w - self.hscroll.offset
        y = top + self._caret[0] * self._line_h - self.vscroll.offset
        return (x, y)

    def selection_spans_for_line(self, li):
        """(c0, c1) columns the selection band covers on line `li`, or None. The
        trailing-newline hint (+1 col) on continued lines matches the rebuild."""
        sel = self._sel_ordered()
        if sel is None:
            return None
        (s0l, s0c), (s1l, s1c) = sel
        if not (s0l <= li <= s1l):
            return None
        c0 = s0c if li == s0l else 0
        c1 = s1c if li == s1l else len(self.document.line(li)) + 1
        return (c0, c1)

    def _ensure_caret_visible(self):
        """Scroll so the caret stays inside the content viewport (follow-caret)."""
        lh, aw = self._line_h, self._adv_w
        line, col = self._caret
        vh = self._content_view_h()
        if vh > 0:
            top_px, bot_px = line * lh, (line + 1) * lh
            if top_px < self.vscroll.offset:
                self.vscroll.offset = top_px
            elif bot_px > self.vscroll.offset + vh:
                self.vscroll.offset = bot_px - vh
            self.vscroll.clamp()
        vw = self._content_view_w()
        if vw > 0:
            xl, xr = col * aw, (col + 1) * aw
            if xl < self.hscroll.offset:
                self.hscroll.offset = xl
            elif xr > self.hscroll.offset + vw:
                self.hscroll.offset = xr - vw
            self.hscroll.clamp()

    # -- key handlers (thin wrappers; host forwards KEY_DOWN/UP — terrainedit
    #    idiom). ALL bindings are WIDGET-FOCUS-LOCAL: they act only when this
    #    CodeView owns key focus, so they never collide with app-level chords. ---

    @staticmethod
    def _char_for_key(kc, shift):
        if 65 <= kc <= 90:                        # letters arrive as uppercase codes
            return chr(kc) if shift else chr(kc).lower()
        if kc == 32:
            return " "
        if 48 <= kc <= 57:
            return _KEY_DIGITS_SHIFTED[kc] if shift else chr(kc)
        pair = _KEY_PUNCT.get(kc)
        if pair is not None:
            return pair[1] if shift else pair[0]
        return None

    def handleKeyDown(self, ev):
        kc = ev.keycode
        shift = bool(ev.shift)
        sup = bool(ev.super)
        ctrl = bool(ev.ctrl)
        alt = bool(ev.alt)
        cmd = sup or ctrl                         # clipboard / undo accelerator
        # -- accelerators --
        if cmd and kc == ord("C"):
            self.copy(); return
        if cmd and kc == ord("X"):
            self.cut(); return
        if cmd and kc == ord("V"):
            self.paste(); return
        if cmd and kc == ord("A"):
            self.select_all(); return
        if cmd and kc == ord("Z"):
            self.redo() if shift else self.undo(); return
        if cmd and kc == ord("Y"):
            self.redo(); return
        if cmd and kc == 93:                      # RIGHT_BRACKET (]) -> indent block
            self.indent_selection(); return
        if cmd and kc == 91:                      # LEFT_BRACKET ([) -> dedent block
            self.dedent_selection(); return
        # -- navigation (super => line/doc extents; ctrl|alt => by word) --
        word = alt or ctrl
        if kc == 263:                             # LEFT
            self.move_caret("home" if sup else "left", by_word=word, select=shift); return
        if kc == 262:                             # RIGHT
            self.move_caret("end" if sup else "right", by_word=word, select=shift); return
        if kc == 265:                             # UP
            self.move_caret("doc_home" if sup else "up", select=shift); return
        if kc == 264:                             # DOWN
            self.move_caret("doc_end" if sup else "down", select=shift); return
        if kc == 268:                             # HOME
            self.move_caret("home", select=shift); return
        if kc == 269:                             # END
            self.move_caret("end", select=shift); return
        if kc == 266:                             # PAGE_UP
            self.move_caret("page_up", select=shift); return
        if kc == 267:                             # PAGE_DOWN
            self.move_caret("page_down", select=shift); return
        # -- edits (each self-gates on read-only) --
        if kc == 259:                             # BACKSPACE
            self.backspace(); return
        if kc == 261:                             # DELETE
            self.del_forward(); return
        if kc == 257:                             # ENTER
            self.newline_with_autoindent(); return
        if kc == 258:                             # TAB / SHIFT-TAB
            if shift:                             # shift-tab -> dedent (selection or caret line)
                self.dedent_selection()
            elif self._selection_is_multiline():  # multi-line selection -> block indent
                self.indent_selection()
            else:                                 # single-line/no-selection: unchanged tab-stop / replace
                self.insert_tab()
            return
        if not (cmd or alt):                      # printable (bare / shifted only)
            ch = self._char_for_key(kc, shift)
            if ch is not None:
                self.insert_text(ch)

    def handleKeyUp(self, ev):
        pass                                      # modifier state is read live per KEY_DOWN

    # -- factories (NodeEditor idiom) ------------------------------------------

    @staticmethod
    def uifactory(parent_layoutgroup, args):
        """args: [name, document?, font_id?, bg_color?]"""
        name = args[0]
        doc = args[1] if len(args) > 1 else None
        font_id = args[2] if len(args) > 2 else "i16"
        bg = args[3] if len(args) > 3 else PALETTE["bg"]
        canvas_item = parent_layoutgroup.makeChild(uiclass=lev2.ui.PrimCanvas, args=[name])
        canvas = canvas_item.widget
        canvas.bg_color = bg
        canvas.draw_background = True
        cv = CodeView(canvas, document=doc, font_id=font_id)
        canvas.uservars.code_view = cv
        return canvas_item

    @staticmethod
    def wfactory(args):
        """args: [name, document?, font_id?, bg_color?]"""
        name = args[0]
        doc = args[1] if len(args) > 1 else None
        font_id = args[2] if len(args) > 2 else "i16"
        bg = args[3] if len(args) > 3 else PALETTE["bg"]
        canvas = lev2.ui.PrimCanvas.wfactory([name])
        canvas.bg_color = bg
        canvas.draw_background = True
        cv = CodeView(canvas, document=doc, font_id=font_id)
        canvas.uservars.code_view = cv
        return canvas
