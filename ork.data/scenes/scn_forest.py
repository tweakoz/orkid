###############################################################################
# scn_forest.py — the xxx3 erosion terrain wearing a SCATTERED FOREST: 16 tree
# variants (2 species x 8 seed-variants) stamped across the terrain by the
# "trees" scatter sink (xxx3_trees), placed by a treeline + slope mask. Each
# variant is the full LsAnim tree (lsystem -> leaves -> bark merge) + ONE extra
# call, instance_source(type_id=N), so it draws once per scattered point of its
# type in a single indirect call (E.4 per-view cull per variant). The terrain is
# declared FIRST (it bakes the placement the trees read).
#
#   ork.scene.viewer.py scn_forest
###############################################################################

from orkengine.core import vec3, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.dflow.hypermesh import Archetype, GpuMeshRenderSource
from ork.hypergraph.assets.hypermesh.ls_anim import LsAnim
from ork.hypergraph.assets.materials.terrain.solid import Solid
from ork.hypergraph.assets.materials.bark import Bark
from ork.hypergraph.assets.materials.leaf import LeafProc
from ork.hypergraph.colors import hsv
from orkengine.lev2 import PostFxNodeHSVG

lev2_pyexdir.addToSysPath()

TERRAIN   = "forest_terra"
NUM_SEEDS = 8

# the 2 SPECIES = two tree hypermeshes (LsAnim subclasses). seed varies the variant within a species.
class Broadleaf(LsAnim):
  """A — oak-like broadleaf: LsAnim's defaults (sympodial crown, depth 7)."""
  # no overrides — the defaults ARE the broadleaf


class Conifer(LsAnim):
  """B — central-leader conifer: narrow branch angles, strong apical dominance, small dense leaves.
  **overrides take precedence over the fixed params so fork(depth=N) re-runs a coarser LOD."""
  def __init__(self, seed=21, **overrides):
    defaults = dict(archetype=Archetype.CONIFER,
                    depth=4,
                    children=1,
                    sides=6,
                    branch_angle=22.0,
                    internodes=2,
                    seg_len=0.5,
                    base_radius=0.12,
                    apical=0.6,
                    jitter=0.25,
                    leaf_size=0.06,
                    leaf_per_node=5,
                    leaf_min_gen=3.0)
    super().__init__(seed=seed, **{**defaults, **overrides})


SPECIES = [Broadleaf, Conifer]


def _make_tree(species_cls, seed):
  """A per-variant subclass of the SPECIES tree hypermesh — binds this variant's seed only.
     The geometry is SCATTER-FREE now: the scatter bridge moved to the drawable
     (drawable_data(instance_source=(asset, sink, type_id))), so one resolved set can route
     to N LOD meshes (the geometry forks; the scatter binds once per variant at the drawable)."""
  class _Tree(species_cls):
    def __init__(self, **overrides):
      super().__init__(seed=seed, **overrides)  # forward fork() overrides (e.g. depth=N for a LOD)
  return _Tree


