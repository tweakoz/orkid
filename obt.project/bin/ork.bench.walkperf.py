#!/usr/bin/env ork.python
###############################################################################
# ork.bench.walkperf.py — the STEREO WALK BENCH runner: play a walkable scene
# offscreen at VR stereo (2560x1280, no vsync) while a deterministic camera
# walks a GRAND TOUR of the map, log every displayed frame's wall time, then
# report percentiles PER LOCATION AND PER DIRECTION.
#
# LAUNCH (the --project flag is required, else the player has no asset root):
#
#   obt.env.launch.py --stagedir ~/.staging-jul03 --project ~/orkid \
#     --command 'ork.bench.walkperf.py scn_forest_procsky'
#
# HYGIENE this runner owns, because getting any of it wrong silently changes the
# number being measured:
#   ORKID_DRM_MODE / ORKID_VR_DRIVER  UNSET — a DRM swapchain seizes the display
#                                     and an XR runtime would take over
#                                     presentation (and its own pacing)
#   DISPLAY / WAYLAND_DISPLAY         UNSET — the live linux trap: with either one
#                                     leaked in, GLFW takes a real windowing
#                                     platform and the run measures a present
#                                     surface. PROVEN, not assumed: the run is
#                                     rejected unless its log says
#                                     "GLFW platform: NULL"
#   ORKID_PERFHUD=off                 the HUD is a scene-side draw cost
#   --offscreen                       hidden window, no present surface, so the
#                                     ezapp frame governor is disabled: FREERUN
#                                     (it also forces DRM off by itself)
#   ORKEXP_SCENE_WRAP                 preset -> FWDPBRVRDM + the walk driver, and
#                                     sky-IBL refiltering OFF by default
#                                     (BENCH_IBL=live restores it — see the wrap)
#
# NOTHING here keys on an exit code: obt.env.launch.py returns 0 for a signal death,
# so success is judged from artifacts — the witness exists, the frame log has frames,
# the run log proves the headless platform, and the game time reached is reported
# against the game time asked for. A validation run wants ORKID_VULKAN_VALIDATE=2
# (the bare name does nothing) and its own counting, since VUIDs suppress after 3.
#
# The player has no duration option (--offscreen-forever renders until killed),
# so the run ends on SIGTERM to the whole process group once the SIM has walked
# --duration GAME seconds (default: one lap of the tour). Game time, not a wall
# stopwatch: the tour is defined in sim seconds, and the scene's opening bake
# hitches would otherwise cost the run its last segments. A wall cap and a stall
# detector bound the run. The frame log is flushed every 64 frames precisely so
# that ending is lossless enough to measure — and so a mid-run crash still leaves
# a usable log: the tour announces every teleport, so a truncated run is still
# scoreable for the segments it reached.
#
# THREE LOGS come out of a run:
#   walltime.<tag>.log  the C++ per-displayed-frame log (index, period_ms)
#   witness.<tag>.log   the sim-side path witness — DETERMINISTIC, byte-comparable
#                       between two runs of the same tour
#   align.<tag>.log     THE CORRELATION, and the only non-deterministic artifact:
#                       one "<wall_s> <frame_index> <game_time>" sample per poll,
#                       taken by reading the tails of the other two logs. It is
#                       deliberately a SEPARATE file so the witness stays byte-
#                       comparable. The analyzer fits game_time against the frame
#                       log's own cumulative timeline from these samples, which is
#                       how a frame index becomes an eye position and a heading.
###############################################################################

import argparse
import os
import shutil
import signal
import subprocess
import sys
import time

WRAP = "ork.hypergraph.ecs.scene.bench_walk_wrap:wrap"

sys.path.insert(0, os.path.join(
    os.path.dirname(os.path.dirname(os.path.realpath(__file__))), "scripts"))


