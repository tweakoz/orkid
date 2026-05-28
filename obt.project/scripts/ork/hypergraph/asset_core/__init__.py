###############################################################################
# asset_core — core asset infrastructure: registry, materialize, wire.
#
# The actual class definitions still live in ork.hypergraph.ecs.scene.assets
# during the M0 refactor (the per-file split is a follow-up; the namespace
# slots below already give authors the new import paths).
#
# Re-exports the low-level machinery so any module that needs to add a
# constructor wrapper, query/iterate the asset registry, or trigger the
# post-deserialize materialize/wire pass can import from here without
# pulling in the ECS Scene DSL.
###############################################################################

from ork.hypergraph.ecs.scene.assets import (
  _ASSET_REGISTRY,
  _register,
  _materialize,
  materialize_from_scenedata,
  wire_scene_data,
)

__all__ = [
  "_ASSET_REGISTRY",
  "_register",
  "_materialize",
  "materialize_from_scenedata",
  "wire_scene_data",
]
