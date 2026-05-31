###############################################################################
# ork.hypergraph.dflow.terrain.ops — named terrain ops.
#
# Generators (Fbm/Gradient/Const) and ops with no natural operator
# (Terrace/Mix/Min/Max/Clamp/Remap) are named functions; affine + blend compose
# via TerrainNode operators (see _node.py). Each call creates exactly one module
# in the active trace graph and returns a TerrainNode so it composes downstream.
###############################################################################

from orkengine.core import vec2 as _vec2
from orkengine.lev2 import terrain as _terrain
from ._node import (
    TerrainNode,
    anon_name,
    graph_or_raise,
    make_const,
    make_remap,
    make_combine,
    make_maskblend,
    OP_MIX,
    OP_MIN,
    OP_MAX,
)


# --- generators (no input) ---------------------------------------------------

def fbm(frequency=4.0, amplitude=1.0, octaves=5, name=None):
    """Fractal value-noise field. `octaves` is a BAKED loop bound (ctor arg);
    frequency/amplitude are float plugs."""
    g = graph_or_raise("Fbm")
    m = g.create(name or anon_name("fbm", g), _terrain.FbmModule)
    m.octaves = int(octaves)
    m.inputs.frequency = float(frequency)
    m.inputs.amplitude = float(amplitude)
    return TerrainNode(m, m.outputs.Out)


def gradient(dir_x=1.0, dir_y=0.0, scale=1.0, bias=0.0, name=None):
    """Linear ramp: dot(uv, dir) * scale + bias, with dir = (dir_x, dir_y)
    carried as a single vec2 plug."""
    g = graph_or_raise("Gradient")
    m = g.create(name or anon_name("grad", g), _terrain.GradientModule)
    m.inputs.dir = _vec2(float(dir_x), float(dir_y))
    m.inputs.scale = float(scale)
    m.inputs.bias = float(bias)
    return TerrainNode(m, m.outputs.Out)


def const(level=0.5, name=None):
    """Constant field."""
    return make_const(level, name=name)


# --- unary -------------------------------------------------------------------

def remap(node, scale=1.0, bias=0.0, lo=0.0, hi=1.0, name=None):
    """Explicit affine remap WITH clamp: clamp(node*scale + bias, lo, hi). (The
    `*`/`+` operators use the no-clamp affine form; this is the clamping one.)"""
    return make_remap(node, scale=scale, bias=bias, lo=lo, hi=hi, name=name)


def terrace(node, steps=4.0, sharpness=1.0, name=None):
    """Quantize to `steps` plateaus with a `sharpness` riser."""
    g = graph_or_raise("Terrace")
    m = g.create(name or anon_name("terr", g), _terrain.TerraceModule)
    g.connect(m.inputs.In, node.output_plug)
    m.inputs.steps = float(steps)
    m.inputs.sharpness = float(sharpness)
    return TerrainNode(m, m.outputs.Out)


def clamp(node, lo=0.0, hi=1.0, name=None):
    """Clamp to [lo, hi] (Remap with unit scale)."""
    return make_remap(node, scale=1.0, bias=0.0, lo=lo, hi=hi, name=name)


# --- mask generators ("Mask by Feature") -------------------------------------
# A mask is just a [0,1] FIELD on the normal terrain plug type — no special type.
# Generators emit one; apply it compositionally with mix()/masked_by (see _node).

def slope(node, scale=1.0, radius_m=8.0, name=None):
    """Slope mask: the REAL slope (tan of the terrain angle = rise_m/run_m) of
    `node`, soft-rolled-off (Reinhard) into [0,1). High on steep faces, ~0 on flats.
    PRE-BLURRED over a `radius_m`-METER gradient baseline, so it tracks landform
    slope, not per-pixel noise — and because the param is in meters (converted to
    texels per-bake from the graph's extent), the graph is resolution-INDEPENDENT.
    `scale` tunes sensitivity (a 45deg slope reads ~0.5 at scale=1). (Mask by Feature.)"""
    g = graph_or_raise("Slope")
    if not isinstance(node, TerrainNode):
        raise TypeError(f"slope expects a terrain node; got {type(node).__name__}")
    m = g.create(name or anon_name("slope", g), _terrain.SlopeModule)
    m.radius_m = float(radius_m)
    g.connect(m.inputs.In, node.output_plug)
    m.inputs.scale = float(scale)
    return TerrainNode(m, m.outputs.Out)


# CurvatureModule._mode codes — MUST match enum CurvatureMode in hfdflow.h.
_CURV_MODE = {"convex": 0, "concave": 1, "magnitude": 2}


