###############################################################################
# ls_anim — a sympodial L-system tree BAKED into ONE mesh, gid-partitioned 3 ways:
#   gid 0  bark      — the lower trunk/branches
#   gid 1  branch    — the UPPER branch faces (above foliage_y; the old foliage split)
#   gid 2  leaf       — phyllotactic leaf cards (procedural A2C LeafProc)
# The trunk does its own select()+assign_gid() upper-branch split (gid 1); merge()
# then PRESERVES that split (gid_a=None) and stamps the leaf cards gid 2. One mesh ->
# one drawable, three materials, ONE frustum-cull. All three carry VS Wind (bulk
# sway); the leaves additionally get LeafFlutter (per-leaf shimmer vs the branches).
#
#   ork.hypermesh.viewer.py ls_anim --msaa 2   # the tree, swaying + fluttering
#   ork.scene.viewer.py scn_ls_anim            # same, in a full scene
###############################################################################
from ork.hypergraph.dflow.hypermesh import (Hypermesh, Archetype, LeafStyle, HmMaterial,
                                            Wind, LeafFlutter, S, replace, group)
from ork.hypergraph.assets.materials.terrain.solid import Solid
from ork.hypergraph.assets.materials.bark import Bark
from ork.hypergraph.assets.materials.leaf import LeafProc
from ork.hypergraph.colors import hsv


TREE_WIND = dict(amp=0.015, freq=0.3, dir=(1, 0, 0))


class LsAnim(Hypermesh):
  # SHAPE params default to the broadleaf (oak-like) species; a second species (conifer) just passes a
  # different archetype + angles/decay + leaf settings. seed varies the variant within a species.
  def __init__(self, 
               seed=21, 
               budget=3000, 
               archetype=Archetype.SYMPODIAL, 
               depth=7, 
               children=2, 
               foliage_y=1.8,
               seg_len=0.6, 
               base_radius=0.16,
               sides=4, 
               branch_angle=38.0, 
               internodes=3, 
               tropism=0.05, 
               jitter=0.35, 
               apical=0.3,
               leaf_per_node=3, 
               leaf_min_gen=4.0, 
               leaf_size=0.1, 
               leaf_jitter=0.3):
    super().__init__()
    # skeleton -> swept trunk (lsystem returns the sweep mesh AND stashes self._skeleton for leaves()).
    trunk = self.lsystem(archetype=archetype,
                         depth=depth,
                         children=children,
                         seg_len=seg_len,
                         base_radius=base_radius,
                         sides=sides,
                         branch_angle=branch_angle,
                         internodes=internodes,
                         tropism=tropism,
                         jitter=jitter,
                         apical=apical,
                         seed=seed,
                         budget=budget)
    # UPPER-BRANCH SPLIT: faces above foliage_y -> gid 1 (their own material); the rest stay gid 0 (bark).
    trunk = self.select(trunk, S.P.y > foliage_y, op=replace(group(0)))
    trunk = self.assign_gid(trunk, gid=1, slot=0)
    leaves = self.leaves(style=LeafStyle.SINGLE,
                         per_node=leaf_per_node,
                         min_gen=leaf_min_gen,
                         size=leaf_size,
                         jitter=leaf_jitter)
    # BAKE: keep the trunk's gid 0/1 split (gid_a=None), stamp the leaf cards gid 2.
    n = self.merge(trunk, leaves, gid_a=None, gid_b=2)
    self.output(n)

  def materials(self):
    # all colors authored in HSV (hue°, sat, val) via hypergraph colors.py.
    return [HmMaterial(Bark,                               # gid 0 — procedural furrowed bark (lower trunk)
                       vtx_displace=Wind(**TREE_WIND),
                       name="bark"),
            HmMaterial(Solid,                              # gid 1 — upper branches (younger, greener bark)
                       albedo=hsv(96, 0.46, 0.250),
                       roughness=0.7,
                       gid=1,
                       vtx_displace=Wind(**TREE_WIND),
                       name="branch"),
            HmMaterial(LeafProc,                           # gid 2 — procedural A2C leaf
                       albedo=hsv(102, 0.66, 0.252),
                       roughness=0.45,
                       gid=2,
                       vtx_displace=[Wind(**TREE_WIND),     # bulk sway (rides the branch) ...
                                     LeafFlutter(amp=0.02, freq=1.41)],        # ... + per-leaf flutter RELATIVE to it
                       name="leaf")]


__all__ = ["LsAnim"]
