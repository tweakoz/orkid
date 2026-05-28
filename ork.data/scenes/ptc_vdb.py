###############################################################################
# ptc_vdb.py — funnel mesh + particles colliding with its SDF.
#
# Round-trippable asset chain:
#   HollowFunnelMesh (Python recipe; produces (verts, tris))
#     → MeshSdf (reflected; bakes mesh to a cache .obj and voxelizes
#                via openvdb::tools::meshToLevelSet at build time; the
#                .obj path is stored on MeshSdfGenData and survives JSON)
#     → VdbGridToDrawable (reflected; marching-cubes back to a drawable)
#
# The particles' ColVdbSystem references the SDF by asset name through
# the ParticleSystem(collision_sdf=...) cross-asset kwarg — survives the
# tojson→ecsplay round-trip cleanly.
#
# Usage:
#   ork.scene.viewer.py ptc_vdb
###############################################################################

from orkengine.core import vec3, vec4, CrcStringProxy, lev2_pyexdir
from orkengine.lev2 import ParticlesDrawableData

from ork.ecs.scene import Scene, Transform

lev2_pyexdir.addToSysPath()

tokens = CrcStringProxy()


class ColVdbScene(Scene):

  def __init__(self):
    super().__init__()

    # ----- asset chain (eager build via self.asset.*) -----
    funnel_mesh = self.asset.HollowFunnelMesh(
        "funnel_mesh",
        top_outer=4.2, top_inner=3.6,
        bot_outer=1.2, bot_inner=0.4,
        top_y=3.5,     bot_y=-1.0,
        segments=48)

    # MeshSdf is the reflected, round-trippable cousin of MeshToSdf:
    # at build() time it materializes the input mesh, writes a cache .obj
    # at <assetcache>/meshtemp/funnel_sdf.obj, stores that path on the
    # gendata, and voxelizes via meshToLevelSet. At load time, the .obj
    # is read back from the stored path — no Python mesh recipe required.
    funnel_sdf = self.asset.MeshSdf(
        "funnel_sdf",
        input_mesh = funnel_mesh,
        voxel_size = 0.08,
        half_width = 3.0)

    mat_funnel = self.asset.PbrMaterial(
        "funnel_mat",
        base_color = vec4(0.0, 0.5, 1.0, 1.0),  # warm bronze
        metallic   = 0.0,
        roughness  = 0.7)

    funnel_drawable = self.asset.VdbGridToDrawable(
        "funnel_drawable",
        grid     = funnel_sdf,
        material = mat_funnel,
        iso      = 0.0)

    ptc_system = self.asset.ParticleSystem(
        "particles_dd",
        dsl_file             = "col_vdb",
        collision_sdf        = funnel_sdf,
        color                = vec4(0.1,0.1,0.1,0),
        metallic             = 0.0,
        roughness            = 0.0,
        ior                  = 1.8,
        transmission_factor  = 1.0,
        attenuation_color    = vec3(0.10, 0.30, 0.85),
        attenuation_distance = 1.0,
        probe                = "funnel_probe")

    # ----- scenegraph -----
    SG = self.scenegraph(
        preset             = "ForwardPBR",
        skybox_path        = "<ork_envmaps2>/blender_courtyard.xir",
        SkyboxIntensity    = 1.5,
        DiffuseIntensity   = 2.0,
        SpecularIntensity  = 2.0,
        AmbientLight       = vec3(0.0))

    self.system_data("ParticlesGlobalSystem")

    # Visual entity: render the funnel mesh.
    self.entity("funnel_visual",
      transform=Transform(translation=vec3(0, 0, 0)),
      components=[SG.component(nodes={
        "n": {"drawable": funnel_drawable},
      })])

    # Particle entity: ColVdbSystem with the funnel SDF as collider.
    self.entity("particles",
      transform=Transform(translation=vec3(0, 0, 0)),
      components=[self.declare_component(
        "ParticlesComponent",
        drawabledata = ptc_system,
        pool_size    = 1,
        duration     = 0.0)])

    # Dynamic reflection probe positioned just above the funnel's top
    # lip (top_y=3.5) so particles passing through the mouth pick up
    # local reflections of the funnel interior as they descend.
    self.probe("funnel_probe",
        transform     = Transform(translation=vec3(0, 5.5, 0)),
        output_folder = "/tmp/ecs_probes",
        output_prefix = "funnel_probe",
        image_dim     = 512,
        dynamic       = True)
