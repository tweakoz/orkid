#!/usr/bin/env ork.python
###############################################################################
# ork.frametime.harness.py — EXTERNAL wall-clock frame timing for
# ork.ecs.player.exe, and the agreement oracle for the in-engine HUD.
#
# WHY: the player's HUD once labeled its onDraw command-record bracket "frame",
# reading ~3ms while real frames took 14-52ms — the missing time was endFrame's
# present fence wait, outside the bracket. Every perf number downstream of that
# label was wrong. The engine now sources "frame" from the displayed-frame seam
# (OrkEzAppBase::_frame_period_ms) and names the old bracket "cpu-record"; this
# harness is the independent witness that keeps it honest.
#
# HOW: launch the player --offscreen-forever with ORKID_PLAYER_HUD_STDOUT set so
# the HUD prints every frame, timestamp the ARRIVAL of each "frame" line with an
# external monotonic clock, and compare:
#
#   G1 sample count   enough steady-state frames to mean anything
#   G2 AGREEMENT      external wall-clock mean vs the HUD's own "frame" mean,
#                     within --tolerance. Their divergence WAS the bug.
#   G3 subset sanity  "cpu-record" is present and <= frame time (it is a part
#                     of the frame, never the whole of it)
#
# Emits REALCAP/HUDCAP evidence lines, the gate table, and the machine verdict.
#
#   ork.frametime.harness.py                          # default cheap scene
#   ork.frametime.harness.py <scene.ecs> --seconds 30
#   ork.frametime.harness.py --mesh                   # terrain mesh-shader path
#   ork.frametime.harness.py --timestamps             # [t= gap=] prefixed log
###############################################################################

import os

os.environ["PYTHONUNBUFFERED"] = "1"
import re
import sys
import time
import signal
import argparse
import subprocess

# prepend THIS checkout's scripts dir so ork.testing resolves from this tree.
_ROOT = os.path.abspath(__file__)
for _ in range(3):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from ork.testing.verdict import verdict

DEFAULT_ECS = os.path.join(_ROOT, "ork.data", "ecsscenes", "ecsscn1.ecs")
DEFAULT_LOG = os.path.join(_ROOT, ".tmp", "frametime", "frametime.log")

# HUD rows, in LOCKSTEP with perfhud.h's _statsText:
#   "frame %5.2f ms (max %5.2f)"  <- honest wall-clock frame time
#   "cpu-record %5.2f ms"         <- onDraw record bracket (a SUBSET of the frame)
RE_FRAME = re.compile(r"^frame\s+([\d.]+) ms \(max\s+([\d.]+)\)")
RE_CPUREC = re.compile(r"^cpu-record\s+([\d.]+) ms")

BOOT_GRACE_S = 180.0  # asset load/settle headroom before the measurement window


###############################################################################


def _stats(vals):
  """n/mean/p50/p99/max/min over a sample list (empty -> None)."""
  if not vals:
    return None
  s = sorted(vals)
  n = len(s)

  def pct(p):
    return s[min(n - 1, int(round(p / 100.0 * (n - 1))))]

  return {
      "n": n,
      "mean": sum(s) / n,
      "p50": pct(50),
      "p99": pct(99),
      "max": s[-1],
      "min": s[0],
  }


def _fmt(tag, st):
  if st is None:
    return "%s NO_SAMPLES" % tag
  fps = (1000.0 / st["mean"]) if st["mean"] > 0.0 else float("nan")
  return ("%s n=%d mean=%.3f p50=%.3f p99=%.3f max=%.3f min=%.3f fps_from_mean=%.1f"
          % (tag, st["n"], st["mean"], st["p50"], st["p99"], st["max"], st["min"], fps))


def _child_env(args):
  env = os.environ.copy()
  env["PYTHONUNBUFFERED"] = "1"
  # every frame prints its HUD block -> one arrival timestamp per rendered frame.
  env["ORKID_PLAYER_HUD_STDOUT"] = "%g" % args.hud_period
  if args.mesh:
    env["ORKID_TERRAIN_MESHSHADER"] = "1"
  else:
    env.pop("ORKID_TERRAIN_MESHSHADER", None)
  # SELF-DEFENSE: ORKID_DRM_MODE is ambient in the linux seat env and routes the app
  # onto a REAL display (CtxDRM), silently overriding --offscreen-forever — the frames
  # then come from a path that is not the offscreen one we claim to measure. A timing
  # gate must own its render path, so drop it, loudly.
  if env.pop("ORKID_DRM_MODE", None) is not None:
    print("NOTE dropped ambient ORKID_DRM_MODE from the child env (offscreen gate owns its render path)")
  # mac: MoltenVK argument buffers are required by the engine's descriptor path.
  if sys.platform == "darwin":
    env.setdefault("MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS", "1")
  return env


