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


# ── VOLUMETRIC brick lattice (running bond, no loop -> all-runtime) ──────────
_BRICK_SRC = (
  "float _bk_hash(vec3 p){ return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }\n"
  "vec4 _brick_lattice(vec3 p, vec3 n, vec3 invf, float bond, float zbond) {\n"
  "  float course = floor(p.y);\n"
  "  float par = mod(course, 2.0);            // alternate courses\n"
  "  vec3 q = p;\n"
  "  q.x += par * bond;                        // running-bond X offset (0=stack, .5=running)\n"
  "  q.z += par * zbond;                       // depth-row offset\n"
  "  vec3 cell = floor(q);\n"
  "  vec3 f = fract(q);\n"
  "  float id  = _bk_hash(cell);               // per-brick hash\n"
  "  float cid = _bk_hash(vec3(course, 17.0, 3.0));   // per-course hash\n"
  "  vec3 d = min(f, 1.0 - f) * invf;          // per-axis distance to mortar, OBJECT units\n"
  "  float ax = 0.0; float m = d.x;\n"
  "  if (d.y < m) { ax = 1.0; m = d.y; }\n"
  "  if (d.z < m) { ax = 2.0; m = d.z; }\n"
  "  vec3 av = vec3(0.0); av[int(ax)] = 1.0;   // nearest mortar plane's axis\n"
  "  float g = max(sqrt(max(0.0, 1.0 - dot(av, n)*dot(av, n))), 0.25);  // grazing factor\n"
  "  return vec4(id, m / g, f.y, cid);         // uniform-width mortar dist; .z=brick-local up\n"
  "}\n")


def brick_lattice(p, n, invf, bond, zbond):
  """VOLUMETRIC running-bond brick at object-space point `p` (in brick units —
  caller scales by the per-axis brick frequency). A periodic 3D lattice, so it
  has NO loop and every knob is runtime. `n` = object normal (keeps mortar width
  uniform across grazing angles, like box_partition). `invf` = object units per
  brick per axis (1/freq) so mortar width is uniform on all joints regardless of
  brick aspect. `bond`/`zbond` = per-course offsets (0 = stack, 0.5 = running).
  -> vec4(.x=brick id, .y=mortar dist, .z=brick-local up [0,1], .w=course id)."""
  return P.func("_brick_lattice({0}, {1}, {2}, {3}, {4})", [p, n, invf, bond, zbond],
                rtype="vec4", libsrc=_BRICK_SRC)


# ── veined mineral (marble) — domain-warped turbulence, pure DSL ────────────
def _turb3(p, freq, octaves):
  """A vector turbulence field — 3 decorrelated fbm lobes — used to domain-warp
  a coordinate (bends a 1D banding into sinuous 3D veins)."""
  q = p * freq
  return P.vec3(P.fbm(q,        octaves),
                P.fbm(q + 19.7, octaves),
                P.fbm(q + 43.3, octaves))


def domain_warp(p, amount, freq, octaves):
  """Offset `p` by vector turbulence (3 decorrelated fbm) — bends a regular pattern
  organic. A reusable pre-pass for voronoi / grids / lattices: warp the coordinate
  before sampling. `amount` is in the SAME units as `p` (warp the already-scaled
  pattern-space coordinate so it reads as a fraction of a cell). `octaves` BAKES."""
  return p + _turb3(p, freq, octaves) * amount


def vein_field(p, direction, freq, warp, warp_freq, octaves, sharpness):
  """VOLUMETRIC vein intensity at object-space point `p` (marble / veined stone).
  Warp the point by turbulence, then take a sharpened ridge of a directional sine
  band -> thin sinuous veins running through the volume (no triplanar; continuous
  over any shape). -> scalar in [0,1] (1 = on a vein). Built from DSL ops only, so
  it's free to layer: call twice (coarse + fine, different `direction`/`freq`) and
  combine for a vein network. `octaves` BAKES (the fbm loop bound); the rest are
  free to be runtime params."""
  wp   = p + _turb3(p, warp_freq, octaves) * warp
  band = P.sin(P.dot(wp, direction) * freq)
  return P.pow(1.0 - P.abs(band), sharpness)


