###############################################################################
# Lines — a flat, unlit single-color material for LINE primitives. Reusable for BOTH the wireframe
# overlay (drawn on top of a filled mesh) AND lines as a primary renderable primitive (a drawable whose
# main primtype is LINES). `color` drives EMISSIVE (shows directly), with albedo 0 so there's ~no lit
# contribution -> the line reads as exactly `color` under any lighting. Pair with GpuMeshWireSource
# (which can also push verts along the normal for a wireframe depth bias) or any P-reading vertex source.
###############################################################################
from orkengine.core import vec3
from ork.hypergraph.ptex3d import Ptex3d


class Lines(Ptex3d):
  """Flat unlit `color` (via emissive). For wireframe overlays and line-primitive draws."""

  def __init__(self, ctx, *, color=vec3(0.0, 0.0, 0.0), **kw):
    self.surface(albedo=vec3(0.0), metallic=0.0, roughness=1.0, emissive=color)


__all__ = ["Lines"]
