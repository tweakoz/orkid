###############################################################################
# Brick — procedural masonry, mappable onto ANY shape (GEOV2 ptex3d).
#
# VOLUMETRIC: built on brick_lattice — a native 3D running-bond lattice read
# straight off the object-space point, so there is NO triplanar and NO projection
# seams; it wraps any shape (wall, sphere, cube, irregular) as real 3D masonry.
# A brick lattice is periodic (no recursion / no loop), so EVERY knob is a runtime
# ctx.param — which is where the wide range of brick looks comes from:
#
#   bond/zbond   stack <-> running <-> third bond
#   brick_freq   brick size + aspect (vec3: bricks per object unit per axis)
#   mortar       joint width;  mortar_color / mortar_ao  the joint look
#   brick_color / brick_color2 / color_var   per-brick 2-tone range + jitter
#   mottle / mottle_freq         clinker color+roughness texture
#   roughness / rough_var        matte brick <-> glazed
#   height_var                   uneven (raised/sunk) bricks
#   pom_steps (ctor)             flat bump <-> deep parallax relief
#
#   from ork.hypergraph.assets.materials import Brick
#   mat = self.asset.Ptex3d("brick", dsl_class=Brick, bond=0.5)
#   mat = self.asset.Ptex3d("brick", dsl_class=Brick, pom_steps=24)   # deep relief
#
# Bake-time (ctor): mottle_oct (noise octaves), pom_steps (parallax march; 0=bump).
###############################################################################

from orkengine.core import vec3
from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.ptex3d.functions import brick_lattice


class Brick(Ptex3d):
  def __init__(self, ctx, *, mottle_oct=4, pom_steps=0,
               brick_freq=vec3(3.0, 7.0, 3.0), bond=0.5, zbond=0.5,
               mortar=0.012, bump_scale=0.04, relief_depth=0.05,
               brick_color=vec3(0.45, 0.26, 0.25), brick_color2=vec3(0.30, 0.12, 0.10),
               color_var=0.18, mortar_color=vec3(0.62, 0.60, 0.55),
               roughness=0.75, rough_var=0.12, mottle=0.35, mottle_freq=8.0,
               height_var=1.0, mortar_ao=0.6):
    freq  = ctx.param("brick_freq",   brick_freq)    # vec3: bricks per object unit per axis
    bnd   = ctx.param("bond",         bond)          # X course offset (0=stack, .5=running)
    zbnd  = ctx.param("zbond",        zbond)         # depth-row offset
    mortw = ctx.param("mortar",       mortar)        # joint width (object units)
    bumps = ctx.param("bump_scale",   bump_scale)
    depth = ctx.param("relief_depth", relief_depth)  # parallax relief depth
    bc1   = ctx.param("brick_color",  brick_color)
    bc2   = ctx.param("brick_color2", brick_color2)
    cvar  = ctx.param("color_var",    color_var)     # per-brick brightness jitter
    mortc = ctx.param("mortar_color", mortar_color)
    rough = ctx.param("roughness",    roughness)
    rvar  = ctx.param("rough_var",    rough_var)
    mott  = ctx.param("mottle",       mottle)        # clinker color/rough texture amount
    mfreq = ctx.param("mottle_freq",  mottle_freq)
    hvar  = ctx.param("height_var",   height_var)    # uneven brick protrusion
    maoP  = ctx.param("mortar_ao",    mortar_ao)     # cavity AO in the joints

    p     = ctx.P_object * freq
    invf  = 1.0 / freq                               # object units per brick (uniform mortar)
    bk    = brick_lattice(p, ctx.N_object, invf, bnd, zbnd)
    bid   = bk.x                                     # per-brick hash
    bedge = bk.y                                     # mortar distance (grazing-corrected)
    bly   = bk.z                                     # brick-local up [0,1]

    # mortar masks: AA'd for visuals (fwidth), object-space for the bump
    mortarV = P.smoothstep(0.0, mortw + P.fwidth(bedge), bedge)   # 0 in mortar, 1 on brick
    mortarH = P.smoothstep(0.0, mortw, bedge)
    r1 = P.fract(bid * 31.7)
    r2 = P.fract(bid * 91.3)

    mottlev = P.fbm(ctx.P_object * mfreq, mottle_oct)             # clinker noise ~[0,1]

    # per-brick color: 2-tone pick + brightness jitter + clinker mottle + a faint
    # darkening toward the brick base (weathering)
    tone = P.mix(bc1, bc2, r1)
    tone = tone * P.mix(1.0 - cvar, 1.0 + cvar, r2)
    tone = tone * P.mix(1.0 - mott, 1.0 + mott, mottlev)
    tone = tone * P.mix(0.82, 1.0, bly)

    ao   = P.mix(1.0 - maoP, 1.0, mortarV)                        # cavity AO in the joints
    self.surface(
      albedo    = P.mix(mortc, tone, mortarV) * ao,
      metallic  = 0.0,
      # mortar rough; brick per-brick + mottle variation (lower mottle -> glossier)
      roughness = P.mix(0.92, rough + rvar * (r2 - 0.5) + (mottlev - 0.5) * 0.15, mortarV),
    )
    # bricks raised over recessed mortar, each a slightly different height, plus a
    # little surface mottle relief on the brick faces
    brick_h = P.mix(1.0 - hvar, 1.0, r1) + (mottlev - 0.5) * 0.12
    self.displace(mortarH * brick_h, scale=bumps,
                  parallax_steps=pom_steps, depth=depth)


__all__ = ["Brick"]
