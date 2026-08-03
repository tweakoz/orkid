################################################################################
# ork.uitest — playback (PLAY half)
#
# Frame-locked replay of a Session through app.injectUiEvent (the C++ funnel that
# reproduces CtxGLFW::_fire_ui_event minus GLFW). Events are built by the ui.Event
# factories (make_key / make_pointer / make_wheel) from each record's own fields —
# so a replayed POINTER carries the record-time raw pixels + screen_dim, and a
# re-record of the replay is byte-equal to the source session.
#
# Drive point: call Player.on_update(updata) from the app's onUpdate(updata) in
# lockstep/synchronous mode. Injection is SYNCHRONOUS on the update thread; in
# lockstep the render (main thread) never overlaps onUpdate, so handleEvent runs
# race-free and the recorder's frame stamp is coherent (the round-trip proof).
#
# The click clock is virtualized during replay (Context.virtual_time) so
# frame-paced PUSHes reproduce DOUBLECLICK deterministically from frame deltas,
# independent of engine UPS and wall clock.
################################################################################

import math

from . import eventcodes


# Injection target resolution: window=None routes to the main window (app.injectUiEvent);
# window=<EzSecondaryWin> routes to that secondary window (win.injectUiEvent) — both expose
# the SAME injectUiEvent(ev) signature, so a composer is window-agnostic. Keeping window as a
# trailing default-None kwarg leaves every existing main-window call site source-compatible.
def _dst(app, window):
    return app if window is None else window


# One virtual second every 1/_VIRTUAL_DT frames. Chosen so a couple of frames of
# separation stays under the 0.5s double-click window while a handful of frames
# clears the 0.75s re-arm — see Context::_handleEventImpl. Deterministic (keyed on
# the frame counter), so two replays produce identical DOUBLECLICK derivations.
_VIRTUAL_DT = 0.1


_CODES = None


def _codes():
    """name(str) -> crc(int) for the raw event codes, resolved from the engine."""
    global _CODES
    if _CODES is not None:
        return _CODES
    from orkengine.core import CrcStringProxy
    tokens = CrcStringProxy()
    names = ("PUSH", "RELEASE", "MOVE", "DRAG",
             "KEY_DOWN", "KEY_UP", "KEY_REPEAT", "MOUSEWHEEL")
    _CODES = {n: int(getattr(tokens, n).hashed) for n in names}
    return _CODES


def _ui():
    from orkengine import lev2
    return lev2.ui


################################################################################
# record -> ui.Event  (factory composition; raw codes only)


def _wheel_delta(scaled):
    """Recover the make_wheel(dx) input that reproduces a recorded ×10 wheel int
    exactly (make_wheel truncates int(dx*10.0); nudge past the truncation)."""
    if scaled == 0:
        return 0.0
    return (scaled + math.copysign(0.5, scaled)) / 10.0


def _event_from_record(rec):
    """Build a ui.Event for one Session record via the factories. Returns None for
    records whose class this slice does not inject (only raw input is ever stored)."""
    ui = _ui()
    codes = _codes()
    name = rec["code_name"] if "code_name" in rec else eventcodes.code_name_for(rec["code"])
    code = int(rec["code"])
    cls = eventcodes.class_for_name(name)
    if cls == eventcodes.CLASS_KEY:
        return ui.Event.make_key(
            code=code,
            keycode=int(rec["keycode"]),
            shift=bool(rec["shift"]),
            ctrl=bool(rec["ctrl"]),
            alt=bool(rec["alt"]),
            super_=bool(rec["super"]))
    if cls == eventcodes.CLASS_POINTER:
        sd = rec["screen_dim"]
        return ui.Event.make_pointer(
            code=code,
            x=int(rec["x"]),
            y=int(rec["y"]),
            screen_w=int(sd[0]),
            screen_h=int(sd[1]),
            left=bool(rec["left"]),
            middle=bool(rec["middle"]),
            right=bool(rec["right"]),
            shift=bool(rec["shift"]),
            ctrl=bool(rec["ctrl"]),
            alt=bool(rec["alt"]),
            super_=bool(rec["super"]))
    if cls == eventcodes.CLASS_WHEEL:
        return ui.Event.make_wheel(
            dx=_wheel_delta(int(rec["wheel_x"])),
            dy=_wheel_delta(int(rec["wheel_y"])))
    return None


