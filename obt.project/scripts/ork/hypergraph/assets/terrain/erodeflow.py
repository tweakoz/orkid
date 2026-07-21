###############################################################################
# erodeflow — terraced terrain eroded by flow3d/flow_erode
#   ork.terrain.viewer2.py -d 1024 erodeflow
###############################################################################
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.ptex3d import Ptex3d, P   
from ork.hypergraph.colors import hsv
from orkengine.core import vec3, vec2
###############################################################################
HSCALE = 4000.0
###############################################################################
class Material(Ptex3d):

    SAMPLER_CHANNELS = {
       "FlowMetrics": "flow_metrics",
       "FlowDischarge": "flow_discharge",
       "FillDepth": "fill_depth",
       "Basin": "basin",
       "CenterPit": "center_pit",
       "Normal": "normal"}

    def __init__(self, ctx, height_scale = HSCALE):
      fill_depth_tex = ctx.tex("FillDepth")
      basin_tex = ctx.tex("Basin")
      cpit_tex = ctx.tex("CenterPit")
      p     = ctx.P_object
      p2    = p * P.vec3(0.3,0.03,0.3)
      p3    = p * P.vec3(0.03,0.3,0.03)
      p4    = p * P.vec3(0.5,0.5,0.5)
      p5   = p * 0.13
      hs    = ctx.param("height_scale",  height_scale)
      h01   = P.saturate(ctx.P.y / hs)        # elevation [0,1]
      slope = P.saturate(1.0 - ctx.N.y)       # 0 flat .. 1 vertical (mesh normal)
      noise = P.fbm_aa(p*0.01, octaves=8, aa=0.35)
      noise2 = P.pow(P.fbm_aa(p2, octaves=2, aa=0.35),0.35)
      noise3 = P.pow(P.fbm_aa(p3, octaves=2, aa=0.35),0.25)
      noise4 = P.pow(P.fbm_aa(p4, octaves=2, aa=0.35),0.15)
      noise5 = P.pow(P.fbm_aa(p5, octaves=1, aa=0.35),0.9)
      TNY   = ctx.tex("Normal").y
      AO    = P.pow(1.0-ctx.tex("FlowDischarge").x, 0.5)  # flow discharge -> albedo (darken where flow concentrates)
      C     = ctx.tex("FlowMetrics").xyz
      C     = P.vec3(C.x,P.pow(C.z,1.5),0)  # flow metrics -> albedo (red=high-slope, blue=high-discharge)
      C     = P.mix(P.vec3(1,1,1), C, slope)  # slope -> albedo (brighten where steep)
      C     = C*P.pow(noise,0.25)
      mudf  = self._cracked_mud_fields(ctx,                # full fields (one voronoi for albedo + bump)
                               cell_scale=0.5,
                               base_color=(.50,.33,.19),
                               crack=0.05,
                               warp=1.30,
                               bump_scale=0.025,
                               crack_color=(.05,.035,.022))
      mud = P.pow(mudf["albedo"].x, 0.25)  # cracked mud -> albedo (darken the cracks)
      sels  = self.aa_bands(h01+noise5*0.05, 0.0, 0.095, 0.158, 0.245, 0.345, 0.445, 0.555, 1.0, soft=0.15)
      layers = [
        #  select        color   roughness
        ( sels[0], hsv(0,0,1), 1.0 ),
        ( sels[1], hsv(0,0.55,0.5), noise4 ),
        ( sels[2], hsv(5,0.50,0.5), noise4 ),
        ( sels[3], hsv(10,0.45,0.5), 1.0 ),
        ( sels[4], hsv(15.5,0.40,0.5), 1.0 ),
        ( sels[5], hsv(20,0.35,0.5), 1.0 ),
        ( sels[6], hsv(25,0.0,0.5), 0.75 ),
      ]
      aoo = P.mix(mud,noise2,sels[6])
      aoo = P.mix(aoo,noise3,P.pow(slope,0.5)*(1.0-sels[6]))
      aoo = P.pow(aoo,0.75)
      albedo, rough = self.mix_bands(hsv(0,0,1), 1.0, layers)   # weighted sum (sels sum to 1) -> no base leak
      albedo *= C
      #self.displace(aoo, scale=1.5, parallax_steps=4, depth=0.3)                      # finite-diff analytic bump -> o.normal
      self.surface(albedo=albedo, metallic=0, roughness=rough, ao=AO*aoo)
      # EXPLICIT CAPTURES (terrain mode="stored"): bake the COSTLY composite (strata + flow tint +
      # cracked-mud + the sampled flow/basin channels) into a packed PBR atlas, then reconstruct from it.
      #   base.rgb = albedo, base.a = roughness ;  nrmao.rgb = world normal, nrmao.a = ao
      c_alb = self.capture("base",  albedo,  "xyz")
      c_rgh = self.capture("base",  rough,   "w")
      c_nrm = self.capture("nrmao", ctx.N,   "xyz")
      c_ao  = self.capture("nrmao", AO*aoo,  "w")
      self.surface_stored(
          albedo    = ctx.tex(c_alb),
          metallic  = 0,
          roughness = ctx.tex(c_rgh),
          normal    = ctx.tex(c_nrm),
          ao        = ctx.tex(c_ao))