def parse_args():
  ap = argparse.ArgumentParser(description="deterministic stereo walk frame-time bench")
  ap.add_argument("scene", nargs="?", default="scn_forest_procsky",
                  help="scene name for ork.scene.viewer.py (default scn_forest_procsky)")
  ap.add_argument("--duration", type=float, default=None,
                  help="GAME seconds of walking before SIGTERM — the tour is defined in sim "
                       "time, so the run ends when the walk does, not after a wall stopwatch "
                       "(default: --laps laps of the tour; env BENCH_SECONDS overrides)")
  ap.add_argument("--laps", type=float, default=1.0,
                  help="tour laps to walk when --duration is not given (default 1)")
  ap.add_argument("--max-wall", type=float, default=None,
                  help="wall-clock cap on the walk, seconds (default: 2x the game-time "
                       "target plus 120)")
  ap.add_argument("--stall-timeout", type=float, default=25.0,
                  help="seconds without sim progress before the run is declared STALLED "
                       "(default 25)")
  ap.add_argument("--segments", type=int, default=None,
                  help="walk only the first N tour segments (WALK_SEGMENTS)")
  ap.add_argument("--seg-seconds", type=float, default=None,
                  help="seconds per segment (WALK_SEG_SECONDS; path default 40)")
  ap.add_argument("--load-timeout", type=float, default=300.0,
                  help="seconds to wait for the sim to go live before failing (default 300)")
  ap.add_argument("--size", default=None, metavar="WxH",
                  help="window size for the player (-W/-H). REQUIRED for a valid "
                       "BENCH_PRESET mono-vs-stereo comparison: the VR preset renders "
                       "two 1280x1280 eyes regardless of the window, while a mono preset "
                       "renders the WINDOW, which defaults to 1280x720 — so the two runs "
                       "differ in pixels as well as in passes unless the mono leg is given "
                       "--size 1280x1280 (one eye's worth)")
  ap.add_argument("--outdir", default="/tmp/orkid_walkperf",
                  help="directory for walltime / witness / align / run logs")
  ap.add_argument("--tag", default=None,
                  help="run tag (default: a timestamp) — names the logs")
  ap.add_argument("--warmup", type=float, default=4.0,
                  help="analyzer warm-up discard, seconds (default 4)")
  ap.add_argument("--settle", type=float, default=3.0,
                  help="analyzer per-segment teleport settle discard, seconds (default 3)")
  ap.add_argument("--no-analyze", action="store_true",
                  help="leave the logs on disk without running the analyzer")
  return ap.parse_args()


def tail_fields(path, want, nbytes=1024):
  """last COMPLETE line of a growing log, split — or None. A line still being
     written (no trailing newline yet) is skipped, never half-parsed."""
  try:
    with open(path, "rb") as f:
      f.seek(0, 2)
      size = f.tell()
      f.seek(max(0, size - nbytes))
      data = f.read().decode("utf-8", "ignore")
  except OSError:
    return None
  lines = data.split("\n")
  if len(lines) < 2:
    return None
  for line in reversed(lines[:-1]):     # lines[-1] is the incomplete tail
    parts = line.split()
    if len(parts) == want:
      return parts
  return None


