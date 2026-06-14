from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def vdb_collider(upstream, *packs, name=None, sdf_grid=None, follow_entity="",
                 **plug_kwargs):
    """Arbitrary-surface collider sampled from an OpenVDB signed-distance
    field. The grid is typically built once via
    `lev2.openvdb.meshToLevelSet(verts, tris, voxel_size)` on a CLOSED
    triangle mesh. Maps to particles.VdbCollider.

    Plugs:    Restitution (float), Friction (float).
    Non-plug: sdf_grid       (FloatGrid) — required; assign before first
                              frame.
              follow_entity  (str) — name of a published ECS entity. When
                              set and resolvable, the collider treats the
                              SDF as living in that entity's local frame
                              and tracks the entity each tick (move the
                              entity, the collider moves). Empty (default)
                              or unresolved → world-space sampling
                              (legacy behavior). The entity must opt into
                              publication via its spawner's publish_xf.
    """
    # chain_op only knows about plugs (Restitution / Friction). The grid
    # and follow_entity are set via properties exposed in the pyext binding.
    node = chain_op(_particles.VdbCollider, "VCOL", upstream,
                    name=name, packs=packs, **plug_kwargs)
    if sdf_grid is not None:
        node.module.sdf_grid = sdf_grid
        # ASSET-REFERENCE stamp: if this grid is a registered cross-asset artifact
        # (the ParticleSystem wrapper registers {artifact: name} around the trace),
        # record its NAME on the module — the live grid can't serialize, the name
        # round-trips, and the host re-resolves it post-deserialize. Empty for
        # standalone/imperative grids (no registration) — legacy behavior intact.
        from .._artifact_names import name_for
        node.module.sdf_asset = name_for(sdf_grid)
    node.module.follow_entity = follow_entity
    return node
