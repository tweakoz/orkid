#!/usr/bin/env ork.python
###############################################################################
# ork.spatialaudio.harness.py — HEADLESS spatialization gate for
# scn_spatial_audio_showcase. Instrumented, not eyeballed:
#
#   CHILD (--role render): boots an offscreen freerun ECS app with BOTH clocks
#   pinned, which is what makes the gate reproducible:
#     WAV clock  — always fixed: the SYNC stream device is advanced manually by
#                  exactly 1/UPS per update tick (dev.advanceTime).
#     SIM clock  — fixed because the child passes fixed_sim_rate=UPS, so
#                  Simulation::desiredFrameRate() is nonzero and every sim tick
#                  spends exactly 1/UPS of game time instead of wall-clock dt.
#                  Without it the stochastic emitters integrate wall jitter and
#                  the band table (and G4) drift run to run.
#   Audio never touches a device (ORKID_AUDIO_IOCLASS=STREAM). Plays the
#   AudioHarnessScene (full soundscape, camera-only scenegraph), drives the
#   listener along the scene's scripted walk (listener_pose) through the REAL
#   camera->listener path (UpdateCamera notify), runs the host WindLayer
#   (the runtime-synthesized source), and tees every generated sample to a
#   WAV (the T1 offline-render vehicle).
#
#   DRIVER (no --role): pure stdlib+numpy. Parses the float WAV, band-filters
#   into the per-source spectral bands the bake script owns, and scores:
#
#     G1 water approach   band-S RMS grows as the walk closes on the pool
#     G2 water L/R flip   band-S stereo asymmetry reverses sign across the
#                         strafe-past (seg B) — spatialization audible-by-
#                         numbers, sign-convention-agnostic
#     G3 wind  L/R flip   band-W asymmetry reverses across seg C (the
#                         SYNTHESIZED source is positioned too)
#     G4 clank approach   band-K RMS grows across seg D (urban corner)
#     G5 runtime chimes   band-C energy ~absent before spawn time, present
#                         after (the runtime-spawned emitters are REAL)
#     G6 bird transients  band-B one-shot peaks present in the treeline pass
#     G7 wav sanity       2ch float 48k, expected duration, non-silent
#     G8 probe bed        band-P (the scene's one SoundFieldProbe) is audible
#                         at the near pass and falls off radially: three points
#                         down the falloff, strictly ordered, far point back at
#                         the band's no-probe floor
#     G9 dc offset        neither channel carries a DC pedestal (|dc| <= 1e-3,
#                         the ork.vet.audio.py dc_offset threshold). Guards the
#                         sample loader's bias removal: when it subtracted the
#                         peak midpoint instead of the mean it froze a per-asset
#                         offset (to -0.125 FS) into every normalized block,
#                         which the amp/panner gains turned into an audible
#                         one-sided LF thump on close approach while every
#                         ratio-based band gate above still read PASS.
#
#   Emits the per-segment measurement table and a machine verdict line.
#
# NO REAL AUDIO DEVICE EVER: ORKID_AUDIO_IOCLASS=STREAM pinned in the child
# env BEFORE engine import (genviron trap) AND lockstep forces it again.
#
#   ork.spatialaudio.harness.py                 # full gate
#   ork.spatialaudio.harness.py --keep          # keep the WAV for listening
#   ork.spatialaudio.harness.py --wav /x.wav    # explicit output path
###############################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import argparse

# prepend THIS checkout's scripts dir so ork.testing resolves from this tree.
_ROOT = os.path.abspath(__file__)
for _ in range(3):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

SELF = os.path.abspath(__file__)
SCENE_PY = os.path.join(_ROOT, "ork.data", "scenes", "scn_spatial_audio_showcase.py")
DEFAULT_WAV = os.path.join(_ROOT, ".tmp", "spatial_audio", "showcase_walk.wav")

UPS = 60.0
TAIL_SECS = 2.0      # keep capturing after the walk ends (voice releases)