###############################################################################
class ErodeFlow(HeightField):
    EXTENT_M = 16384.0
    MATERIAL_CLASS  = Material
    MATERIAL_PARAMS = {}

    def __init__(self, origin=vec3(0), iters=32):
        super().__init__()
        # offset the 2D field domain by the world XZ origin -> position-decorrelated terrain
        # (offset is in lattice-cell units, so this reseeds per origin). field axes = world X,Z.
        z = (T.fbm(offset=vec2(origin.x, origin.z), frequency=6.0, octaves=5) * 0.5 + 0.5) * HSCALE
        z = T.basin_fill(z,blend=0.5)
        z = T.terrace(z,step_m=HSCALE/32.0,blend=0.75)   # 32 plateaus over the [0,HSCALE] range
        #############################
        # first erosion pass (low freq features)
        # T.loop (not raw for): the document keeps ONE loop group (editor-collapsible,
        # count editable). filt uses L.i arithmetic; fii*fii (L.i exprs have no pow).
        #############################
        with T.loop(int(iters), z=z) as L:
          fi = L.i/float(iters)
          fii = 1.0 - fi
          filt = 2+((fii*fii)*128)  # smoothstep filter for the blend (optional)
          flow = T.flow3d(L.z)
          zz   = T.flow_erode(L.z, flow.discharge,
                              dt=0.5,           # ~1  (NOT 1000 — past ~1 the relief clamp saturates)
                              k_erode=0.5,      # incision strength        (0.05–0.3)
                              k_deposit=0.05,    # 0 = pure incision; raise to ~0.03 for valley fans
                              m=0.5, n=1.5,     # area / slope exponents    (n>1 sharpens canyons)
                              dep_m=0.5,        # deposition AREA exponent  (~0.5 — was 20.5 !)
                              clamp_frac=1.0,
                              blend = 1.0)   # master per-step amount = fraction of local relief
          zz = T.erode_thermal(zz, iterations=8, blend=1.0)  # smooth the jaggedness from discrete steps (NOT the flow-erode clamp )
          L.z = T.lpf(zz, cutoff=filt, units='meters',blend = 1.0)
        z = L.z
        #############################
        # first filter pass (remove hifreq detail, progressively)
        #############################
        zn = T.normalize(z)                      # [0,1] so the band edges are range-robust
        hi = T.band(zn, 0.45, 1.0, soft=0.05)   # high-elevation mask, open top (~ sels[6])
        bl = T.mix(0.85,0.45,hi)
        z = T.lpf(z, cutoff=256, units='meters', blend=bl)     # FIELD blend -> MaskBlend: smooth only the high band
        z = T.lpf(z, cutoff=128, units='meters', blend=bl)
        z = T.lpf(z, cutoff=64, units='meters',  blend=bl)
        z = T.lpf(z, cutoff=32, units='meters',  blend=bl)
        #############################
        # second erosion pass (add a bit of hifreq detail back in)
        #############################
        iters = 5
        with T.loop(int(iters), z=z) as L:
          fi = L.i/float(iters)
          fii = 1.0 - fi
          filt = 2+((fii*fii)*40)  # smoothstep filter for the blend (optional)
          flow = T.flow3d(L.z)
          zz   = T.flow_erode(L.z, flow.discharge,
                              dt=0.15,           # ~1  (NOT 1000 — past ~1 the relief clamp saturates)
                              k_erode=0.5,      # incision strength        (0.05–0.3)
                              k_deposit=0.05,    # 0 = pure incision; raise to ~0.03 for valley fans
                              m=0.5, n=1.5,     # area / slope exponents    (n>1 sharpens canyons)
                              dep_m=0.5,        # deposition AREA exponent  (~0.5 — was 20.5 !)
                              clamp_frac=1.0,
                              blend = 1.0)   # master per-step amount = fraction of local relief
          L.z = T.lpf(zz, cutoff=filt, units='meters',blend = 1.0)
        z = L.z
        #############################
        # METERS CALIBRATION (natural-units): the raw fbm->terrace->erode chain only
        # occupies ~30% of [0,HSCALE] (multi-octave fbm never reaches its extremes,
        # basin_fill lifts the lows — a 15000 HSCALE baked [9679..14200], relief 4521m
        # on a 9.7km pedestal). Pre-natural-units the bake's flush AUTO-EXPOSED [0,1]
        # x height-scale, so HSCALE read as true relief anyway. Make that contract
        # EXPLICIT: pin the eroded field to [0, HSCALE] — HSCALE == total relief in
        # METERS, floor at 0 (affine rescale; erosion shape untouched). Downstream
        # consumers (fcb min_depth=0.02*HSCALE, the material's h01=y/HSCALE strata
        # bands) now agree with the same meters.
        #############################
        z = T.normalize(z, out_lo=0.0, out_hi=HSCALE)
        #############################
        # gen shader data
        #############################
        zf = T.basin_fill(z,blend=1.0)
        fcb = T.fill_closed_basins(z, min_depth=0.02*HSCALE)   # min_depth is METERS (was normalized)
        flow = T.flow3d(z)
        #############################
        # captures
        #############################
        # EXPLICIT [0,1] for the material samplers (the flush no longer auto-exposes).
        self.capture(T.normalize(flow.discharge), "flow_discharge", cache=True)
        self.capture(flow.metrics,   "flow_metrics",   cache=True)
        self.capture(z, "height", cache=True)
        self.capture(z, "normal", cache=True)
        self.capture(fcb.filled,        "filled",      cache=True)   # mono pour-point elevation
        self.capture(fcb.basin,      "basin",      cache=True)   # RGBA: spill/depth/id/mask
        self.capture(fcb.center_pit, "center_pit", cache=True)   # RGBA: 3D offset to pit + dist
        self.capture(T.normalize(fcb.filled-z), "fill_depth", cache=True)
        # EQUAL-AREA UV RELAXATION (slope-stretch fix): captures "relaxed_uv" + "binormal". In mode='stored'
        # the proctex atlas is rasterized + sampled in this relaxed space, so the deep eroded canyons/cliffs
        # get an equal texel budget (sharper) instead of the planar parameterization starving steep faces.
        # The grid-baked channels above stay PLANAR — the material samples them at planar uv during the bake.
        self.relax_uv(z)
        self.z = z   # final height node — subclasses (e.g. scatter) build masks off it
