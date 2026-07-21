###############################################################################
# Erox — fbm terrain run through PHYSICAL (Mei virtual-pipes) hydraulic erosion.
# Every knob is in METERS / SECONDS, so the bake is RESOLUTION-INDEPENDENT: the
# SAME graph baked at dim 512 / 1024 / 2048 converges in large-scale drainage
# structure, differing only below the grid feature size. dt + iteration count are
# derived per-bake from cell_size_m = extent_m/dim via a CFL bound — you author a
# physical sim_time_s, not an iteration count. Deterministic Eulerian field solver
# (the Houdini-grade primary; the texel droplet stays as the fast/stylized tool).
#   ork.terrain.view.py erox --dim 512
#   ork.terrain.view.py erox --dim 512 --extent 4096 --height-scale 1024
#   ork.terrain.view.py erox -p sim_time_s=120 -p erosion_rate_per_s=6
###############################################################################
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
###############################################################################
LERP = 1.0
# authored vertical relief in meters (natural units; heights are TRUE METERS end-to-end).
AMPLITUDE_M = 4000.0
# AUTHORED erosion exaggeration (replaces the old per-op `exaggerated_height_m` plug):
# the thermal op carves at ERO_EXAG x the true relief via a visible remap (scale up
# before, scale back after). Erosion is nonlinear in slope, so this authors strong
# carving at gentle true relief. (Old EROSION_HEIGHT_M=164000 at a 4000 m world -> 41x.)
ERO_EXAG = 41.0
###############################################################################
P1_FBM = T.ParamPack(
    frequency=4.2, 
    octaves=3   
)
P2_FBM = T.ParamPack(
    frequency=9.2, 
    octaves=8   
)
###############################################################################
P1_THERM = T.ParamPack(
    talus_deg=16.0,
    rate=0.15,
    iterations=50 )
P2_THERM = T.ParamPack(
    talus_deg=16.0,
    rate=0.10,
    iterations=170 )
###############################################################################
P1_EROX = T.ParamPack(
    sim_time_s=0.5,
    rain_mps=0.06,
    evaporation_per_s=0.05,
    flow_speed_max_mps=22.0,
    capacity_Kc=2.5,
    erosion_rate_per_s=1.0,
    deposition_rate_per_s=1.0,
    creep_m2ps=16.0 )
P2_EROX = T.ParamPack(
    sim_time_s=3.5,
    rain_mps=0.06,
    evaporation_per_s=0.05,
    flow_speed_max_mps=22.0,
    capacity_Kc=5.5,
    erosion_rate_per_s=3.0,
    deposition_rate_per_s=1.0,
    creep_m2ps=16.0 )
###############################################################################
P1_PHA = T.ParamPack(
    strength=0.01,
    gully_weight=1.0,
    detail=0.5,
    scale=0.125,
    cell_scale=2.7,
    normalization=0.5,
    lacunarity=2.0,
    gain=0.5,
    default_height=0.5*AMPLITUDE_M,   # mid-height reference is a VALUE on the height axis (meters)
    octaves=1 )
P2_PHA = T.ParamPack(
    strength=0.17,
    gully_weight=3.0,
    detail=0.6,
    scale=0.0125,
    cell_scale=2.7,
    normalization=0.5,
    lacunarity=2.0,
    gain=0.5,
    default_height=0.5*AMPLITUDE_M,   # mid-height reference is a VALUE on the height axis (meters)
    octaves=1 )
###############################################################################

class XXX(HeightField):
    # ---- authored PHYSICAL world scale (read by the viewer / asset / segmentation) ----
    # Heights are TRUE METERS (natural units). The erosion exaggeration is authored above
    # as ERO_EXAG (a visible remap around the thermal op), not a hidden per-op plug.
    EXTENT_M = 32768.0    # XZ meters the heightfield spans (centered at origin)

    def __init__(self,iters = 64):
        ####################################
        super().__init__()
        ero_out = T.Const(0)
        ####################################
        base = (T.Fbm( T.lerp(P1_FBM,P2_FBM,LERP) ) * 0.5 + 0.5) * AMPLITUDE_M
        ero_bas = base*0.03
        ####################################
        # T.loop (not raw for): the document keeps ONE loop group (editor-collapsible,
        # count editable). ero_bas is loop-invariant (created outside, read each pass).
        with T.loop(iters, ero_out=ero_out) as L:
          ero_inp = L.ero_out+ero_bas
          eo = T.basin_fill(ero_inp,blend=0.5)
          # AUTHORED exaggeration remap (replaces exaggerated_height_m): carve at ERO_EXAG x
          # the true relief, then scale back to true meters.
          eo = T.erode_thermal( eo*ERO_EXAG, T.lerp(P1_THERM,P2_THERM,LERP)) * (1.0/ERO_EXAG)
          flow = T.flow3d(eo)
          eo = T.flow_erode(eo, flow.discharge,
                              dt=8.5,           # ~1  (NOT 1000 — past ~1 the relief clamp saturates)
                              k_erode=0.5,      # incision strength        (0.05–0.3)
                              k_deposit=0.05,    # 0 = pure incision; raise to ~0.03 for valley fans
                              m=0.5, n=1.5,     # area / slope exponents    (n>1 sharpens canyons)
                              dep_m=0.5,        # deposition AREA exponent  (~0.5 — was 20.5 !)
                              clamp_frac=8.0,
                              blend = 1.0)   # master per-step amount = fraction of local relief
          eo = T.pha(eo,T.lerp(P1_PHA,P2_PHA,LERP),blend=0.1)
          L.ero_out = T.lpf(eo, cutoff=2, units='meters', blend=0.35)
        ero_out = L.ero_out
        ####################################
        ero_out = T.basin_fill(ero_out, blend=0.5) 
        ero_out = T.lpf(ero_out, cutoff=16, units='meters', blend=0.35)  
        ero_out = T.lpf(ero_out, cutoff=8, units='meters', blend=0.35)  
        ####################################
        self.capture(ero_out,"height",cache=True)
        self.capture(ero_out,"normal",cache=True)
