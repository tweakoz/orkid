################################################################################
# ork.uitest — session format v1
#
# A session is INPUT ONLY: a header plus frame-indexed ui::Event records. The
# on-disk form is JSON-Lines: line 1 is the header object, each subsequent line
# is one record object, every object emitted with sorted keys and compact
# separators. This is byte-deterministic (a given Session -> identical bytes) and
# gives the smallest reviewable git diffs (one record == one line).
#
# Coordinates are RAW pixels plus the screen_dim that routing actually saw at
# record time (replay renormalizes against the current surface size — no unit
# coords are ever stored). The file carries NO machine-specific data: no paths,
# no hostnames, no usernames, no wall-clock timestamps.
################################################################################

import json

from . import eventcodes

SESSION_VERSION = 1

# record dict keys, per event class (see eventcodes.class_for_name)
_COMMON_KEYS = ("frame", "code", "code_name")
_KEY_KEYS = ("keycode", "shift", "ctrl", "alt", "super")
_POINTER_KEYS = ("x", "y", "screen_dim", "left", "middle", "right",
                 "shift", "ctrl", "alt", "super")
_WHEEL_KEYS = ("wheel_x", "wheel_y")


def _screen_dim_of(ev):
    """Extract [w, h] ints from a duck-typed event's screen_dim (vec2 or seq)."""
    sd = getattr(ev, "screen_dim", None)
    if sd is None:
        return None
    try:
        return [int(round(sd.x)), int(round(sd.y))]
    except AttributeError:
        return [int(round(sd[0])), int(round(sd[1]))]


def record_from_event(frame, ev, win=None):
    """Build one normalized, JSON-ready record dict from a duck-typed event.

    Reads every field eagerly (the primary window mutates one shared Event in
    place — see spec trap #1 — so the snapshot must be taken at tap time). Only
    ints, strings and int-lists land in the dict, so serialization is stable.

    `win` (optional str) tags the source window: ABSENT means the primary
    window; secondary windows carry the recorder-assigned sticky key ("sec1",
    "sec2", ...). Additive/advisory — playback ignores it.
    """
    code = int(ev.code)
    name = eventcodes.code_name_for(code)
    rec = {
        "frame": int(frame),
        "code": code,
        "code_name": name,
    }
    if win:
        rec["win"] = str(win)
    cls = eventcodes.class_for_name(name)
    if cls == eventcodes.CLASS_KEY:
        rec["keycode"] = int(ev.keycode)
        rec["shift"] = int(bool(ev.shift))
        rec["ctrl"] = int(bool(ev.ctrl))
        rec["alt"] = int(bool(ev.alt))
        rec["super"] = int(bool(ev.super))
    elif cls == eventcodes.CLASS_POINTER:
        rec["x"] = int(ev.x)
        rec["y"] = int(ev.y)
        rec["screen_dim"] = _screen_dim_of(ev) or [0, 0]
        rec["left"] = int(bool(ev.left))
        rec["middle"] = int(bool(ev.middle))
        rec["right"] = int(bool(ev.right))
        rec["shift"] = int(bool(ev.shift))
        rec["ctrl"] = int(bool(ev.ctrl))
        rec["alt"] = int(bool(ev.alt))
        rec["super"] = int(bool(ev.super))
    elif cls == eventcodes.CLASS_WHEEL:
        rec["wheel_x"] = int(ev.wheel_x)
        rec["wheel_y"] = int(ev.wheel_y)
    return rec


class Record:
    """Typed, read-only view over a normalized record dict.

    Attribute access exposes the record fields; as_dict() returns the canonical
    serialized form. Round-trip equality is by canonical dict.
    """

    __slots__ = ("_d",)

    def __init__(self, d):
        object.__setattr__(self, "_d", dict(d))

    def __getattr__(self, key):
        try:
            return object.__getattribute__(self, "_d")[key]
        except KeyError:
            raise AttributeError(key)

    def __getitem__(self, key):
        return self._d[key]

    def __contains__(self, key):
        return key in self._d

    def get(self, key, default=None):
        return self._d.get(key, default)

    def as_dict(self):
        return dict(self._d)

    def __eq__(self, other):
        if isinstance(other, Record):
            return self._d == other._d
        if isinstance(other, dict):
            return self._d == other
        return NotImplemented

    def __repr__(self):
        return "Record(%r)" % (self._d,)


def _dumps(obj):
    # compact + sorted keys == byte-deterministic + minimal diffs
    return json.dumps(obj, sort_keys=True, separators=(",", ":"))


class Session:
    """An ordered, frame-indexed input session (format v1)."""

    def __init__(self, screen_dim=None):
        self._records = []  # list[dict] — canonical serialized form
        self._screen_dim = [int(screen_dim[0]), int(screen_dim[1])] if screen_dim else [0, 0]

    # -- construction --------------------------------------------------------

    def add_event(self, frame, ev, win=None):
        """Snapshot a duck-typed event at `frame` and append it as a record."""
        rec = record_from_event(frame, ev, win)
        # adopt the record-time screen_dim from the first event that reports one,
        # unless a header screen_dim was set explicitly.
        if self._screen_dim == [0, 0]:
            sd = _screen_dim_of(ev)
            if sd and sd != [0, 0]:
                self._screen_dim = sd
        self._records.append(rec)
        return rec

    def add_record(self, rec):
        """Append an already-normalized record dict (or Record)."""
        d = rec.as_dict() if isinstance(rec, Record) else dict(rec)
        self._records.append(d)
        return d

    # -- accessors -----------------------------------------------------------

    @property
    def screen_dim(self):
        return list(self._screen_dim)

    @screen_dim.setter
    def screen_dim(self, value):
        self._screen_dim = [int(value[0]), int(value[1])]

    @property
    def event_count(self):
        return len(self._records)

    @property
    def records(self):
        """Typed, read-only records in record (frame) order."""
        return [Record(d) for d in self._records]

    def header(self):
        return {
            "version": SESSION_VERSION,
            "screen_dim": list(self._screen_dim),
            "event_count": len(self._records),
        }

    # -- serialization -------------------------------------------------------

    def to_text(self):
        """Deterministic JSON-Lines text (newline-terminated)."""
        lines = [_dumps(self.header())]
        lines.extend(_dumps(rec) for rec in self._records)
        return "\n".join(lines) + "\n"

    def to_bytes(self):
        return self.to_text().encode("utf-8")

    def write(self, path):
        with open(path, "wb") as f:
            f.write(self.to_bytes())

    # -- deserialization -----------------------------------------------------

    @classmethod
    def from_text(cls, text):
        lines = [ln for ln in text.split("\n") if ln.strip() != ""]
        if not lines:
            raise ValueError("uitest session: empty file")
        header = json.loads(lines[0])
        version = header.get("version")
        if version != SESSION_VERSION:
            raise ValueError(
                "uitest session: unsupported version %r (expected %d)"
                % (version, SESSION_VERSION))
        sess = cls(screen_dim=header.get("screen_dim") or [0, 0])
        for ln in lines[1:]:
            sess._records.append(json.loads(ln))
        return sess

    @classmethod
    def read(cls, path):
        with open(path, "rb") as f:
            return cls.from_text(f.read().decode("utf-8"))
