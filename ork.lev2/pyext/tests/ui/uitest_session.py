#!/usr/bin/env ork.python
################################################################################
# ork.uitest — U0 session + recorder gate (headless, pure-stdlib logic).
#
# Covers the RECORD half of ork.uitest: session format v1 serialization
# determinism, schema/sorted-keys, machine-data scrub, and the recorder tap /
# dedupe path — all with duck-typed fake events.
#
# HONESTY: an end-to-end record of REAL input is impossible headless until U1
# injection exists (the --offscreen GLFW pump is dead — spec findings). The
# inject -> record round-trip is the planned end-to-end proof and lands in U2.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import socket
import getpass
import tempfile
import collections
import json

# lane-local scripts dir first, so `ork.uitest` resolves from THIS checkout
# (ork.python otherwise binds `ork` to the primary checkout). Harmless once the
# branch is integrated.
ROOT = os.path.abspath(__file__)
for _ in range(5):
    ROOT = os.path.dirname(ROOT)
sys.path.insert(0, os.path.join(ROOT, "obt.project", "scripts"))

from orkengine import core  # house law: core before lev2 / before ork.*
from orkengine import lev2
from orkengine.core import CrcStringProxy

from ork.uitest import Session, Recorder, SESSION_VERSION, record_from_event
from ork.uitest import eventcodes

tokens = CrcStringProxy()

################################################################################
# duck-typed fake events (U1 gives real ones; U0 proves format+tap logic)
################################################################################


class Vec2:
    __slots__ = ("x", "y")

    def __init__(self, x, y):
        self.x = x
        self.y = y


class FakeEvent:
    def __init__(self, code, **kw):
        self.code = int(code)
        self.keycode = kw.get("keycode", 0)
        self.x = kw.get("x", 0)
        self.y = kw.get("y", 0)
        self.wheel_x = kw.get("wheel_x", 0)
        self.wheel_y = kw.get("wheel_y", 0)
        self.left = kw.get("left", 0)
        self.middle = kw.get("middle", 0)
        self.right = kw.get("right", 0)
        self.shift = kw.get("shift", 0)
        self.ctrl = kw.get("ctrl", 0)
        self.alt = kw.get("alt", 0)
        self.super = kw.get("super", 0)
        sd = kw.get("screen_dim", (1920, 1080))
        self.screen_dim = Vec2(sd[0], sd[1])


class FakeUpdata:
    def __init__(self, counter):
        self.counter = counter


def crc(name):
    return int(getattr(tokens, name).hashed)


def ev_key(name, keycode, **mods):
    return FakeEvent(crc(name), keycode=keycode, **mods)


def ev_pointer(name, x, y, **kw):
    return FakeEvent(crc(name), x=x, y=y, **kw)


def ev_wheel(wx, wy):
    return FakeEvent(crc("MOUSEWHEEL"), wheel_x=wx, wheel_y=wy)


UNKNOWN_CRC = 0xDEADBEEF  # deliberately not an EventCode


def build_reference_session():
    """A representative session: all three event classes + modifier combos."""
    s = Session(screen_dim=(1920, 1080))
    frame = 0
    # key events, modifier sweep
    s.add_event(frame, ev_key("KEY_DOWN", ord("A"))); frame += 1
    s.add_event(frame, ev_key("KEY_DOWN", ord("B"), shift=1)); frame += 1
    s.add_event(frame, ev_key("KEY_REPEAT", ord("B"), shift=1, ctrl=1)); frame += 1
    s.add_event(frame, ev_key("KEY_UP", ord("B"), shift=1, ctrl=1, alt=1, super=1)); frame += 1
    # pointer events, buttons + mods
    s.add_event(frame, ev_pointer("MOVE", 100, 200)); frame += 1
    s.add_event(frame, ev_pointer("PUSH", 100, 200, left=1)); frame += 1
    s.add_event(frame, ev_pointer("DRAG", 140, 260, left=1, ctrl=1)); frame += 1
    s.add_event(frame, ev_pointer("RELEASE", 140, 260, right=1, shift=1,
                                  screen_dim=(2560, 1440))); frame += 1
    # wheel
    s.add_event(frame, ev_wheel(0, 30)); frame += 1
    s.add_event(frame, ev_wheel(-10, 0)); frame += 1
    # unknown code -> code_name "" fallback
    s.add_event(frame, FakeEvent(UNKNOWN_CRC)); frame += 1
    return s


################################################################################
# harness
################################################################################

CHECKS = 0


def check(cond, msg):
    global CHECKS
    if not cond:
        print("  FAIL:", msg, flush=True)
        raise AssertionError(msg)
    CHECKS += 1
    print("  ok:", msg, flush=True)


################################################################################
# T1 — serialization determinism
################################################################################

def test_determinism():
    print("[T1] serialization determinism", flush=True)
    s = build_reference_session()

    # same object -> identical bytes, twice
    b1 = s.to_bytes()
    b2 = s.to_bytes()
    check(b1 == b2, "same session object -> identical bytes")

    # two independently-built identical sessions -> identical bytes
    s2 = build_reference_session()
    check(s.to_bytes() == s2.to_bytes(), "two identical sessions -> identical bytes")

    # write -> read -> write round-trip is byte-equal
    with tempfile.TemporaryDirectory() as d:
        pa = os.path.join(d, "a.uisession")
        pb = os.path.join(d, "b.uisession")
        s.write(pa)
        loaded = Session.read(pa)
        loaded.write(pb)
        with open(pa, "rb") as f:
            ba = f.read()
        with open(pb, "rb") as f:
            bb = f.read()
        check(ba == bb, "write->read->write round-trip byte-equal")
        check(ba == b1, "on-disk bytes match in-memory to_bytes()")
        check(ba.endswith(b"\n"), "file is newline-terminated")


