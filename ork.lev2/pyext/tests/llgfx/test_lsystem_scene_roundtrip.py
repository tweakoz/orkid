#!/usr/bin/env python3
###############################################################################
# GR1.e gate — a GRAMMAR-CARRYING scene authored in Python survives the .ecs round
# trip and plays back through the C++ player with ZERO Python in the loop.
#
# scn_lsystem is the exemplar: its tree geometry exists only as an LRuleSet the
# grammar rewrites at load, so every reflected op/kind (L3's named enums, L2's family
# vocabulary) has to survive serdes or the played tree is a DIFFERENT tree.
#
# The cycle, in one process each:
#   A. live    — the SHIPPED scn_lsystem.py TreeScene is built in-process, hosted on a
#                real ECS controller, rendered offscreen -> live.png. The SAME SceneData
#                is serialized to lsys.ecs (the ork.scene.tojson phase) and round-tripped
#                in python (deserialize -> reserialize -> byte compare).
#   B. player  — ork.ecs.player.exe lsys.ecs --offscreen -S play1.png. No python.
#   C. floor   — a SECOND identical player run -> play2.png. The bark material's VS wind
#                reads the sim clock (RCFD_TIME <- Simulation::gameTime), and offscreen
#                freerun has no fixed timestep, so even player-vs-player is NOT byte
#                identical. This run MEASURES that noise floor instead of assuming it.
#   D. serdes  — a THIRD player run with --roundtrip: the player serializes its RUNNING
#                scenedata, deserializes a clone, and byte-compares the reserialization.
#                That is the sha-grade oracle the pixel paths cannot give.
#
# Verdicts are numeric: live-vs-player RMS is gated against LIVE_RMS_MAX, and the
# measured floor is required to stay under FLOOR_RMS_MAX (a floor blowout means the
# scene became time-unstable, which would silently loosen the real gate). Pixel sha
# equality is NOT a gate — it is unreachable even player-vs-player for this scene, and
# both shas are printed so a reader can see that for themselves.
#
# Channel normalization matters: the player writes RGBA, the in-process capture RGB.
# Diffing them raw pins RMS at exactly 0.5 on the missing alpha and hides everything.
#
# Hand-rolled boot instead of ork.testing.capture_app: capture_app hosts exactly one
# fill PrimCanvas and exposes no onGpuUpdate hook, so it cannot drive an ECS
# SceneGraphViewport nor pump controller.gpuUpdate (the hypermesh cook lives there).
# The capture ordering (settle -> readback complete -> write -> only then exit) and the
# verdict-before-teardown protocol are reused verbatim.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import argparse, hashlib, re, shutil, subprocess, sys, tempfile

from ork.testing import verdict

SCENE       = "scn_lsystem"
WIDTH       = 1280      # AppInitData defaults — the player's offscreen surface
HEIGHT      = 720
CAMDIST     = 20.0      # ork.ecs.player.exe defaults; mirrored exactly by the live host
CAMHEIGHT   = 8.0
CAMTARGET   = (0.0, 2.0, 0.0)
CAMFOV_DEG  = 65.0      # player main.cpp: uicam->_fov / near_min / far_max
CAMNEAR     = 0.75
CAMFAR      = 100000.0
LIVE_SETTLE = 300       # live-host frames before the grab (grammar cook + IBL prefilter)
PLAY_SETTLE = 30        # player -F: frames after first-lit
PLAY_FRAMES = 600       # player --frames safety cap
RT_SETTLE   = 300       # --roundtrip run: post-lit frames (keeps the run alive past T+1s)
RT_FRAMES   = 900

# thresholds — RGB RMS bands, calibrated on vrstudio (4 gate repeats, jul27):
#   live-vs-player  0.00258 .. 0.00272  -> ceiling 0.008 (3x the worst observed)
#   player floor    0.000045 .. 0.00041 -> ceiling 0.002 (5x the worst observed)
# The residual is wind phase + settle state, not geometry: a grammar-level divergence
# moves whole branches, and the tree covers enough of the frame that even losing it
# entirely scores an order of magnitude above these ceilings.
LIVE_RMS_MAX  = 0.008
FLOOR_RMS_MAX = 0.002

