###############################################################################
# ork.hypergraph.dflow._parampack — ParameterPacks: blendable, typed parameter
# bundles for the HyperSyn DSL, usable across domains (terrain / particles / ...).
#
# A ParamPack is PURE TRACE-TIME Python sugar — NOT a dflow graph node. It holds
# {name -> typed value} (int / float / vec2 / vec3 / vec4 / quat) and resolves, when a
# module consumes it, to exactly the m.inputs.NAME = value assignments the explicit
# kwargs already make. No new C++/dflow machinery. lerp() of two INT values rounds to
# the nearest int (eager `*`/`+` promote int->float, Python-style); the value is
# converted back to float when assigned to a (float) input plug.
#
#   x = T.ParamPack(sim_time_s=0.5, rain_mps=0.06, ...)
#   y = T.ParamPack(sim_time_s=2.5, rain_mps=0.09, ...)
#   p = x*0.5 + y*0.5            # PLAIN component math (untouched) — correct lerp for float/vec,
#                               #   component-wise for quat (use T.lerp for a real slerp)
#   q = T.lerp(x, y, 0.5)       # STRICT blend: lerp for float/vec, SLERP for quat
#   out = T.erox(node, p)       # a module consumes pack(s) positionally
#   out = T.erox(node, p, q)    # multiple packs (NO duplicate keys across them)
#   out = T.erox(node, p, capacity_Kc=3.0)  # explicit kwargs OVERRIDE pack values
#
# `*`/`+`/`-`/`*scalar` are EAGER plain arithmetic (per the design call): they do the
# obvious component math. For proper interpolation (esp. quaternion SLERP) use lerp()/mix().
###############################################################################

import re
import math
from orkengine import core

_VECS = (core.vec2, core.vec3, core.vec4)


def _rint(x):
    """round to NEAREST integer, ties away from zero (the intuitive 'round')."""
    return int(math.floor(x + 0.5)) if x >= 0.0 else int(math.ceil(x - 0.5))


def _is_vec(v):
    return isinstance(v, _VECS)


def _is_quat(v):
    return isinstance(v, core.quat)


def _q(x, y, z, w):
    q = core.quat()
    q.x = x; q.y = y; q.z = z; q.w = w
    return q


def _canon(v):
    """Coerce a python/core value into a canonical ParamPack value (int/float/vecN/quat)."""
    if isinstance(v, bool):
        raise TypeError("ParamPack value cannot be bool")
    if isinstance(v, int):
        return v          # int stays int: lerp() rounds an int->int blend to nearest.
    if isinstance(v, float):
        return float(v)
    if _is_vec(v) or _is_quat(v):
        return v
    if isinstance(v, (tuple, list)):
        n = len(v)
        if n == 2: return core.vec2(float(v[0]), float(v[1]))
        if n == 3: return core.vec3(float(v[0]), float(v[1]), float(v[2]))
        if n == 4: return core.vec4(float(v[0]), float(v[1]), float(v[2]), float(v[3]))
        raise TypeError(f"ParamPack tuple value must have len 2/3/4; got {n}")
    # _LazyColor / hsv() etc. (best-effort)
    for attr in ("to_vec4", "to_vec3"):
        f = getattr(v, attr, None)
        if callable(f):
            return f()
    raise TypeError(f"ParamPack value must be float / vec2/3/4 / quat / 2-4 tuple; got {type(v).__name__}")


def _scale(v, s):
    """v * scalar (plain, per-type). vec stays on the LEFT (no reflected scalar*vec binding).
    NOTE: scaling an int by a (float) scalar PROMOTES to float — eager math is plain; use
    lerp() if you want an int blend to stay a (rounded) int."""
    if isinstance(v, (int, float)):
        return v * s
    if _is_quat(v):
        return _q(v.x * s, v.y * s, v.z * s, v.w * s)
    if _is_vec(v):
        return v * s
    raise TypeError(f"cannot scale {type(v).__name__}")


def _add(a, b):
    """a + b (plain, per-type, same type required). int+int stays int; int+float -> float."""
    if isinstance(a, (int, float)) and isinstance(b, (int, float)):
        return a + b
    if _is_quat(a) and _is_quat(b):
        return _q(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w)
    if _is_vec(a) and _is_vec(b) and type(a) is type(b):
        return a + b
    raise TypeError(f"cannot add {type(a).__name__} + {type(b).__name__}")


def _blend(a, b, t):
    """STRICT interpolation a->b by t (per-type): int LERP rounded to nearest, float/vec
    LERP, quat SLERP. A mixed int/float blend promotes to float (only int->int rounds)."""
    if isinstance(a, int) and isinstance(b, int):
        return _rint(a + (b - a) * t)
    if isinstance(a, (int, float)) and isinstance(b, (int, float)):
        return a + (b - a) * t
    if _is_quat(a) and _is_quat(b):
        return core.quat.slerp(a, b, t)
    if _is_vec(a) and _is_vec(b) and type(a) is type(b):
        return a * (1.0 - t) + b * t
    raise TypeError(f"cannot lerp {type(a).__name__} and {type(b).__name__}")


