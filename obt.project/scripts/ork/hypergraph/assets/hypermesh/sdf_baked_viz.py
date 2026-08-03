###############################################################################
# SdfBakedViz — SdfBaked geometry shaded by SECTION LAYER (ctx.layer), no texture.
# The unwrap/layer proof: each section renders a distinct emissive color from its
# UV0.z layer index, so the SectionUnwrap gid partition reads directly.
#
#   ./ork.hypermesh.viewer.py sdf_baked_viz
#   _ork.hypermesh.validate.py sdf_baked_viz -o /tmp/sdf_baked_viz.png
###############################################################################
from ork.hypergraph.assets.hypermesh.sdf_baked import _SdfBakedBase
from ork.hypergraph.assets.materials.hypermesh.section_array import SectionViz


class SdfBakedViz(_SdfBakedBase):
  MATERIAL_CLASS = SectionViz


__all__ = ["SdfBakedViz"]
