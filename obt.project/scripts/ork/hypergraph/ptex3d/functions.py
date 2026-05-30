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


# ── panels / greebles (sci-fi hull, tech walls, circuit boards) ─────────────
def _panels_src(levels):
  """GLSL for an IMPLICIT recursive rectangular subdivision. Start in a grid
  cell; `levels` times, split the LONGER side at a hashed position and descend
  into the half holding the point. Result is an irregular panel. `levels` is a
  loop bound -> baked into the function (and its name) per the bake/runtime rule.
  Returns vec4(id, edge, local.x, local.y): a per-panel hash, the distance to the
  nearest panel wall (UV units -> uniform groove width), and panel-local UV."""
  return ("float _pan_hash(vec2 p){ return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }\n"
          "vec4 _ptex_panels_%d(vec2 uv) {\n"
          "  vec2 cmin = floor(uv);\n"
          "  vec2 cmax = cmin + 1.0;\n"
          "  float id = _pan_hash(cmin);\n"
          "  for (int L = 0; L < %d; L++) {\n"
          "    vec2 sz = cmax - cmin;\n"
          "    float ht = 0.35 + 0.30 * _pan_hash(vec2(id*1.7 + 3.0, float(L)));\n"
          "    float sx = step(sz.y, sz.x);                 // 1 = x is the longer side\n"
          "    if (_pan_hash(vec2(id*2.3 + 1.0, float(L))) < 0.25) sx = 1.0 - sx;\n"
          "    if (sx > 0.5) {\n"
          "      float mid = mix(cmin.x, cmax.x, ht);\n"
          "      if (uv.x < mid) { cmax.x = mid; id = _pan_hash(vec2(id + float(L)*0.13, 1.0)); }\n"
          "      else            { cmin.x = mid; id = _pan_hash(vec2(id + float(L)*0.13, 2.0)); }\n"
          "    } else {\n"
          "      float mid = mix(cmin.y, cmax.y, ht);\n"
          "      if (uv.y < mid) { cmax.y = mid; id = _pan_hash(vec2(id + float(L)*0.13, 3.0)); }\n"
          "      else            { cmin.y = mid; id = _pan_hash(vec2(id + float(L)*0.13, 4.0)); }\n"
          "    }\n"
          "  }\n"
          "  vec2 dmin = uv - cmin;\n"
          "  vec2 dmax = cmax - uv;\n"
          "  float edge = min(min(dmin.x, dmax.x), min(dmin.y, dmax.y));\n"
          "  vec2 sz = max(cmax - cmin, vec2(0.0001));\n"
          "  vec2 local = (uv - cmin) / sz;\n"
          "  return vec4(id, edge, local.x, local.y);\n"
          "}\n") % (levels, levels)


def panel_split(uv, levels=6):
  """Irregular rectangular panel plating at `uv` (the hull keystone; also tech
  walls / circuit boards). -> vec4(.x=id, .y=edge-dist, .zw=panel-local uv).
  `levels` BAKES (loop bound) -> more levels = finer panels."""
  return P.func("_ptex_panels_%d({0})" % int(levels), [uv],
                rtype="vec4", libsrc=_panels_src(int(levels)))


_GREEBLE_SRC = (
  "float _grb_hash(vec2 p){ return fract(sin(dot(p, vec2(269.5, 183.3))) * 43758.5453); }\n"
  "float _ptex_greeble(vec2 local, float id, float cover, float grid) {\n"
  "  vec2 g  = local * grid;\n"
  "  vec2 gi = floor(g);\n"
  "  vec2 gf = fract(g);\n"
  "  float raised = step(1.0 - cover, _grb_hash(gi + vec2(id*7.0, id*13.0)));\n"
  "  vec2 d  = min(gf, 1.0 - gf);\n"
  "  float plate = smoothstep(0.04, 0.12, min(d.x, d.y));   // inset sub-plate, beveled\n"
  "  vec2 dl = min(local, 1.0 - local);\n"
  "  float rim = smoothstep(0.02, 0.10, min(dl.x, dl.y));   // keep greebles off the panel rim\n"
  "  return raised * plate * rim;\n"
  "}\n")


def greeble(local, panel_id, cover, grid=4.0):
  """Raised sub-plates inside a panel (greebles). `local`/`panel_id` come from
  panel_split; `cover` in [0,1] is the runtime density dial (0 = clean panels).
  -> height in [0,1]. `grid` (sub-cells per panel side) BAKES into geometry."""
  return P.func("_ptex_greeble({0}, {1}, {2}, %s)" % repr(float(grid)),
                [local, panel_id, cover], rtype="float", libsrc=_GREEBLE_SRC)