# ── crumple (foil / crinkled-metal normal field) — pure DSL ─────────────────
def crumple(p, octaves=6, gain=0.55, lacunarity=2.13, antialias=True):
  """VOLUMETRIC crumple height at object-space point `p` — ridged multi-octave
  turbulence (a tent `1-|2n-1|` per octave gives sharp creases with flatter facets
  between, summed across scales for big folds + fine glints). Feed it to displace()
  and the finite-diff bump turns it into the shattered, faceted normal field that
  reads as crinkled foil. Normalised to ~[0,1]. `octaves` BAKES (the unrolled
  sum); `gain`/`lacunarity` bake too (structural). Generic — the metal tint /
  roughness are the material's job, not this field's.

  ANALYTIC AA: with `antialias`, each octave is faded out as its wavelength drops
  below the pixel footprint (fwidth(p)) — band-limiting the normal field at its
  source so sub-pixel creases stop aliasing the specular (instead of relying only
  on the screen-derivative geometric AA downstream). Distant foil smoothly flattens
  toward mirror, which is the correct prefiltered result. The footprint is constant
  across the bump's finite-diff taps, so the gradient stays consistent."""
  foot = P.length(P.fwidth(p)) if antialias else None
  h = None
  amp = 1.0
  fr  = 1.0
  tot = 0.0
  for _ in range(int(octaves)):
    n     = P.noise(p * fr)
    ridge = 1.0 - P.abs(n * 2.0 - 1.0)          # tent ridge in [0,1]: sharp creases
    w     = amp
    if antialias:
      w   = w * (1.0 - P.smoothstep(0.5, 1.0, foot * fr))   # drop sub-pixel octaves
    term  = ridge * w
    h     = term if h is None else h + term
    tot  += amp
    amp  *= gain
    fr   *= lacunarity
  return h * (1.0 / tot)


# ── wood grain (growth rings + long grain) — pure DSL, volumetric ───────────
class _WoodFields:
  __slots__ = ("ring", "band", "grain", "along", "r")

  def __init__(self, ring, band, grain, along, r):
    self.ring  = ring   # [0,1] sawtooth across each annual ring
    self.band  = band   # earlywood(0) -> latewood(1) gradient
    self.grain = grain  # fine long-grain streak noise [0,1]
    self.along = along  # coordinate along the grain axis
    self.r     = r      # warped radius from the grain axis


def wood_grain(p, axis, ring_freq, ring_warp, warp_freq, octaves,
               grain_freq=45.0, ring_contrast=2.2, along_squash=0.12, antialias=True):
  """VOLUMETRIC wood grain at object-space point `p`. Growth rings are concentric
  cylinders around the grain `axis` (the trunk) — a property of 3-space, so reading
  them on a surface reproduces the real cut pattern (cathedral arches on a flat
  cut, straight lines on a quarter cut) with NO triplanar. The radius is domain-
  warped for organic wavy rings; the long grain is fbm stretched ALONG the axis so
  the streaks follow the grain. `octaves` BAKES (fbm loop bound); the rest are free
  to be runtime params. -> _WoodFields(ring, band, grain, along, r).

  ANALYTIC AA: with `antialias`, the ring band (the fract/modulo — the worst
  aliaser) is faded toward its per-period mean once the ring period drops below a
  pixel (fwidth of the phase), and the long grain toward its mean when sub-pixel.
  The footprint is consistent across the bump's finite-diff taps, so the same fade
  also band-limits the relief. Distant wood smoothly averages to a flat tone."""
  along = P.dot(p, axis)
  perp  = p - axis * along
  r0    = P.length(perp)
  r     = r0 + ring_warp * (P.fbm(p * warp_freq, octaves) - 0.5)   # wavy rings
  phase = r * ring_freq
  ring  = P.fract(phase)                                           # [0,1] per ring
  band  = P.pow(ring, ring_contrast)                              # earlywood -> latewood
  gp    = perp + axis * (along * along_squash)                    # squash along axis
  grain = P.fbm(gp * grain_freq, octaves)                         # long streaks
  if antialias:
    # rings: fade the latewood band toward its per-period mean (1/(k+1)) as the
    # ring period goes sub-pixel — the fract/modulo would otherwise alias hard.
    rfade = 1.0 - P.smoothstep(0.5, 1.0, P.fwidth(phase))
    band  = P.mix(1.0 / (ring_contrast + 1.0), band, rfade)
    # long grain: fade toward its mean (0.5) when the grain frequency is sub-pixel.
    gfade = 1.0 - P.smoothstep(0.5, 1.0, grain_freq * P.length(P.fwidth(gp)))
    grain = P.mix(0.5, grain, gfade)
  return _WoodFields(ring, band, grain, along, r)