# spectral bands (Hz) — the bake-script contract
BANDS = {
  "W": (55.0, 95.0),       # wind (synthesized) — BELOW the machine hum's
                           # 100-260 Hz band; the wider 60-320 gate drowned
                           # in hum once SimpleSound loops came alive
  "S": (500.0, 1500.0),    # water grains / stream
  "K": (1150.0, 1450.0),   # clanks (upper modal cluster, above the droplets)
  "C": (1600.0, 2300.0),   # chimes (runtime-spawned)
  "B": (2500.0, 5000.0),   # birds
  "P": (6500.0, 7500.0),   # the AmbiX SoundFieldProbe bed — the probe is the
                           # ONLY source in this band (measured no-probe floor
                           # ~2e-6, four orders under every gated band), so
                           # band-P energy IS probe energy
}

# measurement segments (label, t0, t1) — sub-windows of the scene walk
SEGMENTS = [
  ("A_far", 0.5, 2.5), ("A_near", 10.0, 12.0),
  ("B_in", 13.0, 18.0), ("B_out", 20.0, 25.0),
  ("C_in", 27.0, 32.0), ("C_out", 34.0, 39.0),
  ("D_far", 40.0, 44.0), ("D_near", 46.0, 54.0),
  ("E_hold", 56.0, 70.0),
  ("PRE_CHIME", 10.0, 40.0),
  # probe-bed near pass: seg C crosses within ~6-9 m of AMBIX_BED_POS, i.e.
  # inside refDistance -> weight 1. (A_far is 77 m out = past maxDistance =
  # weight exactly 0; D_near averages the mid-falloff.)
  ("P_near", 31.0, 35.0),
]

# G9 threshold — same number ork.vet.audio.py's dc_offset check uses, so a
# harness PASS and a vet PASS on the same render cannot disagree.
DC_LIMIT = 0.0010


###############################################################################
# CHILD — one lockstep engine boot; all engine imports live here.
###############################################################################

