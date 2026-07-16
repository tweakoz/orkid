###############################################################################
# ren_scatter.py — the HYPERECS E.2 DEMO: the TYPED INSTANCE EDGE end to end.
# A terrain asset bakes its relief AND places a two-type scatter (the C++ placer,
# at load, cook-cached); two rock hypermesh graphs each carry an instance_source
# (ScatterSource module — scatter_asset + sink + type_id, the portable reference)
# and draw ONCE per placed instance in a single indirect call, per-instance data
# riding the typed attrs SSBO. Everything authored ONCE here; the scene JSON is
# self-contained; the C++ player materializes bake → placement → instancing with
# ZERO Python.
#
#   ork.scene.tojson.py -i ren_scatter -o /tmp/scat.ecs
#   ork.ecs.player.exe /tmp/scat.ecs --camdist 220 --camheight 90
###############################################################################

from orkengine.core import vec3, vec4, CrcStringProxy, lev2_pyexdir
from orkengine.lev2 import TerrainChunkDrawableData, PostFxNodeHeatDistort

from ork.hypergraph.ecs.scene import Scene, Transform
from ork.hypergraph.dflow.hypermesh import (Hypermesh as HypermeshDSL, GpuMeshRenderSource,
                                            S, replace, group)
from ork.hypergraph.dflow.terrain.gpu_chunk import TerrainChunkVertexSource
from ork.hypergraph.assets.materials.terrain.solid import Solid
from ork.hypergraph.assets.materials.terrain.ground import Ground
from ork.hypergraph.colors import hsv
from ork.hypergraph.units import meters

lev2_pyexdir.addToSysPath()

tokens = CrcStringProxy()

# one set of scale numbers, shared by the HF gen AND the material's vertex source
DIM      = 2048     # 2k bake resolution (1 texel = 1m)
EXTENT_M = 2048.0   # 2km x 2km
HEIGHT_M = 175.0    # total relief — navigable rolling hills (see scatterhills.py)
CHUNK    = 128
BALL_R    = 0.25   # projectile radius (collider; visual = instance scale on a ~1m model)
MAX_BALLS = 256    # instanced-node capacity


class Boulder(HypermeshDSL):
  """Valley rock (type 0): a chunky low-poly sphere, placed by the 'rocks' sink.
  subdivisions is parametric so fork(subdivisions=0) yields a coarser distance-LOD mesh."""
  def __init__(self, subdivisions=3):
    super().__init__()
    n = self.icosphere(radius=1.1, subdivisions=subdivisions)
    self.output(self.smooth_normals(n))
    # scatter-free geometry: the bridge moved to drawable_data(instance_source=...)


class Shard(HypermeshDSL):
  """Ridge rock (type 1): a tall faceted spike. E.3: the TIP faces carry
  gid 1 — they draw in their own indirect-draw bucket with the tip material
  (buckets x instancing: instanceCount rides every gid slot's command)."""
  def __init__(self):
    super().__init__()
    n = self.cone(radius=0.7, height=3.2, sides=5)
    # the cone's sides are SINGLE base->apex triangles (centroid y ~ h/3) — no
    # face can be "the tip" until subdivision creates one. Two levels = 16
    # tris/side; the top sub-faces clear the threshold.
    n = self.subdivide(n, level=2)
    n = self.face_normals(n)                       # AFTER topo ops; tags ride from here
    n = self.select(n, S.P.y > 0.6 * 3.2, op=replace(group(0)))
    n = self.assign_gid(n, gid=1, slot=0)          # the spike tip
    self.output(n)
    # scatter-free geometry: the bridge moved to drawable_data(instance_source=...)


