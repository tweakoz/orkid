###############################################################################
# scn_hamlet.py — GR-B EXEMPLAR: GRAMMAR BUILDINGS on terrain. Three building
# variants (cottage / longhouse / tower — each the full _building recipe: box ->
# storey extrudes -> roof -> facade-SHELL SDF carve -> re-gid -> merged trims)
# scattered into hamlet clusters by hamletvale's "buildings" sink, whose mask is
# a KEEP-OUT product (steepness x elevation band x village cells). Each variant
# draws ONCE per placed instance in a single indirect call; every building is
# ONE mesh with FIVE gid material buckets, bound here by NAME:
#
#   gid 0 WALL   plaster    gid 1 GLASS  A2C glazing (needs the msaa=2 below)
#   gid 2 DOOR   dark oak   gid 3 ROOF   terracotta   gid 4 TRIM  dressed stone
#
# Walkable: WASD + cursor to wander the hamlet; box proxies ride the scatter so
# walls block. Declaration order = dependency order (terrain bakes the placement
# the building drawables read).
#
#   ork.scene.viewer.py scn_hamlet
#   ork.scene.tojson.py -i scn_hamlet -o /tmp/hamlet.ecs && ork.ecs.player.exe /tmp/hamlet.ecs
###############################################################################

from orkengine.core import vec3, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.dflow.hypermesh import GpuMeshRenderSource
from ork.hypergraph.assets.hypermesh.bld_cottage import Cottage
from ork.hypergraph.assets.hypermesh.bld_longhouse import Longhouse
from ork.hypergraph.assets.hypermesh.bld_tower import Tower
from ork.hypergraph.assets.materials.terrain.solid import Solid
from ork.hypergraph.assets.materials.glass import Glass
from ork.hypergraph.colors import hsv

lev2_pyexdir.addToSysPath()

TERRA = "hamlet_terra"

# scatter type order in hamletvale = declaration index: 0 cottage, 1 longhouse, 2 tower
VARIANTS = [("cottage", Cottage), ("longhouse", Longhouse), ("tower", Tower)]


class HamletScene(Scene):

  def __init__(self):
    super().__init__()

    self.scenegraph(
        preset            = "ForwardPBR",
        skybox_path       = "<ork_envmaps2>/desert4k.xir",
        SkyboxIntensity   = 1.0,
        DiffuseIntensity  = 1.5,
        SpecularIntensity = 1.0,
        AmbientLight      = vec3(0.0),
        msaa              = 2,     # REQUIRED: the glass gid is alpha-to-coverage
        ssaa              = 0)

    self.system_data("HypermeshSystem")

    # TERRAIN FIRST — bakes the relief AND the "buildings" placement the variants
    # read. Spawn above the vale's max relief (~55m) so the walker drops onto grass.
    self.terrain(TERRA,
                 dsl_file         = "hamletvale",
                 spawn            = vec3(0.0, 75.0, 0.0),
                 chunk            = 128,
                 render_dimension = 1024,
                 bake_dimension   = 2048,
                 bake_res         = 2048,
                 walkable         = True)

    # shared INSTANCED gid materials — one set serves all three variants
    def _solid(name, albedo, rough, metallic=0.0, var=0.0):
      return self.asset.Ptex3d(name,
                               dsl_class          = Solid,
                               vertex_source      = GpuMeshRenderSource(instanced=True),
                               albedo             = albedo,
                               metallic           = metallic,
                               roughness          = rough,
                               instance_variation = var)

    wall  = _solid("bld_wall", hsv(38, 0.18, 0.80), 0.90, var=0.15)   # limewash plaster
    door  = _solid("bld_door", hsv(20, 0.55, 0.22), 0.75)             # dark oak
    roof  = _solid("bld_roof", hsv(12, 0.60, 0.45), 0.95, var=0.20)   # terracotta
    trim  = _solid("bld_trim", hsv(45, 0.12, 0.62), 0.85)             # dressed stone
    glass = self.asset.Ptex3d("bld_glass",
                              dsl_class     = Glass,
                              vertex_source = GpuMeshRenderSource(instanced=True))

    # one instanced drawable per variant — mesh once, drawn at every scatter point
    # of its type_id; per-gid buckets route the five materials in the same call.
    for tid, (nm, cls) in enumerate(VARIANTS):
      mesh = self.asset.Hypermesh("bld_" + nm, dsl_class=cls)
      self.entity(
          "hamlet_" + nm,
          components=[self.declare_component(
              "HypermeshComponent",
              drawabledata = mesh.drawable_data(
                  material        = wall,                  # gid 0 (default bucket)
                  materials       = {1: glass, 2: door, 3: roof, 4: trim},
                  cull            = True,                  # per-view GPU frustum cull
                  instance_source = (TERRA, "buildings", tid)),
              layername = "std_forward",
              nodename  = "hm_" + nm)])

    # walls block: ONE compound static from the scatter's per-type box proxies
    self.scatter_collider(TERRA, sink="buildings", friction=0.9, restitution=0.05)


__all__ = ["HamletScene"]
