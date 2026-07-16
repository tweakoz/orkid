###############################################################################
# strata_bake — Phase-1 acceptance for the unified procedural substrate.
#
# Bakes ptex3d EXPRESSIONS (authored once with P + ctx, the SAME surface a Ptex3d
# material uses) to data channels via self.hfbake -> a generic compute ExprModule.
# Proves: SurfNode -> emit_compute_field -> compute shell -> bake -> channel, with
# the canonical lib_pnoise noise() running in compute (byte-identical to the
# fragment, so a baked field and the shaded field coincide).
#
# NOTE (scope): hfbake is a GENERATOR (no height input), so these fields are
# functions of WORLD XZ. True elevation-coupled strata that line up with TERRACES
# needs the height as an INPUT -> self.hfdisplacement (Phase 2). Phase 1 proves the
# substrate; Phase 2 closes the strata<->terrace loop.
###############################################################################
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.ptex3d import P

_TAU = 6.28318530718
AMPLITUDE_M = 1000.0   # authored vertical relief in meters (natural units; was HEIGHT_M)


def stripes(ctx, *, period_m=256.0):
    """Vertical stripes in world X — PURE ALU (sin/dot). The simplest bake-pipe proof
    (no noise, no libblock): exercises emit_compute_field + the compute shell alone."""
    p = ctx.P_object
    return P.sin(p.x * (_TAU / period_m)) * 0.5 + 0.5


def mottle(ctx, *, freq=0.01):
    """A value-noise field over world XZ — exercises lib_pnoise noise() in COMPUTE
    (the same basis the fragment shades with -> bake and shade coincide)."""
    p = ctx.P_object
    return P.noise(p * freq)


def compressed(ctx, *, freq=0.01):
    """A deliberately NARROW-range field (~[0.5,0.8]) to exercise NormalizeModule:
    T.normalize rescales its raw [min,max] to [0,1] (visible in FieldStats)."""
    return P.noise(ctx.P_object * freq) * 0.3 + 0.5


def terrace_strata(ctx, *, band_period_m=80.0):
    """Phase-2 (hfdisplacement): snap the CURRENT height to strata-band elevations, so
    geometric BENCHES coincide with the sin(phase) strata bands a material shades. Reads
    input-0 (the height) via ctx.P_object.y = in0 (TRUE METERS)."""
    y  = ctx.P_object.y                                  # physical elevation in meters (= input-0 height)
    ph = y / band_period_m
    snapped_m = (P.floor(ph) + P.smoothstep(0.35, 0.65, P.fract(ph))) * band_period_m
    return snapped_m                                     # already in meters (heights are meters)


class StrataBake(HeightField):
    # small physical scale for the test bake
    EXTENT_M = 4096.0

    def __init__(self):
        super().__init__()
        self.capture(T.Const(0.5 * AMPLITUDE_M), "height")   # flat placeholder height (meters)
        self.hfbake(stripes, "stripes")        # pure-ALU expression -> channel
        self.hfbake(mottle,  "mottle")         # noise (lib_pnoise) expression -> channel
        # NormalizeModule check: bake a narrow ~[0.5,0.8] field, AND its normalized
        # [0,1] version, so the test can contrast their FieldStats.
        self.hfbake(compressed, "compressed")                          # raw ~[0.5,0.8]
        comp = T.expr_field(self._eval_expr(compressed))               # same field as a node
        self.capture(T.normalize(comp, 0.0, 1.0), "normalized")        # rescaled to [0,1]
        # Phase-2: a varying base height, TERRACED via hfdisplacement (reads height via In0).
        base = (T.Fbm(frequency=3.0, octaves=5) * 0.5 + 0.5) * AMPLITUDE_M   # base height (meters)
        self.capture(self.hfdisplacement(terrace_strata, base), "terraced")