def main():
  args = parse_args()

  viewer = shutil.which("ork.scene.viewer.py")
  if viewer is None:
    print("ork.bench.walkperf: ork.scene.viewer.py not on PATH — run this inside "
          "obt.env.launch.py --stagedir <staging> --project <orkid>", file=sys.stderr)
    return 2

  # the path knobs must be in the environment BEFORE the tour module is imported —
  # both this runner and the player read the same env to build the same path.
  if args.segments is not None:
    os.environ["WALK_SEGMENTS"] = str(args.segments)
  if args.seg_seconds is not None:
    os.environ["WALK_SEG_SECONDS"] = str(args.seg_seconds)
  from ork.bench import walk_path as WP

  duration = args.duration
  if duration is None:
    duration = float(os.environ.get("BENCH_SECONDS", args.laps * WP.TOUR_SECONDS))
  max_wall = args.max_wall if args.max_wall is not None else (2.0 * duration + 120.0)
  args.max_wall = max_wall

  tag = args.tag or time.strftime("%Y%m%d-%H%M%S")
  os.makedirs(args.outdir, exist_ok=True)
  walltime_log = os.path.join(args.outdir, "walltime.%s.log" % tag)
  witness_log  = os.path.join(args.outdir, "witness.%s.log" % tag)
  align_log    = os.path.join(args.outdir, "align.%s.log" % tag)
  run_log      = os.path.join(args.outdir, "run.%s.log" % tag)

  # a stale witness from a same-tag run would end the sim-live wait instantly, timing
  # the new run's load phase as if it were the walk
  for stale in (walltime_log, witness_log, align_log, run_log):
    if os.path.exists(stale):
      os.remove(stale)

  env = dict(os.environ)
  env.pop("ORKID_DRM_MODE", None)
  env.pop("ORKID_VR_DRIVER", None)
  # a leaked DISPLAY / WAYLAND_DISPLAY is the live linux trap: GLFW then picks the
  # real windowing platform and the run acquires a present surface (and its pacing)
  # instead of the headless NULL platform. --offscreen forces DRM off by itself, so
  # the display vars are what is left to get wrong.
  env.pop("DISPLAY", None)
  env.pop("WAYLAND_DISPLAY", None)
  env["ORKID_PERFHUD"] = "off"
  env["ORKEXP_SCENE_WRAP"] = WRAP
  env["ORKID_FRAME_WALLTIME_LOG"] = walltime_log
  env["WALK_WITNESS_LOG"] = witness_log

  cmd = [viewer, args.scene, "--offscreen"]
  if args.size is not None:
    try:
      w, h = (int(v) for v in args.size.lower().split("x"))
    except ValueError:
      print("ork.bench.walkperf: --size wants WxH, got %r" % args.size, file=sys.stderr)
      return 2
    cmd += ["-W", str(w), "-H", str(h)]
  print("ork.bench.walkperf: %s  (%.0fs)\n  preset %s  msaa %s\n  %s\n  walltime %s\n"
        "  witness  %s\n  align    %s\n  run log  %s" % (
            " ".join(cmd), duration, os.environ.get("BENCH_PRESET", "FWDPBRVRDM"),
            os.environ.get("BENCH_MSAA", "scene default"), WP.describe(), walltime_log,
            witness_log, align_log, run_log), flush=True)

  with open(run_log, "w") as rl:
    # own process group: the viewer EXECS the player, and the terminating signal
    # must reach whatever the tree became.
    proc = subprocess.Popen(cmd, env=env, stdout=rl, stderr=subprocess.STDOUT,
                            start_new_session=True)
    # A heavy scene spends MINUTES in asset load while the render loop is already
    # publishing (near-empty, sub-millisecond) frames. Timing the run from launch would
    # therefore measure mostly loading. The witness log's first line is written by the
    # walk driver's first system update, i.e. the instant the sim goes live — that is
    # where the walk (and the measurement) begins.
    died = None
    load_deadline = time.time() + args.load_timeout
    while not os.path.exists(witness_log):
      if proc.poll() is not None:
        died = proc.returncode
        break
      if time.time() > load_deadline:
        os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        proc.wait()
        print("ork.bench.walkperf: the sim never went live within %.0fs (no %s) — "
              "see %s" % (args.load_timeout, witness_log, run_log), file=sys.stderr)
        return 2
      time.sleep(0.25)
    # frames rendered BEFORE the walk began are load-phase frames, not samples of it
    skip_frames = 0
    if died is None and os.path.exists(walltime_log):
      with open(walltime_log) as wl:
        skip_frames = sum(1 for _ in wl)
      print("ork.bench.walkperf: sim live after %d load-phase frames; walking %.0fs of game "
            "time (wall cap %.0fs)" % (skip_frames, duration, max_wall), flush=True)
    # THE WALK ENDS ON GAME TIME, not on wall time: the tour is defined in sim
    # seconds, and a bake hitch or a slow first second would otherwise eat segments
    # off the end of the run. Two safety exits bound it — a wall cap, and a STALL
    # detector (game time stops advancing while the process lives), because the
    # known live-clock stall would otherwise burn the whole cap doing nothing.
    ended = "reached the end of the tour"
    wall_cap = time.time() + args.max_wall
    last_progress = time.time()
    last_gt = -1.0
    with open(align_log, "w", buffering=1) as al:
      # stamp the path knobs: re-analysing this log later must rebuild THIS tour, not
      # whatever the analyzer's environment happens to default to.
      al.write("# %s\n" % " ".join("%s=%s" % kv for kv in WP.env_state()))
      last_index = None
      while died is None:
        if proc.poll() is not None:
          died = proc.returncode
          break
        # THE CORRELATION SAMPLE. The frame log is flushed in blocks, so a sample only
        # says "frame N had been written by the time the sim stood at game time G".
        # Consecutive samples BRACKET each flush, and the analyzer fits the two
        # timelines from the brackets — see ork.bench.frametimes.py.
        wt = tail_fields(walltime_log, 2)
        wi = tail_fields(witness_log, 7)
        gt = None
        if wi is not None:
          try:
            gt = float(wi[0])
          except ValueError:
            gt = None
        if wt is not None and gt is not None:
          try:
            idx = int(wt[0])
          except ValueError:
            idx = None
          if idx is not None and idx != last_index:
            al.write("%.4f %d %.4f\n" % (time.monotonic(), idx, gt))
            last_index = idx
        if gt is not None and gt > last_gt:
          last_gt = gt
          last_progress = time.time()
        if last_gt >= duration:
          break
        if time.time() - last_progress > args.stall_timeout:
          ended = ("STALLED at game time %.1f of %.1f — %.0fs with no sim progress and no "
                   "new frames (the known silent render stall; the run is TRUNCATED and "
                   "scores only the segments it reached)" % (
                       last_gt, duration, args.stall_timeout))
          break
        if time.time() > wall_cap:
          ended = ("hit the %.0fs WALL CAP at game time %.1f of %.1f (the sim ran slower "
                   "than real time)" % (args.max_wall, last_gt, duration))
          break
        time.sleep(0.1)
    print("ork.bench.walkperf: walk ended — %s" % ended, flush=True)
    if died is None:
      os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
      try:
        proc.wait(timeout=20)
      except subprocess.TimeoutExpired:
        os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        proc.wait()
    else:
      print("ork.bench.walkperf: player EXITED EARLY at rc=%d — the run is TRUNCATED; "
            "the logs below cover only the segments it reached (see %s)" % (died, run_log),
            file=sys.stderr)

  # SENTINEL EVIDENCE, NOT rc. The launcher's exit code cannot be trusted (a signal
  # death comes back as 0 through obt.env.launch.py's os.system shift), so every
  # verdict below is drawn from artifacts the run itself had to produce.
  if not os.path.exists(walltime_log):
    print("ork.bench.walkperf: no walltime log at %s — the player never reached a "
          "displayed frame (see %s)" % (walltime_log, run_log), file=sys.stderr)
    return 2
  with open(run_log, errors="ignore") as rl:
    run_text = rl.read()
  if "GLFW platform: NULL" not in run_text:
    print("ork.bench.walkperf: the run log does NOT contain 'GLFW platform: NULL' — this "
          "run took a real windowing platform (leaked DISPLAY/WAYLAND_DISPLAY?), so its "
          "frame times carry a present surface's pacing and are NOT the offscreen "
          "measurement. See %s" % run_log, file=sys.stderr)
    return 2

  # THE SILENT RENDER STALL (observed on live-clock forest runs): the player keeps
  # ticking the sim at rc=0 while displayed frames simply STOP. The walltime log then
  # accounts for far less wall time than the run actually took, which is the only
  # in-band evidence — say it here instead of letting the analyzer report a short but
  # healthy-looking sample.
  walked = 0.0
  with open(walltime_log) as wl:
    for i, line in enumerate(wl):
      parts = line.split()
      if i >= skip_frames and len(parts) == 2:
        try:
          walked += float(parts[1]) / 1000.0
        except ValueError:
          pass
  print("ork.bench.walkperf: game time reached %.1f of %.1f; the frame log accounts for "
        "%.1fs of frames" % (last_gt, duration, walked), flush=True)
  if last_gt > 5.0 and walked < 0.6 * last_gt:
    print("ork.bench.walkperf: WARNING — the frame log covers only %.0f%% of the game time "
          "walked. Frames STOPPED while the sim ran on (the known silent render stall); the "
          "tables below score only what was rendered." % (100.0 * walked / last_gt),
          file=sys.stderr)

  if args.no_analyze:
    return 0

  analyzer = shutil.which("ork.bench.frametimes.py")
  if analyzer is None:
    print("ork.bench.walkperf: ork.bench.frametimes.py not on PATH", file=sys.stderr)
    return 2
  return subprocess.call([analyzer, walltime_log,
                          "--skip-frames", str(skip_frames),
                          "--warmup", str(args.warmup),
                          "--align", align_log,
                          "--settle", str(args.settle)])


if __name__ == "__main__":
  sys.exit(main())