# ── animal-skin pattern primitives (volumetric, analytic-AA) ────────────────
class _Spots:
  __slots__ = ("mask", "f1", "cell", "cell2")

  def __init__(self, mask, f1, cell, cell2):
    self.mask, self.f1, self.cell, self.cell2 = mask, f1, cell, cell2


class _Rosette:
  __slots__ = ("ring", "fill", "f1", "cell")

  def __init__(self, ring, fill, f1, cell):
    self.ring, self.fill, self.f1, self.cell = ring, fill, f1, cell


def stripes(p, direction, freq, warp, warp_freq, octaves, antialias=True, duty=0.0):
  """VOLUMETRIC sharp stripes (zebra / tiger). A directional sine band domain-
  warped by turbulence -> irregular branching stripes, thresholded to a hard edge.
  -> [0,1] (the two stripe colours). ANALYTIC AA: the edge is a smoothstep over the
  band's pixel footprint (fwidth), so it never aliases and fades to grey when sub-
  pixel. `duty` shifts the black/white balance. `octaves` BAKES."""
  ph = P.dot(p, direction) * freq + warp * (P.fbm(p * warp_freq, octaves) - 0.5)
  s  = P.sin(ph * 3.14159265) + duty
  if antialias:
    fw = P.fwidth(s) + 0.001
    return P.smoothstep(-fw, fw, s)
  return P.step(0.0, s)


def _maybe_warp(q, warp, warp_freq, octaves):
  """domain_warp(q, ...) unless warp is a literal 0 — so the (3-fbm) turbulence is
  only paid for when a warp is actually requested (a param or non-zero constant)."""
  if isinstance(warp, (int, float)) and warp == 0.0:
    return q
  return domain_warp(q, warp, warp_freq, octaves)


def spots(p, freq, radius=0.32, softness=0.03, warp=0.0, warp_freq=1.0, octaves=3,
          antialias=True):
  """VOLUMETRIC round spots (cheetah). Voronoi disc: 1 inside `radius` of each
  (jittered) cell centre, 0 outside, with an analytic-AA edge. `warp` domain-warps
  the (scale-space) input so the spots read organic, not grid-like. Returns
  _Spots(mask, f1, cell, cell2) — per-spot hashes for size/colour variation."""
  v  = P.voronoi(_maybe_warp(p * freq, warp, warp_freq, octaves))
  w  = (P.fwidth(v.f1) + softness) if antialias else softness
  mask = 1.0 - P.smoothstep(radius - w, radius + w, v.f1)
  return _Spots(mask, v.f1, v.cell, v.cell2)


def rosette(p, freq, ring=0.34, thickness=0.12, break_amount=0.55,
            break_freq=16.0, octaves=3, warp=0.0, warp_freq=1.0, antialias=True):
  """VOLUMETRIC leopard/jaguar rosettes — a BROKEN dark ring (annulus of spots)
  around a centre. Voronoi `f1` gives the radius; an annulus around `ring` is broken
  into spots by a turbulence threshold. `warp` domain-warps the input for organic,
  non-grid placement. Returns _Rosette(ring, fill, f1, cell): `ring` = the dark
  broken-ring intensity [0,1] (AA'd), `fill` = the rosette interior [0,1]. BAKES octaves."""
  v    = P.voronoi(_maybe_warp(p * freq, warp, warp_freq, octaves))
  w    = (P.fwidth(v.f1) + 0.01) if antialias else 0.02
  band = 1.0 - P.smoothstep(0.0, thickness + w, P.abs(v.f1 - ring))   # annulus at `ring`
  brk  = P.smoothstep(0.5 - break_amount * 0.5, 0.5 + break_amount * 0.5,
                      P.fbm(p * break_freq, octaves))                 # break into spots
  fill = 1.0 - P.smoothstep(ring - thickness, ring + w, v.f1)         # interior mask
  return _Rosette(band * brk, fill, v.f1, v.cell)


