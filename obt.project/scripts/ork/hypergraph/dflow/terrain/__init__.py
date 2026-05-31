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

# DSL op aliases (CamelCase). Affine + blend additionally compose via operators
# on the returned TerrainNode: node*k+b -> Remap, a*b / a+b -> Combine.
Fbm      = ops.fbm
Gradient = ops.gradient
Const    = ops.const
Remap    = ops.remap
Terrace  = ops.terrace
Clamp    = ops.clamp
Mix      = ops.mix
Min      = ops.minimum
Max      = ops.maximum
Slope     = ops.slope      # Mask by Feature: slope -> [0,1] mask field
slope     = ops.slope      # lower-case too (reads naturally: steep = T.slope(h))
Curvature = ops.curvature  # Mask by Feature: curvature (convex/concave/magnitude)
curvature = ops.curvature

__all__ = [
    "HeightField",
    "Fbm", "Gradient", "Const", "Remap", "Terrace", "Clamp", "Mix", "Min", "Max",
    "Slope", "slope", "Curvature", "curvature",
]