################################################################################
# Player — frame-locked session replay


class Player:
    """Replays a Session into an OrkEzApp, keyed on updata.counter.

    Attach by calling on_update(updata) from the app's onUpdate(updata). Records
    whose `frame` equals the current counter are injected in list order. The click
    clock is virtualized every frame so double-click derivation is deterministic.
    """

    def __init__(self, app, session, virtual_dt=_VIRTUAL_DT, virtualize_clock=True,
                 window=None, rebase=False):
        self._app = app
        self._session = session
        self._window = window            # None=main window, else an EzSecondaryWin target
        self._virtual_dt = float(virtual_dt)
        self._virtualize_clock = bool(virtualize_clock)
        # A Player targets ONE window. Records tagged with a recording-session
        # window key ("win": secondary-window events) are SKIPPED — the tag names
        # the recording session's window, which need not exist on replay (v1).
        # rebase=True shifts frames so the earliest record fires at counter 0
        # (live captures carry absolute frame stamps from mid-session).
        self._skipped_tagged = 0
        recs = []
        for rec in session.records:
            d = rec.as_dict()
            if d.get("win"):
                self._skipped_tagged += 1
                continue
            recs.append(d)
        base = min((int(d["frame"]) for d in recs), default=0) if rebase else 0
        self._by_frame = {}
        for d in recs:
            self._by_frame.setdefault(int(d["frame"]) - base, []).append(d)
        self._max_frame = max(self._by_frame) if self._by_frame else -1
        self._done = self._max_frame < 0
        self._injected = 0
        # catch-up cursor: on_update fires every pending frame in (last, counter],
        # so a driver whose sampling is coarser than the record clock (a render-tick
        # drive over update-counter stamps) never skips records. Sorted once here.
        self._frames_sorted = sorted(self._by_frame)
        self._next_index = 0

    @property
    def session(self):
        return self._session

    @property
    def done(self):
        return self._done

    @property
    def injected_count(self):
        return self._injected

    @property
    def skipped_tagged(self):
        return self._skipped_tagged

    @property
    def max_frame(self):
        return self._max_frame

    def on_update(self, updata):
        counter = int(updata.counter)
        if self._virtualize_clock:
            # virtual clock lives on the TARGET window's ui context (double-click
            # derivation is per-context), so a secondary-window replay virtualizes
            # the secondary context's clock.
            ctx = self._window.ui_context if self._window is not None else self._app.uicontext
            if ctx is not None:
                ctx.virtual_time_enabled = True
                ctx.virtual_time = counter * self._virtual_dt
        dst = _dst(self._app, self._window)
        # catch-up: fire EVERY not-yet-fired frame <= counter, in frame order (a
        # coarser driver clock lands the skipped frames' records in this batch —
        # natively multiple events arrive in one poll batch the same way).
        while (self._next_index < len(self._frames_sorted)
               and self._frames_sorted[self._next_index] <= counter):
            for rec in self._by_frame[self._frames_sorted[self._next_index]]:
                ev = _event_from_record(rec)
                if ev is not None:
                    dst.injectUiEvent(ev)
                    self._injected += 1
            self._next_index += 1
        if counter >= self._max_frame:
            self._done = True


def play(app, source, **kwargs):
    """Build a Player for `source` (a Session or a path to a session file).

    Frame-locked replay is driven by the caller: forward the app's update tick via
    player.on_update(updata) from onUpdate(updata) in lockstep mode.
    """
    from .session import Session
    session = source if isinstance(source, Session) else Session.read(source)
    return Player(app, session, **kwargs)