def cell_lod(coord, lo=0.35, hi=0.85):
  """Sub-pixel LOD for a unit-cell field (voronoi / grid): 0 = cells resolved,
  1 = cells smaller than a pixel. Fade DISCRETE per-cell values (cell-hash colours,
  gaps, relief) toward their mean by this to antialias them — edge-AA (fwidth
  smoothstep) cannot touch per-cell discontinuities. `coord` is the cell-space
  coordinate fed to the voronoi/grid (cells ~1 apart)."""
  return P.smoothstep(lo, hi, P.length(P.fwidth(coord)))


def streaks(p, direction, freq, squash=0.12, octaves=4):
  """Anisotropic noise — fbm of a coordinate COMPRESSED along `direction`, so its
  features stretch into streaks that follow it (grass blades, brushed metal, hair,
  fur). -> [0,1]. `octaves` BAKES (fbm loop bound)."""
  along = P.dot(p, direction)
  perp  = p - direction * along
  q     = perp + direction * (along * squash)
  return P.fbm(q * freq, octaves)


def domain_xf(ctx, coord, name, inp_xf):
  """Transform `coord` (vec3) by a 4x4 affine `inp_xf` (a mtx4) to steer a NOISE DOMAIN (scale
  / rotate / translate the noise frame independently of the surface). The LINEAR cousin of
  domain_warp. The matrix rides as 3 RUNTIME vec4 ctx.params — its top 3 rows, `<name>_r0..r2`
  — so it is a UNIFORM you can rebind/animate WITHOUT recompiling (the GLSL is identical for any
  matrix; only the bound rows change -> shader-cache hit). out.i = dot(row_i, vec4(coord,1)) =
  (M*v)_i. NB ctx.P is WORLD meters, so inp_xf is usually mostly a small SCALE."""
  rp = [ctx.param("%s_r%d" % (name, i), inp_xf.getRow(i)) for i in range(3)]   # 3 vec4 row uniforms
  v  = P.vec4(coord.x, coord.y, coord.z, 1.0)
  return P.vec3(P.dot(rp[0], v), P.dot(rp[1], v), P.dot(rp[2], v))


def fbm_stack(noise_fn, p, octaves):
  """fBm-stack a single-eval noise `noise_fn(coord)->float` over `octaves` (freq x2, amp /2
  per octave), normalized to ~[0,1]. The generic stacker for bases with no built-in octave
  form (e.g. the cellular P.voronoi outputs); P.fbm / P.fbm_aa already stack value noise."""
  acc = None; amp = 1.0; nrm = 0.0; freq = 1.0
  for _ in range(max(1, int(octaves))):
    v = noise_fn(p * freq) * amp
    acc = v if acc is None else acc + v
    nrm += amp; amp *= 0.5; freq *= 2.0
  return acc * (1.0 / nrm)


# ── metric-space 2D fBm (planar-chart noise idiom) ──────────────────────────
def fbm2d(uv, octaves=4, aa=1.0):
  """Value-fBm over a 2D coordinate `uv` (vec2) — the metric-space noise idiom for
  UV-charted surfaces (road aggregate, panel grime, decals). P.fbm / P.fbm_aa are
  vec3-ONLY (3D value noise); this lifts the 2D chart into the z=0 plane, so a planar
  material samples a coherent noise slice WITHOUT hand-padding (and can't accidentally
  hand P.fbm_aa a vec2 -> a GLSL 'no matching overload' compile failure). `aa` is the
  footprint-AA bias (P.fbm_aa); pass aa=None for the plain, non-band-limited P.fbm.
  `octaves` BAKES (fbm loop bound). -> float ~[0,1]."""
  p3 = P.vec3(uv, 0.0)
  return P.fbm(p3, octaves) if aa is None else P.fbm_aa(p3, octaves, aa)


