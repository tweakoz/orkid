#!/usr/bin/env ork.python
###############################################################################
# scn_forestg.py — scn_forest (the procedural-sky forest on the celestial clock)
# WEARING GRASS. Everything the parent scene declares — terrain, 16 tree variants,
# sky ensemble, cloud decks, walker, post chain — is inherited untouched; this file
# adds the two halves of the hybrid grass:
#
#   CARPET  a real forward-PBR ptex3d material (grass_mat) whose geometry comes from
#           a TASK+MESH pass (GrassFieldSource): the task stage decides which
#           camera-relative tiles carry blades and how many, the mesh stage builds
#           each blade. It is lit, shadowed, depth-prepassed and tonemapped like any
#           other surface, and its whole look rides ublk_ptex_params (A8) — the
#           GrassDrawableData below binds those knobs and can rebind them live.
#
#   CLUMPS  two instanced hypermesh tussocks placed by the terrain's "grass_clumps"
#           scatter (lush / dry), i.e. the ordinary tree path with a different mesh.
#
# The terrain DSL is forked to carry the fields both halves read (xxx3_grass: the
# xxx3_trees erosion + forest scatter VERBATIM, plus grass_density / grass_dryness /
# grass_clumps). Forking the DSL re-keys the stored-atlas cache, so the FIRST run of
# this scene re-bakes the 8192^2 terrain atlas once — scn_forest's cache is untouched.
#
#   ork.scene.viewer.py scn_forestg
#
# ENV: every knob scn_forest documents still applies (ORK_FORESTSKY_TOD /
# _VISTA / ...). The carpet itself adds none — it is authored here
# and tuned by rebinding its params — but the sky fork below adds two, for looking at
# grass through atmospheric haze:
#
#   ORK_FORESTG_HAZE=<preset>       replaces the parent's haze declaration:
#                                   "clear" | "hazy_day" | "bladerunner" | "off"
#   ORK_FORESTG_HAZE_DIST=<metres>  extinction e-folding distance, overriding whichever
#                                   declaration is in force ("how far can I see")
###############################################################################

import importlib.util
import os
import sys

from orkengine.core import vec3
from orkengine.lev2 import GrassDrawableData

from ork import path as ork_path
from ork.hypergraph.colors import hsv
from ork.hypergraph.dflow.grass import GrassFieldSource, GrassSurface
from ork.hypergraph.dflow.hypermesh import Archetype, GpuMeshRenderSource, Hypermesh, Wind
from ork.hypergraph.assets.hypermesh.ls_anim import TREE_WIND
from ork.hypergraph.assets.materials.terrain.solid import Solid
from ork.hypergraph.ecs.scene.content.forest import TERRAIN

# The scene this one forks is a SIBLING file (scn_forest is a runnable scene, not
# library content), loaded through ork_path.data so the fork resolves in any
# checkout / worktree — the scn_forest_moonprobe pattern.
_REAL = str(ork_path.data / "scenes" / "scn_forest.py")
_spec = importlib.util.spec_from_file_location("scn_forest_real", _REAL)
_mod = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_mod)
ForestProcSkyScene = _mod.ForestProcSkyScene

TERRAIN_DSL = "xxx3_grass"   # xxx3_trees + the grass fields (assets/terrain/xxx3_grass.py)

###############################################################################
# ONE WIND for everything that sways in this scene. Direction is the trees'
# (TREE_WIND) so the canopy and the ground move with the same weather; amplitude and
# rate are the GRASS ones, and they are deliberately not the trees': hm_wind scales
# its displacement by height above the root, so a tree's 0.015 over ~40 m of trunk is
# a canopy sway of half a meter, while over a knee-high blade it would be 6 mm — a dead
# carpet under a moving forest. Same curve, same clock (RCFD_TIME), same direction.
###############################################################################
GRASS_WIND = dict(amp=0.22, freq=0.9, dir=TREE_WIND["dir"])

