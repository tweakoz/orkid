###############################################################################
# _cloudgauge_input.py — GAMEPAD control layer for scn_cloudgauge (B4 cloud-
# texture VR gauge). Appended system script: composes with walk_input_system
# (which keeps locomotion: left stick / L-dpad walk, face buttons snap camera,
# R1 jump). This script owns the CLOUD knobs on inputs the walker leaves free:
#
#   OPTIONS (menu/start)    cycle layer   ALL -> cirrus -> alto -> cumulus
#   SHARE   (select/back)   cumulus resolution toggle 1024 <-> 2048
#   R3      (R-stick click) wind pause / resume
#   RIGHT STICK up/down     coverage threshold (up = clearer sky)
#   RIGHT STICK left/right  tile world-size    (right = larger tiles)
#
# (L1 is avoided: the player consumes it host-side as the VR perf-HUD toggle.)
#
# CONTROL BUS (no engine change): every runtime knob rides the cloud-plane
# ENTITY TRANSFORMS, which the sim script may freely write:
#   translation.y  -> coverage threshold   (shader: t=(wpos.y-AltLo)*InvSweep)
#   uniform scale  -> tile world-size      (shader UVs are OBJECT-space)
#   translation.xz -> wind scroll          (plane translates; pattern rides it;
#                                           wrapped modulo one tile period)
#   hide           -> scale ~0 + park far underground
#
# Initial state ALSO comes from env vars (shared with the scene author phase,
# used by offscreen verification): ORK_CLOUDGAUGE_MODE / _T / _TILE / _RES /
# _WIND. Every state change prints to stdout so the owner (in the HMD, over
# ssh) can read their preferred values back afterwards.
###############################################################################

import math
import os

# --------------------------------------------------------------------------
# shared constants — the scene file imports these (KEEP scene + script in sync
# through THIS module; nothing here may import orkengine.ecssim at top level
# unguarded, since the scene author process also imports it).
# --------------------------------------------------------------------------

LAYER_MODES = ["all", "cirrus", "alto", "cumulus"]

# per-layer look constants (CHANNELS.md suggested tile sizes; altitudes to
# spec: cirrus high ~8km, altocumulus mid ~4km, cumulus low ~1.5km).
# base_scale: the plane meshes are authored 400m wide LOCAL (the mesh pipeline
# mangles multi-km vertex coords); entity scale = base_scale * tile-mult blows
# them up to world size (cirrus 100km / alto 70km / cumulus 50km at x1).
LAYERS = {
  "cirrus": dict(
      alt_m       = 8000.0,
      sweep_m     = 600.0,     # threshold sweep rides +-300m of altitude (invisible at 8km)
      tile_base_m = 30000.0,
      base_scale  = 250.0,     # 400m local -> 100km world at tile x1
      wind_mps    = 45.0,      # jetstream-fast so the strands visibly stream
      wind_dir    = (1.0, 0.0)),   # strands are +U aligned; wind along +X
  "alto": dict(
      alt_m       = 4000.0,
      sweep_m     = 600.0,
      tile_base_m = 8000.0,
      base_scale  = 175.0,     # 400m local -> 70km world
      wind_mps    = 18.0,
      wind_dir    = (0.94, 0.34)),
  "cumulus": dict(
      alt_m       = 1500.0,
      sweep_m     = 600.0,
      tile_base_m = 12000.0,
      base_scale  = 125.0,     # 400m local -> 50km world
      wind_mps    = 9.0,
      wind_dir    = (0.87, -0.50)),
}

# entity names (scene declares these four planes)
ENT_FOR = {
  "cirrus":  "cloud_cirrus",
  "alto":    "cloud_alto",
}
CUMULUS_ENT = {1024: "cloud_cumulus_1k", 2048: "cloud_cumulus_2k"}

# WIND MULTIPLIER — 1.0 = physically-scaled drift (owner-set jul25; was a 4x
# exaggeration for the first animation gauge).
# Override: ORK_CLOUDGAUGE_WINDX=<mult>
try:
  WIND_MULT = float(os.environ.get("ORK_CLOUDGAUGE_WINDX", "1.0"))
except ValueError:
  WIND_MULT = 1.0

HIDE_Y      = -9000.0    # parked far underground (plus shrunk) when hidden
HIDE_SCALE  = 1.0e-3
TILE_MIN, TILE_MAX     = 0.35, 2.8
THRESH_MIN, THRESH_MAX = 0.0, 1.05    # 1.05 -> fully clear sky
RATE_THRESH = 0.30       # threshold units/sec at full stick deflection
RATE_TILE   = 0.80       # log-space/sec at full deflection (~2.2x per second)
STICK_DEADZONE = 0.18


