###############################################################################
# functions — reusable ptex3d procedural FUNCTIONS (GEOV2 "function assets").
#
# These are building blocks that compose into materials, the same way DSL ops
# (P.voronoi, P.hexgrid, ...) compose. Two flavors live here:
#
#   * pure 2D pattern functions  fn(uv) -> SurfNode   (greyscale / vecN outputs)
#       e.g. carbon_weave(uv) -> vec2(height, warp/weft mask)
#   * projectors / combinators   fn(ctx, ...) -> SurfNode
#       e.g. triplanar(ctx, fn2d) maps any 2D pattern onto an arbitrary 3D shape
#
# A material imports what it needs and maps the greyscale outputs to PBR fields:
#       from ork.hypergraph.ptex3d.functions import triplanar, carbon_weave
#       w = triplanar(ctx, carbon_weave, scale=10.0)   # w.x = height, w.y = mask
#
# Keeping these decoupled means the SAME function feeds many materials and (later)
# many backends — the weave doesn't know whether it lands on a plane or a sphere;
# triplanar doesn't know what pattern it carries.
###############################################################################

from ork.hypergraph.ptex3d import P

_PI = 3.14159265359


# ── projectors ──────────────────────────────────────────────────────────────
def triplanar(ctx, fn2d, *, scale=1.0, sharpness=6.0):
  """Map a 2D pattern fn2d(uv) -> SurfNode onto an ARBITRARY 3D surface with no
  UVs: sample the pattern on the three object-space axis planes (YZ / ZX / XY)
  and blend by the object-space normal. Works uniformly on planes, spheres,
  cubes and irregular meshes; the only seams are the soft normal-weighted blends
  where a face turns past 45 degrees (raise `sharpness` to tighten them).

  `scale` sets the pattern frequency in object units. `fn2d` may return any
  type (float / vecN); the same type comes back, blended."""
  p = ctx.P_object * scale
  n = ctx.N_object
  w = P.pow(P.abs(n), P.vec3(sharpness))       # bias toward the dominant axis (pow needs matching types)
  wsum = w.x + w.y + w.z + 1e-5
  wx = w.x / wsum
  wy = w.y / wsum
  wz = w.z / wsum
  fx = fn2d(P.vec2(p.y, p.z))                  # x-facing -> project onto YZ
  fy = fn2d(P.vec2(p.z, p.x))                  # y-facing -> project onto ZX
  fz = fn2d(P.vec2(p.x, p.y))                  # z-facing -> project onto XY
  return fx * wx + fy * wy + fz * wz


# ── 2D pattern functions ────────────────────────────────────────────────────
def carbon_weave(uv):
  """A 2x2-twill carbon-fiber weave at `uv`. Pure greyscale outputs (no color):
      .x = height  — the woven tow relief in [0,1] (feed bump + roughness)
      .y = over    — 1 where the warp tow is on top, 0 where the weft is
  Each tow is a rounded ridge; the twill phase (over 2 / under 2, shifting one
  per row) gives carbon's characteristic diagonal interlace. Map .x to a bump
  and to roughness (glossy on the tow crowns) and .y to a faint warp/weft tonal
  split for a convincing clear-coated carbon look."""
  fc = P.fract(uv)
  ic = P.floor(uv)
  warp = P.sin(fc.x * _PI)                      # vertical tow ridge (varies across u)
  weft = P.sin(fc.y * _PI)                      # horizontal tow ridge (varies across v)
  phase = P.mod(ic.x - ic.y, 4.0)              # 2x2 twill: diagonal over/under bands
  over = P.step(2.0, phase)                     # 1 = warp crosses over, 0 = weft over
  height = P.mix(weft, warp, over)             # show whichever tow is on top
  return P.vec2(height, over)


__all__ = ["triplanar", "carbon_weave"]