################################################################################
# T2 — schema
################################################################################

def test_schema():
    print("[T2] schema", flush=True)
    s = build_reference_session()
    text = s.to_text()
    lines = text.rstrip("\n").split("\n")

    # header present with version
    header = json.loads(lines[0])
    check(header.get("version") == SESSION_VERSION, "header carries version==%d" % SESSION_VERSION)
    check(header.get("screen_dim") == [1920, 1080], "header screen_dim recorded")
    check(header.get("event_count") == len(lines) - 1, "header event_count matches record count")

    # sorted keys verified on the RAW text (textual key order == sorted)
    for i, ln in enumerate(lines):
        od = json.loads(ln, object_pairs_hook=collections.OrderedDict)
        keys = list(od.keys())
        check(keys == sorted(keys), "line %d keys sorted in raw text" % i)

    # one-record-per-frame ordering preserved on read-back
    frames_written = [json.loads(ln)["frame"] for ln in lines[1:]]
    reloaded = Session.from_text(text)
    frames_read = [r.frame for r in reloaded.records]
    check(frames_read == frames_written, "record (frame) order preserved through read")
    check(frames_written == sorted(frames_written), "frames monotonic in reference session")

    # unknown-code fallback -> code_name ""
    last = reloaded.records[-1]
    check(last.code == UNKNOWN_CRC, "unknown code preserved verbatim")
    check(last.code_name == "", "unknown code_name falls back to empty string")

    # known code_name resolves via the engine crc reverse map
    check(eventcodes.code_name_for(crc("KEY_DOWN")) == "KEY_DOWN", "crc->name resolves KEY_DOWN")
    kd = reloaded.records[0]
    check(kd.code_name == "KEY_DOWN", "first record resolves code_name KEY_DOWN")

    # version guard: a bad version must raise on read
    bad = text.replace('"version":1', '"version":99', 1)
    raised = False
    try:
        Session.from_text(bad)
    except ValueError:
        raised = True
    check(raised, "unsupported version rejected on read")


################################################################################
# T3 — machine-data scrub
################################################################################

def test_scrub():
    print("[T3] machine-data scrub", flush=True)
    s = build_reference_session()
    text = s.to_text()

    # (label, live-substring) pairs — the ASSERTION uses live machine identity,
    # but only the generic label is printed so no hostname/username/home path
    # ever reaches stdout (or any captured gate log).
    forbidden = [("macos-users-prefix", "/Users/")]
    home = os.path.expanduser("~")
    if home and home != "~":
        forbidden.append(("home-path", home))
    try:
        forbidden.append(("hostname", socket.gethostname()))
    except Exception:
        pass
    try:
        forbidden.append(("nodename", os.uname().nodename))
    except Exception:
        pass
    try:
        forbidden.append(("username", getpass.getuser()))
    except Exception:
        pass
    user_env = os.environ.get("USER")
    if user_env:
        forbidden.append(("user-env", user_env))

    for label, sub in forbidden:
        if not sub:
            continue
        check(sub not in text, "serialized text excludes %s" % label)


################################################################################
# T4 — recorder tap / frame-correlation / dedupe
################################################################################

def test_recorder():
    print("[T4] recorder tap + dedupe", flush=True)
    rec = Recorder()  # no app, no path — drive taps directly

    # frame 0: a main-window key event, seen by BOTH taps (global then preview)
    rec.on_update(FakeUpdata(0))
    e1 = ev_key("KEY_DOWN", ord("A"))
    rec._on_global_event(e1)
    hr = rec._on_preview_event(e1)  # same object via both taps -> dedupe
    check(isinstance(hr, lev2.ui.HandlerResult), "preview tap returns a HandlerResult")
    check(rec.session.event_count == 1, "same event via both taps recorded once (dedupe)")
    check(rec.session.records[0].frame == 0, "frame index stamped 0")

    # frame 5: a main-window pointer event, both taps again
    rec.on_update(FakeUpdata(5))
    e2 = ev_pointer("PUSH", 100, 200, left=1)
    rec._on_global_event(e2)
    rec._on_preview_event(e2)
    check(rec.session.event_count == 2, "second main-window event deduped to one record")
    check(rec.session.records[1].frame == 5, "advanced frame index stamped 5")

    # frame 7: a popup-only event, seen ONLY by the preview tap -> must record
    rec.on_update(FakeUpdata(7))
    e3 = ev_pointer("MOVE", 10, 20)
    rec._on_preview_event(e3)
    check(rec.session.event_count == 3, "popup-only preview event recorded")

    frames = [r.frame for r in rec.session.records]
    check(frames == [0, 5, 7], "frame indices land correctly across taps")

    # a distinct main-window event with identical fields but a NEW frame is NOT
    # falsely deduped (dedupe keys include the frame)
    rec.on_update(FakeUpdata(9))
    e4 = ev_key("KEY_DOWN", ord("A"))
    rec._on_global_event(e4)
    check(rec.session.event_count == 4, "identical fields on a new frame not falsely deduped")


################################################################################

def main():
    test_determinism()
    test_schema()
    test_scrub()
    test_recorder()
    print("=== uitest_session gate PASSED (%d checks) ===" % CHECKS, flush=True)
    sys.exit(0)


main()
