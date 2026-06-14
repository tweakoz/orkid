###############################################################################
# ork.hypergraph.dflow.terrain — terrain heightfield family DSL vocabulary.
#
# Expression-first: terrain is a DAG authored as algebra over TerrainNode handles
# (affine + blend via operators), with named ops for generators and the ops that
# have no natural operator. Each op call / operator creates one dflow module.
#
#   from ork.hypergraph.dflow.terrain import HeightField
#   from ork.hypergraph.dflow import terrain as T
#
#   class RollingHills(HeightField):
#     def __init__(self, octaves=5, steps=6):
#       super().__init__()
#       h = T.Fbm(frequency=3.0, octaves=octaves) * 0.5 + 0.5   # operators -> Remap
#       self.capture(T.Terrace(h, steps=steps, sharpness=4.0), "height")
#
# Naming: CamelCase aliases because each call constructs a module (or sub-DAG),
# reading like class instantiation — matching the particles family convention.
###############################################################################

from .base import HeightField
from . import ops
# ParameterPacks (cross-domain, from dflow core). NOTE: T.mix is the FIELD blend
# (TerrainNodes); T.lerp here is the VALUE/PACK blend (float/vec lerp, quat slerp).
from .._parampack import ParamPack, lerp as _pp_lerp

ParamPack = ParamPack
lerp      = _pp_lerp   # value/pack interpolation (strict: slerp for quat)

# DSL op aliases (CamelCase). Affine + blend additionally compose via operators
# on the returned TerrainNode: node*k+b -> Remap, a*b / a+b -> Combine.
Fbm      = ops.fbm
fbm      = ops.fbm
Perlin   = ops.perlin    # noise-basis primitives (generators): perlin/simplex/worley-F1/voronoi.
perlin   = ops.perlin    #   frequency = lattice cells across map; octaves=1 = pure primitive.
Simplex  = ops.simplex
simplex  = ops.simplex
WorleyF1 = ops.worleyf1
worleyf1 = ops.worleyf1
Voronoi  = ops.voronoi
voronoi  = ops.voronoi
Gradient = ops.gradient
gradient = ops.gradient
Const    = ops.const
const    = ops.const
Remap    = ops.remap
remap    = ops.remap
Terrace  = ops.terrace
terrace  = ops.terrace   # lower-case too (it's a process/filter op, like erox/lpf/basin_fill)
Clamp    = ops.clamp
clamp    = ops.clamp
Pow      = ops.pow        # per-texel field ** exponent (sharpen/soften a mask; gamma)
pow      = ops.pow
Mix      = ops.mix
mix      = ops.mix        # FIELD blend (TerrainNodes); cf. lerp = VALUE/pack blend
Min      = ops.minimum    # CamelCase-ONLY: lowercase min() would shadow the Python builtin
Max      = ops.maximum    # CamelCase-ONLY: lowercase max() would shadow the Python builtin
Slope     = ops.slope      # Mask by Feature: slope -> [0,1] mask field
slope     = ops.slope      # lower-case too (reads naturally: steep = T.slope(h))
Curvature = ops.curvature  # Mask by Feature: curvature (convex/concave/magnitude)
curvature = ops.curvature
SmoothStep = ops.smoothstep  # Mask by Value: soft rising edge (GLSL smoothstep) -> [0,1]
smoothstep = ops.smoothstep
Band      = ops.band       # Mask by Value: soft elevation band (~ shader aa_band / sels[k])
band      = ops.band
ErodeThermal = ops.erode_thermal  # thermal (talus) erosion — relaxes slopes
erode_thermal = ops.erode_thermal
Erox = ops.erox                   # PHYSICAL Mei grid erosion (meters/seconds, resolution-independent)
erox = ops.erox
Pha = ops.pha                     # PROCEDURAL phacelle erosion filter (single-pass, RI, speckle-free)
pha = ops.pha
Lpf = ops.lpf                     # separable gaussian low-pass (cutoff in texels)
lpf = ops.lpf
BasinFill = ops.basin_fill        # depression/pit fill (priority-flood, CPU) -> flat lakes, drainable
basin_fill = ops.basin_fill
Flow3D = ops.flow3d               # continuous flow field: RGBA dir/slope (.dir) + mono discharge (.discharge)
flow3d = ops.flow3d
FlowErode = ops.flow_erode        # continuous flow-map erosion+deposition step (z, discharge) — no SFD tree
flow_erode = ops.flow_erode
FillClosedBasins = ops.fill_closed_basins  # detect+fill CLOSED basins w/ persistence (min_depth) control
fill_closed_basins = ops.fill_closed_basins
Normalize = ops.normalize         # explicit [min,max]->[out_lo,out_hi] rescale (vs the flush auto-exposure)
normalize = ops.normalize
ExprField = ops.expr_field        # bake a ptex3d SurfNode to a field (unified substrate; backs hfbake)
expr_field = ops.expr_field

__all__ = [
    "HeightField",
    "Fbm", "fbm", "Perlin", "perlin", "Simplex", "simplex",
    "WorleyF1", "worleyf1", "Voronoi", "voronoi",
    "Gradient", "gradient", "Const", "const", "Remap", "remap",
    "Terrace", "terrace", "Clamp", "clamp", "Pow", "pow", "Mix", "mix", "Min", "Max",
    "Slope", "slope", "Curvature", "curvature",
    "SmoothStep", "smoothstep", "Band", "band",
    "ErodeThermal", "erode_thermal",
    "Erox", "erox",
    "Pha", "pha",
    "Lpf", "lpf",
    "BasinFill", "basin_fill",
    "Flow3D", "flow3d",
    "FlowErode", "flow_erode",
    "FillClosedBasins", "fill_closed_basins",
    "Normalize", "normalize",
    "ExprField", "expr_field",
    "ParamPack", "lerp",
]