###############################################################################
# THE PALETTE + DRY MIX — passed to BOTH halves of the carpet material (the geometry
# source's param defaults and the surface that reads them are the same UBO members;
# stating them once here is what keeps them one number rather than two).
###############################################################################
GRASS_COLOR_A   = (0.17, 0.34, 0.07, 1.0)   # deep shaded green
GRASS_COLOR_B   = (0.47, 0.63, 0.13, 1.0)   # sunlit green (clump hue mixes A<->B)
GRASS_DRY_COLOR = (0.55, 0.47, 0.18, 1.0)   # straw
# ROUND 2: the terrain under a dense carpet is now ITSELF grass-tinted (xxx3_grass's
# fork-local material, grass_tint_lush ~= (0.21,0.34,0.10) at weight <= 0.85), so the
# tone the far blades dissolve into is that tinted floor, not the old grey-beige — the
# round-1 "palette jump" was the carpet fading toward a color the ground didn't have.
# ROUND 3 retune: the capture-alpha fix restored the floor to its AUTHORED albedo —
# the old (0.24,0.35,0.13) matched the accidentally-brightened round-2 atlas, so the
# mid-distance fade ring read as a pale grey-green film over the now-darker ground.
# Value = the measured dense-carpet floor mean of the corrected atlas.
GRASS_GROUND    = (0.15, 0.20, 0.095, 1.0)  # the terrain tone the carpet dissolves into
GRASS_DRY       = (0.30, 0.30, 0.85, 1.0)   # x fraction  y,z dry-mix window  w backlit

###############################################################################
# CARPET SIZING. Structural-vs-parametric: none of this is baked into shader text.
#   field_dim  the terrain channels are resampled to this grid. 1024 matches the
#              terrain's own RENDER mesh resolution (render_dimension), so a blade
#              root reads the same surface the player is standing on rather than a
#              blur of it — the single most visible number here (16 MB of SSBO).
#   tile/grid  128x128 tiles of 4 m = a 512 m square of task workgroups centred on the
#              eye, so the cull radius below fits inside it with room for the fade.
#              SCALE NOTE: this forest's trees are 60-120 m tall over a 32 km terrain and
#              the walker's camera looks along the ground rather than down at it, so the
#              nearest ground IN FRAME is already tens of meters out: a carpet radius
#              sized for a human-scale scene would put its cull ring inside the shot.
#              The radius is set against THIS scene's distances, and the surface fade
#              (below) is set to finish AT the ring so the carpet dissolves into the
#              ground colour instead of ending at an edge.
###############################################################################
GRASS_FIELD_DIM = 1024
GRASS_TILE      = 4.0
GRASS_GRID      = 128
GRASS_LOD0      = 50.0     # full density inside this radius
GRASS_LOD1      = 90.0     # ... and the colour fade toward the ground starts here
GRASS_CULL      = 240.0    # nothing beyond this (< half the tile grid's span)
GRASS_CLUSTERS  = 96       # mesh workgroups per full tile -> 96 * 8 = 768 blades / 16 m^2
# ROUND 2, tussocks: the clump-height seam (GrassClump2.x) is the tool now — whole clumps
# stand tall or low TOGETHER (mix(1-amp, 1+amp, clump_hash)), so per-blade height_var drops
# (0.65 -> 0.35): blade-level noise was fighting the clump read. Owner verdict (round-2
# live pass): the 0.52 base carpet was TOO TALL — the BASE carpet reads shin-height
# (0.32 * (1±0.35) = 0.21..0.43 m) and only the tall CLUMPS reach the knee
# (0.32 * 1.75 ~= 0.56..0.75 m); radius 1.3 m makes a tussock a patch you could step
# around, not a speckle.
GRASS_BLADE     = dict(height=0.32, height_var=0.35, width=0.065, taper=0.60)
GRASS_CLUMP     = dict(radius=1.30, lean=0.50, phase_var=1.0, hue_var=0.40, height_var=0.75)


class Tussock(Hypermesh):
  """A hero grass tuft: a shallow L-system of thin, wide-angled shoots off one base —
  the sparse, real-geometry half of the placement. Deliberately NOT a tree grammar
  (no apical dominance, no leaves, depth 3): a tussock is a spray, and at 40k
  instances every extra generation is a million triangles for something ankle high.
  Authored ~0.5 m tall in object units; the scatter's per-point scale sizes it."""

  def __init__(self, seed=11, depth=3, children=6, seg_len=0.34, base_radius=0.045,
               sides=3, branch_angle=54.0, internodes=1, tropism=-0.03, jitter=0.55,
               apical=0.15, budget=900):
    super().__init__()
    n = self.lsystem(archetype=Archetype.SYMPODIAL,
                     depth=depth, children=children, seg_len=seg_len,
                     base_radius=base_radius, sides=sides, branch_angle=branch_angle,
                     internodes=internodes, tropism=tropism, jitter=jitter,
                     apical=apical, seed=seed, budget=budget)
    self.output(n)


