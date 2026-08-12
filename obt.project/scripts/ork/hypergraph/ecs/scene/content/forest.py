###############################################################################
# forest.py — the xxx3 erosion terrain wearing a SCATTERED FOREST: 16 tree
# variants (2 species x 8 seed-variants) stamped across the terrain by the
# "trees" scatter sink (xxx3_trees), placed by a treeline + slope mask. Each
# variant is the full LsAnim tree (lsystem -> leaves -> bark merge) + ONE extra
# call, instance_source(type_id=N), so it draws once per scattered point of its
# type in a single indirect call (E.4 per-view cull per variant). The terrain is
# declared FIRST (it bakes the placement the trees read).
#
# LIBRARY CONTENT (promoted VERBATIM out of ork.data/scenes/scn_forest.py, which
# is retired): a base scene other scenes subclass, not a scene you run. The
# runnable member of this family is scn_forest, which subclasses
# ForestScene and puts it under the procedural sky:
#
#   ork.scene.viewer.py scn_forest
#
# Everything below the imports is the scene file's content unchanged — the
# species grammars, the material recipes, the terrain bake config, the LOD/cull
# constants and the post-fx chain. Same extraction shape as _cloud_deck.py.
###############################################################################

from orkengine.core import vec3, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.dflow.hypermesh import Archetype, GpuMeshRenderSource, LeafStyle
from ork.hypergraph.assets.hypermesh.ls_anim import LsAnim
from ork.hypergraph.assets.hypermesh.ls_trunk_baked import (
    LsTrunkSections, LsLeavesOnly, GID_BARK, GID_BRANCH)
from ork.hypergraph.assets.materials.terrain.solid import Solid
from ork.hypergraph.assets.materials.bark import Bark
from ork.hypergraph.assets.materials import bark_alpine as _bark_mod
from ork.hypergraph.assets.materials.bark_alpine import (
    BarkAlpineLive, BarkAlpineStored, BarkSectionPBR, BARK_SPECIES, bark_content_key)
from ork.hypergraph.assets.materials.leaf import LeafProc, LeafCard, leaf_fields, LEAF_PRESETS
from ork.hypergraph.assets.materials.needle import NeedleProc, NeedleCard, needle_fields
from ork.hypergraph.ptex3d.card_bake import bake_card

# STORED leaf/needle cards (owner directive aug09): shared field functions author the look
# once — the live GLSL classes and the numpy card baker execute the same body, so the baked
# card cannot drift from the authored material. Cards are content-addressed on source+params
# (ptex3d/card_bake.py); per-type tint and instance variation stay live, so two cards serve
# every tree. Measured +8 FPS at eye level over the live analytic forms at visual parity.
# Requires the impostor capture pipe to carry material _bound_params and mip-mapped
# sampler_textures binds (both landed; gate test_sampler_texture_mips.py + the capture-pipe
# _bound_params loop in hmdflow_render.cpp buildCapturePipe). False = live analytic forms,
# kept as the look-authoring/debug path.
STORED_CARDS = True

