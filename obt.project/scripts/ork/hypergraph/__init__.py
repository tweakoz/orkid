###############################################################################
# ork.hypergraph — unified procedural authoring namespace.
#
# Layout:
#   hypergraph/
#     colors.py         — universal: hsv(), colors palette, _Hsv
#     asset_core/       — constructors, converters, loaders (infrastructure)
#       material/       —   PbrMaterial, FreestyleMaterial
#       sdf/            —   ImplicitSdf, MeshToSdf, MeshSdf, VdbFileSdf
#       drawable/       —   VdbGridToDrawable
#       particle/       —   ParticleSystem
#       probe/          —   HdriToXir
#     assets/           — concrete named asset wrappers (parametric primitives)
#       sdf/            —   SphereSdf, ThickSaddleSdf
#       mesh/           —   HollowFunnelMesh
#     scenegraph/       — reserved: future non-ECS scenegraph
#     ecs/              — ECS-coupled Scene DSL + runtime
#       scene/          —   Scene, Transform, SceneGraphHandle
#       runtime.py      —   EcsRuntime
#     dflow/            — dataflow DSL
#
# Convenience re-exports below let casual authors do:
#   from ork.hypergraph import Scene, Transform, hsv, colors
###############################################################################

from ork.hypergraph.colors import hsv, wavelength, colortemp, mix, colors
from ork.hypergraph.ecs.scene import Scene, Transform, axis_angle, SceneGraphHandle

__all__ = [
  "Scene", "Transform", "axis_angle", "SceneGraphHandle",
  "hsv", "wavelength", "colortemp", "mix", "colors",
]