class DryTussock(Tussock):
  """The dry variant — sparser, and it flops: fewer shoots at a wider angle."""

  def __init__(self, **overrides):
    defaults = dict(seed=29, children=5, branch_angle=66.0, seg_len=0.26, tropism=-0.06)
    super().__init__(**{**defaults, **overrides})


class ForestGrassScene(ForestProcSkyScene):

  def terrain(self, name, **kwargs):
    # THE FORK POINT: the content library declares this terrain with the trees DSL;
    # the grass one is that file plus the fields the carpet and the clump scatter
    # read, so swapping it here keeps every other terrain argument (extent, bake
    # dimension, walkability, spawn) exactly as the forest declared it.
    kwargs["dsl_file"] = TERRAIN_DSL
    return super().terrain(name, **kwargs)

  def __init__(self):
    super().__init__()

    ##########################
    # THE CARPET MATERIAL. GrassFieldSource is the geometry (task + mesh stages);
    # GrassSurface is the fragment. They share the ublk_ptex_params members named
    # below, which is why both are constructed from the same constants.
    ##########################

    if True:
      grass_src = GrassFieldSource(
        tile            = GRASS_TILE,
        grid            = GRASS_GRID,
        cull_r          = GRASS_CULL,
        lod0            = GRASS_LOD0,
        lod1            = GRASS_LOD1,
        clusters        = GRASS_CLUSTERS,
        height          = GRASS_BLADE["height"],
        height_var      = GRASS_BLADE["height_var"],
        width           = GRASS_BLADE["width"],
        taper           = GRASS_BLADE["taper"],
        clump_radius    = GRASS_CLUMP["radius"],
        clump_lean      = GRASS_CLUMP["lean"],
        clump_phase_var = GRASS_CLUMP["phase_var"],
        clump_hue_var   = GRASS_CLUMP["hue_var"],
        clump_height_var = GRASS_CLUMP["height_var"],
        color_a         = GRASS_COLOR_A,
        color_b         = GRASS_COLOR_B,
        dry_color       = GRASS_DRY_COLOR,
        dry_fraction    = GRASS_DRY[0],
        dry_fade_start  = GRASS_DRY[1],
        dry_fade_end    = GRASS_DRY[2],
        backlit         = GRASS_DRY[3],
        wind_amp        = GRASS_WIND["amp"],
        wind_freq       = GRASS_WIND["freq"],
        wind_dir        = GRASS_WIND["dir"])

      self.asset.Ptex3d(
        "grass_mat",
        dsl_class     = GrassSurface,
        vertex_source = grass_src,
        color_a       = GRASS_COLOR_A,
        color_b       = GRASS_COLOR_B,
        dry_color     = GRASS_DRY_COLOR,
        dry           = GRASS_DRY,
        ground_color  = GRASS_GROUND,
        # the colour fade runs from deep mid-field to the cull radius, so the last
        # blades are already the ground's colour when the task stage stops emitting them —
        # pushed out (130 m) and softened (0.65) so the mid-field keeps its green instead
        # of greying into the terrain at 90 m.
        fade          = (130.0, GRASS_CULL, 0.65, 0.0),
        surf          = (0.78, 0.50, 3.0, 0.50))

    ##########################
    # THE CARPET ENTITY. SG.component(nodes=) puts the node on the primary forward
    # layer only: skip_auto_dpp keeps the carpet OUT of the depth_prepass layer, which
    # is also the shadow-caster set — so the 128x128 task+mesh grid dispatches ONCE per
    # frame (color) instead of six times (color + prepass + 4 sun cascade bands). Blade
    # shadows at this scale read as noise; the carpet still RECEIVES the cascades in the
    # color pass (receiving samples the atlas, it never depended on caster membership).
    # Everything below is a reflected default the drawable binds into the material at
    # its first GPU update; nothing here re-materializes a shader.
    ##########################

    if True:
      self.entity(
        "grass_carpet",
        components=[self.SG.component(nodes={
            "grass": {
              "skip_auto_dpp": True,
              "drawable": GrassDrawableData(
                hf_asset         = TERRAIN,
                material_asset   = "grass_mat",
                field_dim        = GRASS_FIELD_DIM,
                tile_size        = GRASS_TILE,
                grid_dim         = GRASS_GRID,
                lod0_radius      = GRASS_LOD0,
                lod1_radius      = GRASS_LOD1,
                cull_radius      = GRASS_CULL,
                lod_ceiling      = float(GRASS_CLUSTERS),
                # the baked field is a COVERAGE mask, not a blade count: lifting it here is how
                # a thin-looking carpet is thickened without re-baking the terrain.
                density_scale    = 3.2,
                density_channel  = "grass_density",
                dryness_channel  = "grass_dryness",
                blade_height     = GRASS_BLADE["height"],
                blade_height_var = GRASS_BLADE["height_var"],
                blade_width      = GRASS_BLADE["width"],
                blade_taper      = GRASS_BLADE["taper"],
                clump_radius     = GRASS_CLUMP["radius"],
                clump_lean       = GRASS_CLUMP["lean"],
                clump_phase_var  = GRASS_CLUMP["phase_var"],
                clump_hue_var    = GRASS_CLUMP["hue_var"],
                clump_height_var = GRASS_CLUMP["height_var"],
                color_a          = vec3(*GRASS_COLOR_A[:3]),
                color_b          = vec3(*GRASS_COLOR_B[:3]),
                dry_color        = vec3(*GRASS_DRY_COLOR[:3]),
                dry_fraction     = GRASS_DRY[0],
                dry_fade_start   = GRASS_DRY[1],
                dry_fade_end     = GRASS_DRY[2],
                backlit          = GRASS_DRY[3],
                wind_dir         = vec3(*GRASS_WIND["dir"]),
                wind_amp         = GRASS_WIND["amp"],
                wind_freq        = GRASS_WIND["freq"])},
        })])

    ##########################
    # HERO CLUMPS — the tree path with tussocks: one instanced draw per variant,
    # filtered to its type_id in the terrain's "grass_clumps" scatter. Wind rides the
    # material's vertex source (the same primitive, the same clock as the carpet).
    ##########################

    lush_mtl = self.asset.Ptex3d(
        "grass_tuft_lush", dsl_class=Solid,
        vertex_source=GpuMeshRenderSource(instanced=True, vtx_displace=Wind(**GRASS_WIND)),
        albedo=hsv(96, 0.60, 0.34), roughness=0.85, instance_variation=0.25)
    dry_mtl = self.asset.Ptex3d(
        "grass_tuft_dry", dsl_class=Solid,
        vertex_source=GpuMeshRenderSource(instanced=True, vtx_displace=Wind(**GRASS_WIND)),
        albedo=hsv(52, 0.50, 0.34), roughness=0.9, instance_variation=0.25)

    if True:
      for tid, (cls, mtl) in enumerate(((Tussock, lush_mtl), (DryTussock, dry_mtl))):
        tuft = self.asset.Hypermesh("grass_tuft%d" % tid, dsl_class=cls)
        self.entity(
          "grass_clumps%d" % tid,
          components=[self.declare_component(
              "HypermeshComponent",
              drawabledata=tuft.drawable_data(
                  material=mtl,
                  instance_source=(TERRAIN, "grass_clumps", tid),
                  cull=True,
                  # a tussock is ankle-high: past ~120 m it is under a pixel, and the
                  # carpet has already covered that ground.
                  cull_distance=120.0),
              layername="std_forward",
              # NOT A CASTER. A hypermesh joins the depth_prepass layer by
              # default, and that layer is also the sun-cascade caster set — so
              # these tussocks were paying a depth pass in EVERY band (six
              # dispatches a frame at five bands) to cast ankle-high shadows that
              # read as noise at any distance the cascades reach. The CARPET half
              # has always opted out this way (skip_auto_dpp on its node); this
              # is the same call for the clumps. They still RECEIVE the cascades.
              skip_auto_dpp=True,
              nodename="hm_tuft%d" % tid)])


__all__ = ["ForestGrassScene"]