################################################################################
# immediate-injection helpers (compose factory + inject; call from onUpdate)
#
# These inject NOW (no scheduling) — use them to script input a frame at a time.
# Record raw only: click/drag emit PUSH/RELEASE/DRAG and let the Context derive
# DOUBLECLICK / BEGIN_DRAG / END_DRAG.


def push(app, x, y, screen_w, screen_h, button="left", mods=None, window=None):
    ui = _ui()
    codes = _codes()
    b = _button_flags(button)
    m = _mod_flags(mods)
    _dst(app, window).injectUiEvent(ui.Event.make_pointer(
        code=codes["PUSH"], x=x, y=y, screen_w=screen_w, screen_h=screen_h,
        left=b[0], middle=b[1], right=b[2], **m))


def release(app, x, y, screen_w, screen_h, button="left", mods=None, window=None):
    ui = _ui()
    codes = _codes()
    m = _mod_flags(mods)
    # a RELEASE reports the buttons as up (mirrors the GLFW button-up fill).
    _dst(app, window).injectUiEvent(ui.Event.make_pointer(
        code=codes["RELEASE"], x=x, y=y, screen_w=screen_w, screen_h=screen_h,
        left=False, middle=False, right=False, **m))


def move(app, x, y, screen_w, screen_h, last=None, button=None, mods=None, window=None):
    ui = _ui()
    codes = _codes()
    # button held during a move => DRAG (mirrors CtxGLFW::_on_callback_cursor:
    # MOVE when no button, DRAG when a button is down). Raw DRAG is what a real
    # drag records; the Context synthesizes BEGIN_DRAG / END_DRAG around it.
    b = _button_flags(button) if button else (False, False, False)
    code = codes["DRAG"] if button else codes["MOVE"]
    lx, ly = (last if last is not None else (x, y))
    m = _mod_flags(mods)
    _dst(app, window).injectUiEvent(ui.Event.make_pointer(
        code=code, x=x, y=y, screen_w=screen_w, screen_h=screen_h,
        last_x=lx, last_y=ly, left=b[0], middle=b[1], right=b[2], **m))


def click(app, x, y, screen_w, screen_h, button="left", mods=None, window=None):
    """A PUSH then RELEASE at (x, y). Sets focus / caret on click-aware widgets."""
    push(app, x, y, screen_w, screen_h, button=button, mods=mods, window=window)
    release(app, x, y, screen_w, screen_h, button=button, mods=mods, window=window)


def drag(app, x0, y0, x1, y1, screen_w, screen_h, steps=8, button="left", mods=None, window=None):
    """PUSH at start, `steps` interpolated DRAGs, RELEASE at end — the Context
    derives BEGIN_DRAG on the first DRAG and END_DRAG on the RELEASE."""
    push(app, x0, y0, screen_w, screen_h, button=button, mods=mods, window=window)
    steps = max(1, int(steps))
    last = (x0, y0)
    for i in range(1, steps + 1):
        t = i / float(steps)
        xi = int(round(x0 + (x1 - x0) * t))
        yi = int(round(y0 + (y1 - y0) * t))
        move(app, xi, yi, screen_w, screen_h, last=last, button=button, mods=mods, window=window)
        last = (xi, yi)
    release(app, x1, y1, screen_w, screen_h, button=button, mods=mods, window=window)


def wheel(app, dx, dy, window=None):
    """One wheel event; dx/dy are raw scroll notches (×10-normalized by the factory)."""
    ui = _ui()
    _dst(app, window).injectUiEvent(ui.Event.make_wheel(dx=dx, dy=dy))


def key_down(app, keycode, mods=None, window=None):
    ui = _ui()
    codes = _codes()
    _dst(app, window).injectUiEvent(ui.Event.make_key(code=codes["KEY_DOWN"], keycode=keycode, **_mod_flags(mods)))


def key_up(app, keycode, mods=None, window=None):
    ui = _ui()
    codes = _codes()
    _dst(app, window).injectUiEvent(ui.Event.make_key(code=codes["KEY_UP"], keycode=keycode, **_mod_flags(mods)))


