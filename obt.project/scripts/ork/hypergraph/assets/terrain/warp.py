###############################################################################
# warp — DOMAIN-WARPED terrain HEIGHT. The worleyf1 noise the SHAPE is built from
# samples a RADIALLY ring-displaced XZ domain: each point's XZ is pushed in/out
# ALONG ITS RADIUS by sin(radius), so concentric rings ripple through the cellular
# noise — the geometry gets rings, not just the shading.
#
# Authored as a ptex3d EXPRESSION baked to the height channel via expr_field
# (self.hfbake) — no new C++; the warp is ANALYTIC (closed-form in radius), strictly
# better than an RGBA-image warp for a radial pattern. (T.worleyf1 the GENERATOR has
# no domain-warp input, so we bake the warped worley as a ptex3d voronoi.f1 expr.)
#   ork.terrain.viewer2.py warp
#   ork.terrain.viewer2.py warp -p ring_amp_m=500 -p ring_period_m=400 -p frequency=10
###############################################################################
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.ptex3d import P
from ork.hypergraph.ptex3d.functions import fbm_stack
from ork.hypergraph.assets.materials.terrain import Fbm
from orkengine.core import vec3, mtx4, quat

_TAU      = 6.28318530718
_EXTENT_M = 8192.0

xf = mtx4.composed(
    vec3(0, 0, 0),
    quat(vec3(0, 1, 0), .5),
    0.01)


class Warp(HeightField):
    EXTENT_M       = _EXTENT_M
    HEIGHT_M       = 1000.0
    MATERIAL_CLASS = Fbm
    MATERIAL_PARAMS = {
      "albedo":    vec3(0.25),
      "metallic":  1.0,
      "roughness": 0.0,
      "inp_xf":    xf,
      "octaves":   8,
      "aa":        0.5,
    }

    def __init__(self, frequency=3.0, octaves=2,
                 ring_amp_m=50.0, ring_period_m=250.0, center=(0.0, 0.0)):
        super().__init__()

        def ring_worley(ctx):
            """worleyf1 height on a radially ring-displaced XZ domain:
            warped_xz = xz + radial_dir * amp * sin(radius * 2pi/period). A generator
            (function of world XZ only)."""
            p     = ctx.P_object                              # world pos; XZ in meters (Y unused)
            dx    = p.x - center[0]
            dz    = p.z - center[1]
            r     = P.length(P.vec2(dx, dz)) + 1e-3           # radius (m), guarded from 0
            disp  = ring_amp_m * P.sin(r * (_TAU / ring_period_m))   # in/out push along the radius
            wx    = p.x + (dx / r) * disp                     # warped world XZ (m)
            wz    = p.z + (dz / r) * disp
            coord = P.vec3(wx, 0.0, wz) * (frequency / _EXTENT_M)    # -> noise units (frequency cells/extent)
            return fbm_stack(lambda q: P.voronoi(q).f1, coord, octaves)  # worley F1, ring-warped, fBm-stacked

        self.hfbake(ring_worley, "height")
