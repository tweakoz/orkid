###############################################################################
# scn_roadscene.py — roads ON terrain, as a HYPERECS scene (the render_roadscene
# lane composition, ported to the zero-Python player / one-playback-path).
#
#   * terrain SURFACE = the RoadsHills valley (the SHARED roadshills_height DAG,
#     baked by the terrain-chunk driver), surfaced with the Ground ptex3d material.
#   * road MESH       = build_roads_layout(sinks=False) — the Y-network ribbon on
#     the SAME height (route_spine + road_mesh), dark asphalt with a dashed
#     centreline + edge lines; the junction apron paves with the same material.
#   * road LIFT       = a single scene-wide +Y VS displace (issue #69 workaround —
#     see RoadLift below). A scene-node transform is a NO-OP for the SSBO-sourced
#     compute drawable, so the lift MUST be a VS displace.
#   * lighting        = raking-sun HDRI + IBL (the ECS scene family is IBL-only; a
#     strongly-directional skybox gives the gentle relief its directional read).
#   * nav             = the SHIPPED walk library (terrain_collider + walker); the
#     spawn sits just south of the fork so the third-person follow cam frames the
#     junction from a low oblique (the character faces -Z at spawn — the money view).
#
# The road + terrain + lighting are fully static; the only dynamic element is the
# invisible walk character (the requested nav), whose sub-pixel resting-contact settle
# nudges the follow cam by <1 px — two offscreen snapshots agree everywhere except a
# handful of anti-aliased road-edge pixels (~0.02% of the frame). Visually identical.
#
#   ork.scene.viewer.py scn_roadscene
#   ork.scene.tojson.py -i scn_roadscene -o /tmp/rs.ecs && ork.ecs.player.exe /tmp/rs.ecs
###############################################################################

import os
from orkengine.core import vec3, lev2_pyexdir
from orkengine.lev2 import TerrainChunkDrawableData, HypermeshGenData

from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.dflow.hypermesh import GpuMeshRenderSource, VertexDisplace
from ork.hypergraph.dflow.terrain.gpu_chunk import TerrainChunkVertexSource
from ork.hypergraph.assets.materials.terrain.ground import Ground
from ork.hypergraph.assets.materials.asphalt import Asphalt
from ork.hypergraph.assets.terrain import roadshills

lev2_pyexdir.addToSysPath()


def _envf(name, dflt):
  v = os.environ.get(name)
  return float(v) if v is not None else dflt

# The erox incision in roadshills_height is RESOLUTION-DEPENDENT, so the terrain-chunk
# bake dimension MUST equal the road's routing field_dim (build_roads_layout field_dim=256,
# 8 m/texel) — only then is the surface the chunks render the SAME field the ribbon routes
# on (roadshills consistency oracle O1: byte-identical cross-driver at DIM=256).
DIM      = 256
EXTENT_M = roadshills.EXTENT_M
CHUNK    = 128

# ROAD LIFT — v2.5: the road now GROUNDS via the shoulder skirt (RoadMesh height=), so the
# ribbon no longer needs a big lift to hide a floating silhouette. A SMALL deck lift is kept
# (owner: "its actually ok if road slightly above terrain") so the slightly-proud deck reads
# as a paved band at the grazing follow-cam angle (a flush ribbon foreshortens to a hairline)
# and clears the ~1.3 m deepest cut burial (route_spine holds max_grade below the un-carved
# terrain until the flatten seam lands). CRUCIALLY the lift is now gated per-vertex by the
# shoulder LIFT factor (Cd.y): the deck + shoulder-inner lift, but the shoulder OUTER ring
# stays pinned to the terrain — so grounding survives the lift. RS_LIFT overrides for tuning.
LIFT_M = _envf("RS_LIFT", 1.5)

# Money-view spawn (from the geo/lift probe): the Y-fork sits at ~(-667, 124, -667) with the
# trunk to POI0 (NW) and branches to POI1 (NE) / POI2 (E). The walker spawns facing -Z, so a
# spawn just SOUTH (+Z) of the fork frames the junction + the three radiating roads (yellow
# dashed centreline + white edge lines) in the third-person follow cam — the money view. The
# spawn Y is advisory (the walker ground-snaps down onto the static terrain).
SPAWN_X    = -667.0
SPAWN_Z    = -640.0
SPAWN_Y    =  600.0
CAM_DIST   =  45.0     # follow-cam standoff: frames the fork + branch-starts as readable asphalt
EYE_HEIGHT =  2.0


class RoadLift(VertexDisplace):
  """Per-vertex +Y (world-up) lift of the road deck, applied in the pull VS. A bindable
  ublk_ptex_params member (A8), NOT a scene-node transform (which the FWD_SSBO_CUSTOM
  compute drawable ignores). Gated by the shoulder LIFT factor (vtxcolor.y = Cd.y): the
  deck + shoulder-inner ring lift, but the shoulder OUTER ring (Cd.y=0) stays PINNED to the
  terrain — so the road reads slightly proud yet its skirt still grounds into the ground."""
  def __init__(self, meters):
    self._m = float(meters)

  def params(self):
    return [("RoadLift", "vec4", (0.0, self._m, 0.0, 0.0))]

  def glsl_call(self):
    return "position.y += RoadLift.y * vtxcolor.y;"


