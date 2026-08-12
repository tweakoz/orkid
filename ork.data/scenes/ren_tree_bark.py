###############################################################################
# ren_tree_bark.py — TREES2 Track 2 GATE SCENE: the alpine bark family baked
# NON-instanced onto a single prototype broadleaf via the per-section
# texture-ARRAY path (the pueblo ren_section_bake recipe applied to the tree).
#
# OBJECT-SPACE CONTRACT (owner ruling 2026-08-11: "if its the same tree it
# should look the same … it should be textured in object space"): the mesh is
# the raw ~2.34 m prototype (NO in-graph scale — that path is removed), the
# bark ladder is authored once in bark_alpine.py in that object space, and this
# scene presents the tree via INSTANCE MATRICES — the same currency as the
# forest scatter's per-instance transforms. (The entity transform is NOT a
# route: HypermeshComponent deliberately keeps its node at identity — the
# graph/instances own placement, HypermeshComponent.cpp; and a single
# instance_matrices entry does not reach the instanced path (instCount>1 gate,
# hmdflow_render.cpp) — so this scene draws TWO instances: the forest's MEAN
# x30 at the origin as the camera subject, plus a x45 individual alongside,
# which doubles as the ruling's exhibit that instance scale magnifies bark
# with the tree.) The bake is byte-identical material data to the forest's
# (the shared gauge key proves it); only the per-scene bake RES budget differs
# (4096 here for close inspection vs 1024 x 16 variants in the forest).
#
# ONE tree, TWO drawables (the Track split):
#   * trunk  — LsTrunkSections (lsystem sweep, gid partition {0 lower trunk,
#     1 upper branch}, SectionUnwrap). Drawn ONCE with the BarkSectionPBR
#     stored sampler; the per-gid BAKE MAP {0: furrow-language bark, 1: smooth
#     young-wood bark} fills the two array layers at load (content-addressed
#     cache; bake rasterize forces CullTest=OFF per bake law).
#   * leaves — LsLeavesOnly (the SAME skeleton's cards, same seed) on the
#     existing STORED leaf-card material. Deliberately NOT unwrapped/baked:
#     leaves stay on shared cards; xatlas only ever sees the trunk skeleton.
#
# Species language is DATA (BARK_SPECIES) consumed VERBATIM — no per-scene
# overrides (an override would fork the look across scenes AND bypass the
# content-key re-bake; the same-tree-same-look contract in bark_alpine.py).
# Physical-albedo doctrine: bark value ~0.15-0.35, no lighting-mood gain.
#
#   REGEN LOOP (both steps EVERY time — the material content-hash names are
#   computed at tojson time and live inside the .ecs; a player-only rerun can
#   never pick up material edits). PIN the live checkout if the ambient shell's
#   PYTHONPATH/ORKID_WORKSPACE_DIR point at a different checkout:
#
#   PYTHONPATH=<venv-site>:<repo>/obt.project/scripts \
#   ORKID_WORKSPACE_DIR=<repo> \
#     ork.scene.tojson.py -i ren_tree_bark -o /tmp/tree_bark.ecs
#   ork.ecs.player.exe /tmp/tree_bark.ecs -S /tmp/tree_bark.png -F 60 \
#       --camdist 15 --camheight 1.7     # eye-level trunk fill on the x30 tree
#
#   (grep the player log for "section_bake(C++): COLD" = your edit re-baked;
#    WARM = content unchanged. Edits in ptex3d/dsl.py|functions.py are OUTSIDE
#    the content hash — wipe the logged key dirs for those.)
###############################################################################

from orkengine.core import vec3, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.dflow.hypermesh import GpuMeshRenderSource
from ork.hypergraph.assets.hypermesh.ls_trunk_baked import (
    LsTrunkSections, LsLeavesOnly, GID_BARK, GID_BRANCH)
from ork.hypergraph.assets.materials import bark_alpine as _bark_mod
from ork.hypergraph.assets.materials.bark_alpine import (
    BarkAlpineStored, BarkSectionPBR, BARK_SPECIES, bark_content_key)
from ork.hypergraph.assets.materials.leaf import LeafCard, leaf_fields, LEAF_PRESETS
from ork.hypergraph.ptex3d.card_bake import bake_card
from ork.hypergraph.colors import hsv

lev2_pyexdir.addToSysPath()

# ── the gate's data knobs ────────────────────────────────────────────────────
TREE_SCALE   = 30.0      # PRESENTATION only (instance matrix) — the forest
                         # scatter's MEAN per-instance scale (xxx3_trees
                         # scale=(30,15)). ~2.34 m prototype -> ~70 m displayed
                         # tree. Never touches the mesh graph or the bake.
TREE_SCALE_B = 45.0      # the second instance: the scatter spread's TOP end —
                         # proportionally bigger bark, same bake (the ruling)
