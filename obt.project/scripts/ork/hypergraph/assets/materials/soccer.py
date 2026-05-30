###############################################################################
# SoccerBall — procedural soccer ball / football (GEOV2 ptex3d).
#
# The classic truncated-icosahedron: 12 black pentagons + 20 white hexagons, with
# recessed seams. Built as a 32-point Voronoi on the sphere — pentagon centers =
# icosahedron vertices, hexagon centers = dodecahedron vertices. For a surface
# direction we find the nearest of the 32 centers (which face) and the gap to the
# 2nd-nearest (the seam). Bump only (no parallax), matte synthetic-leather PBR.
#
#   from ork.hypergraph.assets.materials import SoccerBall
#   mat = self.asset.Ptex3d("ball", dsl_class=SoccerBall)
#
# Knobs (runtime ctx.param): seam (width), pentagon_color, hexagon_color,
#                            seam_color, bump_scale.
###############################################################################

import math

from orkengine.core import vec3
from ork.hypergraph.ptex3d import Ptex3d, P, rgb


# --- the 32 face centers (computed once, baked into the GLSL const array) -------
def _norm(v):
  l = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
  return (v[0] / l, v[1] / l, v[2] / l)

_PHI = (1.0 + math.sqrt(5.0)) / 2.0

# 12 icosahedron vertices -> PENTAGON centers (first 12 -> classified pentagon)
_ICO = []
for _s1 in (1, -1):
  for _s2 in (1, -1):
    _ICO += [(0, _s1, _s2 * _PHI), (_s1, _s2 * _PHI, 0), (_s1 * _PHI, 0, _s2)]

# 20 hexagon centers = the icosahedron FACE centroids (the true dual). NOTE: the
# standard-coordinate dodecahedron is a DIFFERENT orientation than the dual of the
# standard icosahedron, so using dodeca verts broke the 5-fold symmetry (pentagons
# came out irregular). Compute the real face centroids instead.
def _d2(a, b): return sum((a[i] - b[i]) ** 2 for i in range(3))
_E2 = min(_d2(_ICO[i], _ICO[j]) for i in range(12) for j in range(i + 1, 12))  # edge length^2
def _adj(i, j): return abs(_d2(_ICO[i], _ICO[j]) - _E2) < 1e-6
_FACES = [(i, j, k)
          for i in range(12) for j in range(i + 1, 12) for k in range(j + 1, 12)
          if _adj(i, j) and _adj(i, k) and _adj(j, k)]
assert len(_FACES) == 20
_HEXA = [tuple(_ICO[i][d] + _ICO[j][d] + _ICO[k][d] for d in range(3)) for (i, j, k) in _FACES]

_CENTERS = [_norm(v) for v in _ICO] + [_norm(v) for v in _HEXA]   # 32 (12 penta, 20 hexa)
assert len(_CENTERS) == 32

# nearest-of-32 -> vec2(.x = isPentagon (1/0), .y = wall = dot(best) - dot(2nd)).
# shadlang doesn't take const-array initializers (no `vec3[32](...)`), so the 32
# faces are UNROLLED — each carries its pentagon flag (first 12 = pentagons).
_LINES = []
for _i, _c in enumerate(_CENTERS):
  _pf = "1.0" if _i < 12 else "0.0"
  _LINES.append("  d = dot(n, vec3(%.7f, %.7f, %.7f));"
                " if (d > best) { second = best; best = d; pen = %s; }"
                " else if (d > second) { second = d; }" % (_c[0], _c[1], _c[2], _pf))

_SOCCER_SRC = """
vec2 _soccer(vec3 p) {           // .x = isPentagon, .y = wall (gap to 2nd-nearest)
  vec3 n = normalize(p);
  float best = -2.0, second = -2.0, pen = 0.0, d;
__BODY__
  return vec2(pen, best - second);
}
""".replace("__BODY__", "\n".join(_LINES))


class SoccerBall(Ptex3d):
  def __init__(self, ctx, *, grain_oct=3,
               seam=0.02, bump_scale=0.02,
               grain_freq=25.0, grain_amt=0.10,
               pentagon_color=vec3(0.03, 0.03, 0.03),
               hexagon_color=vec3(0.93, 0.93, 0.93),
               seam_color=vec3(0.02, 0.02, 0.02)):
    seam_w = ctx.param("seam",           seam)
    bumps  = ctx.param("bump_scale",     bump_scale)
    gfreq  = ctx.param("grain_freq",     grain_freq)   # fine leather-grain scale
    gamt   = ctx.param("grain_amt",      grain_amt)    # grain bump strength (tiny)
    penta  = ctx.param("pentagon_color", pentagon_color)
    hexa   = ctx.param("hexagon_color",  hexagon_color)
    seamc  = ctx.param("seam_color",     seam_color)

    soc = P.func("_soccer({0})", [ctx.P_object], rtype="vec2", libsrc=_SOCCER_SRC)
    is_penta = soc.x
    wall     = soc.y                                   # ~0 at a seam, larger mid-face
    seamline = P.smoothstep(0.0, seam_w, wall)         # 0 near seam, 1 on the face

    grain = P.fbm(ctx.P_object * gfreq, grain_oct)     # fine synthetic-leather grain ~[0,1]

    facecol = P.mix(hexa, penta, is_penta)             # white hexagon / black pentagon
    self.surface(
      albedo    = P.mix(seamc, facecol, seamline),
      metallic  = 0.0,                                 # dielectric synthetic leather
      # shinier satin; the grain peaks on the faces read a touch glossier
      roughness = P.mix(0.50, P.mix(0.45, 0.06, grain), seamline),
    )
    # recessed seam grooves + a tiny fine grain on the faces
    panel_h = P.mix(1.0 - gamt, 1.0, grain)
    self.displace(P.mix(0.0, panel_h, seamline), scale=bumps)


__all__ = ["SoccerBall"]