class ReadableAsphalt(Asphalt):
  """The R-family Asphalt (dashed centreline, edge lines, aggregate — all inherited) with a
  lighter asphalt_color default so the ribbon reads as a DARK band against the lighter,
  textured Ground at aerial framing. asphalt_color is a bindable ctx.param (A8) — this only
  changes the value's default, not the material's structure."""
  def __init__(self, ctx, **kw):
    kw.setdefault("asphalt_color", vec3(0.085, 0.085, 0.10))
    super().__init__(ctx, **kw)


class RoadScene(Scene):

  def __init__(self):
    super().__init__()

    ##########################
    # SceneGraph — raking-sun HDRI + IBL (the ECS scene family is IBL-only; a strongly
    # directional skybox gives the gentle relief its directional read without an explicit
    # sun, which the zero-Python player has no light-component surface for).
    ##########################

    SG = self.scenegraph(
        preset            = "ForwardPBR",
        skybox_path       = "<ork_envmaps2>/desert4k.xir",
        SkyboxIntensity   = 1.3,
        DiffuseIntensity  = 1.0,
        SpecularIntensity = 0.7,
        AmbientLight      = vec3(0.10),
        msaa              = 2,
        ssaa              = 0 )

    self.system_data("HypermeshSystem")

    ##########################
    # Assets — material BY NAME, mesh/height graphs EMBEDDED (model B)
    ##########################

    # terrain: the RoadsHills valley (shared height + slope-driven scatter WEIGHTS baked but
    # NOT placed — no entity references the "cover" sink), surfaced with Ground.
    terra = self.asset.HeightField(
        "terra",
        dsl_file  = "roadshills",
        dsl_class = "RoadsHills",
        dimension = DIM,
        extent_m  = EXTENT_M)

    terra_mat = self.asset.Ptex3d(
        "terra_mat",
        dsl_class     = Ground,
        vertex_source = TerrainChunkVertexSource(dim=DIM, extent_m=EXTENT_M, chunk=CHUNK),
        roughness     = 0.92)

    # road: the Y-network ribbon alone (sinks=False drops the roadbed/keepout/building-seed
    # sinks — a mesh render wants the ribbon, not the frontage InstanceSet), dark asphalt,
    # VS-lifted. build_roads_layout returns a dflow GraphData; embed it in a HypermeshGenData.
    road_mesh = self.asset.Hypermesh(
        "road_mesh",
        gendata = HypermeshGenData(
            graph      = roadshills.build_roads_layout(with_mesh=True, sinks=False),
            dsl_file   = "roadshills",
            vtx_budget = 1 << 20))

    road_mat = self.asset.Ptex3d(
        "road_mat",
        dsl_class     = ReadableAsphalt,
        vertex_source = GpuMeshRenderSource(vtx_displace=RoadLift(LIFT_M)),
        width_m       = 6.0,
        lane_count    = 2,
        roughness     = 0.92)

    ##########################
    # Entities
    ##########################

    self.entity(
        "terrain0",
        components = [SG.component(nodes={
            "terra": {"drawable": TerrainChunkDrawableData(
                hf_asset       = "terra",
                material_asset = "terra_mat",
                chunk          = CHUNK)},
        })])

    self.entity(
        "road0",
        components = [self.declare_component(
            "HypermeshComponent",
            drawabledata = road_mesh.drawable_data(material=road_mat),
            layername    = "std_forward",
            nodename     = "road_mesh")])

    ##########################
    # Ground-truth navigation — the SHIPPED walk library. The collider collides with the
    # SAME baked height the chunks render; the walker ground-snaps onto it. cam_distance
    # pulls the follow cam back+up (pitch 0.34) for the low-oblique money view; spawn_above>0
    # so the spawn Y is advisory (robust placement — no need to know the surface height).
    ##########################

    self.terrain_collider(terra, friction=1.0, restitution=1.0)
    self.walker(
        spawn         = vec3(SPAWN_X, SPAWN_Y, SPAWN_Z),
        radius        = 0.35,
        height        = 2.3,
        mass          = 80.0,
        move_force    = 6200.0,
        max_speed     = 10.0,
        jump_impulse  = 1260.0,
        brake         = 10.0,
        eye_height    = EYE_HEIGHT,
        cam_distance  = CAM_DIST,
        cam_far       = 9000.0,
        spawn_above   = 0.2,    # minimal fall — snaps onto the ridge and settles fast
        rest_friction = 8.0,    # holds the capsule still on the slope (no creep)
        gravity       = vec3(0.0, -19.8, 0.0))


__all__ = ["RoadScene"]
