################################################################################
# ork.uitest — event-code <-> name resolution
#
# EVENT_CODE_NAMES below MIRRORS ork::ui::EventCode in
#   ork.lev2/inc/ork/lev2/ui/enum.h
# and is the single place that enumerates the codes for this package. crc values
# are NOT hardcoded here: they are resolved at runtime from the engine's own
# CrcStringProxy (the same `tokens.<NAME>.hashed` idiom the ui tests use — see
# ork.lev2/pyext/tests/ui/globalevents.py), so a resolved code_name always
# matches the raw `ev.code` crc the pyext produced for that event. An unknown
# crc (one not present in the enum) resolves to "" and never raises.
################################################################################

# ork::ui::EventCode — keep in lockstep with enum.h (order mirrors the header for
# reviewability; order is not load-bearing since resolution is by crc).
EVENT_CODE_NAMES = (
    "UNKNOWN",
    "SHOW",
    "HIDE",
    "PUSH",
    "DOUBLECLICK",
    "RELEASE",
    "BEGIN_DRAG",
    "DRAG",
    "END_DRAG",
    "MOVE",
    "KEY_DOWN",
    "KEY_REPEAT",
    "KEY_UP",
    "RESIZED",
    "DRAW",
    "MOUSEWHEEL",
    "MULTITOUCH",
    "TABLET_BRUSH",
    "GOT_KEYFOCUS",
    "LOST_KEYFOCUS",
    "MOUSE_ENTER",
    "MOUSE_LEAVE",
    "ACTION",
    "PASTE_TEXT",
    "MIDI_CONTROLLER",
    "MIDI_KEY_DOWN",
    "MIDI_KEY_UP",
)

# Per-event-class field sets (spec pump table). Classification is by name so any
# code — raw or derived — routes to the right serialized field set. Raw record/
# replay only ever sees the non-derived codes, but the derived ones are mapped
# here too so a stray derived code still serializes coherently.
KEY_CODE_NAMES = frozenset({
    "KEY_DOWN",
    "KEY_REPEAT",
    "KEY_UP",
})
POINTER_CODE_NAMES = frozenset({
    "PUSH",
    "RELEASE",
    "MOVE",
    "DRAG",
    "DOUBLECLICK",
    "BEGIN_DRAG",
    "END_DRAG",
    "MOUSE_ENTER",
    "MOUSE_LEAVE",
})
WHEEL_CODE_NAMES = frozenset({
    "MOUSEWHEEL",
})

CLASS_KEY = "key"
CLASS_POINTER = "pointer"
CLASS_WHEEL = "wheel"
CLASS_OTHER = "other"

# lazily-built, cached-only-on-success reverse map (crc:int -> name:str)
_reverse_map = None


def _build_reverse_map():
    """Resolve every enum name to its engine crc via CrcStringProxy.

    Returns an empty dict (never raises) if the engine is not importable — the
    fail-safe path so serialization degrades to code_name "" rather than dying.
    """
    result = {}
    try:
        from orkengine.core import CrcStringProxy
    except Exception:
        return result
    tokens = CrcStringProxy()
    for name in EVENT_CODE_NAMES:
        try:
            result[int(getattr(tokens, name).hashed)] = name
        except Exception:
            continue
    return result


def reverse_map():
    """crc(int) -> EventCode name(str). Built once, cached only when non-empty."""
    global _reverse_map
    if _reverse_map:
        return _reverse_map
    built = _build_reverse_map()
    if built:
        _reverse_map = built
    return built


def code_name_for(code):
    """Best-effort crc -> name; "" for any crc not in the enum (never raises)."""
    try:
        return reverse_map().get(int(code), "")
    except Exception:
        return ""


def class_for_name(name):
    """Map an EventCode name to its serialized field-set class."""
    if name in KEY_CODE_NAMES:
        return CLASS_KEY
    if name in POINTER_CODE_NAMES:
        return CLASS_POINTER
    if name in WHEEL_CODE_NAMES:
        return CLASS_WHEEL
    return CLASS_OTHER
