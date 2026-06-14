###############################################################################
# ork.dflow.particles.ops.vdb_level_set_renderer — VdbLevelSetRenderer DSL op.
#
# Splats live particles into a per-graphinst OpenVDB FloatGrid as a density
# field, runs marching cubes (openvdb::tools::volumeToMesh) at IsoLevel,
# and draws the extracted triangle mesh via an internally-owned RigidPrimitive.
# This is the "metaball" renderer — particles merge into smooth blobby
# surfaces when they get close enough that their kernels overlap.
#
# Bindable plugs (FloatXf, uniform):
#   Radius    — per-particle kernel falloff radius (world units)
#   Strength  — kernel amplitude (density contribution per particle)
#   IsoLevel  — marching-cubes threshold (the iso-surface value)
#
# Construction-only properties (kwargs, not bindable):
#   voxel_size — VDB grid voxel size. Smaller = sharper but quadratic
#                memory cost per splat sphere.
#   kernel     — VdbLevelSetKernel enum (Wyvill / Cubic / Quartic / Gaussian)
#   material   — any material (FreestyleMaterial / PBR / etc.)
#
# Authoring:
#
#   self.blobs = P.VdbLevelSetRenderer(upstream,
#                                      material=mat,
#                                      voxel_size=0.05,
#                                      kernel=particles.VdbLevelSetKernel.Wyvill,
#                                      Radius=0.4, Strength=1.0, IsoLevel=0.5)
#   self.render(self.blobs)
###############################################################################

from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def vdb_level_set_renderer(upstream, *packs, name=None, material=None,
                            material_gen=None,
                            voxel_size=None, kernel=None, **plug_kwargs):
    """Metaball-style renderer using OpenVDB level set + marching cubes.

    material     — a LIVE lev2 material (PBR/Freestyle); runtime-only (does not
                   serialize — a graph relying on it alone renders nothing after
                   a JSON round-trip).
    material_gen — a PbrMaterialGenData RECIPE; reflected, so it round-trips
                   with the graph and re-materializes at first render when the
                   live material is absent. Serializable systems pass BOTH
                   (live for in-process parity, gen for the round-trip)."""
    node = chain_op(_particles.VdbLevelSetRenderer, "VDBLS", upstream,
                    name=name, material=material, packs=packs, **plug_kwargs)
    # voxel_size and kernel are construction-only PROPERTIES, not plugs —
    # set after chain_op since they aren't input-plug names.
    if material_gen is not None:
        node.module.material_gen = material_gen
    if voxel_size is not None:
        node.module.voxel_size = float(voxel_size)
    if kernel is not None:
        node.module.kernel = kernel
    return node
