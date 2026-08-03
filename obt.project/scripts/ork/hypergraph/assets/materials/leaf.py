###############################################################################
# LeafProc — a PROCEDURAL broadleaf material (no texture). Computes the leaf
# silhouette + midrib straight from the card UV0 (u = width, symmetric about
# 0.5; v = length, 0 petiole .. 1 tip), and renders it with ALPHA-TO-COVERAGE
# (two-sided, order-independent — needs an MSAA RtGroup, e.g. viewer --msaa 2).
# Per-leaf brightness varies off Cd.y (the LeafScatter per-leaf hash). The card
# geometry comes from Hypermesh.leaves(); the material owns the alpha source —
# this is the procedural one; a texture-atlas LeafTex is a drop-in sibling.
###############################################################################
from ork.hypergraph.ptex3d import Ptex3d, P
from orkengine.core import vec3


class LeafProc(Ptex3d):
  def __init__(self, ctx, *, albedo=vec3(0.24, 0.52, 0.17), roughness=0.55, metallic=0.0):
    uv   = ctx.uv
    half = P.abs(uv.x - 0.5)                       # 0 at midrib .. 0.5 at the card edge
    v    = uv.y                                    # 0 petiole .. 1 tip
    # OVATE blade: half-width 0 at base+tip, peak skewed toward the base (pow<1).
    hw   = 0.46 * P.sin(P.pow(v, 0.65) * 3.14159265)
    # silhouette alpha: 1 inside the blade, soft edge so A2C dithers the boundary. BANDLIMITED via
    # aa_ramp — the edge never narrows below one pixel footprint (P.fwidth), so distant/grazing leaves
    # don't sparkle: close up it's the crisp 0.05-wide edge, far away it widens to ~1px (and a sub-pixel
    # leaf fades its coverage instead of shimmering).
    alpha = 1.0 - self.aa_ramp(half, hw - 0.025, hw + 0.025)
    # midrib (central vein) darkening — also bandlimited so the thin vein line doesn't crawl at distance.
    midrib = 1.0 - self.aa_ramp(half, 0.0, 0.05)
    base   = albedo * P.mix(0.82, 1.18, ctx.Cd.y)  # per-leaf brightness variation
    col    = P.mix(base, base * 0.55, midrib)       # vein darker
    # alpha_cutout matches the color-pass discard below (0.5) — emits the A3 masked
    # depth prepass so leaf holes neither z-occlude nor cast solid-card shadows.
    self.surface(albedo=col, opacity=alpha, roughness=roughness, metallic=metallic,
                 alpha_to_coverage=True, cull="off", alpha_cutout=0.5)
    # ALPHA MASK: a hard discard cuts the rectangle to the leaf shape EVEN WITHOUT MSAA (A2C only
    # softens the surviving edge when MSAA is on). TWO-SIDED: flip the surface normal on back faces so
    # back-lit leaves shade correctly (cull="off" otherwise reuses the front normal). Injected after
    # `s = ptex_surface(...)`, before lighting.
    self._surf_body_append = (
      "if (s.opacity < 0.5) discard;\n"
      "if (!gl_FrontFacing) s.normal = -s.normal;")


__all__ = ["LeafProc"]