def env_state():
  """Initial gauge state from env (defaults = the owner's first look)."""
  e = os.environ
  mode = e.get("ORK_CLOUDGAUGE_MODE", "all").strip().lower()
  if mode not in LAYER_MODES:
    mode = "all"
  def _f(k, d):
    try:
      return float(e.get(k, d))
    except ValueError:
      return d
  res = 2048 if e.get("ORK_CLOUDGAUGE_RES", "2048").strip() != "1024" else 1024
  return dict(
      mode    = mode,
      thresh  = min(THRESH_MAX, max(THRESH_MIN, _f("ORK_CLOUDGAUGE_T", 0.55))),
      tile    = min(TILE_MAX,   max(TILE_MIN,   _f("ORK_CLOUDGAUGE_TILE", 1.0))),
      res     = res,
      wind_on = e.get("ORK_CLOUDGAUGE_WIND", "1").strip() != "0")


def layer_y(spec, thresh):
  """Altitude encoding of the coverage threshold. Shader-side:
  t = (wpos.y - (alt - sweep/2)) / sweep  ->  y = alt + (t - 0.5)*sweep."""
  return spec["alt_m"] + (thresh - 0.5) * spec["sweep_m"]


def layer_visible(name, res, mode):
  """Which of the four PLANE ENTITIES is visible in a given mode/res.
  `name` is a LAYERS key with res deciding between the two cumulus planes."""
  if name == "cumulus1k":
    return (mode in ("all", "cumulus")) and res == 1024
  if name == "cumulus2k":
    return (mode in ("all", "cumulus")) and res == 2048
  return mode in ("all", name)


PLANE_ENTITIES = {
  # entity name -> LAYERS spec key
  "cloud_cirrus":     "cirrus",
  "cloud_alto":       "alto",
  "cloud_cumulus_1k": "cumulus",
  "cloud_cumulus_2k": "cumulus",
}


def plane_key(ent_name):
  """visibility key for layer_visible()"""
  if ent_name == "cloud_cumulus_1k":
    return "cumulus1k"
  if ent_name == "cloud_cumulus_2k":
    return "cumulus2k"
  return PLANE_ENTITIES[ent_name]


CHEAT_SHEET = """
[cloudgauge] ============ GAMEPAD CONTROLS (cloud gauge) ============
[cloudgauge]  OPTIONS (menu/start)   : cycle layer  ALL -> cirrus -> altocumulus -> cumulus
[cloudgauge]  SHARE   (select/back)  : cumulus texture res 1024 <-> 2048
[cloudgauge]  R3 (right-stick CLICK) : (retired — wind rides the GPU clock;
[cloudgauge]                            launch with ORK_CLOUDGAUGE_WINDX=0 for still air)
[cloudgauge]  RIGHT STICK up/down    : coverage threshold  (up = clearer sky)
[cloudgauge]  RIGHT STICK left/right : tile world-size     (right = larger)
[cloudgauge]  walker keeps: left stick/dpad walk, face buttons snap camera, R1 jump
[cloudgauge]  (L1 = VR perf HUD, host-owned)
[cloudgauge] ========================================================
"""


def fmt_state(st):
  cover = max(0.0, min(1.0, 1.0 - st["thresh"]))
  return ("[cloudgauge] layer=%s  thresh=%.2f (sky cover ~%d%%)  tile=x%.2f "
          "(cirrus %.1fkm / alto %.1fkm / cumulus %.1fkm)  cumulus_res=%d  wind=%s"
          % (st["mode"].upper(), st["thresh"], int(round(cover * 100.0)),
             st["tile"],
             LAYERS["cirrus"]["tile_base_m"]  * st["tile"] * 1e-3,
             LAYERS["alto"]["tile_base_m"]    * st["tile"] * 1e-3,
             LAYERS["cumulus"]["tile_base_m"] * st["tile"] * 1e-3,
             st["res"],
             ("ON(x%g)" % WIND_MULT) if st["wind_on"] else "PAUSED"))


###############################################################################
# runtime half — only live inside the player's PythonSystem (ecssim).
###############################################################################

try:
  from orkengine.ecssim import *          # noqa: F401,F403
  _ECSSIM = True
except ImportError:
  _ECSSIM = False                          # author-process import: constants only