class ParamPack:
    """A typed, blendable bundle of named parameter values. Trace-time only."""
    __slots__ = ("_v",)

    def __init__(self, **kwargs):
        self._v = {k: _canon(val) for k, val in kwargs.items()}

    @classmethod
    def _wrap(cls, d):
        p = cls.__new__(cls)
        p._v = d
        return p

    # --- plain eager arithmetic (component math, untouched) ------------------
    def __mul__(self, s):
        s = float(s)
        return ParamPack._wrap({k: _scale(v, s) for k, v in self._v.items()})
    __rmul__ = __mul__

    def __add__(self, other):
        if not isinstance(other, ParamPack):
            return NotImplemented
        out = dict(self._v)
        for k, v in other._v.items():
            out[k] = _add(out[k], v) if k in out else v
        return ParamPack._wrap(out)

    def __sub__(self, other):
        if not isinstance(other, ParamPack):
            return NotImplemented
        return self.__add__(other * -1.0)

    # --- access -------------------------------------------------------------
    def keys(self):
        return self._v.keys()

    def as_dict(self):
        return dict(self._v)

    def __contains__(self, k):
        return k in self._v

    def __repr__(self):
        return "ParamPack(%s)" % ", ".join(f"{k}={v}" for k, v in self._v.items())


def lerp(a, b, t):
    """STRICT interpolation (lerp for float/vec, SLERP for quat). Works on ParamPacks
    (per-key) or on bare values. `mix` is an alias."""
    t = float(t)
    if isinstance(a, ParamPack) and isinstance(b, ParamPack):
        out = dict(a._v)
        for k, v in b._v.items():
            out[k] = _blend(out[k], v, t) if k in out else v
        return ParamPack._wrap(out)
    if isinstance(a, ParamPack) or isinstance(b, ParamPack):
        raise TypeError("lerp: both args must be ParamPacks, or neither")
    return _blend(_canon(a), _canon(b), t)


mix = lerp


# --- module consumption -------------------------------------------------------

_SCHEMA_RE = re.compile(r"input\s+(\w+)\s*:[^\n<]*inplugdata\s*<\s*(\w+)\s*>", re.IGNORECASE)

_TYPECHECK = {
    "float": lambda v: isinstance(v, (int, float)) and not isinstance(v, bool),
    "int":   lambda v: isinstance(v, (int, float)) and not isinstance(v, bool),
    "vec2":  lambda v: isinstance(v, core.vec2),
    "vec3":  lambda v: isinstance(v, core.vec3),
    "vec4":  lambda v: isinstance(v, core.vec4),
    "quat":  lambda v: isinstance(v, core.quat),
}


def _input_schema(inputs_proxy):
    """{name -> value-type} parsed from the inputs proxy repr (avoids per-name getattr,
    which can hard-abort on a bad name). Empty dict if the repr format is unrecognized."""
    schema = {}
    for name, tname in _SCHEMA_RE.findall(repr(inputs_proxy)):
        schema[name] = tname.replace("xf", "").lower()  # floatxf->float, vec3xf->vec3, ...
    return schema


def _baked_scalar_kind(module, k):
    """If `k` is a numeric (non-bool) attribute ON THE MODULE itself — a BAKED scalar such
    as `iterations` / `octaves`, not an input plug — return int or float (its kind); else
    None. getattr on the module (not module.inputs) is safe — a missing name just raises."""
    try:
        cur = getattr(module, k)
    except Exception:
        return None
    if isinstance(cur, bool):
        return None
    if isinstance(cur, int):
        return int
    if isinstance(cur, float):
        return float
    return None


def _apply_packs(module, packs, overrides=None):
    """Merge ParamPack(s) (no duplicate keys across packs -> ValueError) + explicit overrides
    (which WIN), validate every key against the module's input-plug name+type schema (in
    Python, BEFORE assigning — a bad assignment aborts the process), then set m.inputs.NAME."""
    overrides = overrides or {}
    merged = {}
    owner = {}
    for i, p in enumerate(packs):
        if not isinstance(p, ParamPack):
            raise TypeError(f"erox/op expects ParamPack positional args; got {type(p).__name__}")
        for k, v in p._v.items():
            if k in owner:
                raise ValueError(f"param {k!r} supplied by multiple packs (no duplicates allowed)")
            owner[k] = i
            merged[k] = v
    for k, v in overrides.items():
        merged[k] = _canon(v)   # explicit kwargs override pack values

    if not merged:
        return
    inputs = module.inputs
    schema = _input_schema(inputs)
    for k, v in merged.items():
        if k in schema:
            # an INPUT PLUG (float-typed). int pack values (e.g. a rounded lerp) are
            # converted to float at the plug boundary.
            chk = _TYPECHECK.get(schema[k])
            if chk and not chk(v):
                raise TypeError(f"param {k!r}: module expects {schema[k]}, pack has {type(v).__name__}")
            setattr(inputs, k, float(v) if isinstance(v, int) else v)
            continue
        # not a plug: maybe a BAKED SCALAR on the module itself (iterations / octaves / ...),
        # a numeric module property. Pack it through too — an int baked scalar takes a
        # rounded int (so a lerped `iterations` snaps to a whole step count).
        kind = _baked_scalar_kind(module, k)
        if kind is int:
            setattr(module, k, _rint(v))
            continue
        if kind is float:
            setattr(module, k, float(v))
            continue
        if schema:  # schema parsed but k is neither a plug nor a baked scalar -> typo
            raise KeyError(f"module {type(module).__name__} has no input plug or scalar {k!r}; "
                           f"plugs: {sorted(schema)}")
        # no schema parsed at all -> legacy best-effort plug assignment
        setattr(inputs, k, float(v) if isinstance(v, int) else v)