# THE THREE BARKS, LIVE-SWITCHABLE (owner request aug11). Every tree variant is
# declared THREE times over the SAME scatter set, each declaration tagged with a
# visgroup; exactly one group is visible and the player's HUD FOLIAGE row switches
# between them at runtime (HypermeshComponentData.visgroup / .visible ->
# HypermeshSystem SET_VISGROUP). The three:
#
#   foliage:proctex  DEFAULT — the merged tree with the LIVE procedural alpine bark
#                    (BarkAlpineLive, per-pixel ladder). This is the look the scene
#                    ships; the other two draw nothing until asked for.
#   foliage:solid    the merged tree with the pre-campaign flat Solid constants,
#                    verbatim (albedo/roughness below) — the "before" picture.
#   foliage:stored   TREES2 Track 2: each variant splits into TWO instanced
#                    drawables sharing one scatter set —
#                      * trunk  LsTrunkSections (gid {0 lower trunk, 1 upper wood}
#                        + SectionUnwrap), drawn with the BarkSectionPBR stored
#                        sampler; the per-gid bake map fills the texture-array
#                        layers (bake NON-instanced on the prototype — the Track 0
#                        shape the ren_section_bake_scatter gate proves; the ladder
#                        is pure ctx.P_object, see the ruling note above).
#                      * leaves LsLeavesOnly (the same skeleton's cards;
#                        merge->delete idiom so the C++ terminal-pick cannot
#                        resolve the dangling sweep) on the existing card
#                        materials. Leaves NEVER meet xatlas — the Track 1/2 split.
#
# WHY THIS IS AFFORDABLE: a hypermesh whose scenegraph node launches disabled is
# skipped by both the per-frame gpu update and the per-view pre-render, so it never
# materializes a mesh, never allocates an impostor atlas and never starts a section
# bake. The two hidden sets cost their declaration and nothing else; the first
# switch to one pays its build in a single hitch.
#
# ...WHICH IS ALSO WHY ONLY THE DEFAULT SET CARRIES FAR TIERS. The impostor bake is
# the scene's VRAM cliff (measured +10.4 GB peak at tile 512 / bake-msaa 1, and the
# whole point of that number is that it is paid ONCE). Giving the alternates their
# own atlases would double it the moment a viewer visited both, so solid and stored
# draw as mesh to 600 m and then cull: trees pop out past 600 m in the non-default
# modes. That is a deliberate trade for an editor toggle — the modes exist to be
# looked at up close, which is the only range at which bark reads at all.
BARK_BAKE_RES   = 1024        # per-layer bake res BUDGET (16 variants x 3 targets x 2 layers
                              # ~ 400 MB) — res is the ONLY sanctioned per-scene difference in
                              # the bark path (ren_tree_bark inspects at 4096).
# OBJECT-SPACE BARK (owner ruling 2026-08-11): the ladder is authored ONCE in
# bark_alpine.py, in the raw ~2.34 m prototype's object space. NO scale
# compensation here — the scatter's per-instance transform (xxx3_trees mean
# x30, spread x15-45) magnifies bark WITH the tree; a x45 individual gets
# proportionally bigger furrows. Content re-keying rides the shared
# bark_content_key() (bark_alpine.py — the module hash IS the material
# identity; both scenes print the same gauge key).


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
  """B — central-leader conifer: narrow branch angles, strong apical dominance, needled boughs.
  **overrides take precedence over the fixed params so fork(depth=N) re-runs a coarser LOD.

  NEEDLE REWORK (aug09): the old settings (leaf_size 0.06 / per_node 5 / min_gen 3.0) placed
  ZERO leaf cards — the CONIFER preset grammar only emits generations 0 (leader+laterals) and
  1 (tip shoots), so min_gen 3 selected nothing and the tree shipped literally bare (the
  owner's "leafless tree"). Now: needles on EVERY node (min_gen 0 — a young spruce is
  foliated to the ground), card sites along the bough length (internodes 3), large SINGLE
  tuft cards fanned by phyllotaxis (CROSS read marginally fuller at eye range but doubled
  the fill in a fill-bound scene — SINGLE holds the same silhouette for half the card
  area). Measured on cf0: 648 -> 1974 faces, well under the broadleaf's 3572."""
  def __init__(self, seed=21, **overrides):
    # archetype= selects the CONIFER PRESET GRAMMAR EMITTER (GR1.d: species are data —
    # lsystem/presets.py); same surface, no C++ enum behind it anymore.
    # SHAPE + LEAF param sets live at module scope (CONIFER_SHAPE / CONIFER_LEAF)
    # so the foliage:stored split assets (trunk-only / leaves-only) derive from the
    # SAME generating data as this merged tree — one source, no drift.
    defaults = dict(**CONIFER_SHAPE, **CONIFER_LEAF)
    super().__init__(seed=seed, **{**defaults, **overrides})


CONIFER_SHAPE = dict(archetype=Archetype.CONIFER,
                    depth=5,                    # 5 whorls -> a taller, denser cone
                    children=2,                 # 2 tip shoots per lateral (dense bough ends)
                    sides=5,                    # needle-covered trunk: pentagon reads = hexagon, -17% sweep verts
                    branch_angle=22.0,
                    internodes=3,               # card sites ALONG the bough, not just its ends
                    seg_len=0.5,
                    base_radius=0.12,
                    apical=0.6,
                    jitter=0.15)                # straighter leader (0.25 wandered)