if _ECSSIM:

  tokens = CrcStringProxy()

  GP_OPTIONS = tokens.OPTIONS.hashed
  GP_SHARE   = tokens.SHARE.hashed
  GP_R3      = tokens.R3.hashed

  class CloudGauge:
    def __init__(self):
      self.state    = env_state()
      self.wind_pos = {k: [0.0, 0.0] for k in LAYERS}   # accumulated wind offset (m)
      self.rx       = 0.0     # right stick, live
      self.ry       = 0.0
      self.ents     = {}
      self.dirty    = True
      self.print_t  = 0.0     # throttle for stick-driven prints

  _FREEZE = os.environ.get("ORK_CLOUDGAUGE_FREEZE", "0") == "1"
  _APPLYONCE = os.environ.get("ORK_CLOUDGAUGE_APPLYONCE", "0") == "1"

  def _apply(simulation):
    """Write the whole gauge state onto the four plane-entity transforms."""
    if _FREEZE:      # debug: leave the author-time spawner transforms untouched
      return
    G  = simulation.vars.cloudgauge
    if _APPLYONCE and getattr(G, "applies", 0) >= 1:
      return
    st = G.state
    for ent_name, ent in G.ents.items():
      if not ent:
        continue
      spec = LAYERS[PLANE_ENTITIES[ent_name]]
      if layer_visible(plane_key(ent_name), st["res"], st["mode"]):
        # SHELL STAYS PINNED at the origin (owner jul25: translating the dome
        # swept its curvature/veil structures across the sky = "swimming").
        # Wind is now a GPU-clock UV scroll inside the material.
        ent.translation = vec3(0.0, layer_y(spec, st["thresh"]), 0.0)
        ent.scale       = spec["base_scale"] * st["tile"]
      else:
        ent.translation = vec3(0.0, HIDE_Y, 0.0)
        ent.scale       = HIDE_SCALE
    G.applies = getattr(G, "applies", 0) + 1

  def _report(simulation):
    print(fmt_state(simulation.vars.cloudgauge.state), flush=True)

  def onSystemInit(simulation):
    simulation.vars.cloudgauge = CloudGauge()
    print(CHEAT_SHEET, flush=True)
    print(fmt_state(simulation.vars.cloudgauge.state), flush=True)

  def onSystemLink(simulation):
    G = simulation.vars.cloudgauge
    for ent_name in PLANE_ENTITIES:
      G.ents[ent_name] = simulation.findEntityByName(ent_name)
    found = [n for n, e in G.ents.items() if e]
    print("[cloudgauge] linked plane entities: %s" % ", ".join(found), flush=True)
    G.dirty = True

  def onSystemNotify(simulation, evID, table):
    G  = simulation.vars.cloudgauge
    st = G.state
    if evID.hashed == tokens.GamepadButton.hashed:
      if not table[tokens.down]:
        return
      h = table[tokens.button].hashed
      if h == GP_OPTIONS:                       # cycle layer solo/all
        st["mode"] = LAYER_MODES[(LAYER_MODES.index(st["mode"]) + 1) % len(LAYER_MODES)]
      elif h == GP_SHARE:                       # cumulus 1024 <-> 2048
        st["res"] = 1024 if st["res"] == 2048 else 2048
      elif h == GP_R3:                          # wind rides the GPU clock now
        print("[cloudgauge] wind pause unavailable (wind rides the GPU clock; "
              "relaunch with ORK_CLOUDGAUGE_WINDX=0 for still air)", flush=True)
        return
      else:
        return
      G.dirty = True
      _report(simulation)
      return
    if evID.hashed == tokens.GamepadAxes.hashed:
      if table[tokens.connected]:
        G.rx = table[tokens.rx]
        G.ry = table[tokens.ry]
      else:
        G.rx = G.ry = 0.0

  def onSystemUpdate(simulation):
    G  = simulation.vars.cloudgauge
    st = G.state
    dt = simulation.deltaTime
    if dt <= 0.0 or dt > 1.0:
      dt = 1.0 / 60.0
    changed = False
    # right stick: threshold (y, up=clearer -> ry is negative when pushed up)
    ry = G.ry if abs(G.ry) > STICK_DEADZONE else 0.0
    rx = G.rx if abs(G.rx) > STICK_DEADZONE else 0.0
    if ry != 0.0:
      st["thresh"] = min(THRESH_MAX, max(THRESH_MIN,
                         st["thresh"] + (-ry) * RATE_THRESH * dt))
      changed = True
      G.dirty = True
    if rx != 0.0:
      st["tile"] = min(TILE_MAX, max(TILE_MIN,
                       st["tile"] * math.exp(rx * RATE_TILE * dt)))
      changed = True
      G.dirty = True
    if changed or G.dirty:
      _apply(simulation)
    # stick-driven prints, throttled to ~3/s
    if G.dirty:
      t = simulation.gameTime
      if t - G.print_t > 0.33:
        G.print_t = t
        _report(simulation)
      G.dirty = False