# ── 1D repeating-boundary lattice + periodic dashing ────────────────────────
class _StripeLattice:
  __slots__ = ("boundary", "center", "interior", "cell")

  def __init__(self, boundary, center, interior, cell):
    self.boundary = boundary   # x-distance to the nearest cell DIVIDER (integer of x*count)
    self.center   = center     # x-distance to the nearest cell CENTER (half-integer)
    self.interior = interior   # 1 when that nearest divider is interior; 0 at the two outer edges
    self.cell     = cell       # cell index the point falls in (0 .. count-1)


def stripe_lattice(x, count):
  """Partition a normalized coordinate `x` in [0,1] into `count` equal cells and report,
  in x units, the distance to the nearest cell DIVIDER and cell CENTER, plus an
  interior-divider flag that is 0 at the two outer edges (x=0, x=1). `count` is a RUNTIME
  scalar (a param) — the whole thing is closed-form (no Python loop, nothing baked), so the
  same generated shader serves any count. The generic 1D repeating-boundary primitive:
  road lane dividers, fence pickets, tick marks, ladder rungs. -> _StripeLattice."""
  f        = x * count
  cell     = P.floor(f)
  # distance (in f units) to the nearest integer boundary / half-integer center, /count -> x units
  dbound   = P.abs(P.fract(f + 0.5) - 0.5) / count
  dcent    = P.abs(P.fract(f) - 0.5) / count
  idx      = P.floor(f + 0.5)                       # nearest boundary index (0 .. count)
  interior = P.step(0.5, idx) * (1.0 - P.step(count - 0.5, idx))   # 1 iff 1 <= idx <= count-1
  return _StripeLattice(dbound, dcent, interior, cell)


def dash_mask(v, period, duty=0.5):
  """Periodic dashed on/off mask along a 1D metric coordinate `v` (e.g. arc-length meters):
  1 on the painted dash, 0 in the gap, antialiased over one footprint so the dash edges don't
  crawl. `period` is the full on+off length (v units), `duty` the painted fraction in [0,1].
  The AA metric is fwidth(v) — smooth (v is continuous), so it has no wrap discontinuity."""
  s    = P.fract(v / period) * period               # 0 .. period within one cycle
  aa   = P.fwidth(v) + 1e-4
  edge = duty * period
  on   = P.smoothstep(-aa, aa, s) - P.smoothstep(edge - aa, edge + aa, s)
  return P.saturate(on)


# ── participating media (Beer-Lambert) ─────────────────────────────────────
def beer_transmittance(core, sigma):
  """Beer-Lambert TRANSMITTANCE exp(-core*sigma) through a participating medium:
  the surviving fraction in [0,1] of radiance that entered it. Absorbed fraction
  (opacity / occlusion) is 1 - this.

  `core` is the medium's OPTICAL-DEPTH PROXY at the fragment — for the cloud
  decks, the G channel of the deck texture (CHANNELS.md), 0 at a wispy fringe
  rising toward 1 in a thick core; it carries no unit, so `sigma` (the extinction
  coefficient, a per-site runtime plug) sets how fast depth accumulates. Passing
  sigma per call is deliberate: alpha, body shading and the silver lining each
  read the SAME depth proxy through their own extinction.

  THE ONE TERM (master ruling, jul28 — one cloud transmittance, five consumers:
  captured IBL, the sun disc, the moon, stars via occlusion, direct light and
  ground shadows): every occlusion/capture consumer must obtain cloud
  transmittance HERE rather than re-deriving exp(-...) inline, so a change to the
  extinction model reaches all of them at once."""
  return P.func("exp(-{0})", [core * sigma], rtype="float")