class ForestScene(Scene):

  def __init__(self):
    super().__init__()

    postNode = PostFxNodeHSVG()
    postNode.hue = 0.0
    postNode.saturation = 0.75
    postNode.value = 1.0
    postNode.gamma = 1.0
    #postNode.addToSceneVars(sceneparams,"PostFxChain")
    #self.post_node = postNode

    self.scenegraph(
        preset            = "ForwardPBR",
        skybox_path       = "<ork_envmaps2>/desert4k.xir",
        SkyboxIntensity   = 1.0,
        DiffuseIntensity  = 3.0,
        SpecularIntensity = 1.0,
        AmbientLight      = vec3(0.0),
        CullFrustumScale   = 0.90,   # OWNER DEBUG AID (deliberate): narrow cull frustum so culling
                                     # is VISIBLE at view edges — keep until owner says otherwise
        DepthPrepass       = False,  # TEST TOGGLE (invisibility bisection): prepass suspected of stereo-matrix
                                     # mismatch (drawn-but-invisible = color pass z-fails vs misplaced prepass depth).
                                     # Revert to True once the prepass is verified/fixed under DMVR.
        msaa = 2,
        ssaa = 0,
        postfx = [("hsvg", postNode)],  # post-fx chain (list order = apply order); reflected -> round-trips
         )

    self.system_data("HypermeshSystem")

    # TERRAIN FIRST — bakes the relief AND the "trees" scatter the variants read.
    self.terrain(TERRAIN,
                 dsl_file         = "xxx3_trees",
                 # spawn ABOVE the terrain's GLOBAL max so the drop-in always finds ground.
                 # Baked xxx3 height (true meters, natural-units era — read from the
                 # .terrain.json manifest at HEAD): min 1164.7, max 1815.9, mean 1450.8.
                 # The old y=868.9 was tuned against the PRE-natural-units bake ("ground
                 # ~2510m") and sat BELOW today's global min -> spawned underground,
                 # free-fell forever (looked like dead input). If the relief changes,
                 # re-read the manifest; the walker kill-Z respawn self-defends regardless.
                 spawn            = vec3(-8398.7, 1900.0, 7889.3),
                 chunk            = 128,
                 bake_dimension   = 4096,                
                 render_dimension = 1024,
                 bake_res         = 4096,                
                 walkable         = True,
                 mode             = "stored",   # Phase-1: capture the proctex to <assetcache>/ptex3d_capture/<key>/ (cached)
                 )

    # shared INSTANCED tree materials (3 gids: bark / branch / leaf). All 16 variants draw with these.
    # impostor=True on ALL THREE gid materials: the far-LOD impostor bake renders each gid bucket (bark
    # trunk / branch / leaf) with ITS material's capture technique into the one atlas, so the billboard is
    # correctly per-region coloured (not a single-material silhouette).
    bark = self.asset.Ptex3d(
        "tree_bark", dsl_class=Solid,
        vertex_source=GpuMeshRenderSource(instanced=True),
        albedo=hsv(30, 0.36, 0.1), impostor=True)
    branch = self.asset.Ptex3d(
        "tree_branch", dsl_class=Solid,
        vertex_source=GpuMeshRenderSource(instanced=True),
        albedo=hsv(96, 0.46, 0.250), roughness=0.7, impostor=True)
    leaf = self.asset.Ptex3d(
        "tree_leaf", dsl_class=Solid,
        vertex_source=GpuMeshRenderSource(instanced=True),
        albedo=hsv(102, 0.66, 0.252), roughness=0.45, impostor=True)

    # 16 variant meshes — each filters the "trees" scatter to its type_id, one instanced draw.
    tid = 0
    for s, species_cls in enumerate(SPECIES):
      for v in range(NUM_SEEDS):
        seed = 100 + s * 50 + v
        tree = self.asset.Hypermesh("tree%d" % tid, dsl_class=_make_tree(species_cls, seed))
        tree2 = tree.fork(sides=4)
        tree3 = tree.fork(sides=3)
        if True:
          self.entity(
            "trees%d" % tid,
            components=[self.declare_component(
                "HypermeshComponent",
                drawabledata=tree.drawable_data(material=bark,
                                                materials={
                                                  1: branch,
                                                  2: leaf},
                                                # DRAWABLE-LEVEL scatter: resolve the "trees" sink filtered
                                                # to this variant's type_id (was instance_source() in the DSL).
                                                instance_source=(TERRAIN, "trees", tid),
                                                # DISTANCE LOD: beyond 400m draw a coarser tree (lower
                                                # branch depth = far fewer polys) — same bark/branch/leaf
                                                # materials (no per-LOD override). One shared scatter.
                                                # ...and past 2400m the IMPOSTOR billboard (baked from the
                                                # full tree, per-gid coloured). tree3 is its bake-unavailable
                                                # fallback mesh.
                                                lods={0.0: tree,
                                                      1250.0: tree.imposter()},
                                                cull=True,
                                                cull_distance=8000.0,
                                                cull_slabs=4,        # occludee = 4 vertical slabs (trunk..canopy)
                                                cull_tightness=1.0), # shrink boxes 10% (sparse foliage; cull harder)
                layername="std_forward",
                nodename="hm_tree%d" % tid)])
        tid += 1

    self.projectile_pool(fire=True)   # '/' shoots fireballs (gaze-aimed in VR)

__all__ = ["ForestScene"]