def _role_render(wav_path):
  import math
  import importlib.util

  import faulthandler
  faulthandler.enable()
  if os.environ.get("ORK_SA_HEARTBEAT", "0") == "1":
    faulthandler.dump_traceback_later(40, exit=False)

  import orkengine.core                     # core FIRST (import-order law)
  from orkengine.core import vec3, vec4, CrcStringProxy, lev2_pyexdir
  from orkengine import lev2
  from orkengine import ecs

  lev2_pyexdir.addToSysPath()
  from ork.app.application import ComponentizedApplication
  from ork.hypergraph.ecs import EcsRuntime

  tokens = CrcStringProxy()

  # import the scene module by path (the resolver idiom)
  spec = importlib.util.spec_from_file_location("scn_spatial_audio_showcase", SCENE_PY)
  scn_mod = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(scn_mod)

  total_secs = float(os.environ.get("ORK_SA_MAXSECS", scn_mod.WALK_TOTAL + TAIL_SECS))
  total_frames = int(total_secs * UPS)
  heartbeat = os.environ.get("ORK_SA_HEARTBEAT", "0") == "1"

  class App(ComponentizedApplication):

    def __init__(self):
      super().__init__()
      self.frame = 0
      self.wind = scn_mod.WindLayer()
      self.wind_ok = False
      self.wind_done = False
      self.exited = False
      self.runtime = EcsRuntime()
      no_audio = os.environ.get("ORK_SA_NOAUDIO", "0") == "1"
      audio_kw = dict(
          enable_audio=True,
          enable_audio_output=True,
          enable_audio_synth=True,
          audio_stream_sync=True,
          wav_output_path=wav_path) if not no_audio else dict()
      self.createEzApp(
          name="SpatialAudioHarness",
          offscreen=True,
          width=int(os.environ.get("ORK_SA_WIDTH", "960")),
          height=int(os.environ.get("ORK_SA_HEIGHT", "540")),
          msaa=int(os.environ.get("ORK_SA_MSAA", "2")),
          ssaa=0,
          # FREERUN, not lockstep: ezapp lockstep gates renders on COMPLETED
          # update ticks, while the ECS transport FSM blocks an update tick on
          # a render-thread gpu-phase rendezvous (_runGpuPhaseOnRenderThread)
          # -> deadlock at activation (observed; filed as an engine seam).
          # Freerun renders continuously (the ecsplay/player shape), and the
          # SYNC STREAM device is advanced MANUALLY 1/60 s per update tick, so
          # WAV time == sim time (both fixed-step, see fixed_sim_rate below)
          # regardless of wall rate.
          freerun=True,
          target_ups=UPS,
          target_fps=UPS,
          # pin the SIM clock too: freerun paces how often ticks are issued, this
          # sets how much game time each tick is worth. Wall-clock dt would leak
          # run-to-run jitter into the stochastic emitters' Poisson integration.
          fixed_sim_rate=UPS,
          pre_init_fns=[ecs.ecsInitCallback],
          **audio_kw)

    ##################################################
    # a REAL (offscreen) SceneGraphViewport must exist: the sim's scenegraph
    # publishes drawqueues through a triple buffer whose PRODUCER BLOCKS when
    # nothing consumes — a viewport-less sim wedges on update tick 3.
    ##################################################

    def _onUiInit(self):
      lg = self.ezapp.topLayoutGroup
      lg.margin = 0
      lg.clearColorStd = vec4(0.05, 0.05, 0.06, 1)
      vp_item = lg.makeChild(
          fill=True,
          margin=0,
          uiclass=lev2.ui.SceneGraphViewport,
          args=["Viewport", vec4(0.05, 0.05, 0.06, 1)])
      self.sgv = vp_item.widget

    ##################################################

    def _onGpuInit(self, ctx):
      self.sgv.forkDB()
      scene = scn_mod.AudioHarnessScene()
      sd = ecs.SceneData()
      scene.build(sd)
      self.runtime.scene_data = sd
      self.runtime.create_scenegraph()
      self.runtime.bind_to_viewport(self.sgv)
      self.sim_started = False
      print("CHILD_GPU_INITED", flush=True)

    ##################################################

    def _onGpuUpdate(self, ctx):
      self.gpu_updates = getattr(self, "gpu_updates", 0) + 1
      if heartbeat and self.gpu_updates <= 5:
        print("CHILD_GPUUPD n=%d" % self.gpu_updates, flush=True)
      self.runtime.gpuUpdate(ctx)

    ##################################################

    def _drive_listener(self, t):
      eye, tgt = scn_mod.listener_pose(t)
      self.runtime.controller.systemNotify(
          self.runtime._sys_ref,
          tokens.UpdateCamera,
          {
            tokens.eye: eye,
            tokens.tgt: tgt,
            tokens.up: vec3(0, 1, 0),
            tokens.near: 0.1,
            tokens.far: 500.0,
            tokens.fovy: math.radians(75.0),
          })

    ##################################################

    def _onUpdate(self, updata):
      if self.exited:
        return
      self.sgv.setDirty()
      # DEFERRED SIM START: starting in _onGpuInit (before any frame) makes
      # the ECS staging gpu-phase run Context::_loadingPhaseOperations OUTSIDE
      # a frame on the OFFSCREEN context — LightManager::gpuInit segfaults in
      # VkTextureInterface::initTextureArray2D (observed). After a few real
      # frames the loading phase has run in-frame and staging is safe.
      if not self.sim_started:
        if getattr(self, "gpu_updates", 0) >= 3:
          self.runtime.start_simulation()
          self.sim_started = True
          print("CHILD_SIM_STARTED", flush=True)
        return
      if self.runtime.controller is None:
        return

      # the walk (and the WAV clock) only tick once the SYNC audio device and
      # synth are live — before that, just keep the sim pumping.
      no_audio = os.environ.get("ORK_SA_NOAUDIO", "0") == "1"
      dev = None if no_audio else self.ezapp.audio_device
      synth = None if no_audio else self.ezapp.audio_synth
      if not no_audio and (dev is None or synth is None):
        self.runtime.controller.updateSimulation()
        return

      t = self.frame / UPS
      self.frame += 1
      if heartbeat and (self.frame <= 12 or (self.frame % int(UPS)) == 1):
        print("CHILD_TICK f=%d t=%.2f" % (self.frame, t), flush=True)

      if os.environ.get("ORK_SA_NOCAM", "0") != "1":
        self._drive_listener(t)
      self.runtime.controller.updateSimulation()

      # host wind layer (the runtime-SYNTHESIZED positioned source)
      if synth is not None:
        if not self.wind_ok and t >= 0.25:
          self.wind_ok = self.wind.setup(synth)
          print("CHILD_WIND_SETUP=%s" % self.wind_ok, flush=True)
        if self.wind_ok:
          self.wind.update(synth)

      # manual T1 offline advance: exactly one update delta of audio per tick.
      if dev is not None and os.environ.get("ORK_SA_NOADVANCE", "0") != "1":
        dev.advanceTime(1.0 / UPS)

      if self.frame >= total_frames and not self.exited:
        self.exited = True
        if self.wind_ok and not self.wind_done:
          self.wind.teardown(synth)
          self.wind_done = True
        print("CHILD_WALK_DONE frames=%d" % self.frame, flush=True)
        self.ezapp.signalExit()

  app = App()
  app.ezapp.mainThreadLoop()
  print("CHILD_FRAMES=%d" % app.frame, flush=True)
  print("CHILD_WIND=%d" % int(app.wind_ok), flush=True)
  sys.exit(0 if (app.frame >= total_frames and app.wind_ok) else 3)