def curvature(node, scale=1.0, mode="magnitude", radius_m=96.0, name=None):
    """Curvature mask: a [0,1] field from the band-pass (difference-of-box ~ LoG)
    curvature of `node`, soft-rolled-off (Reinhard, no hard clamp -> magnitude
    survives). PRE-BLURRED over a `radius_m`-METER scale, so it tracks landform
    ridges/valleys, not per-pixel noise — and because the param is in meters
    (converted to texels per-bake from the graph's extent), the graph is
    resolution-INDEPENDENT. `mode`:
      "convex"    -> ridges / peaks   (e.g. snow, exposed rock)
      "concave"   -> valleys / pits   (e.g. sediment, water pooling)
      "magnitude" -> both ("where the terrain bends")
    `scale` tunes sensitivity; `radius_m` picks the curvature scale. An edge ring of
    width radius_m is 0 (curvature is undefined at the border). (Mask by Feature.)"""
    g = graph_or_raise("Curvature")
    if not isinstance(node, TerrainNode):
        raise TypeError(f"curvature expects a terrain node; got {type(node).__name__}")
    if mode not in _CURV_MODE:
        raise ValueError(f"curvature mode must be one of {sorted(_CURV_MODE)}; got {mode!r}")
    m = g.create(name or anon_name("curv", g), _terrain.CurvatureModule)
    m.mode = _CURV_MODE[mode]
    m.radius_m = float(radius_m)
    g.connect(m.inputs.In, node.output_plug)
    m.inputs.scale = float(scale)
    return TerrainNode(m, m.outputs.Out)


# --- erosion -----------------------------------------------------------------

def erode_thermal(node, iterations=40, talus_deg=33.0, rate=0.15, name=None):
    """THERMAL (talus) erosion — the slope-relaxation kind (cf. erode_hydro etc.,
    not yet built). Relax slopes toward the angle of repose over
    `iterations` steps -> scree/talus, softened ridges, filled hollows. Mass-
    conserving (material moves downhill, total height preserved). `talus_deg` is the
    PHYSICAL angle of repose (resolution-independent via the meters model); `rate` is
    the per-step relaxation (keep <~0.25 for stability); `iterations` is the baked
    step count. The single op ping-pongs two buffers internally across the steps."""
    g = graph_or_raise("ThermalErode")
    if not isinstance(node, TerrainNode):
        raise TypeError(f"erode expects a terrain node; got {type(node).__name__}")
    m = g.create(name or anon_name("erode", g), _terrain.ThermalErodeModule)
    m.iterations = max(1, int(iterations))
    g.connect(m.inputs.In, node.output_plug)
    m.inputs.talus_deg = float(talus_deg)
    m.inputs.rate = float(rate)
    return TerrainNode(m, m.outputs.Out)


def erode_hydro(node, iterations=50, rain=0.012, evaporation=0.015,
                capacity=0.30, erosion=0.30, deposition=0.30, name=None):
    """HYDRAULIC erosion (Mei et al. virtual-pipes grid model) — the kind that CARVES
    channels (unlike erode_thermal, which only relaxes slopes). Rain adds water, water
    flows downhill through virtual pipes, moving water picks up sediment to a velocity/
    slope capacity and deposits it where flow slows -> drainage networks, gullies,
    alluvial fans. `iterations` is the baked sub-pass-cycle count; `rain`/`evaporation`
    set the water budget; `capacity`/`erosion`/`deposition` are the sediment rates
    (tune for the look). Heavier than thermal (multi-field sim)."""
    g = graph_or_raise("HydroErode")
    if not isinstance(node, TerrainNode):
        raise TypeError(f"erode_hydro expects a terrain node; got {type(node).__name__}")
    m = g.create(name or anon_name("hydro", g), _terrain.HydroErodeModule)
    m.iterations = max(1, int(iterations))
    g.connect(m.inputs.In, node.output_plug)
    m.inputs.rain = float(rain)
    m.inputs.evaporation = float(evaporation)
    m.inputs.capacity = float(capacity)
    m.inputs.erosion = float(erosion)
    m.inputs.deposition = float(deposition)
    return TerrainNode(m, m.outputs.Out)


# --- binary (join) -----------------------------------------------------------

def mix(a, b, t=0.5, name=None):
    """Linear blend mix(a, b, t). `t` may be a SCALAR (uniform blend, Combine MIX)
    or a FIELD/TerrainNode (per-texel masked blend, MaskBlend). Scalar a/b operands
    auto-wrap as Const."""
    if isinstance(t, TerrainNode):
        return make_maskblend(a, b, t, name=name)
    return make_combine(a, b, OP_MIX, t=t, name=name)


def minimum(a, b, name=None):
    """Per-texel min(a, b). (Aliased T.Min.)"""
    return make_combine(a, b, OP_MIN, name=name)


def maximum(a, b, name=None):
    """Per-texel max(a, b). (Aliased T.Max.)"""
    return make_combine(a, b, OP_MAX, name=name)
