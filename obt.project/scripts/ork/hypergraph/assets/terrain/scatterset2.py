###############################################################################
# scatterset2 — ScatterDemo, but the scattered props are HYPERMESHES (GPU mesh-dataflow), not loaded
# RigidPrimitives. Two types are placed on the ErodeFlow terrain: a smooth-subdivided box (box_subd.Box)
# and a hypermesh cone. The terrain bakes a ScatterSet (xform + type_id); the render side hands the asset's
# `scatter_hypermeshes()` recipes to scatter_consumer.install_hypermeshes -> ONE instanced indirect draw
# per type (FWD_SSBO_CUSTOM_INSTANCED), geometry computed once, placed at the baked per-instance matrices.
#
#   ./ork.terrain.viewer2.py scatterset2
###############################################################################
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.dflow.hypermesh import Hypermesh
from ork.hypergraph.assets.terrain.erodeflow import ErodeFlow
from ork.hypergraph.assets.hypermesh.box_subd import Box     # type 0: smooth-subdivided cube
from ork.hypergraph.assets.hypermesh.droop_demo import DroopDemo     # type 0: smooth-subdivided cube
from orkengine.core import vec3

###############################################################################


class ConeHM(Hypermesh):
  """A hypermesh cone (mixed tri sides + 1 base n-gon), flat-shaded. Sized in METERS (the scatter scale
  multiplies). Base near the origin so the instance matrix drops it onto the terrain."""
  def __init__(self, radius=-3.0, height=6.0, sides=16):
    super().__init__()
    n = self.cone(radius=radius, height=height, sides=sides)
    self.cone = n
    n = self.face_normals(n)
    self.output(n)
  def onUpdate(self,updinfo):
    t     = updinfo.absolutetime
    phase = (t % 15.0) / 15.0
    tri   = 1.0 - abs(2.0 * phase - 1.0)         # 0 -> 1 -> 0 over 15s
    self.cone.inputs.sides = int(round(3 + (72 - 3) * tri))

###############################################################################


class ScatterSet2(ErodeFlow):

  def __init__(self, iters=32):
    super().__init__(iters=iters, origin=vec3(0))          # eroded terrain + captures; sets self.z
    z     = self.z
    slope = T.slope(z, radius_m=8.0)
    alt   = T.normalize(z)
    flat  = 1.0 - T.smoothstep(slope, 0.10, 0.40)          # 1 on flat ground, 0 on steep
    self.scatter("props",
        density = 0.00001,                                  # sparse (ErodeFlow is ~16 km)
        seed    = 11,
        cutoff  = 0.1,
        jitter  = 1.0,
        align   = "up",
        lift    = 4.0,                                      # raise each instance +1m along world +Y (baked in)
        scale   = (0.7, 1.4),
        types   = {                                        # per-type weight fields (exclusive within)
          "boxes": T.band(alt, 0.30, 0.55, soft=0.1) * flat,   # type_id 0  mid/high, flat
          #"cones": T.band(alt, 0.12, 0.40, soft=0.1) * flat,   # type_id 1  low/mid, flat
        })

  #################################################
  # RENDER side: the ASSET owns the per-type HYPERMESH (+ material). Indexed by type_id.
  # recipe = (hypermesh_asset, material_cls or None, albedo). Sizes bumped for the ~16 km terrain.
  #################################################

  def scatter_hypermeshes(self, sink_name, ctx):
    dd  = DroopDemo()                                   # type 0
    cone = ConeHM(radius=3.0, height=6.0, sides=16)        # type 1
    return [(dd,  None, vec3(0.15))]          # default (terrain Solid) material + per-type albedo
            #(cone, None, vec3(0.90, 0.50, 0.20))]


__all__ = ["ScatterSet2", "ConeHM"]
