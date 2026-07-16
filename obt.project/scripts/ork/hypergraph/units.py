###############################################################################
# ork.hypergraph.units — the shared UNIT-CONSTRUCTOR vocabulary (JUL13 DFLOW E0 part 2,
# typed literals). ONE notation across the .py DSL, doc-JSON, and the editor: kind AND
# unit ride the VALUE as a constructor tag rather than as a param-name suffix or an
# annotation field. `meters(2000)` is a float that folds numerically like 2000.0
# everywhere a float does (no dimensional algebra in v1 — the tag is metadata), but it
# carries its unit so a tagged ctor-kwarg DEFAULT promotes into the document params table
# as a typed literal and round-trips as `{"meters": 2000}` in doc-JSON.
#
# Serves ALL dataflow families (terrain today; hypermesh / particles / ptex as they adopt
# document params). The vocabulary is FIXED at v1 (adjudication 10b) — extend by review,
# not accretion. `int`/`bool` need no constructor (Python natives carry their kind).
#
# OUT of this slice: compound hsv-style literals (a color VALUE constructor already lives
# in ork.hypergraph.colors.hsv — a separate concern from scalar unit tags; unifying them
# is deferred) and dimensional algebra (meters*unitless=meters etc.).
###############################################################################


# The v1 unit vocabulary (adjudication 10b). meters/texels/uv for space, cycles for
# spatial frequency, seconds/hz for time, db for level, degrees for angle, and the
# compound rates mps (m/s) / m2ps (m^2/s) / per_s (1/s).
_UNIT_TAGS = (
    "meters", "texels", "uv", "cycles", "seconds", "hz", "db",
    "degrees", "mps", "m2ps", "per_s",
)
UNIT_TAGS = frozenset(_UNIT_TAGS)

# Param-name suffixes the writer recognizes as REDUNDANT once the value carries the unit
# (the naming rule: unit suffixes come off the name once the type carries the unit). Used
# only to emit an advisory comment — no automatic renames this slice.
SUFFIX_UNITS = {
    "_m":    "meters",
    "_s":    "seconds",
    "_hz":   "hz",
    "_db":   "db",
    "_deg":  "degrees",
    "_uv":   "uv",
    "_mps":  "mps",
    "_m2ps": "m2ps",
}


###############################################################################

class _Tagged(float):
    """A float carrying a UNIT tag. Folds numerically like a float everywhere (any op that
    reads it as a number gets its value); the tag rides along so a tagged ctor-kwarg default
    can be recognized and promoted to a document typed literal. Immutable (float subclass)."""

    __slots__ = ("_unit",)

    def __new__(cls, value, unit):
        self = float.__new__(cls, value)
        self._unit = unit
        return self

    def __repr__(self):
        return "%s(%s)" % (self._unit, _fmt_number(float(self)))


###############################################################################

def _fmt_number(x):
    """Integral floats render bare (2000.0 -> '2000') so `meters(2000)` reads cleanly;
    fractional values keep full round-trip precision."""
    f = float(x)
    return str(int(f)) if f.is_integer() else repr(f)


def unit_of(value):
    """The unit tag of a tagged value, or None for a plain literal / native scalar."""
    return value._unit if isinstance(value, _Tagged) else None


def is_tagged(value):
    return isinstance(value, _Tagged)


def tagged(unit, value):
    """Reconstruct a tagged value from a (unit, number) pair — doc-JSON deserialization and
    the pywriter source form. LOUD on a unit outside vocabulary v1 (fail-loud, no shim)."""
    if unit not in UNIT_TAGS:
        raise ValueError(
            "unknown unit tag %r; vocabulary v1 = %s" % (unit, sorted(UNIT_TAGS)))
    return _Tagged(value, unit)


def coerce_to_tag(unit, value):
    """Coerce an editor-supplied value to a unit tag, LOUD on an uncoercible value or an
    unknown unit. All v1 units are float-like, so this returns a PLAIN float: the tag is
    metadata held on the params table, NOT on the plug value, so the elaborated graph stays
    tag-free and unchanged values bake byte-identically."""
    if unit not in UNIT_TAGS:
        raise ValueError(
            "unknown unit tag %r; vocabulary v1 = %s" % (unit, sorted(UNIT_TAGS)))
    return float(value)


def unit_source(unit, value):
    """The `.py` source form of a typed literal — e.g. `meters(2000)` — for the pywriter."""
    return "%s(%s)" % (unit, _fmt_number(value))


###############################################################################
# the constructors — one per vocabulary tag. `int`/`bool` are deliberately absent
# (Python natives carry their kind); compound hsv-style literals are out of this slice.
###############################################################################

def meters(v):  return _Tagged(v, "meters")
def texels(v):  return _Tagged(v, "texels")
def uv(v):      return _Tagged(v, "uv")
def cycles(v):  return _Tagged(v, "cycles")
def seconds(v): return _Tagged(v, "seconds")
def hz(v):      return _Tagged(v, "hz")
def db(v):      return _Tagged(v, "db")
def degrees(v): return _Tagged(v, "degrees")
def mps(v):     return _Tagged(v, "mps")
def m2ps(v):    return _Tagged(v, "m2ps")
def per_s(v):   return _Tagged(v, "per_s")