TREE_B_X     = 80.0      # world-x offset of the second instance (meters)
BAKE_RES     = 4096      # per-scene bake res BUDGET (close-inspection scene);
                         # the forest budgets 1024 x 16 variants — res is the
                         # ONLY sanctioned per-scene difference in the bark path
SPECIES_LO = "furrow"    # gid 0 lower-trunk language (furrow | plate | smooth)
SPECIES_HI = "smooth"    # gid 1 upper young wood


def _instances():
  """COLUMN-MAJOR mat4 floats (translation = flat 12,13,14; scale = diag
  0,5,10 — the ren_section_bake layout note: row-major drops tx into the
  projective slot and every instance collapses to identity)."""
  out = []
  for s, tx in ((TREE_SCALE, 0.0), (TREE_SCALE_B, TREE_B_X)):
    out += [s,   0.0, 0.0, 0.0,     # col 0
            0.0, s,   0.0, 0.0,     # col 1
            0.0, 0.0, s,   0.0,     # col 2
            tx,  0.0, 0.0, 1.0]     # col 3 = translation
  return out


class TreeBarkScene(Scene):

  def __init__(self):
    super().__init__()

    self.scenegraph(
        preset            = "ForwardPBR",
        skybox_path       = "<ork_envmaps2>/blender_forest.xir",
        SkyboxIntensity   = 1.0,
        DiffuseIntensity  = 1.0,
        SpecularIntensity = 1.0,
        AmbientLight      = vec3(0.0),
        msaa              = 2,      # A2C leaf cards need an MSAA RtGroup
        ssaa              = 0)

    self.system_data("HypermeshSystem")

    ##########################
    # materials
    ##########################

    sampler = self.asset.Ptex3d(
        "bark_sampler",
        dsl_class     = BarkSectionPBR,
        vertex_source = GpuMeshRenderSource(instanced=True),   # instance_matrices path
        env_specular  = 0.15)   # match the forest sampler's IBL-sheen clamp
                                # (drawn-sampler response, not bake content)

    # species param sets VERBATIM (same-tree-same-look contract)
    lo_params = BARK_SPECIES[SPECIES_LO]
    hi_params = BARK_SPECIES[SPECIES_HI]

    # SHARED content key (bark_alpine.bark_content_key): any bark material edit
    # re-keys the section-bake cache through the asset NAMES. The print is the
    # STALENESS + SAMENESS GAUGE: it must CHANGE after a bark edit, it must
    # MATCH the forest run's printed key (byte-identical material data), and
    # the resolved path must be the checkout you are editing; pair it with the
    # player's "section_bake(C++): COLD/WARM".
    ck = bark_content_key()
    import sys
    print(f"ren_tree_bark: bark content key {ck} <- {_bark_mod.__file__}",
          file=sys.stderr)

    bark_lo = self.asset.Ptex3d(
        "bark_alpine_lo_" + ck,
        dsl_class     = BarkAlpineStored,
        vertex_source = GpuMeshRenderSource(),
        **lo_params)

    bark_hi = self.asset.Ptex3d(
        "bark_alpine_hi_" + ck,
        dsl_class     = BarkAlpineStored,
        vertex_source = GpuMeshRenderSource(),
        **hi_params)

    leaf = self.asset.Ptex3d(
        "tree_leaf",
        dsl_class        = LeafCard,
        vertex_source    = GpuMeshRenderSource(instanced=True),   # instance_matrices path
        sampler_textures = {"CardTex": bake_card(leaf_fields,
                                                 res    = 512,
                                                 params = LEAF_PRESETS["beech"])},
        albedo           = hsv(100.0, 0.66, 0.252),
        roughness        = 0.45,
        env_specular     = 0.15)

    ##########################
    # meshes — one skeleton, two drawables (same seed => identical skeleton).
    # RAW object space (~2.34 m) — the same mesh the forest scatters.
    ##########################

    trunk = self.asset.Hypermesh(
        "bark_trunk",
        dsl_class = LsTrunkSections)

    leaves = self.asset.Hypermesh(
        "bark_leaves",
        dsl_class = LsLeavesOnly)

    ##########################
    # entities — presented at forest instance scales via instance_matrices
    # (the same per-instance currency as the scatter; see the header note on
    # why the entity transform and a single matrix are not routes)
    ##########################

    inst = _instances()

    self.entity(
        "trunk0",
        components = [self.declare_component(
            "HypermeshComponent",
            drawabledata = trunk.drawable_data(
                material     = sampler,                                 # drawn stored sampler
                materials    = {GID_BARK: bark_lo, GID_BRANCH: bark_hi},  # per-gid BAKE MAP
                section_bake = True,
                bake_res     = BAKE_RES,
                instance_matrices = inst),
            layername = "std_forward",
            nodename  = "trunk0")])

    self.entity(
        "leaves0",
        components = [self.declare_component(
            "HypermeshComponent",
            drawabledata = leaves.drawable_data(
                material = leaf,
                instance_matrices = inst),
            layername = "std_forward",
            nodename  = "leaves0")])


__all__ = ["TreeBarkScene"]