def fractional_occlusion(presence, depth_proxy, sigma):
  """How much a PARTIALLY PRESENT medium hides what is behind it, in [0,1]:

      occlusion = presence * (1 - beer_transmittance(depth_proxy, sigma))

  COVERAGE IS NOT DEPTH — the distinction this function exists to enforce.
  `presence` is the fraction of the fragment the medium actually occupies (the
  edge feather, the distance fade, the rim clamp: everything that says "there is
  less medium HERE"); `depth_proxy` is how thick the medium is WHERE it is. Only
  depth belongs inside the exponential. Folding presence into the optical depth
  instead makes occlusion saturate exponentially while the medium's own radiance
  fades linearly, and the two curves diverge hardest in the middle of a fade —
  which renders as a fade that stops glowing but keeps blocking, i.e. a BLACK
  fringe (regression, jul28: 'cloud fadeouts are black, not transparent').

  Composing linearly instead makes occlusion and radiance fade at the same rate,
  so a fading edge goes TRANSPARENT and the ratio between what a fragment adds
  and what it hides stays constant across the whole fade. The limits are exact
  and are the contract: presence 0 -> 0 (invisible medium hides nothing, whatever
  its depth), presence 1 + thick depth -> 1 (thick medium stays opaque)."""
  return presence * (1.0 - beer_transmittance(depth_proxy, sigma))


# ── tangent-space normal-map bake (section capture) ─────────────────────────
def luminance(c):
  """Perceptual luminance (Rec.601) of a vec3 color -> float. The relief height a
  normal-map bake reads when the surface has no explicit displacement field: albedo
  tone tracks the baked grain, so its gradient is the micro-surface slope."""
  return P.dot(c, P.vec3(0.299, 0.587, 0.114))


def section_normal(ctx, n_world, *, renormalize=True):
  """Encode a WORLD-space shading normal into a TANGENT-SPACE normal-map value (n*0.5+0.5,
  RGBA-packable). The spec contract: project the world normal into the surface's tangent
  frame (transpose(tbn) * n) — the orientation-independent store the forward re-expands via
  its own tbn at sample time. A flat/undisturbed surface (n == the geometric normal tbn[2])
  bakes to (0,0,1) -> (0.5, 0.5, 1.0). ctx.tbn is the world tangent/bitangent/normal basis,
  always threaded into ptex_capture. This authors mip-0; the section-bake C++ mip chain is
  responsible for renormalizing the "SectionNormal" mips."""
  n = P.normalize(n_world) if renormalize else n_world
  nt = P.func("(transpose({0}) * {1})", [ctx.tbn, n], rtype="vec3")   # world -> section tangent frame
  return P.normalize(nt) * 0.5 + 0.5


def world_bump_normal(ctx, height, *, strength=1.0):
  """Build a WORLD-space shading normal by perturbing the geometric normal with a scalar
  `height` relief. In the section CAPTURE each section rasterizes into its own 0-1 UV atlas,
  so screen derivatives (dFdx/dFdy) ARE the tangent-plane slope of the relief; that tangent
  perturbation `(-s*du, -s*dv, 1)` is lifted to world via ctx.tbn (tbn * v = v.x*T + v.y*B + v.z*N).
  The result is the material's world shading normal — the same currency the impostor/world-normal
  bake and forward lighting use. `strength` scales the relief (higher = deeper normal, but keep
  the tangent Z dominant so the packed map stays blue-dominant)."""
  du = P.dFdx(height)
  dv = P.dFdy(height)
  ts = P.normalize(P.vec3(-strength * du, -strength * dv, 1.0))       # tangent-frame micro-normal
  return P.func("({0} * {1})", [ctx.tbn, ts], rtype="vec3")           # tbn * ts -> world


def section_normal_from_albedo(ctx, albedo, *, strength=6.0):
  """Convenience for stored materials with no explicit displacement: bake the tangent-space
  normal-map layer from the material's albedo relief (luminance gradient). Composes
  world_bump_normal (albedo luminance -> world shading normal) with section_normal (world ->
  tangent store). A CONSTANT albedo -> flat (0.5, 0.5, 1.0) (the neutral fallback). `strength`
  amplifies the per-texel luminance slope so the packed normal reads while staying blue-dominant."""
  return section_normal(ctx, world_bump_normal(ctx, luminance(albedo), strength=strength))


__all__ = ["triplanar", "carbon_weave", "panel_split", "greeble", "box_partition",
           "brick_lattice", "domain_warp", "vein_field", "crumple", "wood_grain",
           "stripes", "spots", "rosette", "streaks", "cell_lod", "domain_xf", "fbm_stack",
           "fbm2d", "stripe_lattice", "dash_mask", "beer_transmittance",
           "fractional_occlusion",
           "luminance", "section_normal", "world_bump_normal", "section_normal_from_albedo"]
