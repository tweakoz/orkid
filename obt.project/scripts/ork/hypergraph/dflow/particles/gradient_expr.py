###############################################################################
# ork.hypergraph.dflow.particles.gradient_expr — the particle RENDERER color-ramp adoption of
# the shared ExprIR (JUL13 DFLOW E2.5, S9; context "particles.color"). The CONTEXT model
# (adjudication superseding Q7): particles expose a SECOND expression environment — a color
# ramp — with a vastly different vocabulary + evaluation site than the force context (S8).
#
#   vocabulary: unit_age is the ONLY symbol honestly available. v1 BAKES the ramp: the material
#   gradient's 256-entry LUT is a 1-D function of unit_age, so per-particle symbols (velocity /
#   random) are NOT honestly available at bake time and are NOT in the vocabulary (fail-loud,
#   never a silent zero / black ramp). v1 evaluates at BAKE TIME, not per-particle per-frame.
#
# The consumer (bake_gradient) evaluates r/g/b/a channel expressions of unit_age across [0,1]
# and fills the reflected gradient's color stops (the cleanest existing seam — the gradient
# already bakes to a 256x1 GradientMap each update). REUSES the shared builder + evaluator from
# the sibling exprir_particles.py (one node set, one JSON, one evaluator across both contexts).
###############################################################################

from ... import exprir as _x
from .exprir_particles import (
    _math_functions, _SymbolNS, _wrap, eval_expr, _check_symbols,
    PTC_LEAF, ParticleExprError)  # noqa: F401  (shared vocabulary + builder + evaluator)


# the ONLY symbol honestly available at gradient-LUT BAKE time (the ramp is 1-D over unit_age).
_COLOR_SYMBOLS = ("unit_age",)

COLOR_CTX = _x.register_context(_x.ExprContext(
    "particles.color", functions=_math_functions(),
    leaves=[_x.LeafSpec(PTC_LEAF)],
    doc="renderer color-ramp scalar expression over unit_age; baked to the 256-entry LUT"))

# PC — the COLOR author namespace (PC.unit_age, PC.smoothstep(...), ...).
PC = _SymbolNS(_COLOR_SYMBOLS)


def _color_json(x):
    node = _wrap(x).node
    COLOR_CTX.validate(node)
    _check_symbols(node, _COLOR_SYMBOLS, "particles.color")
    return node


def lut_signature(r, g, b, a=0.0, n=256):
    """The deterministic LUT the bake would produce (WITHOUT touching a gradient) — the byte-key
    for the S9 determinism gate: same exprs -> identical tuple; a different expr -> a different
    tuple (the expression provably changes the ramp as authored)."""
    rj, gj, bj, aj = (_color_json(r), _color_json(g), _color_json(b), _color_json(a))
    out = []
    for i in range(n):
        t = i / float(n - 1) if n > 1 else 0.0
        s = {"unit_age": t}
        out.append((round(eval_expr(rj, s), 6), round(eval_expr(gj, s), 6),
                    round(eval_expr(bj, s), 6), round(eval_expr(aj, s), 6)))
    return tuple(out)


def bake_gradient(gradient, r, g, b, a=0.0, n=256):
    """Fill `gradient` (a reflected GradientMaterial.gradient) from color-channel expressions of
    unit_age (each of r/g/b/a is a PC.* builder or a number). Evaluates at BAKE time -> the
    256-entry ramp LUT the renderer samples. Returns the deterministic LUT tuple (== lut_signature)."""
    from orkengine.core import vec4
    rj, gj, bj, aj = (_color_json(r), _color_json(g), _color_json(b), _color_json(a))
    stops = {}
    lut = []
    for i in range(n):
        t = i / float(n - 1) if n > 1 else 0.0
        s = {"unit_age": t}
        cr, cg, cb, ca = (eval_expr(rj, s), eval_expr(gj, s),
                          eval_expr(bj, s), eval_expr(aj, s))
        stops[t] = vec4(cr, cg, cb, ca)
        lut.append((round(cr, 6), round(cg, 6), round(cb, 6), round(ca, 6)))
    gradient.setColorStops(stops)
    return tuple(lut)
