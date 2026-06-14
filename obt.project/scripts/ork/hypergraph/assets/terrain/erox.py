###############################################################################
# Erox — fbm terrain through PHYSICAL (Mei virtual-pipes) hydraulic erosion, tuned
# for FLUVIAL realism (Houdini-style). Three things make it read as water erosion:
#
#  1. OPEN boundaries (erox.cpp v8): water DRAINS off-map, so flow CONCENTRATES into
#     channels instead of flooding the domain into a uniform sheet. This is the single
#     biggest fix — a closed domain can only sheet-flow + evaporate, never channelize.
#  2. THIN, concentrating flow: the steady sheet depth is ~ rain_mps/evaporation_per_s,
#     so we keep rain LOW vs evap (~5 cm) — shallow water follows micro-topography and
#     concentrates into incising channels (positive feedback), rather than drowning it.
#  3. Interleaved THERMAL slumping: hydraulic erox carves, thermal relaxes the over-
#     steepened channel walls down to the angle of repose -> talus aprons + the V-valley
#     profile the eye recognizes. (Houdini always pairs hydro + thermal/debris.)
#
# Resolution: channels can't form below the cell size (cell_m = extent_m/dim). At the
# default 32768 m extent, -d 4096 = 8 m/cell (fine enough for gullies). Run high:
#   ork.terrain.viewer2.py -d 4096 -f erox
#
# Tuning levers (per-pass, in EROX below):
#   rain_mps / evaporation_per_s  steady sheet depth = rain/evap (thinner -> sharper channels)
#   capacity_Kc                   how much sediment the flow carries (more -> stronger transport)
#   erosion_rate_per_s            incision strength on fast/steep flow
#   deposition_rate_per_s         fan/valley-floor build-up in slow flow
#   creep_m2ps                    LOW = sharper walls (but must stay >0 to kill checkerboard)
# Bigger relief: bump exaggerated_height_m in EROX (erosion-scoped vertical exaggeration).
###############################################################################
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T

# hydraulic incision — THIN concentrating flow, meaningful sediment capacity, gentle
# rates so channels develop over the iterations (sim_time/CFL-dt) rather than blowing up.
EROX = dict(
    sim_time_s            = 250.0,   # simulated seconds per pass (-> iterations via CFL dt)
    rain_mps              = 0.006,   # thin sheet: depth ~ rain/evap ~ 5 cm -> concentrates
    evaporation_per_s     = 0.08,    # removes the standing sheet; edges drain the rest
    flow_speed_max_mps    = 10.0,    # CFL cap -> dt = 0.5*cell/vmax; bounds velocity
    capacity_Kc           = 0.5,     # sediment transport (erode steep/fast, deposit slow)
    erosion_rate_per_s    = 0.7,     # gentle incision
    deposition_rate_per_s = 1.0,     # slight net incision; fans still build where flow slows
    creep_m2ps            = 3.0,     # less smoothing than before -> sharper channel walls
)


class Erox(HeightField):
    def __init__(self):
        super().__init__()
        h = T.Fbm(frequency=9.2, octaves=7) * 0.5 + 0.5
        # hydraulic (incise) + thermal (slump walls to talus), interleaved — the Houdini
        # pairing. Re-running erox re-reads the carved terrain so channels deepen each pass.
        for i in range(30):
            h = T.erox(h, **EROX)
            #h = T.erode_thermal(h, talus_deg=35.0, rate=0.10, iterations=120)
        h = T.lpf(h, cutoff_m=10)   # settle single-texel noise; keep landform detail
        self.capture(h, "height")
        self.capture(h, "normal")