PLAYER_TIMEOUT = 180     # per player run
LIVE_TIMEOUT   = 240     # the live host (author + cook + 300 frames + readback)


###############################################################################
# phase LIVE — python-authored, python-hosted, offscreen. Runs in its own process
# (the player subprocesses must not share this one's GPU device).
###############################################################################

def phase_live(ecs_out, png_out):
  from orkengine import core          # core before lev2 (import-order law)
  from orkengine import lev2
  from orkengine import ecs
  from orkengine.core import vec3, vec4, CrcStringProxy, Object

  from ork.hypergraph.ecs.scene.resolve import resolve_scene_file, load_scene_class

  tokens = CrcStringProxy()
  DTOR   = 3.14159265358979 / 180.0

  scene_class = load_scene_class(resolve_scene_file(SCENE), None)

  class LiveApp:

    def __init__(self):
      self.ezapp = lev2.OrkEzApp.createEx(
          self, [ecs.ecsInitCallback],
          name="gr1e_live", width=WIDTH, height=HEIGHT, offscreen=True, ssaa=0)
      self.ezapp.setRefreshPolicy(lev2.RefreshFastest, -1)
      # A SceneGraphViewport hosting the ECS-OWNED scenegraph. Not a second render
      # path: the compositor is the one SceneGraphSystem built from the scene's merged
      # params (the player's compositor); the viewport only presents it. The UI draw
      # path is also the ONLY one that fires onGpuPostFrame — a player-shaped
      # ezapp.onDraw skips it entirely (ezapp_topwidget.cpp DoDraw), leaving no
      # in-process capture hook.
      self.ezapp.topWidget.enableUiDraw()
      lg = self.ezapp.topLayoutGroup
      lg.margin = 0
      self.sgv = lg.makeChild(
          fill=True, margin=0,
          uiclass=lev2.ui.SceneGraphViewport,
          args=["Viewport", vec4(0, 0, 0, 1)]).widget
      self.sgv.cameraName = "spawncam"
      self._bound    = False
      self._camlogged = False
      self.ctrl      = None
      self.sysref    = None
      self._frame    = 0

      self._inflight = False
      self._async    = None
      self._buf      = None
      self.result    = None
      self.serdes_ok = False
      self.js_bytes  = 0

    def onGpuInit(self, ctx):
      self.ctx = ctx

      # ---- author (the tojson phase, verbatim) ----
      scene = scene_class()
      sd    = ecs.SceneData()
      scene.build(sd)
      js = sd.serializeJson()
      with open(ecs_out, "w") as f:
        f.write(js)
      self.js_bytes = len(js)

      # ---- python-side serdes oracle on the SAME bytes the player will eat ----
      clone = Object.deserializeJson(js)
      assert clone is not None, "GR1.e: .ecs failed to deserialize in python"
      self.serdes_ok = (clone.serializeJson() == js)

      # ---- LIVE-HOST WIRE PASS -------------------------------------------------
      # A live-authored SceneData is NOT host-ready: skybox_path -> the _userParams key
      # the compositor reads, and the default layer declarations, are both produced only
      # by the DESERIALIZE-side wire pass (AssetSystem.cpp materializeAndWireScene, and
      # its python twin). Without it the live host renders the default catalog skybox
      # into no layers — a black tree in the wrong sky. Run AFTER serializeJson so the
      # .ecs the player eats is byte-for-byte what ork.scene.tojson.py writes.
      from ork.hypergraph.ecs.scene.assets import wire_scene_data
      wire_scene_data(sd, ezapp=self.ezapp)

      # ---- camera: the player's orbit rig, value for value ----
      from lev2utils.cameras import setupUiCameraX
      self.cameralut = lev2.CameraDataLut()
      self.camera, self.uicam = setupUiCameraX(
          cameralut=self.cameralut, camname="spawncam")
      self.uicam.fov      = CAMFOV_DEG * DTOR
      self.uicam.near_min = CAMNEAR
      self.uicam.far_max  = CAMFAR
      self.uicam.lookAt(vec3(CAMDIST, CAMHEIGHT, CAMDIST),
                        vec3(*CAMTARGET), vec3(0, 1, 0))
      self.uicam.updateMatrices()
      self.camera.copyFrom(self.uicam.cameradata)
      self.sgv.forkDB()   # the viewport needs its own DB fork before it can present

      # ---- host: bind + createSimulation with NO injected scenegraph, so the C++
      # SceneGraphSystem builds the compositor from the SAME merged params the player
      # gets. Any host-side lev2.scenegraph.Scene here would be a SECOND render path
      # and the pixel comparison below would measure the host, not the round trip.
      self.ctrl = ecs.Controller()
      self.ctrl.bindScene(sd)
      self.ctrl.createSimulation()

    def onGpuUpdate(self, ctx):
      if self.ctrl:
        self.ctrl.gpuUpdate(ctx)

    def onUpdateInit(self):
      # startSimulation belongs on the UPDATE thread (player main.cpp onUpdateInit) —
      # starting it from onGpuInit stalls the sim's link rendezvous and the draw loop
      # never reaches UPDRUNNING (one frame drawn, then silence).
      self.ctrl.startSimulation()
      self.sysref = self.ctrl.findSystem("SceneGraphSystem")

    def onUpdate(self, updinfo):
      if self.ctrl and self.sysref:
        if not self._bound:
          sim = self.ctrl.simulation
          sg  = sim.sceneGraphScene if sim else None
          if sg:
            self.sgv.scenegraph = sg
            self._bound = True
        UIC = self.uicam.cameradata
        if not self._camlogged:
          self._camlogged = True
          sys.stdout.write("LIVE_CAM eye=%s tgt=%s fovy=%s near=%s far=%s bound=%d\n"
                           % (UIC.eye, UIC.target, UIC.fovy, UIC.near, UIC.far,
                              int(self._bound)))
          sys.stdout.flush()
        self.ctrl.systemNotify(self.sysref, tokens.UpdateCamera, {
            tokens.eye:  UIC.eye,  tokens.tgt: UIC.target, tokens.up:   UIC.up,
            tokens.near: UIC.near, tokens.far: UIC.far,    tokens.fovy: UIC.fovy})
        self.ctrl.updateSimulation()
      self.sgv.setDirty()

    def onGpuPostFrame(self, ctx):
      # capture -> settle -> readback complete -> write -> ONLY THEN signalExit
      # (ork.testing.capture ordering; a teardown crash cannot eat the PNG).
      self._frame += 1
      if self._frame <= 2 or (self._frame % 60) == 0:
        sys.stdout.write("LIVE_FRAME=%d inflight=%d\n" % (self._frame, int(self._inflight)))
        sys.stdout.flush()
      if (self.result is not None) or (self._frame < LIVE_SETTLE):
        return
      if not self._inflight:
        self._buf   = lev2.CaptureBuffer()
        rtg         = ctx.FBI.main_RTG
        self._async = ctx.FBI.captureAsFormat(rtg.buffer(0), self._buf, "RGBA8")
        self._inflight = True
        return
      if self._async is None or bool(self._async.is_ready):
        self._finish(png_out)

    def _finish(self, path):
      import numpy
      from PIL import Image
      cb = self._buf
      w, h = cb.width, cb.height
      arr = numpy.array(cb, dtype=numpy.uint8).reshape(h, w, 4)[..., :3]
      # NO vertical flip: the ECS/SceneGraphViewport composite lands in main_RTG already
      # top-down (ork.testing.capture's PrimCanvas path is the one that needs the flip).
      Image.fromarray(arr).save(path)
      self.result = {"path": path, "w": w, "h": h,
                     "mean": float(arr.mean()), "max": int(arr.max())}
      sys.stdout.write("LIVE_CAPTURE=%s width=%d height=%d mean=%.4f max=%d\n"
                       % (path, w, h, self.result["mean"], self.result["max"]))
      sys.stdout.write("LIVE_SERDES=%d bytes=%d\n" % (int(self.serdes_ok), self.js_bytes))
      sys.stdout.flush()
      self.ezapp.signalExit()

  app = LiveApp()
  app.ezapp.mainThreadLoop()
  ok = (app.result is not None
        and app.result["max"] > 0 and app.result["mean"] > 1.0
        and app.serdes_ok)
  rc = verdict(ok, "phase=live capture=%s serdes=%d"
                   % (bool(app.result), int(app.serdes_ok)))
  app.ezapp.shutdown()
  return rc