def _capture(args):
  """Run the player and return (arrivals, hud_frame_ms, cpu_rec_ms, last_lines, rc).

  arrivals/hud_frame_ms are index-aligned: one entry per printed HUD frame row.
  """
  os.makedirs(os.path.dirname(os.path.abspath(args.log)), exist_ok=True)
  cmd = [args.player, args.ecs, "--offscreen-forever"] + args.extra
  print("LAUNCH cwd=%s cmd=%s" % (_ROOT, " ".join(cmd)))
  proc = subprocess.Popen(
      cmd,
      cwd=_ROOT,
      env=_child_env(args),
      stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT,
      bufsize=1,
      universal_newlines=True,
      preexec_fn=os.setsid)

  arrivals = []
  hud_ms = []
  cpu_ms = []
  last = {"frame": None, "cpu-record": None}
  t_start = time.monotonic()
  # the measurement window opens at the FIRST frame line (boot/asset load is not
  # frame time); BOOT_GRACE_S is the hang backstop until then.
  t_first = None
  t_hard = t_start + BOOT_GRACE_S + args.seconds + 30.0
  logf = open(args.log, "w")
  try:
    while True:
      line = proc.stdout.readline()
      now = time.monotonic()
      if not line:
        if proc.poll() is not None:
          break
        if now >= t_hard:
          break
        continue
      if args.timestamps:
        logf.write("[t=%8.3f] %s" % (now - t_start, line))
      else:
        logf.write(line)
      stripped = line.strip()
      m = RE_FRAME.match(stripped)
      if m:
        if t_first is None:
          t_first = now
        arrivals.append(now)
        hud_ms.append(float(m.group(1)))
        last["frame"] = stripped
      else:
        m = RE_CPUREC.match(stripped)
        if m:
          cpu_ms.append((now, float(m.group(1))))
          last["cpu-record"] = stripped
      if t_first is not None and now >= t_first + args.seconds:
        break
      if now >= t_hard:
        break
  finally:
    logf.close()
    try:
      os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
    except Exception:
      pass
    try:
      proc.wait(timeout=10)
    except Exception:
      try:
        os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
      except Exception:
        pass
  # --offscreen-forever is killed by US; rc is the signal, never a verdict input.
  return arrivals, hud_ms, cpu_ms, last, proc.returncode, t_first


###############################################################################


