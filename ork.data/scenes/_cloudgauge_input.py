###############################################################################
# _cloudgauge_input.py — the CLOUD DECKS' runtime state applier. Attached with
# the decks themselves (_cloud_deck.py appends it), so every scene that raises
# decks gets the one thing that can move them.
#
# IT READS NO INPUT. It was born as a hand-held gauge for one dedicated scene
# and grabbed the right stick, OPTIONS and SHARE directly; attached to every
# deck-bearing scene those grabs became a second claimant on pad controls the
# game already owns (the right stick moved cloud cover while it was trimming
# walk speed). The HUD editor's CLOUDS rows are THE control surface now, and
# they arrive here as ONE message: CloudSet {cover?, tile?, alt_offset?}.
#
# CONTROL BUS (no engine change): every runtime knob rides the cloud-plane
# ENTITY TRANSFORMS, which a sim script may freely write:
#   translation.y  -> coverage threshold   (shader: t=(wpos.y-AltLo)*InvSweep)
#   uniform scale  -> tile world-size      (shader UVs are OBJECT-space)
#   hide           -> scale ~0 + park far underground (an EMPTY sky, and every
#                     layer the current mode does not show)
#
# The state here starts from env vars shared with the scene author phase
# (ORK_CLOUDGAUGE_MODE / _T / _TILE / _RES / _WIND) — a STARTING POINT, never
# applied unasked: the author-time transforms are the scene's truth until a
# host says otherwise. Every change prints one line so the state is readable
# back out of a log.
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


def cover_to_thresh(cover):
  """Sky-cover fraction (what a scene author and the HUD speak in) -> the gauge's
  inverse THRESHOLD. ONE conversion for both the author-time transforms and the
  runtime applier, so the two can never drift.

  ZERO COVER IS THE CLEAR END, not 1.0. At thresh 1.0 the deck sits exactly at the
  top of its sweep band and the material's softness/erode tail still leaves a faint
  veil — visible, and no way to ask for none through this API. THRESH_MAX is the
  documented fully-clear stop, so that is where a cover of 0 lands."""
  c = min(1.0, max(0.0, float(cover)))
  if c <= 0.0:
    return THRESH_MAX
  return min(THRESH_MAX, max(THRESH_MIN, 1.0 - c))


def decks_clear(thresh):
  """Is the sky asked to be EMPTY? Then the planes are parked rather than drawn
  fully transparent — same pixels, none of the fill, and it is what makes the
  procedural-sky default (cover 0) cost nothing to look at."""
  return thresh >= 1.0


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


# The deck state, as one human line. Printed when something actually changes it —
# which is now only a CloudSet from a host (the HUD editor's CLOUDS rows).