###############################################################################
# DRIVER helpers — RIFF/float-WAV parse + band metrics (stdlib + numpy only).
###############################################################################

def _parse_wav(path):
  import struct
  import numpy as np
  with open(path, "rb") as f:
    data = f.read()
  if data[0:4] != b"RIFF" or data[8:12] != b"WAVE":
    raise ValueError("not a RIFF/WAVE file")
  fmt = None
  audio = None
  pos = 12
  while pos + 8 <= len(data):
    cid = data[pos:pos + 4]
    csz = struct.unpack("<I", data[pos + 4:pos + 8])[0]
    body = data[pos + 8:pos + 8 + csz]
    if cid == b"fmt ":
      fmt = body
    elif cid == b"data":
      audio = body
    pos += 8 + csz + (csz & 1)
  if fmt is None or audio is None:
    raise ValueError("missing fmt/data chunk")
  fmt_tag, channels, sr, _br, block_align, bits = struct.unpack("<HHIIHH", fmt[:16])
  is_float = (fmt_tag == 3)
  if fmt_tag == 0xFFFE and len(fmt) >= 26:
    is_float = (struct.unpack("<H", fmt[24:26])[0] == 3)
  frames = len(audio) // block_align if block_align else 0
  floats = np.frombuffer(audio[: frames * block_align], dtype="<f4")
  return dict(channels=channels, sr=sr, is_float=is_float, frames=frames,
              floats=floats)


def _bandfilter(x, sr, lo, hi):
  import numpy as np
  X = np.fft.rfft(x)
  f = np.fft.rfftfreq(len(x), 1.0 / sr)
  X[(f < lo) | (f > hi)] = 0.0
  return np.fft.irfft(X, len(x))


def _seg_rms(x, sr, t0, t1):
  import numpy as np
  a, b = int(t0 * sr), min(int(t1 * sr), len(x))
  if b <= a:
    return 0.0
  seg = x[a:b]
  return float(np.sqrt(np.mean(seg * seg)))


