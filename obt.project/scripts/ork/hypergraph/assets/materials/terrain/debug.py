###############################################################################
# terrain DEBUG materials — the [M] material-override looks for the C++ player.
#
# Four ptex3d DSL materials, each generating its OWN FWD_SSBO_CUSTOM material (zero
# generator changes). They share the terrain's TerrainChunkVertexSource, so the C++
# terrain drawable can SWAP its material to any of them at runtime (SetTerrainMaterialMode).
#
#   normals   — world-normal visualization (unlit): out = normal*0.5+0.5
#   slope     — steepness by world-normal up-ness (unlit): green(flat) -> red(vertical)
#   white     — flat white albedo through the STANDARD forward lighting (shape/AO read)
#   headlight — eye-light (unlit): a light AT THE CAMERA, intensity = dot(N, dir-to-eye)
#
# Viz semantics are FIXED debug-tool conventions, not artist-tweakable material params.
#
#   from ork.hypergraph.assets.materials.terrain import DebugNormals, DebugSlope, DebugWhite, DebugHeadlight
###############################################################################
from ork.hypergraph.ptex3d import Ptex3d, rgb, P


class DebugNormals(Ptex3d):
  """World-normal RGB visualization (unlit). height_scale accepted-but-unused (the terrain
  asset wrapper forwards it to every terrain material's ctor)."""
  def __init__(self, ctx, *, height_scale=2000.0):
    # standard normal -> RGB encoding; unlit + opaque (blend off) so it reads as solid terrain.
    self.unlit(color=ctx.N * 0.5 + rgb(0.5, 0.5, 0.5), blend="off", cull="front", depth_write=True)


class DebugSlope(Ptex3d):
  """Steepness visualization (unlit): green where flat (normal points up), red where vertical."""
  def __init__(self, ctx, *, height_scale=2000.0):
    up = P.saturate(ctx.N.y)  # 1 = flat (world normal up), 0 = vertical
    self.unlit(color=P.mix(rgb(0.85, 0.15, 0.10), rgb(0.15, 0.55, 0.15), up), blend="off", cull="front", depth_write=True)


class DebugWhite(Ptex3d):
  """Flat white albedo through the STANDARD forward lighting (lighting / AO / shape inspection)."""
  def __init__(self, ctx, *, height_scale=2000.0, roughness=1.0):
    self.surface(albedo=rgb(1, 1, 1), metallic=0.0, roughness=roughness)


class DebugHeadlight(Ptex3d):
  """Eye-light visualization (unlit): a headlight AT THE CAMERA. Intensity is the front-facing
  cosine dot(worldN, dir-to-eye) affine-remapped to [0.15, 1] — bright where the surface faces
  the camera, dimming as it turns away; the 0.15 floor keeps backfacing-ish slopes off pure black.
  Neutral gray-white so the falloff reads as shape, not colour. ctx.eye is the camera world
  position (EyePostion), ctx.P the fragment world position — both valid in the forward technique.
  The floor / base tone are FIXED debug semantics (like the other debug looks), not params."""
  def __init__(self, ctx, *, height_scale=2000.0):
    to_eye = P.normalize(ctx.eye - ctx.P)               # surface -> camera direction
    d      = P.dot(ctx.N, to_eye)                        # facing cosine, [-1,1] (unit vectors)
    # AFFINE remap d[-1,1] -> [0.15,1] (== clamp(d,0.15,1) for the shy-of-floor half, minus the
    # hard knee). NB: P.clamp(d, 0.15, 1.0) here renders the whole terrain BLACK in the terrain
    # FWD_SSBO_CUSTOM unlit path (generated GLSL is correct — clamp(dot,0.15,1.0)*0.92 ∈ [0.138,
    # 0.92] — yet the draw comes out 0; a shadlang/MoltenVK clamp codegen quirk, NOT a value bug).
    # The affine avoids clamp entirely and renders correctly; keep it until that quirk is chased down.
    lin    = d * 0.425 + 0.575                           # -> [0.15, 1.0]
    shade  = lin * lin                                   # pow(lin, 2.0): steeper falloff (mul, not
    self.unlit(color=rgb(0.92, 0.92, 0.92) * shade,      # P.pow — same math, dodges intrinsic quirks
               blend="off", cull="front", depth_write=True)


class DebugRimlight(Ptex3d):
  """Rim-light visualization (unlit): the INVERSE of headlight — the same curve with the
  facing cosine negated, so silhouette/grazing/away-facing surfaces glow and camera-facing
  surfaces go dark. Same affine-then-square shaping (and the same clamp-quirk avoidance)."""
  def __init__(self, ctx, *, height_scale=2000.0):
    to_eye = P.normalize(ctx.eye - ctx.P)
    d      = P.dot(ctx.N, to_eye)
    lin    = d * -0.425 + 0.575                          # inverse: bright away/edge-on -> [0.15,1]
    shade  = lin * lin
    self.unlit(color=rgb(0.92, 0.92, 0.92) * shade,
               blend="off", cull="front", depth_write=True)


__all__ = ["DebugNormals", "DebugSlope", "DebugWhite", "DebugHeadlight", "DebugRimlight"]