def fmt_state(st, base=None):
  """`base` is the deck LIFT in metres (G.alt_offset) — a live control now (the HUD's
  cloud-base row), so it belongs on the line that reads the state back out of a log.
  None where there is nothing to report yet (the pre-CloudSet init print)."""
  cover = max(0.0, min(1.0, 1.0 - st["thresh"]))
  return ("[cloudgauge] layer=%s  thresh=%.2f (sky cover ~%d%%)  tile=x%.2f "
          "(cirrus %.1fkm / alto %.1fkm / cumulus %.1fkm)  cumulus_res=%d  wind=%s%s"
          % (st["mode"].upper(), st["thresh"], int(round(cover * 100.0)),
             st["tile"],
             LAYERS["cirrus"]["tile_base_m"]  * st["tile"] * 1e-3,
             LAYERS["alto"]["tile_base_m"]    * st["tile"] * 1e-3,
             LAYERS["cumulus"]["tile_base_m"] * st["tile"] * 1e-3,
             st["res"],
             ("ON(x%g)" % WIND_MULT) if st["wind_on"] else "PAUSED",
             "" if base is None else ("  base=%+.0fm" % base)))


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

  class CloudGauge:
    def __init__(self):
      self.state    = env_state()
      self.wind_pos = {k: [0.0, 0.0] for k in LAYERS}   # accumulated wind offset (m)
      self.ents     = {}
      # NOT DIRTY AT BIRTH. These numbers are the ENV launch knobs — the starting point
      # for the sticks, not a description of the scene. Applying them unasked is what
      # overwrote every scene-authored cover on the first tick (and would have raised the
      # decks on a sky the scene declared empty). The author-time transforms stand until
      # an actual op moves them; a host syncs this state to the scene's with its own
      # CloudSet at startup.
      self.dirty    = False
      # The decks' ASL lift, which the SCENE declared and this script cannot read
      # from scene data: a host that knows it (it rides the scenegraph params) sends
      # it with CloudSet, and it is remembered from then on. 0 until told — which is
      # correct for every scene that declares no offset.
      self.alt_offset = 0.0

  _FREEZE = os.environ.get("ORK_CLOUDGAUGE_FREEZE", "0") == "1"
  _APPLYONCE = os.environ.get("ORK_CLOUDGAUGE_APPLYONCE", "0") == "1"

  # RESOLVED, OR NOT PRESENT AT ALL. A scene declares whichever decks it wants —
  # scn_swest's one cumulus shell is a whole deck set — so most of the four names
  # below miss in most scenes. findEntityByName hands back a NULL handle for a
  # miss, and that handle is TRUTHY in python (the Entity binding has no
  # __bool__), so `if ent` waved the null through and the first transform write
  # dereferenced it on the update thread. repr() is the one accessor that reads
  # the handle without dereferencing it, so it is what decides here; a miss is
  # then dropped at link and never enters the applier's map.
  def _resolved(ent):
    if ent is None:
      return False
    text = repr(ent)
    head = text.find("0x")
    if head < 0:
      return False
    digits = ""
    for ch in text[head + 2:]:
      if ch not in "0123456789abcdefABCDEF":
        break
      digits += ch
    return bool(digits) and int(digits, 16) != 0

  def _apply(simulation):
    """Write the gauge state onto the deck entities THIS SCENE declared."""
    if _FREEZE:      # debug: leave the author-time spawner transforms untouched
      return
    G  = simulation.vars.cloudgauge
    if _APPLYONCE and getattr(G, "applies", 0) >= 1:
      return
    st = G.state
    clear = decks_clear(st["thresh"])
    for ent_name, ent in G.ents.items():
      spec = LAYERS[PLANE_ENTITIES[ent_name]]
      if (not clear) and layer_visible(plane_key(ent_name), st["res"], st["mode"]):
        # SHELL STAYS PINNED at the origin (owner jul25: translating the dome
        # swept its curvature/veil structures across the sky = "swimming").
        # Wind is now a GPU-clock UV scroll inside the material.
        ent.translation = vec3(0.0, layer_y(spec, st["thresh"]) + G.alt_offset, 0.0)
        ent.scale       = spec["base_scale"] * st["tile"]
      else:
        ent.translation = vec3(0.0, HIDE_Y, 0.0)
        ent.scale       = HIDE_SCALE
    G.applies = getattr(G, "applies", 0) + 1

  def _report(simulation):
    G = simulation.vars.cloudgauge
    print(fmt_state(G.state, G.alt_offset), flush=True)

  def onSystemInit(simulation):
    simulation.vars.cloudgauge = CloudGauge()
    print(fmt_state(simulation.vars.cloudgauge.state), flush=True)

  def onSystemLink(simulation):
    G = simulation.vars.cloudgauge
    absent = []
    for ent_name in PLANE_ENTITIES:
      ent = simulation.findEntityByName(ent_name)
      if _resolved(ent):
        G.ents[ent_name] = ent
      else:
        absent.append(ent_name)
    print("[cloudgauge] linked %d/%d decks: %s%s" % (
        len(G.ents), len(PLANE_ENTITIES),
        ", ".join(G.ents) if G.ents else "none",
        (" (%s absent)" % ", ".join(absent)) if absent else ""), flush=True)
    # NO APPLY AT LINK. The author-time transforms already carry the state the SCENE
    # declared, and this script's own state is seeded from the ENV launch knobs — which
    # are not the same thing. Applying here overwrote every scene-authored cover with
    # whatever ORK_CLOUDGAUGE_T happened to be (and would erase a saved cover the host
    # is about to restore). The decks stand as authored until something actually asks
    # them to move: a stick, or the CloudSet a host sends.

  def onSystemNotify(simulation, evID, table):
    G  = simulation.vars.cloudgauge
    st = G.state
    # HOST-DRIVEN DECK STATE — the same gauge state the sticks move, reachable by any host
    # that can send a controller message (the player's HUD CLOUDS page). Fields are
    # OPTIONAL and independent: a message carrying only `cover` leaves tile alone. Cover is
    # the sky-cover fraction the scene author speaks in (Scene.cloud_decks(cover=...)); the
    # gauge's own currency is the inverse THRESHOLD, and the conversion lives here so the
    # two can never drift apart.
    if evID.hashed == tokens.CloudSet.hashed:
      # A DataTable read of an ABSENT key yields an empty value rather than raising, so
      # "was this field sent" is a conversion test, not a membership test.
      def _opt(tok):
        try:
          return float(table[tok])
        except Exception:
          return None
      touched = False
      # The decks' lift, sent by a host that can read the scenegraph params — and a LIVE
      # control (the HUD's cloud-base row), not just a launch constant. Remembered, not
      # per-message: everything after it moves the same decks.
      #
      # ONLY HALF THE MOVE LANDS HERE. Deck altitude IS the coverage encoding
      # (t=(wpos.y-CgAltLo)/sweep), so a lift that moves the shells without moving the
      # material's baked CgAltLo band shifts t by (metres/sweep) and corrupts coverage.
      # The material is out of reach from here (this runs in the sim sub-interpreter, whose
      # API is entities/components/datatables — no lev2 at all), so the SENDER owns the band
      # half: the player rebinds CgAltLo on each deck material from the same number it sends
      # here. A host that sends alt_offset without doing that is asking for a coverage bug.
      alt_off = _opt(tokens.alt_offset)
      if alt_off is not None and alt_off != G.alt_offset:
        G.alt_offset = alt_off
        touched = True
      cover = _opt(tokens.cover)
      if cover is not None:
        st["thresh"] = cover_to_thresh(cover)
        touched = True
      tile = _opt(tokens.tile)
      if tile is not None:
        st["tile"] = min(TILE_MAX, max(TILE_MIN, tile))
        touched = True
      if touched:
        G.dirty = True
        _apply(simulation)
        _report(simulation)
      return
  # NO INPUT HANDLING LIVES HERE (owner aug08). This script was born as a hand-held
  # gauge for one dedicated scene, and it read the right stick, OPTIONS and SHARE
  # directly. It is now attached to EVERY deck-bearing scene, so those grabs became
  # a second claimant on pad controls the rest of the game owns — the right stick
  # moved cloud cover while it was trimming walk speed. The HUD editor's CLOUDS rows
  # are THE control surface for decks; this script is a pure CloudSet consumer.
