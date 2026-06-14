###############################################################################
# 2saddles.py — twin saddles cloned from saddle.py, each with its own
# PbrMaterial color and its own per-drawable HDRI probe override. Acid
# test for PBR2 Phase 0 per-drawable HDRI override (P0.4b).
#
# Saddles keep saddle.py's 12m width (half_extent=6) so col_vdb's
# ring-emitter sizing matches. Spaced at ±8m on x (4m gap edge-to-edge,
# 16m center-to-center).
###############################################################################

from orkengine.core import vec3, vec4, CrcStringProxy, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene, Transform

lev2_pyexdir.addToSysPath()

tokens = CrcStringProxy()

IOR = 1.2
PROBE_H = 7.5

class TwoSaddlesScene(Scene):

  def __init__(self):
    super().__init__()

    ##########################
    # SceneGraph
    ##########################

    SG = self.scenegraph(
        preset             = "ForwardPBR",
        skybox_path        = "<ork_envmaps2>/blender_courtyard.xir",
        SkyboxIntensity    = 1.0,
        DiffuseIntensity   = 1.0,
        SpecularIntensity  = 1.0,
        AmbientLight       = vec3(0.0))

    # parallel_compute=True fans each active slot's graphinst->compute()
    # onto opq::concurrentQueue (one op per slot). With 2 psys's of
    # ~10k particles each, this should approximately halve per-tick CPU
    # cost. Flip to False if anything looks wrong — runs serially then.
    self.system_data("ParticlesGlobalSystem", parallel_compute=True)

    ##########################
    # Shared saddle SDF — same dimensions as saddle.py (half_extent=6,
    # saddle ≈12m wide). Keeps the col_vdb DSL's ring-emitter sizing
    # consistent with saddle.py.
    ##########################

    saddle_sdf = self.asset.ThickSaddleSdf(
        "saddle_sdf",
        half_extent_x = 6.0,
        half_extent_z = 6.0,
        saddle_coef   = 0.25,
        thickness     = 0.8,
        voxel_size    = 0.05)

    ##########################
    # Saddle A — yellow, nebula probe override, x=-2.
    ##########################

    mat_a = self.asset.PbrMaterial(
        "saddle_mat_a",
        base_color = vec4(1, 1, 0.0, 1.0),
        metallic   = 1.0,
        roughness  = 0.0)

    drawable_a = self.asset.VdbGridToDrawable(
        "saddle_drawable_a",
        grid     = saddle_sdf,
        material = mat_a,
        iso      = 0.0)

    self.entity("saddle_a",
      transform=Transform(translation=vec3(-8, 0, 0)),
      publish_xf="saddle_a",
      components=[SG.component(nodes={
        "n": {"drawable": drawable_a},
      })])

    ##########################
    # Saddle B — magenta, arena probe override, x=+2.
    ##########################

    mat_b = self.asset.PbrMaterial(
        "saddle_mat_b",
        base_color = vec4(1, 0, 1, 1.0),
        metallic   = 1.0,
        roughness  = 0.0)

    drawable_b = self.asset.VdbGridToDrawable(
        "saddle_drawable_b",
        grid     = saddle_sdf,
        material = mat_b,
        iso      = 0.0)

    self.entity("saddle_b",
      transform=Transform(translation=vec3(8, 0, 0)),
      publish_xf="saddle_b",
      components=[SG.component(nodes={
        "n": {"drawable": drawable_b},
      })])

    ##########################
    # Reflection Probes — one per saddle/psys pair, declared BEFORE the
    # particle systems so each ParticleSystem(probe=…) ref is in scope.
    # Centered above the saddle at half the height to the particle
    # emitter ring (y=4, midway between saddle y=0 and particles y=8).
    # Autobaked at ork.scene.tojson.py time — the scene is rendered
    # from each probe's position into a cubemap, prefiltered into a
    # .xir under <assetcache>/xirtemp/<entity_name>.xir, which becomes
    # the particle drawable's IBL source. dynamic=False (default) means
    # probes do NOT re-render every frame at runtime; flip to True for
    # live updates.
    ##########################

    probe_a = self.probe("studio_probe_a",
        transform = Transform(translation=vec3(-8, PROBE_H, 0)),
        image_dim = 512,
        dynamic   = True)

    probe_b = self.probe("studio_probe_b",
        transform = Transform(translation=vec3(8, PROBE_H, 0)),
        image_dim = 512,
        dynamic   = True)

    ##########################
    # Particle systems — one per saddle, each tracking its host's
    # transform and emitting from a particle entity above. probe=
    # references the local reflection probe entity; wire_scene_data
    # resolves to <assetcache>/xirtemp/<probe_entity_name>.xir at
    # load time → set as the particle drawable's _envmapOverride.
    # Saddles intentionally have no per-drawable override yet — they
    # use the scene-global skybox (sourced from probe_saddleA via
    # HdriToXir above).
    ##########################

    ptc_a = self.asset.ParticleSystem(
        "ptc_a",
        dsl_file                = "col_vdb",
        # Water-glass droplets — matches ren_saddle.py particle material.
        color                   = vec4(0,0,0, 1.0),
        metallic                = 1.0,
        roughness               = 0.0,
        ior                     = IOR,
        transmission_factor     = 1.25,
        volume_thickness_factor = 1.0,
        attenuation_color       = vec3(0.85, 0.92, 1.0),
        attenuation_distance    = 0.65,
        collision_sdf           = saddle_sdf,
        follow_entity           = "saddle_a0",
        emitter_entity          = "ptc_a0",
        probe                   = probe_a)

    self.entity("particles_a",
      transform=Transform(translation=vec3(-8, 8, 0)),
      publish_xf="ptc_a",
      components=[self.declare_component(
        "ParticlesComponent",
        drawabledata = ptc_a,
        pool_size    = 1,
        duration     = 0.0)])

    ptc_b = self.asset.ParticleSystem(
        "ptc_b",
        dsl_file                = "col_vdb",
        # Water-glass droplets — matches ren_saddle.py particle material.
        color                   = vec4(0,0,0, 1.0),
        metallic                = 1.0,
        roughness               = 0.0,
        ior                     = IOR,
        transmission_factor     = 1.25,
        volume_thickness_factor = 1.0,
        attenuation_color       = vec3(0.85, 1.0, 0.95),
        attenuation_distance    = 0.65,
        collision_sdf           = saddle_sdf,
        follow_entity           = "saddle_b0",
        emitter_entity          = "ptc_b0",
        probe                   = probe_b)

    self.entity("particles_b",
      transform=Transform(translation=vec3(8, 8, 0)),
      publish_xf="ptc_b",
      components=[self.declare_component(
        "ParticlesComponent",
        drawabledata = ptc_b,
        pool_size    = 1,
        duration     = 0.0)])