###############################################################################
# driver
###############################################################################

_RMS_RE = re.compile(r"RMS error\s*=\s*([0-9.eE+-]+)")


def _sha(path):
  h = hashlib.sha256()
  with open(path, "rb") as f:
    h.update(f.read())
  return h.hexdigest()


def _rgb(path):
  """Normalize to a 3-channel copy. The player writes RGBA, the in-process capture
  writes RGB; idiff would otherwise score the ABSENT alpha channel as a full-scale
  error (RMS pinned at exactly 0.5) and drown the pixels being compared."""
  exe = shutil.which("oiiotool")
  assert exe, "oiiotool not on PATH (OpenImageIO tools)"
  out = os.path.splitext(path)[0] + "_rgb.png"
  pr = subprocess.run([exe, path, "--ch", "R,G,B", "-o", out],
                      capture_output=True, text=True)
  assert os.path.isfile(out), "oiiotool --ch failed for %s: %s" % (path, pr.stderr)
  return out


def _idiff(a, b):
  """Return (rms, raw) over RGB. idiff exits nonzero on any nonzero difference — the
  rc is not the verdict here, the RMS number is."""
  exe = shutil.which("idiff")
  assert exe, "idiff not on PATH (OpenImageIO tools)"
  pr = subprocess.run([exe, "-fail", "0.0", "-warn", "0.0", _rgb(a), _rgb(b)],
                      capture_output=True, text=True)
  out = (pr.stdout or "") + (pr.stderr or "")
  m = _RMS_RE.search(out)
  return (float(m.group(1)) if m else None), out