CONIFER_LEAF = dict(
                    # NEEDLE RETUNE (aug09, owner ruling): needles off the trunk, bases INTO
                    # the wood, smaller cards at higher count. Fill budget: 10 x 0.20^2 = 0.40
                    # card-area units vs the old 4 x 0.38^2 = 0.58 — net fill DOWN ~31%.
                    leaf_size=0.20,             # half the old 0.38 tuft — reads as a needle spray,
                                                # not a paper strip, on the ~3.8 m tree
                    leaf_per_node=10,           # 2.5x count at half size (see fill note above)
                    leaf_min_gen=0.0,           # every generation INCLUDING the leader — the apex
                                                # spire stays clothed (min_gen 1 bared it, rejected);
                                                # the TRUNK cut is radius-based now (leaf_max_radius)
                    leaf_max_radius=0.05,       # grammar radii (presets.py conifer): leader runs
                                                # base_radius 0.12 -> 0 linearly over 5 whorls, so
                                                # >0.05 vetoes the lower ~58% of the trunk (bare
                                                # wood) while the thin apex + all boughs (brad
                                                # <= 0.053, tapering) keep their needles; the
                                                # lowest boughs' innermost internode is vetoed too
                                                # (bare bough shoulder at the trunk — natural)
                    leaf_embed=0.35,            # base edge recessed 0.35 x local radius under the
                                                # bark: needles EMERGE through the surface instead
                                                # of hovering beside the centerline
                    leaf_style=LeafStyle.SINGLE,  # fanned single quads (fill-budget; see docstring)
                    leaf_pitch=40.0,            # sprays stand off the shoot (25 lay flat along it;
                                                # spruce needles emerge steeper)
                    leaf_roll=137.5,            # golden-angle spiral: 10 cards never close the
                                                # azimuth circle (roll 90 at per_node 10 wraps ->
                                                # the scatter drops aliased cards); at the new
                                                # small size the spiral reads as bottlebrush shoot
                                                # phyllotaxis, not the old chaotic big-tuft look
                    leaf_up_bias=0.15,          # slight upsweep of the sprays (live spruce habit)
                    leaf_jitter_deg=10.0,       # +/- deg per-needle azimuth/pitch/twist hash —
                                                # kills the machined repeat
                    leaf_aspect=0.5)

# the foliage:stored split needs the broadleaf sets too: shape = ls_trunk_baked's
# BROADLEAF_SHAPE (LsAnim's defaults, one source); leaf = LsLeavesOnly's own
# defaults (also LsAnim's). Empty dict = "use the split assets' defaults".
BROADLEAF_LEAF = dict()

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


def _make_trunk(shape, seed):
  """foliage:stored split: this variant's TRUNK-ONLY sections mesh (same skeleton
  params + seed as the merged tree => identical wood, gid {0,1} + SectionUnwrap)."""
  class _Trunk(LsTrunkSections):
    def __init__(self, **overrides):
      super().__init__(**{**shape, "seed": seed, **overrides})
  return _Trunk


def _make_leaves(shape, leaf, seed):
  """foliage:stored split: this variant's LEAF-CARDS-ONLY mesh (same skeleton)."""
  class _Leaves(LsLeavesOnly):
    def __init__(self, **overrides):
      super().__init__(**{**shape, **leaf, "seed": seed, **overrides})
  return _Leaves