def main():
  ap = argparse.ArgumentParser(description="wall-clock frame timing + HUD agreement gate")
  ap.add_argument("ecs", nargs="?", default=DEFAULT_ECS, help="scene .ecs (default: %s)" % DEFAULT_ECS)
  ap.add_argument("--seconds", type=float, default=20.0, help="measurement window (default 20)")
  ap.add_argument("--warmup", type=float, default=3.0, help="discard this many seconds of frames (default 3)")
  ap.add_argument("--tolerance", type=float, default=0.15, help="max HUD-vs-wall-clock relative error (default 0.15)")
  ap.add_argument("--min-frames", type=int, default=30, help="minimum steady-state frames (default 30)")
  # The HUD print is THROTTLED by this period. Anything above the frame time decouples
  # prints from frames and the inter-arrival deltas stop being frame periods (an
  # unthrottled offscreen run hits 0.05ms/frame — 20x faster than a 1ms throttle).
  # 1e-6 = print every frame, which is what the measurement premise requires.
  ap.add_argument("--hud-period", type=float, default=1e-6, help="ORKID_PLAYER_HUD_STDOUT seconds (default 1e-6 = every frame)")
  ap.add_argument("--mesh", action="store_true", help="ORKID_TERRAIN_MESHSHADER=1")
  ap.add_argument("--timestamps", action="store_true", help="prefix every logged line with elapsed seconds")
  ap.add_argument("--log", default=DEFAULT_LOG, help="child stdout log path")
  ap.add_argument("--player", default="ork.ecs.player.exe", help="player executable")
  ap.add_argument("extra", nargs="*", help="extra player args (after --)")
  args = ap.parse_args()

  if args.seconds > 120.0:
    ap.error("--seconds > 120 : remote gate runs are bounded, keep it short")
  if not os.path.exists(args.ecs):
    ap.error("scene not found: %s" % args.ecs)

  arrivals, hud_ms, cpu_ms, last, rc, t_first = _capture(args)

  # steady state: drop the warmup seconds after the first frame line.
  if t_first is None:
    print("REALCAP NO_FRAMES (the player printed no HUD frame row)")
    sys.exit(verdict(False, "no frames; rc=%s log=%s" % (rc, args.log)))
  cutoff = t_first + args.warmup
  keep = [i for i, a in enumerate(arrivals) if a >= cutoff]
  wall_deltas = [(arrivals[i] - arrivals[i - 1]) * 1000.0 for i in keep if i > 0 and arrivals[i - 1] >= cutoff]
  hud_steady = [hud_ms[i] for i in keep if hud_ms[i] > 0.0]
  cpu_steady = [v for (t, v) in cpu_ms if t >= cutoff]

  wall = _stats(wall_deltas)
  hud = _stats(hud_steady)
  cpu = _stats(cpu_steady)

  # child_rc is EVIDENCE, not a verdict input: the normal exit is our own SIGTERM (-15).
  # Anything else (a signal, a nonzero code) means the player died mid-window.
  print("REALCAP ecs=%s mesh=%d window=%.1fs warmup=%.1fs n_frames_total=%d n_frames_steady=%d child_rc=%s span=%.1fs"
        % (args.ecs, int(args.mesh), args.seconds, args.warmup, len(arrivals), len(keep), rc,
           (arrivals[-1] - arrivals[0]) if len(arrivals) > 1 else 0.0))
  print(_fmt("REALCAP deltas_ms", wall))
  print(_fmt("HUDCAP  frame_ms ", hud))
  print(_fmt("HUDCAP  cpurec_ms", cpu))
  print("HUD SAMPLE frame      : %s" % last["frame"])
  print("HUD SAMPLE cpu-record : %s" % last["cpu-record"])

  gates = []
  gates.append(("G1_samples n_steady=%d (>=%d)" % (len(keep), args.min_frames), len(keep) >= args.min_frames))

  if wall and hud:
    rel = abs(hud["mean"] - wall["mean"]) / max(wall["mean"], 1e-9)
    gates.append(("G2_agreement hud=%.3fms wall=%.3fms rel_err=%.3f (<=%.2f)"
                  % (hud["mean"], wall["mean"], rel, args.tolerance), rel <= args.tolerance))
  else:
    gates.append(("G2_agreement NO_SAMPLES", False))

  if wall and cpu:
    # the record bracket is contained in the frame; 5% slack for sampling skew.
    ok = cpu["mean"] <= wall["mean"] * 1.05
    gates.append(("G3_cpurec_subset cpu-record=%.3fms <= frame=%.3fms" % (cpu["mean"], wall["mean"]), ok))
  else:
    gates.append(("G3_cpurec_subset NO_CPUREC_ROW (HUD label drift?)", False))

  # A crashed subject invalidates the window even when the samples already collected
  # look fine — the normal end is OUR SIGTERM (rc -15), or a clean exit (0).
  crash_ok = rc in (-signal.SIGTERM, 0, None)
  gates.append(("G4_subject_survived child_rc=%s (expect -%d SIGTERM or 0)" % (rc, int(signal.SIGTERM)), crash_ok))

  print("")
  for desc, ok in gates:
    print("  [%s] %s" % ("PASS" if ok else "FAIL", desc))

  passed = all(ok for _, ok in gates)
  detail = ("ecs=%s wall_mean=%.3fms hud_mean=%.3fms cpurec_mean=%.3fms gates=%d/%d log=%s"
            % (os.path.basename(args.ecs),
               wall["mean"] if wall else -1.0,
               hud["mean"] if hud else -1.0,
               cpu["mean"] if cpu else -1.0,
               sum(1 for _, ok in gates if ok), len(gates), args.log))
  sys.exit(verdict(passed, detail))


###############################################################################

if __name__ == "__main__":
  main()
