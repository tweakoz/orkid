"""HyperSyn DSL op registry — the EXTENSION POINT so external packages (e.g. ~/orkflow)
can add DSL ops/functions WITHOUT editing orkid, and to cut the per-op aliasing boilerplate.

A family namespace (P = ptex3d, and — as families opt in — T/H/...) consults this registry
for any op name it does not define natively, so registering an op makes it callable as
`P.myop(...)`. This module is PURE DATA: it imports nothing from the DSL internals, so it is
safe to import from anywhere and to mutate at import time (a single shared module-level table).

Usage from any package (in the orkid python env; obt.project/scripts is on sys.path):

    from ork.hypergraph.registry import op
    from ork.hypergraph.ptex3d import P

    @op("ptex3d", "marble")
    def marble(ctx, scale=4.0):
        return P.fbm(ctx.P_object * scale, octaves=5)

    # then, anywhere:  P.marble(ctx, scale=8.0)

or imperatively:

    from ork.hypergraph.registry import register_op
    register_op("ptex3d", "marble", marble_fn)

The registered callable takes exactly the arguments the DSL caller passes (the family does not
inject `self`); it returns whatever that family's ops return (e.g. a ptex3d SurfNode/Op).
"""

import inspect

# (family, name) -> OpInfo. A single shared module-level table: an `import` from ANY package
# mutates the same dict, which is what makes external registration work.
_OPS = {}


class OpInfo:
    __slots__ = ("family", "name", "fn", "doc", "source")

    def __init__(self, family, name, fn):
        self.family = family
        self.name = name
        self.fn = fn
        self.doc = (getattr(fn, "__doc__", None) or "").strip()
        try:
            self.source = (inspect.getsourcefile(fn), inspect.getsourcelines(fn)[1])
        except (OSError, TypeError):
            self.source = (None, 0)

    def __repr__(self):
        f, ln = self.source
        return f"OpInfo({self.family}.{self.name} @ {f}:{ln})"


def register_op(family, name, fn):
    """Register (or override) a DSL op. Returns fn (so it composes with decorators)."""
    if not callable(fn):
        raise TypeError(f"register_op({family!r},{name!r}): fn must be callable, got {type(fn)}")
    _OPS[(family, name)] = OpInfo(family, name, fn)
    return fn


def op(family, name=None):
    """Decorator. `@op("ptex3d", "marble")` or `@op("ptex3d")` (name defaults to fn.__name__)."""
    def deco(fn):
        register_op(family, name or fn.__name__, fn)
        return fn
    return deco


def get_op(family, name):
    """The registered callable for (family, name), or None."""
    info = _OPS.get((family, name))
    return info.fn if info else None


def has_op(family, name):
    return (family, name) in _OPS


def list_ops(family=None):
    """OpInfo list (sorted), optionally filtered to one family — for tooling/introspection."""
    return [i for (f, n), i in sorted(_OPS.items()) if family is None or f == family]


def list_families():
    return sorted({f for (f, _n) in _OPS})