def _player(ecs_path, snap=None, roundtrip=None, timeout=PLAYER_TIMEOUT):
  exe = shutil.which("ork.ecs.player.exe")
  assert exe, "ork.ecs.player.exe not on PATH"
  cmd = [exe, ecs_path, "--offscreen",
         "--camdist", str(CAMDIST), "--camheight", str(CAMHEIGHT)]
  if roundtrip is not None:
    # the offscreen settle exit beats a T+N trigger — a long post-lit snapshot budget
    # is what keeps the run alive past it (without it the round-trip never fires and
    # the oracle reads as a silent miss).
    cmd += ["--roundtrip", str(roundtrip), "--frames", str(RT_FRAMES),
            "-S", snap, "-F", str(RT_SETTLE)]
  else:
    cmd += ["--frames", str(PLAY_FRAMES), "-S", snap, "-F", str(PLAY_SETTLE)]
  pr = subprocess.run(cmd, timeout=timeout, capture_output=True, text=True)
  return pr.returncode, (pr.stdout or "") + (pr.stderr or "")


def main():
  ap = argparse.ArgumentParser(description="GR1.e scene/player zero-python round trip")
  ap.add_argument("--phase", default=None, choices=["live"],
                  help="internal: the engine-hosted live-render phase (self-respawn)")
  ap.add_argument("--ecs", default=None)
  ap.add_argument("--png", default=None)
  ap.add_argument("--outdir", default=None, help="keep the artifacts here")
  args = ap.parse_args()

  if args.phase == "live":
    return phase_live(args.ecs, args.png)

  wd = args.outdir or tempfile.mkdtemp(prefix="gr1e_")
  os.makedirs(wd, exist_ok=True)
  ecs_path  = os.path.join(wd, "lsys.ecs")
  live_png  = os.path.join(wd, "live.png")
  play1_png = os.path.join(wd, "play1.png")
  play2_png = os.path.join(wd, "play2.png")

  print("[gr1e] workdir %s" % wd, flush=True)

  # ---- A: live python author + host + render -------------------------------
  try:
    pr = subprocess.run([sys.executable, os.path.abspath(__file__),
                         "--phase", "live", "--ecs", ecs_path, "--png", live_png],
                        capture_output=True, text=True, timeout=LIVE_TIMEOUT)
    live_log = (pr.stdout or "") + (pr.stderr or "")
  except subprocess.TimeoutExpired as e:
    # a wedged live host is a FAILURE with a verdict, never a hung lane
    print((e.output or b"").decode("utf-8", "replace")[-3000:], flush=True)
    return verdict(False, "phase=live TIMEOUT after %ds" % LIVE_TIMEOUT)
  for ln in live_log.splitlines():
    if ln.startswith(("LIVE_CAM ", "LIVE_CAPTURE=", "LIVE_SERDES=", "TESTVERDICT=")):
      print("[gr1e] " + ln, flush=True)
  live_ok   = os.path.isfile(live_png) and os.path.getsize(live_png) > 0
  serdes_ok = "LIVE_SERDES=1" in live_log
  ecs_ok    = os.path.isfile(ecs_path) and os.path.getsize(ecs_path) > 0
  if not (live_ok and ecs_ok):
    print(live_log[-3000:], flush=True)
    rc = verdict(False, "phase=live failed live_png=%d ecs=%d" % (live_ok, ecs_ok))
    return rc

  # ---- B/C: two identical zero-python player renders ------------------------
  rc1, log1 = _player(ecs_path, snap=play1_png)
  rc2, log2 = _player(ecs_path, snap=play2_png)
  play_ok = (os.path.isfile(play1_png) and os.path.isfile(play2_png))
  if not play_ok:
    print(log1[-2000:], flush=True)
    return verdict(False, "player snapshot missing rc1=%d rc2=%d" % (rc1, rc2))

  # ---- D: the player's OWN in-engine serdes byte oracle ---------------------
  rt_png    = os.path.join(wd, "roundtrip.png")
  rc3, log3 = _player(ecs_path, snap=rt_png, roundtrip=1.0)
  player_serdes_ok = "ROUND-TRIP serdes BYTE-IDENTICAL" in log3
  if not player_serdes_ok:
    print("[gr1e] player --roundtrip produced no BYTE-IDENTICAL line (rc=%d)" % rc3, flush=True)
  player_serdes_bad = "ROUND-TRIP serdes DIVERGED" in log3

  # ---- compare --------------------------------------------------------------
  floor_rms, floor_raw = _idiff(play1_png, play2_png)
  live_rms,  live_raw  = _idiff(live_png,  play1_png)
  sha_live, sha_p1, sha_p2 = _sha(live_png), _sha(play1_png), _sha(play2_png)

  print("[gr1e] floor  play1-vs-play2 RMS=%s" % floor_rms, flush=True)
  print("[gr1e] gate   live -vs-play1 RMS=%s (max %.4f)" % (live_rms, LIVE_RMS_MAX), flush=True)
  print("[gr1e] sha256 live =%s" % sha_live, flush=True)
  print("[gr1e] sha256 play1=%s" % sha_p1, flush=True)
  print("[gr1e] sha256 play2=%s" % sha_p2, flush=True)
  print("[gr1e] sha equal live==play1: %s   play1==play2: %s"
        % (sha_live == sha_p1, sha_p1 == sha_p2), flush=True)
  print("[gr1e] captures: %s  |  %s" % (live_png, play1_png), flush=True)

  floor_ok = (floor_rms is not None) and (floor_rms <= FLOOR_RMS_MAX)
  live_cmp = (live_rms is not None) and (live_rms <= LIVE_RMS_MAX)

  gates = [
      ("A live render + .ecs",        live_ok and ecs_ok),
      ("B python serdes byte-id",     serdes_ok),
      ("C player serdes byte-id",     player_serdes_ok and not player_serdes_bad),
      ("D player floor RMS<=%.3f" % FLOOR_RMS_MAX, floor_ok),
      ("E live-vs-player RMS<=%.3f" % LIVE_RMS_MAX, live_cmp),
  ]
  for name, ok in gates:
    print("[gr1e]   [%s] %s" % ("PASS" if ok else "FAIL", name), flush=True)

  allok = all(ok for _, ok in gates)
  return verdict(allok,
                 "live_rms=%s floor_rms=%s sha_live_eq_play=%d py_serdes=%d cpp_serdes=%d"
                 % (live_rms, floor_rms, int(sha_live == sha_p1),
                    int(serdes_ok), int(player_serdes_ok)))


if __name__ == "__main__":
  sys.exit(main())