def _transient_count(x, sr, t0, t1, k=4.0, min_gap=0.3):
  """peaks of the 50ms RMS envelope above k x segment median."""
  import numpy as np
  a, b = int(t0 * sr), min(int(t1 * sr), len(x))
  seg = x[a:b]
  if seg.size < sr // 10:
    return 0
  win = int(0.05 * sr)
  env = np.sqrt(np.convolve(seg * seg, np.ones(win) / win, mode="same"))
  med = np.median(env) + 1e-9
  hot = env > (k * med)
  count = 0
  last = -1e9
  for i in np.flatnonzero(hot):
    if (i - last) > min_gap * sr:
      count += 1
    last = i
  return count


def _spawn_child(wav_path, timeout=1200):
  import subprocess
  env = dict(os.environ)
  env.pop("ORKID_DRM_MODE", None)           # never touch the physical display
  # the standing gate idiom: STREAM pinned BEFORE any engine import
  env["ORKID_AUDIO_IOCLASS"] = "STREAM"
  env["ORKID_AUDIO_STREAM_SYNC"] = "1"
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"
  cmd = ["ork.python", SELF, "--role", "render", "--wav", wav_path]
  p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, env=env)
  return p.returncode, (p.stdout or "") + (p.stderr or "")


###############################################################################
# DRIVER — spawn, measure, verdict.
###############################################################################

