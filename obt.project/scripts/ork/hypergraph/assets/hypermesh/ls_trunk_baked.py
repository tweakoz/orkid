###############################################################################
# ls_trunk_baked — the LsAnim tree SPLIT for the per-section BAKE path (TREES2
# Track 2 slice 1): the trunk/branch skeleton section-unwrapped for the baked
# bark texture arrays, the leaf cards kept as their own mesh on the shared card
# material. Same generator params + seed as LsAnim => byte-identical skeleton
# (deterministic), so the two drawables reassemble ONE tree.
#
#   LsTrunkSections — lsystem sweep, gid partition {0 lower trunk, 1 upper
#                     branch}, SectionUnwrap (xatlas per gid -> 2 sections/
#                     layers, layer index in UV0.z). The mesh a
#                     drawable_data(section_bake=True) bakes + draws.
#   LsLeavesOnly    — the SAME skeleton's phyllotactic leaf cards only (shared
#                     0-1 card UVs, existing LeafCard/LeafProc materials).
#                     Deliberately NOT unwrapped: leaves stay on shared cards
#                     (the Track 1 design split), so xatlas only ever sees the
#                     trunk skeleton.
#
#   _ork.hypermesh.validate.py LsTrunkSectionsViz -o /tmp/trunk_sections.obj
###############################################################################
from ork.hypergraph.dflow.hypermesh import Hypermesh, Archetype, LeafStyle, S, replace, group
from ork.hypergraph.assets.materials.hypermesh.section_array import SectionViz

# gid section keys (== texture-array layer order, sorted-gid). GidAssign is the ONLY writer.
GID_BARK   = 0   # lower trunk / main boughs  -> array layer 0
GID_BRANCH = 1   # upper branches (above foliage_y) -> array layer 1
_SEL_UPPER = 0   # free-region select bit driving assign_gid(slot=_SEL_UPPER)

# ONE source of truth for the skeleton shape — LsAnim's broadleaf defaults, verbatim.
# Both classes below consume this dict so trunk and leaves derive from the SAME
# generating data (physics-proxy law: the collider/leaf placement and the render
# trunk share one skeleton parameterization).
BROADLEAF_SHAPE = dict(
    archetype    = Archetype.SYMPODIAL,
    depth        = 7,
    children     = 2,
    seg_len      = 0.6,
    base_radius  = 0.16,
    sides        = 4,
    branch_angle = 38.0,
    internodes   = 3,
    tropism      = 0.05,
    jitter       = 0.35,
    apical       = 0.3,
    seed         = 21,
    budget       = 3000)

FOLIAGE_Y = 1.8   # the LsAnim upper-branch split height (object meters)


class LsTrunkSections(Hypermesh):
  """Trunk+branch sweep only, gid-partitioned {0,1}, section-unwrapped for the
  baked bark texture-array path. `unwrap_padding` = xatlas chart padding (texels).

  OBJECT SPACE ONLY (owner ruling 2026-08-11): the mesh stays the raw ~2.34 m
  prototype — NO draw scale is ever baked into the graph. The bark ladder is
  authored in this object space (bark_alpine.py); scenes present the tree at
  size via the entity/instance transform, exactly as the forest scatter does,
  so the same mesh + same bake serve every scale."""

  def __init__(self, foliage_y=FOLIAGE_Y, unwrap_padding=2, **shape):
    super().__init__()
    k = dict(BROADLEAF_SHAPE)
    k.update(shape)
    trunk = self.lsystem(**k)
    # UPPER-BRANCH SPLIT: faces above foliage_y -> gid 1; the rest stay gid 0 (bark).
    trunk = self.select(trunk, S.P.y > foliage_y, op=replace(group(_SEL_UPPER)))
    trunk = self.assign_gid(trunk, gid=GID_BARK)                  # whole-mesh base stamp (layer 0)
    trunk = self.assign_gid(trunk, gid=GID_BRANCH, slot=_SEL_UPPER)
    # PER-SECTION unwrap: each gid -> its own 0-1 UV domain + layer index in UV0.z.
    self.output(self.section_unwrap(trunk, padding=unwrap_padding))


class LsTrunkSectionsViz(LsTrunkSections):
  """Colors each section by its layer (ctx.layer) — unwrap/layer isolation proof."""
  MATERIAL_CLASS = SectionViz


class LsLeavesOnly(Hypermesh):
  """The same skeleton's leaf cards ONLY (LsAnim's leaf defaults; shared 0-1 card
  UVs — bakes/uv-unwraps never touch these). Draw with LeafProc/LeafCard.

  SHAPE NOTE: built as merge(trunk, leaves) -> delete(trunk faces), NOT as a bare
  leaves() terminal. The C++ player materializer picks "the last producing module"
  as the graph terminal, so a graph with a dangling (unconsumed) trunk sweep can
  resolve the SWEEP as its output and silently draw the trunk instead of the
  cards (observed in the Track 2 slice-1 gate). Merging then deleting keeps every
  module consumed and the delete op is unambiguously last."""

  def __init__(self,
               leaf_per_node   = 3,
               leaf_min_gen    = 4.0,
               leaf_max_radius = 0.0,
               leaf_embed      = 0.0,
               leaf_twist      = 0.0,
               leaf_up_bias    = 0.0,
               leaf_jitter_deg = 0.0,
               leaf_size       = 0.13,
               leaf_jitter     = 0.3,
               leaf_style      = LeafStyle.SINGLE,
               leaf_aspect     = 0.6,
               leaf_pitch      = 50.0,
               leaf_roll       = 137.5,
               **shape):
    super().__init__()
    k = dict(BROADLEAF_SHAPE)
    k.update(shape)
    trunk  = self.lsystem(**k)      # the skeleton hub + its sweep (consumed by the merge below)
    leaves = self.leaves(
        style    = leaf_style,
        per_node = leaf_per_node,
        min_gen  = leaf_min_gen,
        max_radius = leaf_max_radius,
        size     = leaf_size,
        aspect   = leaf_aspect,
        pitch    = leaf_pitch,
        roll     = leaf_roll,
        embed    = leaf_embed,
        twist    = leaf_twist,
        up_bias  = leaf_up_bias,
        jitter   = leaf_jitter,
        jitter_deg = leaf_jitter_deg)
    # merge (consumes the sweep) -> tag trunk faces -> delete them = cards only
    m = self.merge(trunk, leaves, gid_a=0, gid_b=2)
    m = self.select(m, S.gid < 0.5, op=replace(group(0)))
    m = self.delete_faces(m, slot=0)
    self.output(m)


__all__ = ["LsTrunkSections", "LsTrunkSectionsViz", "LsLeavesOnly",
           "GID_BARK", "GID_BRANCH", "BROADLEAF_SHAPE", "FOLIAGE_Y"]