class ScatterScene(Scene):

  def __init__(self):
    super().__init__()

    ##########################
    # SceneGraph
    ##########################

    # E2B item D — heat shimmer over the fire trails: the "heat" aux channel
    # (trails render their heat variant into aux_heat additively) + the
    # heat-distortion postfx refracting the frame by the heat gradient.
    heat_fx = PostFxNodeHeatDistort()
    heat_fx.strength = 0.20   # uv-offset gain on the heat-field gradient
    heat_fx.chroma   = 0.20   # slight r/b spread at the shimmer edges

    SG = self.scenegraph(
        preset             = "ForwardPBR",
        skybox_path        = "<ork_envmaps2>/cold4k.xir",
        SkyboxIntensity    = 1.0,
        DiffuseIntensity   = 1.0,
        SpecularIntensity  = 1.0,
        AmbientLight       = vec3(0.0),
        CullFrustumScale   = 0.75,   # TEMP A/B TEST: narrow cull frustum (cull-more) — revert after
        DepthPrepass       = True,   # resolves a single-sample depth (the HZB occlusion source) + early-Z
        msaa = 2,
        ssaa = 0,
        aux_channels       = ["heat"],
        postfx             = [("heatdistort", heat_fx)])

    self.system_data("HypermeshSystem")

    ##########################
    # Assets — DECLARATION ORDER = DEPENDENCY ORDER: the terrain (which PLACES the
    # scatter at materialize) comes FIRST; the rock graphs' ScatterSources read the
    # placed .ogeo when they materialize after it.
    ##########################

    terra = self.asset.HeightField(
        "terra",
        dsl_file       = "scatterhills",
        dimension      = DIM,
        extent_m       = EXTENT_M,
        relief         = meters(HEIGHT_M))

    # GROUND NOISE (10mph optical flow): patch band 64m..4m mixes dirt<->dry-grass,
    # detail band 4m..0.06m modulates brightness — both band-limited (clean at 2km).
    terra_mat = self.asset.Ptex3d(
        "terra_mat",
        dsl_class     = Ground,
        # natural-units era: heights[] is TRUE METERS — the vertex source takes no
        # height scale (HEIGHT_M lives only in the DSL relief above).
        vertex_source = TerrainChunkVertexSource(
            dim=DIM, extent_m=EXTENT_M, chunk=CHUNK),
        albedo_lo     = hsv(200,0.2,0.7),
        albedo_hi     = hsv(200,0.1,0.75),
        detail        = 0.35,
        patch_m       = 64.0,
        detail_m      = 4.0,
        patch_octaves = 4,
        detail_octaves= 8,
        roughness     = 0.92)

    boulder_mat = self.asset.Ptex3d(
        "boulder_mat",
        dsl_class     = Solid,
        vertex_source = GpuMeshRenderSource(instanced=True),
        albedo        = hsv(200,0.35,0.7),
        roughness     = 1.0,
        instance_variation = 0.0,   # E.4: per-instance brightness from seed01 (frg_clr)
        impostor      = True)        # opt-in the FWD_SSBO_CUSTOM_CAPTURE technique (impostor bake source)
    boulder_mat2 = self.asset.Ptex3d(
        "boulder_mat2",
        dsl_class     = Solid,
        vertex_source = GpuMeshRenderSource(instanced=True),
        albedo        = hsv(240,0.3,0.8),
        roughness     = 1.0,
        instance_variation = 0.35)   # E.4: per-instance brightness from seed01 (frg_clr)

    shard_mat = self.asset.Ptex3d(
        "shard_mat",
        dsl_class     = Solid,
        vertex_source = GpuMeshRenderSource(instanced=True),
        albedo        = hsv(205,0.3,1.0),
        metallic      = 1.0,
        roughness     = 1.0)

    # E.3 — the shard TIP material (gid 1): polished metal catching the sun
    shard_tip_mat = self.asset.Ptex3d(
        "shard_tip_mat",
        dsl_class     = Solid,
        vertex_source = GpuMeshRenderSource(instanced=True),
        albedo        = hsv(45, 0.55, 0.95),
        metallic      = 1.0,
        roughness     = 0.0)

    boulders = self.asset.Hypermesh(
        "boulders",
        dsl_class = Boulder)

    shards = self.asset.Hypermesh(
        "shards",
        dsl_class = Shard)

    # E2B — the projectile fire trail (fire + smoke, world-space, buoyant).
    # emitter_entity="@host" binds each spawned ball's OWN live transform,
    # so one shared graphdata serves every projectile in the pool.
    fire = self.asset.ParticleSystem(
        "fire_trail",
        dsl_file       = "fireball",
        emitter_entity = "@host",
        intensity      = 2.0,
        fire_size      = 0.55,
        buoyancy       = 0.25,
        smoke          = True,
        # item E — the emission point light: color + position track the hot
        # particles (PREMA weighting: fire drives it, smoke doesn't).
        emitter_intensity = 15.0,
        emitter_radius    = 20.0)

    ##########################
    # Entities — terrain chunks + one instanced hypermesh node per rock type
    ##########################

    self.entity(
        "terrain0",
        components = [SG.component(nodes={
            "terra": {"drawable": TerrainChunkDrawableData(
                hf_asset       = "terra",
                material_asset = "terra_mat",
                chunk          = CHUNK)},
        })])

    ##########################
    # E.2-walk: physics + the walkable character — TWO library calls. The collider
    # collides with the SAME baked height artifact the chunks render; the walker is
    # an upright capsule driven by host-forwarded input messages (player: W/S move,
    # A/D strafe, cursor L/R turn, cursor U/D camera pitch, SPACE jump, P pause).
    ##########################

    # CONTACT COMBINE IS MULTIPLICATIVE (bullet: r0*r1, f0*f1): the static
    # colliders carry friction/restitution ~1 so each DYNAMIC body's own
    # values read as its effective contact response. The walker keeps its own
    # friction/restitution at 0 -> products stay 0, walk feel unchanged.
    self.terrain_collider( terra,
                           friction=1.0,
                           restitution=1.0 )
    self.scatter_collider( terra,
                           sink="rocks",
                           friction=1.0,
                           restitution=1.0 )   # rocks collide where they draw (per-item proxies)
    # REALISTIC HUMAN: 2.0m total capsule (1.3 cylinder + 2*0.35 caps), eyes at 1.85m
    # above ground = 0.85 above the capsule center; 10 mph = 4.5 m/s walk.
    self.walker(
        spawn        = vec3(0.0, HEIGHT_M + 0.0, 0.0),
        radius       = 0.35,
        height       = 2.3,
        mass         = 80.0,
        move_force   = 6200.0,
        max_speed    = 10.0,    # 20 mph
        jump_impulse = 1260.0,  # ~3.2 m/s takeoff
        brake        = 10.0,   # release -> stop in ~0.3s; no drag while driving
        eye_height   = 0.85,   # eyes ~1.85m above ground (capsule center +0.85)
        cam_distance = 0.0,    # loc 0: first person — pure rotation at the pivot
        cam_far      = 4000.0, # 2km sightlines
        gravity      = vec3(0.0, -19.8, 0.0))

    ##########################
    # Shootable balls — ONE library call (the walker() pattern): instanced
    # node + name-paired physics/visual archetype + dynamic-only spawner with
    # lifetime recycling. The input script owns the shoot policy ('/' queries
    # charctl CameraRay, spawns with velocity + topspin through the handle
    # it fetched once at link — see walk_input_system.py SHOOT_*).
    ##########################
    self.projectile_pool(
        "ball_spawner",
        radius      = BALL_R,
        mass        = 3.0,
        friction    = 0.9,   # grips terrain: topspin converts to forward drive
        restitution = 0.9,   # 90% bounce (static colliders carry 1.0)
        max_count   = MAX_BALLS,
        lifetime    = 16.0,   # recycle: despawn frees+zeroes the instance slot
        trail       = fire,  # E2B: per-ball world-space fire+smoke (@host)
        trail_delay = 0.20)  # ignite ~3.6m downrange — not in the shooter's face

    self.entity(
        "boulders0",
        components = [self.declare_component(
            "HypermeshComponent",
            drawabledata = boulders.drawable_data(material=boulder_mat, cull=True,
                                                  instance_source=("terra", "rocks", 0),
                                                  # DISTANCE LOD: lod0 = full boulder (near); lod1 = coarse
                                                  # (subdiv 0) boulder past 100m (its own material); lod2 =
                                                  # the IMPOSTOR — past 250m the cull routes the band to a
                                                  # baked hemi-oct billboard per instance (boulder_mat carries
                                                  # impostor=True -> the capture technique). One shared scatter
                                                  # routes to all three tiers; .imposter() rides the coarse
                                                  # fork as its bake-unavailable fallback mesh.
                                                  lods={0.0: boulders,
                                                        150.0: boulders.fork(subdivisions=1),
                                                        300.0: boulders.fork(subdivisions=0).imposter()}),
            layername    = "std_forward",
            nodename     = "hm_boulders")])

    self.entity(
        "shards0",
        components = [self.declare_component(
            "HypermeshComponent",
            drawabledata = shards.drawable_data(
                material        = shard_mat,
                materials       = {1: shard_tip_mat},    # E.3: per-gid bucket draw
                cull            = True,                  # E.4: per-view GPU frustum cull
                instance_source = ("terra", "rocks", 1)),
            layername    = "std_forward",
            nodename     = "hm_shards")])


__all__ = ["ScatterScene"]