def _main_driver(args):
  import subprocess
  import numpy as np
  from ork.testing import verdict

  wav_path = args.wav or DEFAULT_WAV
  os.makedirs(os.path.dirname(wav_path), exist_ok=True)
  if os.path.exists(wav_path):
    os.remove(wav_path)

  try:
    rc, out = _spawn_child(wav_path)
  except subprocess.TimeoutExpired as e:
    print((e.stdout or "") + (e.stderr or ""))
    sys.exit(verdict(False, "child render timed out"))

  print(out)

  if not os.path.isfile(wav_path):
    sys.exit(verdict(False, "child rc=%d and no wav at %s" % (rc, wav_path)))

  w = _parse_wav(wav_path)
  sr = w["sr"]
  fl = w["floats"]
  L = fl[0::2].astype(np.float64)
  R = fl[1::2].astype(np.float64)
  mono = 0.5 * (L + R)
  dur = len(L) / float(sr)

  # ---- per-band, per-segment measurement table -----------------------------
  bandL = {}
  bandR = {}
  for name, (lo, hi) in BANDS.items():
    bandL[name] = _bandfilter(L, sr, lo, hi)
    bandR[name] = _bandfilter(R, sr, lo, hi)

  print("")
  print("MEASUREMENT TABLE (RMS x1e3; asym=(L-R)/(L+R))")
  hdr = "%-10s" % "segment"
  for b in BANDS:
    hdr += " | %14s" % ("band %s L/R/asym" % b)
  print(hdr)
  metrics = {}
  for label, t0, t1 in SEGMENTS:
    row = "%-10s" % label
    for b in BANDS:
      lr = _seg_rms(bandL[b], sr, t0, t1)
      rr = _seg_rms(bandR[b], sr, t0, t1)
      asym = (lr - rr) / (lr + rr + 1e-12)
      metrics[(label, b)] = (lr, rr, asym)
      row += " | %4.1f/%4.1f/%+.2f" % (lr * 1e3, rr * 1e3, asym)
    print(row)

  def tot(label, b):
    lr, rr, _ = metrics[(label, b)]
    return 0.5 * (lr + rr)

  def asym(label, b):
    return metrics[(label, b)][2]

  # ---- gates ---------------------------------------------------------------
  gates = []

  g1_ratio = tot("A_near", "S") / (tot("A_far", "S") + 1e-12)
  gates.append(("G1_water_approach ratio=%.2f (>2.0)" % g1_ratio, g1_ratio > 2.0))

  a_in, a_out = asym("B_in", "S"), asym("B_out", "S")
  g2 = (a_in * a_out < 0.0) and abs(a_in) > 0.05 and abs(a_out) > 0.05
  gates.append(("G2_water_flip asym %+0.2f -> %+0.2f (sign flip, |a|>0.05)"
                % (a_in, a_out), g2))

  w_in, w_out = asym("C_in", "W"), asym("C_out", "W")
  g3 = (w_in * w_out < 0.0) and abs(w_in) > 0.05 and abs(w_out) > 0.05
  gates.append(("G3_wind_flip asym %+0.2f -> %+0.2f (sign flip, |a|>0.05)"
                % (w_in, w_out), g3))

  g4_ratio = tot("D_near", "K") / (tot("D_far", "K") + 1e-12)
  gates.append(("G4_clank_approach ratio=%.2f (>1.8)" % g4_ratio, g4_ratio > 1.8))

  pre_c = tot("PRE_CHIME", "C")
  post_c = tot("E_hold", "C")
  g5_ratio = post_c / (pre_c + 1e-12)
  gates.append(("G5_runtime_chimes post/pre=%.2f (>4.0)" % g5_ratio, g5_ratio > 4.0))

  bmono = _bandfilter(mono, sr, *BANDS["B"])
  n_bird = _transient_count(bmono, sr, 26.0, 40.0)
  gates.append(("G6_bird_transients n=%d in seg C (>=2)" % n_bird, n_bird >= 2))

  rms_all = float(np.sqrt(np.mean(mono * mono)))
  expect_dur = 74.0
  g7 = (w["channels"] == 2 and w["is_float"] and sr == 48000
        and abs(dur - expect_dur) < 2.0 and rms_all > 0.001)
  gates.append(("G7_wav_sanity ch=%d float=%s sr=%d dur=%.1fs rms=%.4f"
                % (w["channels"], w["is_float"], sr, dur, rms_all), g7))

  # G8 — the SoundFieldProbe bed: audible at all, and radially attenuating.
  # Three points down the falloff (predicted radial weights 1.00 / 0.54 / 0.00
  # at 6 / 30 / 77 m from AMBIX_BED_POS) must come out strictly ordered, with
  # the far point back down at the band's no-probe noise floor.
  p_near, p_mid, p_far = tot("P_near", "P"), tot("D_near", "P"), tot("A_far", "P")
  g8_att = p_near / (p_mid + 1e-12)
  g8_off = p_mid / (p_far + 1e-12)
  g8 = (p_near > 1.0e-3) and g8_att > 1.4 and g8_off > 50.0
  gates.append(("G8_probe_bed near=%.4f mid=%.4f far=%.6f near/mid=%.2f (>1.4) "
                "mid/far=%.0f (>50)" % (p_near, p_mid, p_far, g8_att, g8_off), g8))

  # G9 — DC offset. Ratio-based band gates are blind to a DC pedestal (it scales
  # every segment alike), so this is scored on the raw channels, not a band.
  dc_l = float(np.mean(L))
  dc_r = float(np.mean(R))
  g9 = abs(dc_l) <= DC_LIMIT and abs(dc_r) <= DC_LIMIT
  gates.append(("G9_dc_offset dc_L=%+0.6f dc_R=%+0.6f (|dc|<=%.4f)"
                % (dc_l, dc_r, DC_LIMIT), g9))

  print("")
  for desc, ok in gates:
    print("  [%s] %s" % ("PASS" if ok else "FAIL", desc))

  passed = all(ok for _, ok in gates) and rc == 0
  if args.keep or not passed:
    print("\nWAV kept at: %s" % wav_path)
  detail = ("rc=%d gates=%d/%d wav=%s dur=%.1fs" %
            (rc, sum(1 for _, ok in gates if ok), len(gates), wav_path, dur))
  sys.exit(verdict(passed, detail))


###############################################################################

if __name__ == "__main__":
  ap = argparse.ArgumentParser()
  ap.add_argument("--role", default=None)
  ap.add_argument("--wav", default=None)
  ap.add_argument("--keep", action="store_true")
  args = ap.parse_args()

  if args.role == "render":
    _role_render(args.wav or DEFAULT_WAV)
  else:
    _main_driver(args)