def key_chord(app, keycode, mods=None, window=None):
    """A KEY_DOWN then KEY_UP for one keycode with modifier flags (e.g. an
    accelerator like cmd-] => key_chord(app, 93, mods={'super_': True}))."""
    key_down(app, keycode, mods=mods, window=window)
    key_up(app, keycode, mods=mods, window=window)


def _focus(app, code_name, window=None):
    """Inject a raw focus event (GOT_KEYFOCUS / LOST_KEYFOCUS). These are engine-RAW
    (the enterleave callback synthesizes them on window enter/exit) and are NOT in the
    factory's derived-refuse set, so the pointer factory accepts the code; the inject
    funnel fills the last pointer position (a focus event carries no coords of its own).
    Used by gates that reproduce the window-exit kill chain (cross-window drag)."""
    from orkengine.core import CrcStringProxy
    ui = _ui()
    code = int(getattr(CrcStringProxy(), code_name).hashed)
    _dst(app, window).injectUiEvent(ui.Event.make_pointer(
        code=code, x=0, y=0, screen_w=1, screen_h=1))


def lost_keyfocus(app, window=None):
    """Inject LOST_KEYFOCUS — what window-exit manifests as mid-drag (the engine
    synthesizes focus loss from mouse-leave; the app never lost real OS focus)."""
    _focus(app, "LOST_KEYFOCUS", window=window)


def got_keyfocus(app, window=None):
    """Inject GOT_KEYFOCUS — window re-enter."""
    _focus(app, "GOT_KEYFOCUS", window=window)


def type_text(app, text, window=None):
    """Type an ASCII string as KEY_DOWN/KEY_UP pairs (US-layout keycodes). Best
    effort: letters, digits, space and the US punctuation the CodeView maps."""
    for ch in text:
        kc, shift = _keycode_for_char(ch)
        if kc is None:
            continue
        mods = {"shift": True} if shift else None
        key_down(app, kc, mods=mods, window=window)
        key_up(app, kc, mods=mods, window=window)


################################################################################
# helper internals


def _button_flags(button):
    b = (button or "left").lower()
    return (b == "left", b == "middle", b == "right")


def _mod_flags(mods):
    """Normalize a mods dict/set/None to the factory's shift/ctrl/alt/super_ kwargs."""
    out = {"shift": False, "ctrl": False, "alt": False, "super_": False}
    if not mods:
        return out
    if isinstance(mods, (set, frozenset, list, tuple)):
        mods = {k: True for k in mods}
    for k, v in mods.items():
        key = "super_" if k in ("super", "super_", "cmd", "meta") else k
        if key in out:
            out[key] = bool(v)
    return out


# inverse of code_editor._char_for_key (US layout, physical GLFW keycodes)
_DIGITS_SHIFTED = {")": 48, "!": 49, "@": 50, "#": 51, "$": 52,
                   "%": 53, "^": 54, "&": 55, "*": 56, "(": 57}
_PUNCT = {"'": (39, False), '"': (39, True), ",": (44, False), "<": (44, True),
          "-": (45, False), "_": (45, True), ".": (46, False), ">": (46, True),
          "/": (47, False), "?": (47, True), ";": (59, False), ":": (59, True),
          "=": (61, False), "+": (61, True), "[": (91, False), "{": (91, True),
          "\\": (92, False), "|": (92, True), "]": (93, False), "}": (93, True),
          "`": (96, False), "~": (96, True)}


def _keycode_for_char(ch):
    """(keycode, shift) for one character, or (None, False) if unmapped."""
    if "a" <= ch <= "z":
        return (ord(ch.upper()), False)
    if "A" <= ch <= "Z":
        return (ord(ch), True)
    if ch == " ":
        return (32, False)
    if "0" <= ch <= "9":
        return (ord(ch), False)
    if ch in _DIGITS_SHIFTED:
        return (_DIGITS_SHIFTED[ch], True)
    if ch in _PUNCT:
        return _PUNCT[ch]
    return (None, False)