class ForestScene(Scene):

  def __init__(self):
    super().__init__()

    #postNode = PostFxNodeHSVG()
    #postNode.hue = 0.0
    #postNode.saturation = 0.75
    #postNode.value = 1.0
    #postNode.gamma = 1.0
    #postNode.addToSceneVars(sceneparams,"PostFxChain")
    #self.post_node = postNode

    self.scenegraph(
        preset            = "ForwardPBR",
        skybox_path       = "<ork_envmaps2>/desert4k.xir",
        SkyboxIntensity   = 1.0,
        DiffuseIntensity  = 3.0,
        SpecularIntensity = 1.0,
        AmbientLight      = vec3(0.0),
        CullFrustumScale   = 1.0,   # OWNER DEBUG AID (deliberate): narrow cull frustum so culling
                                     # is VISIBLE at view edges — keep until owner says otherwise
        DepthPrepass       = True,   # an engine invariant; declared explicitly here only because
                                     # this scene once hardcoded False as a stale
                                     # invisibility-bisection toggle.
        msaa = 2,
        ssaa = 0,
        #postfx = [("hsvg", postNode)],  # post-fx chain (list order = apply order); reflected -> round-trips
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
                 spawn            = vec3(-6513.0, 1200.0, 7774.0),   # TEMP repro (revert)
                 chunk            = 128,
                 bake_dimension   = 4096,                
                 render_dimension = 1024,
                 bake_res         = 4096,                
                 walkable         = True,
                 mode             = "stored",   # Phase-1: capture the proctex to <assetcache>/ptex3d_capture/<key>/ (cached)
                 )

    # INSTANCED tree materials (3 gids: bark / branch / leaf). bark + branch are shared by all 16
    # variants; the LEAF material is per-SPECIES + mild per-TYPE hue spread (pure data — 16 Solid
    # constants, zero extra per-pixel ALU). instance_variation = per-instance albedo value jitter
    # off the E.2 scatter seed (grass precedent 0.25) — 150k trees stop being byte-identical.
    # env_specular=0.15 (cotton fix, aug09): attenuates IBL environment specular only — the
    # unclamped grazing-angle sky sheen that whitened whole canopies in the 570-980m band;
    # 0.15 keeps a trace of physical sheen. Bark's value also governs the far band (the
    # impostor billboard draws with the base gid-0 material).
    # impostor=True on ALL gid materials: the far-LOD impostor bake renders each gid bucket (bark
    # trunk / branch / leaf) with ITS material's capture technique into the one atlas, so the billboard is
    # correctly per-region coloured (not a single-material silhouette).
    # LIVE procedural alpine bark (owner mode, aug11: procedural, not stored, not
    # Solid) — the previewer's ladder ("furrow" lower trunk / "smooth" upper wood,
    # bark_alpine.py, object-space, same-tree-same-look) evaluated per-pixel at
    # draw on the merged tree. No bake, no cache in the loop.
    bark = self.asset.Ptex3d(
        "tree_bark", dsl_class=BarkAlpineLive,
        vertex_source=GpuMeshRenderSource(instanced=True),
        impostor=True,
        instance_variation=0.12, env_specular=0.15,
        **BARK_SPECIES["furrow"])
    branch = self.asset.Ptex3d(
        "tree_branch", dsl_class=BarkAlpineLive,
        vertex_source=GpuMeshRenderSource(instanced=True),
        impostor=True,
        instance_variation=0.15, env_specular=0.15,
        **BARK_SPECIES["smooth"])
    # conifer upper wood: same smooth ladder (species language spread awaits ratification)
    conifer_branch = self.asset.Ptex3d(
        "tree_branch_cf", dsl_class=BarkAlpineLive,
        vertex_source=GpuMeshRenderSource(instanced=True),
        impostor=True,
        instance_variation=0.15, env_specular=0.15,
        **BARK_SPECIES["smooth"])
    # foliage:solid — the PRE-CAMPAIGN bark, verbatim: three flat Solid constants, no
    # per-pixel ladder at all. Kept as its own asset names (not a variant of the ones
    # above) so switching sets swaps DRAWABLES rather than re-resolving materials on a
    # live one. impostor=False: this set has no far tier to bake for (see the header).
    bark_solid = self.asset.Ptex3d(
        "tree_bark_solid", dsl_class=Solid,
        vertex_source=GpuMeshRenderSource(instanced=True),
        albedo=hsv(30, 0.36, 0.1),
        instance_variation=0.12, env_specular=0.15)
    branch_solid = self.asset.Ptex3d(
        "tree_branch_solid", dsl_class=Solid,
        vertex_source=GpuMeshRenderSource(instanced=True),
        albedo=hsv(96, 0.46, 0.250), roughness=0.7,
        instance_variation=0.15, env_specular=0.15)
    # conifer upper trunk/boughs: gray-brown wood, not the broadleaf's young green
    conifer_branch_solid = self.asset.Ptex3d(
        "tree_branch_cf_solid", dsl_class=Solid,
        vertex_source=GpuMeshRenderSource(instanced=True),
        albedo=hsv(35, 0.30, 0.14), roughness=0.8,
        instance_variation=0.15, env_specular=0.15)
    # per-type leaf materials: broadleaf greens fan hue 96..113 across the 8 seed-variants;
    # conifer needles are darker + bluer-green (spruce), fanning 132..146.
    # STORED CARDS (owner directive aug09): the leaf/needle silhouettes are BAKED once into two
    # shared card textures (one broadleaf blade, one needle comb — every card in the forest
    # shares the 0-1 UV square, so ONE texture serves all trees of a species) and the materials
    # sample them: per-pixel leaf cost is a texture fetch, zero procedural ALU. The bake derives
    # from the SAME field functions the live LeafProc/NeedleProc classes render (the
    # ptex3d/card_bake.py contract) and is content-hash cached — ANY look edit to the fields
    # re-keys and re-cooks automatically. Tint (per-type) and per-leaf Cd.y brightness stay
    # LIVE; A2C + hard mask + masked depth prepass unchanged.
    # species preset is DATA (leaf.py LEAF_PRESETS): the param set folds into the card's
    # content hash, so each preset is its own baked card. Broadleaf bakes at 512² (card.v2
    # carries vein/serration/reticulation detail 256² would soften); needle comb stays 256².
    _bl_preset  = LEAF_PRESETS["beech"]
    leaf_card   = bake_card(leaf_fields, res=512, params=_bl_preset)   # <assetcache>-token PNG (WARM after first run)
    needle_card = bake_card(needle_fields)
    if STORED_CARDS:   # stored sampler forms (see the gate note at the top of the file)
      _bl = dict(dsl_class=LeafCard,   sampler_textures={"CardTex": leaf_card})
      _cf = dict(dsl_class=NeedleCard, sampler_textures={"CardTex": needle_card})
    else:              # live analytic forms — same look (shared fields + preset), higher per-pixel cost
      _bl = dict(dsl_class=LeafProc, **_bl_preset)
      _cf = dict(dsl_class=NeedleProc)
    leaf_mats = []
    for v in range(NUM_SEEDS):    # species A — broadleaf types 0..7
      leaf_mats.append(self.asset.Ptex3d(
          "tree_leaf_bl%d" % v,
          vertex_source=GpuMeshRenderSource(instanced=True),
          albedo=hsv(96.0 + 2.5 * v, 0.66, 0.252), roughness=0.45, impostor=True,
          env_specular=0.15, **_bl))
    for v in range(NUM_SEEDS):    # species B — conifer types 8..15
      leaf_mats.append(self.asset.Ptex3d(
          "tree_leaf_cf%d" % v,
          vertex_source=GpuMeshRenderSource(instanced=True),
          albedo=hsv(132.0 + 2.0 * v, 0.60, 0.16), roughness=0.7, impostor=True,
          env_specular=0.15, **_cf))

    # foliage:stored materials: one INSTANCED stored sampler (drawn; decodes the
    # baked normal via tbn) + ONE bake-map pair shared by all 16 variants —
    # ren_tree_bark's, verbatim ("furrow" lower / "smooth" upper). The ladder
    # is OBJECT-SPACE (one authored set, bark_alpine.py); instance scale
    # magnifies bark with the tree. Asset names carry the shared bark content
    # key so any bark_alpine.py edit re-keys the 16 bakes.
    #
    # THE SAME BARK AS ren_tree_bark, verbatim (owner ruling aug11 restated:
    # the forest uses the previewer's bark — "furrow" lower trunk, "smooth"
    # upper wood — for EVERY variant of BOTH species. The old per-species
    # language spread (conifers on "plate") is REMOVED: it made half the
    # forest carry a bark the previewer never shows; any species language
    # returns only by explicit ratification. No per-scene overrides — see
    # bark_alpine.py's same-tree-same-look contract.)
    bark_sampler = self.asset.Ptex3d(
        "tree_bark_sampler",
        dsl_class=BarkSectionPBR,
        vertex_source=GpuMeshRenderSource(instanced=True),
        instance_variation=0.12, env_specular=0.15)
    _ck = bark_content_key()
    import sys
    print(f"forest: bark content key {_ck} <- {_bark_mod.__file__}",
          file=sys.stderr)
    bark_lo = self.asset.Ptex3d("bark_alpine_lo_" + _ck, dsl_class=BarkAlpineStored,
                                vertex_source=GpuMeshRenderSource(),
                                **BARK_SPECIES["furrow"])
    bark_hi = self.asset.Ptex3d("bark_alpine_hi_" + _ck, dsl_class=BarkAlpineStored,
                                vertex_source=GpuMeshRenderSource(),
                                **BARK_SPECIES["smooth"])
    _species_shape = [dict(), CONIFER_SHAPE]        # broadleaf = split-asset defaults
    _species_leaf  = [BROADLEAF_LEAF, CONIFER_LEAF]

    # 16 variant meshes — each filters the "trees" scatter to its type_id, one instanced draw.
    # Each is declared once per BARK SET (see the header); only the default set draws.
    tid = 0
    for s, species_cls in enumerate(SPECIES):
      for v in range(NUM_SEEDS):
        seed = 100 + s * 50 + v
        tree = self.asset.Hypermesh("tree%d" % tid, dsl_class=_make_tree(species_cls, seed))
        ##########################################################################
        # foliage:proctex — THE DEFAULT SET (visible at launch). Unchanged from
        # what the scene shipped: one merged drawable, live alpine ladder, far
        # impostor tier. This is the only set that carries billboards.
        ##########################################################################
        self.entity(
          "trees%d" % tid,
          components=[self.declare_component(
              "HypermeshComponent",
              drawabledata=tree.drawable_data(material=bark,
                                              materials={
                                                1: branch if s == 0 else conifer_branch,
                                                2: leaf_mats[tid]},
                                              # DRAWABLE-LEVEL scatter: resolve the "trees" sink filtered
                                              # to this variant's type_id (was instance_source() in the DSL).
                                              instance_source=(TERRAIN, "trees", tid),
                                              # DISTANCE LOD: past 600m the IMPOSTOR billboard
                                              # (baked from the full tree, per-gid coloured,
                                              # MIP-FILTERED — moved in from 1250m aug09: past
                                              # the switch every tree is one pre-filtered quad,
                                              # which both kills the needle-card shimmer at
                                              # distance and cuts the mesh-tier fill/vertex load).
                                              # tile 512 (not the 256 default): the switch moving in to 600m
                                              # doubled the on-screen size of the nearest billboard and 256
                                              # read soft there. COST: the atlas is grid*tile square, so this
                                              # is 4x the atlas bytes — 268MB of mipped RGBA8 per variant,
                                              # x16 variants, plus a transient 4x on the MSAA bake target
                                              # (every variant's bake RTG is allocated up front and freed as
                                              # its one-shot completes). Measured +10.4GB peak process
                                              # footprint on mac; msaa=1 would give back ~8GB of that.
                                              lods={0.0: tree,
                                                    # msaa=1 on the BAKE only (not the scene): all 16
                                                    # variants' bake RTGs alloc up front — measured
                                                    # 23GB transient at bake-msaa 2 / tile 512, equal
                                                    # sharpness at msaa 1 (aug09); at seat msaa 4 the
                                                    # doubled peak + SPVR stereo + forestg terrain blew
                                                    # VRAM on the 32GB 5090 (vkAllocateMemory assert in
                                                    # ImpostorBakeJob::renderInFrame, aug11).
                                                    600.0: tree.imposter(tile=512, msaa=1)},
                                              cull=True,
                                              cull_distance=8000.0,
                                              cull_slabs=4,        # occludee = 4 vertical slabs (trunk..canopy)
                                              cull_tightness=1.0), # shrink boxes 10% (sparse foliage; cull harder)
              layername="std_forward",
              nodename="hm_tree%d" % tid,
              visgroup="foliage:proctex",
              visible=True)])
        ##########################################################################
        # foliage:solid — the SAME merged mesh with flat Solid constants. Hidden at
        # launch, so nothing here is built until the row asks for it.
        ##########################################################################
        self.entity(
          "trees%d_solid" % tid,
          components=[self.declare_component(
              "HypermeshComponent",
              drawabledata=tree.drawable_data(material=bark_solid,
                                              materials={
                                                1: branch_solid if s == 0 else conifer_branch_solid,
                                                2: leaf_mats[tid]},
                                              instance_source=(TERRAIN, "trees", tid),
                                              lods={0.0: tree},   # no far tier — see the header
                                              cull=True,
                                              cull_distance=600.0,
                                              cull_slabs=4,
                                              cull_tightness=1.0),
              layername="std_forward",
              nodename="hm_tree%d_solid" % tid,
              visgroup="foliage:solid",
              visible=False)])
        ##########################################################################
        # foliage:stored — TWO instanced drawables per variant sharing ONE scatter
        # set (the Track 2 split: baked trunk sections + card leaves; leaves never
        # meet xatlas). Hidden at launch, which is also what keeps the 16 section
        # bakes (~400 MB of texture arrays at bake_res 1024) out of a run that
        # never selects this set.
        ##########################################################################
        trunk = self.asset.Hypermesh(
            "tree%d_trunk" % tid, dsl_class=_make_trunk(_species_shape[s], seed))
        crown = self.asset.Hypermesh(
            "tree%d_leaves" % tid, dsl_class=_make_leaves(_species_shape[s], _species_leaf[s], seed))
        self.entity(
          "trees%d_stored_trunk" % tid,
          components=[self.declare_component(
              "HypermeshComponent",
              drawabledata=trunk.drawable_data(
                  material=bark_sampler,                       # drawn stored sampler (instanced)
                  materials={GID_BARK: bark_lo,                # per-gid BAKE MAP —
                             GID_BRANCH: bark_hi},             # ren_tree_bark's, verbatim
                  section_bake=True,
                  bake_res=BARK_BAKE_RES,
                  instance_source=(TERRAIN, "trees", tid),
                  # NO trunk impostor tier: in stored mode the impostor bake captures
                  # through the bake-map materials' FWD_SSBO_CUSTOM_CAPTURE, which is the
                  # SECTION-capture program — a UV-space chart rasterizer emitting the
                  # section target layout (tangent normal / mrao) — so every trunk
                  # billboard drew its bark CHART as a floating square with roughness
                  # misread as metal (the "mirror chunks", proven via ORKID_IMPOSTOR_DUMP:
                  # trunk atlases = chart squares, normal.g==0, mr=(.5,.5,1)). Trunks
                  # draw as mesh to 600 m then cull. Restore the tier only with the engine
                  # fix that routes the stored-mode impostor capture through the DRAWN
                  # sampler material with section arrays bound.
                  lods={0.0: trunk},
                  cull=True,
                  cull_distance=600.0,
                  cull_slabs=4,
                  cull_tightness=1.0),
              layername="std_forward",
              nodename="hm_tree%d_stored_trunk" % tid,
              visgroup="foliage:stored",
              visible=False)])
        self.entity(
          "trees%d_stored_crown" % tid,
          components=[self.declare_component(
              "HypermeshComponent",
              drawabledata=crown.drawable_data(
                  material=leaf_mats[tid],
                  instance_source=(TERRAIN, "trees", tid),
                  lods={0.0: crown},   # no far tier — see the header
                  cull=True,
                  cull_distance=600.0,
                  cull_slabs=4,
                  cull_tightness=1.0),
              layername="std_forward",
              nodename="hm_tree%d_stored_crown" % tid,
              visgroup="foliage:stored",
              visible=False)])
        # NOTE (aug09): a leaf-density MID-LOD tier at 400m was tried and REMOVED — measured
        # NET NEGATIVE (~-14 FPS vs the 2-tier chain): the extra per-variant tier (16 more
        # cull dispatches + 48 more indirect-draw buckets) cost more than its fill savings.
        # The old sides=4/sides=3 forks were dead (computed, never wired) and are gone too.
        tid += 1

    self.projectile_pool(fire=True)   # '/' shoots fireballs (gaze-aimed in VR)

__all__ = ["ForestScene", "Broadleaf", "Conifer", "SPECIES",
           "TERRAIN", "NUM_SEEDS"]
