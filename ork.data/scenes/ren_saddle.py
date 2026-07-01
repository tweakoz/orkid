###############################################################################
# saddle.py — Tier 3 Scene with a hyperbolic-paraboloid (saddle) collider.
###############################################################################

from orkengine.core import vec3, vec4, CrcStringProxy, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene, Transform

lev2_pyexdir.addToSysPath()

tokens = CrcStringProxy()


class SaddleScene(Scene):

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
        AmbientLight       = vec3(0.0),
        msaa = 2 )

    self.system_data("ParticlesGlobalSystem") # this ECS has a ParticlesGlobalSystem

    ##########################
    # The Saddle Entity
    ##########################

    mat_saddle = self.asset.PbrMaterial(
        "saddle_mat",
        base_color = vec4(0.5,0.25,0.5, 1.0),  
        metallic   = 0.0,
        roughness  = 0.8,
        clearcoat_factor     = 0.1,
        clearcoat_roughness  = 0.05)

    saddle_sdf = self.asset.ThickSaddleSdf(
        "saddle_sdf",
        half_extent_x = 6.0,
        half_extent_z = 6.0,
        saddle_coef   = 0.25,    # surface = 0.25 * x * z
        thickness     = 0.8,
        voxel_size    = 0.05)

    saddle_drawable = self.asset.VdbGridToDrawable(
        "saddle_drawable",
        flip_windings = False,
        grid     = saddle_sdf,
        material = mat_saddle,
        iso      = 0.0)

    if True:
      self.entity("saddle_collider", # visual entity: render the saddle
        transform=Transform(translation=vec3(0, 0, 0)),
        publish_xf="saddle",       # publish this entity's XF so we can use in Particle System
      )
      self.entity("saddle_visual", # visual entity: render the saddle
        transform=Transform(translation=vec3(0, -0.5, 0)),
        #publish_xf="saddle",       # publish this entity's XF so we can use in Particle System
        components=[SG.component(nodes={
          # layer omitted → SG handle's primary layer (std_forward under
          # ForwardPBR). depth_prepass participation is automatic
          # (NodeDef._skipAutoDepthPrepass defaults false on the C++ side).
          "n": {"drawable": saddle_drawable},
        })])

    ##########################
    # The Particle Entity
    ##########################

    saddle_ptc = self.asset.ParticleSystem(
        "saddle_ptc",
        dsl_file       = "col_vdb",
        # PBR2 Phase 2 — water-glass blobs. IOR=1.33 (water), high
        # transmission, faint cool attenuation. Smooth (roughness=0.05)
        # for sharp refraction; non-metallic dielectric.
        color                   = vec4(0,0,0, 1.0),
        metallic                = 1.0,
        roughness               = 0.0,
        ior                     = 1.23,
        transmission_factor     = 1.25,
        volume_thickness_factor = 1.0,
        attenuation_color       = vec3(0.85, 0.92, 1.0),
        attenuation_distance    = 0.5,
        collision_sdf  = saddle_sdf,
        follow_entity  = "saddle0",
        emitter_entity = "saddle_ptc0")

    self.entity("particles",
      transform=Transform(translation=vec3(0, 8, 0)),
      publish_xf="saddle_ptc",
      components=[self.declare_component(
        "ParticlesComponent",
        # layername omitted → C++ ParticlesGlobalSystem falls back to
        # the SceneGraphSystem's _default_layer (the first layer
        # declared on the SG — std_forward under ForwardPBR).
        drawabledata = saddle_ptc,
        pool_size    = 1,
        duration     = 0.0)])

    ##########################
    # Reflection Probe
    #   Bake in ecsedit via toolbar's Bake Lighting button →
    #   writes /tmp/ecs_probes/saddle_studio_<idx>.png (equirect).
    ##########################

    self.probe("studio_probe",
        transform     = Transform(translation=vec3(0, 4, 0)),
        output_folder = "/tmp/ecs_probes",
        output_prefix = "saddle_studio",
        image_dim     = 512,
        dynamic       = True)
