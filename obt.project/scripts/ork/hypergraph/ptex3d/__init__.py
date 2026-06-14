###############################################################################
# ork.hypergraph.ptex3d — procedural PBR surface (ptex3d) family.
#
# GEOV2 Phase 1: the .fxv2 template + variant-table generator. Given a GLSL
# surface-function body (and optional helper libblock / library inherits /
# imports), assemble a complete, valid forward-PBR .fxv2 — the CV/CT/depth-
# prepass technique table over the proven Phase-0 interface/VS structure — and
# bake it hash-named into <staging>/dslshadercache/. Phase 2 (the DSL + SSA/CSE
# emitter) produces the surface body + libblock; this layer turns either a
# hand-written or generated body into a renderable shader.
###############################################################################

from ork.hypergraph.ptex3d.fxv2_template import (
  generate_surface_fxv2,
  materialize_surface_fxv2,
  SURFACE_OUT_FIELDS,
  CODEGEN_VERSION,
)
from ork.hypergraph.ptex3d.dsl import (
  Ptex3d,
  SurfaceCtx,
  SurfNode,
  Bundle,
  P,
  rgb,
  Param,
  emit_surface,
  materialize_ptex3d,
  materialize_ptex3d_full,
)

__all__ = [
  # Phase 1 — fxv2 template generator
  "generate_surface_fxv2",
  "materialize_surface_fxv2",
  "SURFACE_OUT_FIELDS",
  "CODEGEN_VERSION",
  # Phase 2 — surface DSL
  "Ptex3d",
  "SurfaceCtx",
  "SurfNode",
  "Bundle",
  "P",
  "rgb",
  "Param",
  "emit_surface",
  "materialize_ptex3d",
  "materialize_ptex3d_full",
]
