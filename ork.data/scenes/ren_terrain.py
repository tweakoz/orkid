###############################################################################
# ren_terrain.py — HYPERECS D.5 (+terrain host stage): an ECS scene hosting a baked
# GPU chunked terrain. The terrain DSL runs ONCE here (model B — the compute graph
# embeds in HeightFieldGenData); the bake itself DEFERS to load (cook-cached, C++
# materializer); the chunked render contract (SSBO layout / pull-VS / cull compute)
# is baked into the ptex3d material's generated fxv2 by TerrainChunkVertexSource.
# At load the C++ host bakes + wires + renders with ZERO Python.
#
#   ork.scene.tojson.py -i ren_terrain -o /tmp/terra.ecs
#   ork.ecs.player.exe /tmp/terra.ecs --camdist 220 --camheight 90
###############################################################################

from orkengine.core import vec3, vec4, CrcStringProxy, lev2_pyexdir
from orkengine.lev2 import TerrainChunkDrawableData

from ork.hypergraph.ecs.scene import Scene, Transform
from ork.hypergraph.dflow.terrain.gpu_chunk import TerrainChunkVertexSource
from ork.hypergraph.assets.materials.terrain.solid import Solid

lev2_pyexdir.addToSysPath()

tokens = CrcStringProxy()

# one set of scale numbers, shared by the HF gen AND the material's vertex source
# (the GLSL bakes them at authoring; the manifest carries them for the buffer side).
DIM      = 512
EXTENT_M = 512.0
HEIGHT_M = 60.0
CHUNK    = 128


class TerrainScene(Scene):

  def __init__(self):
    super().__init__()

    ##########################
    # SceneGraph
    ##########################

    SG = self.scenegraph(
        preset             = "ForwardPBR",
        skybox_path        = "<ork_envmaps2>/blender_courtyard.xir",
        SkyboxIntensity    = 2.0,
        DiffuseIntensity   = 1.0,
        SpecularIntensity  = 1.0,
        AmbientLight       = vec3(0.10))

    ##########################
    # Assets — the heightfield graph EMBEDS (bake defers to load, cook-cached);
    # the material carries the chunked-terrain GLSL contract in its generated fxv2.
    ##########################

    terra = self.asset.HeightField(
        "terra",
        dsl_file       = "voronoi",
        dimension      = DIM,
        extent_m       = EXTENT_M,
        height_scale_m = HEIGHT_M)

    terra_mat = self.asset.Ptex3d(
        "terra_mat",
        dsl_class     = Solid,
        vertex_source = TerrainChunkVertexSource(
            dim=DIM, extent_m=EXTENT_M, height_m=HEIGHT_M, chunk=CHUNK),
        albedo        = vec3(0.45, 0.42, 0.35),
        roughness     = 0.9)

    ##########################
    # The terrain entity — the chunk drawable serializes INLINE on the SG node;
    # its asset references resolve BY NAME at load (the C++ wire step).
    ##########################

    self.entity(
        "terrain0",
        components = [SG.component(nodes={
            "terra": {"drawable": TerrainChunkDrawableData(
                hf_asset       = "terra",
                material_asset = "terra_mat",
                chunk          = CHUNK)},
        })])


__all__ = ["TerrainScene"]
