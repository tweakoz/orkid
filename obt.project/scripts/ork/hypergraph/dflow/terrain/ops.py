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


# --- binary (join) -----------------------------------------------------------

def mix(a, b, t=0.5, name=None):
    """Linear blend: mix(a, b, t). Scalar operands auto-wrap as Const."""
    return make_combine(a, b, OP_MIX, t=t, name=name)


def minimum(a, b, name=None):
    """Per-texel min(a, b). (Aliased T.Min.)"""
    return make_combine(a, b, OP_MIN, name=name)


def maximum(a, b, name=None):
    """Per-texel max(a, b). (Aliased T.Max.)"""
    return make_combine(a, b, OP_MAX, name=name)
