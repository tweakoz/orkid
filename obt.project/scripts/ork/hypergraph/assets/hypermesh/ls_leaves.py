###############################################################################
# ls_leaves — broadleaf ORGAN test: an L-system tree SKELETON + a leaf-card mesh
# scattered on it by phyllotaxis (LeafScatterModule). Outputs the LEAF mesh alone
# (a canopy-shaped cloud of cards); the leaf material is the procedural two-sided
# A2C LeafProc, with VS WIND + flutter so the cards shimmer. This is the leaves-only
# placement test; ls_anim is the full tree that bakes these cards into the trunk.
#
#   ork.hypermesh.viewer.py ls_leaves           # the canopy of cards (swaying)
#   ork.hypermesh.viewer.py ls_leaves --msaa 2  # A2C edge dither on top of the alpha-mask
###############################################################################
from orkengine.core import vec3
from ork.hypergraph.dflow.hypermesh import Hypermesh, Archetype, LeafStyle, HmMaterial, Wind, LeafFlutter
from ork.hypergraph.assets.materials.leaf import LeafProc


# WIND shared with the trunk (ls_anim bark) so leaves + branches sway together.
LEAF_WIND = dict(amp=0.035, freq=0.4, dir=(1, 0, 0))


class LsLeaves(Hypermesh):
  def __init__(self, seed=21, budget=4000):
    super().__init__()
    # SKELETON — keep IDENTICAL to ls_anim.LsAnim's lsystem() (the L-system is deterministic, so
    # matching params + seed reproduce the same node frames -> leaves land exactly on the trunk).
    self.lsystem(archetype=Archetype.SYMPODIAL,
                 depth=7,
                 children=2,
                 seg_len=0.6,
                 base_radius=0.16,
                 sides=6,
                 branch_angle=38.0,
                 internodes=3,
                 tropism=0.05,
                 jitter=0.35,
                 apical=0.3,
                 seed=seed,
                 budget=budget)
    n = self.leaves(style=LeafStyle.SINGLE,
                    per_node=4,
                    min_gen=4.0,
                    size=0.1,
                    jitter=0.3)
    self.output(n)

  def materials(self):
    # procedural two-sided A2C leaf (silhouette + midrib from the card UV) + VS wind sway.
    return [HmMaterial(
              LeafProc,
              albedo=vec3(0.24, 0.52, 0.17),
              roughness=0.45,
              vtx_displace=[Wind(**LEAF_WIND), LeafFlutter()],
              name="leaf")]


__all__ = ["LsLeaves"]
