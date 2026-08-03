###############################################################################
# SdfBaked — the O3 per-section texture-ARRAY demo (companion to SdfClean).
#
# A box is voxelized to an SDF brick and clean-remeshed (unwrap=False), then gid-
# PARTITIONED into >=2 SECTIONS (top faces vs the rest), and each section is
# UNWRAPPED into its OWN 0-1 UV domain with its section LAYER index in UV0.z by the
# SectionUnwrap op. The render material (SectionArray) samples ONE sampler2DArray at
# that layer, so a SINGLE material shades every section from its own array LAYER —
# the O3 baked path (no per-gid draw buckets).
#
#   box -> mesh_to_sdf -> sdf_to_mesh_clean(unwrap=False)
#       -> select(top)+assign_gid -> assign_gid(rest) -> section_unwrap -> (render)
#
#   ./ork.hypermesh.viewer.py SdfBakedViz     # colors each section by its layer (unwrap proof)
#   ./ork.hypermesh.viewer.py SdfBaked        # samples the bound texture array per section
#   _ork.hypermesh.validate.py SdfBakedViz -o /tmp/sdf_baked.obj
###############################################################################
from ork.hypergraph.dflow.hypermesh import Hypermesh, S, replace, group, POLY
from ork.hypergraph.assets.materials.hypermesh.section_array import SectionViz, SectionArray

# gid section keys (the render bucketing / array-layer partition). GidAssign is the ONLY writer.
GID_REST = 0   # side/bottom faces  -> array layer 0
GID_TOP  = 1   # up-facing faces    -> array layer 1
_SEL_TOP = 0   # a free-region select bit (0..19) driving assign_gid(slot=_SEL_TOP)


class _SdfBakedBase(Hypermesh):
  DIM          = 96    # SDF brick resolution
  ADAPTIVITY   = 0.5   # 0 = max detail, 1 = flattest
  SIZE         = 1.2   # box edge length
  EXTENT       = 2.4   # brick cube side (contains the box + narrow band)
  NUM_SECTIONS = 2     # gid sections (== texture-array layer count for the bake)

  def __init__(self):
    super().__init__()
    b  = self.box(size=0.5)
    b  = self.transform(b, scale=(self.SIZE, self.SIZE, self.SIZE))
    sd = self.mesh_to_sdf(b, dim=self.DIM, extent=self.EXTENT, center=(0.0, 0.0, 0.0))
    m  = self.sdf_to_mesh_clean(sd, adaptivity=self.ADAPTIVITY, unwrap=False)  # NO unwrap here — SectionUnwrap owns UVs
    # gid PARTITION (>=2 sections): up-facing faces -> TOP, everything else -> REST.
    m  = self.select(m, S.N.y > 0.5, domain=POLY, op=replace(group(_SEL_TOP)))
    m  = self.assign_gid(m, gid=GID_REST)                 # whole-mesh base stamp (layer 0)
    m  = self.assign_gid(m, gid=GID_TOP, slot=_SEL_TOP)   # top faces -> layer 1
    # PER-SECTION unwrap: each gid section -> its own 0-1 UV + dense layer index in UV0.z.
    self.output(self.section_unwrap(m, padding=2))


class SdfBakedViz(_SdfBakedBase):
  """Colors each section by its layer index (ctx.layer) — the unwrap/layer proof, no texture array."""
  MATERIAL_CLASS = SectionViz


class SdfBaked(_SdfBakedBase):
  """Samples the bound per-section sampler2DArray at each section's layer — the O3 baked path.
  Bind the array on the material via material.bindParam(SectionArray.ARRAY_SAMPLER, texarray)."""
  MATERIAL_CLASS = SectionArray


__all__ = ["SdfBaked"]