# ── VOLUMETRIC partition (native 3D — no triplanar, no projection seams) ─────
def _box_hull_src(coarse, fine):
  """GLSL for an IMPLICIT recursive 3D box subdivision (a spatial BSP). `fine`
  times, split the box's longest axis at a hashed position and descend into the
  half holding the point. Because the partition lives in 3-space, reading it at a
  surface point has NO projection and NO seams — it's continuous over any shape.
  Snapshots the panel state at `coarse` levels, then keeps going to `fine` for
  greebles. -> vec4(coarse_id, coarse_edge, fine_id, fine_edge).

  UNIFORM SEAM WIDTH: a 3D wall-distance read on a surface widens wherever the
  surface grazes that wall (normal ~|| the wall axis — e.g. a sphere's 6 axis
  poles), because the distance changes slowly there. We divide each edge by the
  grazing factor sqrt(1-(axis.n)^2) (the rate the wall-distance changes ALONG the
  surface), which cancels the widening -> constant on-surface seam width. Both
  level counts BAKE (loop bounds)."""
  return ("float _bx_hash(vec3 p){ return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }\n"
          "vec4 _box_hull_%d_%d(vec3 p, vec3 n) {\n"
          "  vec3 cmin = floor(p);\n"
          "  vec3 cmax = cmin + 1.0;\n"
          "  float id = _bx_hash(cmin);\n"
          "  float cid = id;\n"
          "  float cedge = 1.0;\n"
          "  float cax = 0.0;\n"
          "  for (int L = 0; L < %d; L++) {\n"
          "    vec3 sz = cmax - cmin;\n"
          "    float ax = 0.0; float mx = sz.x;\n"
          "    if (sz.y > mx) { ax = 1.0; mx = sz.y; }\n"
          "    if (sz.z > mx) { ax = 2.0; mx = sz.z; }\n"
          "    if (_bx_hash(vec3(id*2.3 + 1.0, float(L), 7.0)) < 0.2) ax = mod(ax + 1.0, 3.0);\n"
          "    int axi = int(ax);\n"
          "    float ht = 0.35 + 0.30 * _bx_hash(vec3(id*1.7 + 3.0, float(L), 1.0));\n"
          "    float mid = mix(cmin[axi], cmax[axi], ht);\n"
          "    if (p[axi] < mid) { cmax[axi] = mid; id = _bx_hash(vec3(id + float(L)*0.13, ax, 1.0)); }\n"
          "    else              { cmin[axi] = mid; id = _bx_hash(vec3(id + float(L)*0.13, ax, 2.0)); }\n"
          "    if (L == %d) {\n"
          "      cid = id;\n"
          "      vec3 a0 = min(p - cmin, cmax - p);\n"
          "      cedge = min(a0.x, min(a0.y, a0.z));\n"
          "      cax = 0.0; float m0 = a0.x;\n"
          "      if (a0.y < m0) { cax = 1.0; m0 = a0.y; }\n"
          "      if (a0.z < m0) { cax = 2.0; m0 = a0.z; }\n"
          "    }\n"
          "  }\n"
          "  vec3 a1 = min(p - cmin, cmax - p);\n"
          "  float fedge = min(a1.x, min(a1.y, a1.z));\n"
          "  float fax = 0.0; float m1 = a1.x;\n"
          "  if (a1.y < m1) { fax = 1.0; m1 = a1.y; }\n"
          "  if (a1.z < m1) { fax = 2.0; m1 = a1.z; }\n"
          "  vec3 cav = vec3(0.0); cav[int(cax)] = 1.0;\n"
          "  vec3 fav = vec3(0.0); fav[int(fax)] = 1.0;\n"
          "  float gc = max(sqrt(max(0.0, 1.0 - dot(cav, n)*dot(cav, n))), 0.22);\n"
          "  float gf = max(sqrt(max(0.0, 1.0 - dot(fav, n)*dot(fav, n))), 0.22);\n"
          "  return vec4(cid, cedge / gc, id, fedge / gf);\n"
          "}\n") % (coarse, fine, fine, coarse - 1)


def box_partition(p, n, levels=4, sublevels=3):
  """VOLUMETRIC panel plating + greebles at object-space point `p` — a native 3D
  recursive box subdivision (no triplanar, no projection seams; continuous over
  any shape). `n` is the object-space surface normal, used only to keep the seam
  width uniform across grazing angles. -> vec4(.x=panel id, .y=panel seam dist,
  .z=greeble-cell id, .w=greeble-cell wall dist). `levels` (panels) and
  `sublevels` (greeble depth) BAKE as loop bounds. Pair with the finite-diff bump
  (already 3D)."""
  c = int(levels)
  f = int(levels) + int(sublevels)
  return P.func("_box_hull_%d_%d({0}, {1})" % (c, f), [p, n], rtype="vec4",
                libsrc=_box_hull_src(c, f))


__all__ = ["triplanar", "carbon_weave", "panel_split", "greeble", "box_partition"]
