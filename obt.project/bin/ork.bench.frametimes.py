#!/usr/bin/env python3
###############################################################################
# ork.bench.frametimes.py — percentile report for an ORKID_FRAME_WALLTIME_LOG,
# and — given the bench runner's align log — the SAME frames scored PER LOCATION
# AND PER DIRECTION.
#
# The log is one "<frame_index> <period_ms>" line per DISPLAYED frame, written by
# EzTopWidget::DoDraw (true frame time: record + present wait + swap + pump).
#
#   ork.bench.frametimes.py /path/to/walltime.log
#   ork.bench.frametimes.py walltime.log --align align.log        # tour tables
#
# WARM-UP: the first --warmup seconds (default 4) are DISCARDED — a capture's
# opening second is shader JIT, asset streaming and cache priming, which is not
# the steady-state cost the report is about.
#
# MIN-SUSTAINED FPS is the headline number for a VR-style budget: the worst
# rolling 1-second window in the run, i.e. the FPS a viewer actually endures
# through the worst second, which p99 hides when the slow frames cluster.
#
# THE CORRELATION (--align). The frame log carries no clock, only periods, so its
# timeline is known up to one unknown offset; the sim's witness carries game time
# but not frames. The runner samples both tails, and each sample brackets one
# flush: "frame N was written while the sim went from game time A to B". Fitting
# game_time = rate * cumulative_frame_time + offset over those brackets ties the
# two together; the fit's RATE (expected ~1.0) and its RESIDUAL are printed, and
# the residual is the honest uncertainty on every position/heading attribution
# below. Frames within --settle seconds of a TELEPORT are dropped from the
# per-segment tables — the teleport's streaming spike belongs to no location.
###############################################################################

import argparse
import math
import os
import sys

sys.path.insert(0, os.path.join(
    os.path.dirname(os.path.dirname(os.path.realpath(__file__))), "scripts"))


def parse_log(path):
  """[(frame_index, period_ms)] — the log's own indices, not positions."""
  out = []
  with open(path) as f:
    for line in f:
      parts = line.split()
      if len(parts) != 2:
        continue
      try:
        out.append((int(parts[0]), float(parts[1])))
      except ValueError:
        continue
  return out


def parse_align(path):
  """samples, plus the path knobs the run stamped on the head of the log — applied to
     the environment so the tour module rebuilds the tour that was WALKED."""
  out = []
  stamped = []
  with open(path) as f:
    for line in f:
      if line.startswith("#"):
        for kv in line[1:].split():
          if "=" in kv:
            k, v = kv.split("=", 1)
            os.environ[k] = v
            stamped.append(kv)
        continue
      parts = line.split()
      if len(parts) != 3:
        continue
      try:
        out.append((float(parts[0]), int(parts[1]), float(parts[2])))
      except ValueError:
        continue
  return out, stamped


def percentile(sorted_vals, pct):
  if not sorted_vals:
    return float("nan")
  k = (len(sorted_vals) - 1) * (pct / 100.0)
  lo = int(k)
  hi = min(lo + 1, len(sorted_vals) - 1)
  return sorted_vals[lo] + (sorted_vals[hi] - sorted_vals[lo]) * (k - lo)


def worst_window(periods, window_s=1.0):
  """(fps, i, j) of the worst rolling window_s-second span. Two-pointer over the
     frame periods: every window that REACHES the full second is scored; a trailing
     partial window is not (it would score a fraction of a second as if it were one)."""
  worst = None
  acc = 0.0
  j = 0
  for i in range(len(periods)):
    while j < len(periods) and acc < window_s * 1000.0:
      acc += periods[j]
      j += 1
    if acc < window_s * 1000.0:
      break
    fps = (j - i) / (acc / 1000.0)
    if worst is None or fps < worst[0]:
      worst = (fps, i, j)
    acc -= periods[i]
  return worst


def min_sustained_fps(periods, window_s=1.0):
  w = worst_window(periods, window_s)
  return None if w is None else w[0]


def fit_align(samples, cumtime):
  """game_time = rate * cumulative_frame_time + offset, from bracketed flushes.
     Returns (rate, offset, n, residual_rms) or None."""
  pts = []
  for k in range(1, len(samples)):
    idx = samples[k][1]
    if idx not in cumtime:
      continue
    gt_mid = 0.5 * (samples[k - 1][2] + samples[k][2])
    pts.append((cumtime[idx], gt_mid))
  if len(pts) < 3:
    return None
  n = float(len(pts))
  sx = sum(p[0] for p in pts)
  sy = sum(p[1] for p in pts)
  sxx = sum(p[0] * p[0] for p in pts)
  sxy = sum(p[0] * p[1] for p in pts)
  den = n * sxx - sx * sx
  if abs(den) < 1e-9:
    return None
  rate = (n * sxy - sx * sy) / den
  off = (sy - rate * sx) / n
  rms = math.sqrt(sum((rate * x + off - y) ** 2 for x, y in pts) / n)
  return (rate, off, len(pts), rms)


def block_stats(periods):
  srt = sorted(periods)
  total_s = sum(periods) / 1000.0
  return dict(n=len(periods), secs=total_s,
              p50=percentile(srt, 50), p95=percentile(srt, 95),
              p99=percentile(srt, 99), mx=srt[-1],
              fps=len(periods) / total_s if total_s > 0 else float("nan"),
              msf=min_sustained_fps(periods))


def main():
  ap = argparse.ArgumentParser(description="frame-time percentiles for a walltime log")
  ap.add_argument("log", help="ORKID_FRAME_WALLTIME_LOG path")
  ap.add_argument("--warmup", type=float, default=4.0,
                  help="seconds discarded from the head of the run (default 4)")
  ap.add_argument("--skip-frames", type=int, default=0,
                  help="frames discarded BEFORE the warm-up window — the load-phase frames "
                       "the runner counted before the sim went live (default 0)")
  ap.add_argument("--window", type=float, default=1.0,
                  help="min-sustained-FPS window, seconds (default 1)")
  ap.add_argument("--align", default=None,
                  help="the runner's align log — enables the per-location/per-direction tables")
  ap.add_argument("--settle", type=float, default=3.0,
                  help="seconds after each teleport dropped from the per-segment tables "
                       "(default 3)")
  ap.add_argument("--pairs", type=int, default=10,
                  help="how many worst location x direction pairs to rank (default 10)")
  args = ap.parse_args()

  full = parse_log(args.log)
  if not full:
    print("ork.bench.frametimes: %s has no frame samples" % args.log, file=sys.stderr)
    return 2

  # cumulative wall timeline of the WHOLE log — the frame log has periods but no clock,
  # so this is the only timeline it owns.
  cumtime = {}
  t = 0.0
  for idx, p in full:
    t += p / 1000.0
    cumtime[idx] = t

  fit = None
  stamped = []
  if args.align is not None:
    if not os.path.exists(args.align):
      print("ork.bench.frametimes: no align log at %s — location scoring skipped"
            % args.align, file=sys.stderr)
      return 2
    samples, stamped = parse_align(args.align)
    fit = fit_align(samples, cumtime)

  # WHERE THE WALK STARTS. The runner's --skip-frames is only a lower bound: it counts
  # the log at the instant the sim goes live, and the render loop then spins THOUSANDS
  # more near-empty frames (tens of microseconds each) before the first walked frame.
  # Left in, they halve the reported p50 and triple the mean FPS. With a correlation the
  # cut is exact — the first frame whose fitted game time is not negative.
  walk_start = args.skip_frames
  if fit is not None:
    rate, off, _, _ = fit
    for k, (idx, p) in enumerate(full):
      if rate * (cumtime[idx] - 0.5 * p / 1000.0) + off >= 0.0:
        walk_start = max(walk_start, k)
        break
  raw = full[walk_start:]
  if not raw:
    print("ork.bench.frametimes: %s has %d samples, all of them before the walk"
          % (args.log, len(full)), file=sys.stderr)
    return 2

  # discard the warm-up by ELAPSED TIME, not by frame count
  acc = 0.0
  cut = len(raw)
  for i, (_, p) in enumerate(raw):
    acc += p
    if acc >= args.warmup * 1000.0:
      cut = i + 1
      break
  scored = raw[cut:]
  if len(scored) < 2:
    print("ork.bench.frametimes: %s has %d frames after a %.1fs warm-up discard — "
          "run longer or lower --warmup" % (args.log, len(scored), args.warmup),
          file=sys.stderr)
    return 2

  periods = [p for _, p in scored]
  srt = sorted(periods)
  total_s = sum(periods) / 1000.0
  msf = min_sustained_fps(periods, args.window)

  print("log             : %s" % args.log)
  print("frames (raw)    : %d" % len(full))
  print("load-phase skip : %d frames (%s)" % (
      walk_start, "correlated to game time 0" if fit is not None else "--skip-frames"))
  print("warm-up discard : %.1fs (%d frames)" % (args.warmup, cut))
  print("frames (scored) : %d over %.2fs" % (len(periods), total_s))
  print("frame time ms   : p50 %.2f  p95 %.2f  p99 %.2f  p99.9 %.2f  max %.2f" % (
      percentile(srt, 50), percentile(srt, 95), percentile(srt, 99),
      percentile(srt, 99.9), srt[-1]))
  print("mean FPS        : %.2f" % (len(periods) / total_s))
  print("min-sustained   : %s FPS (worst rolling %.0fs window)" % (
      ("%.2f" % msf) if msf is not None else "n/a (run shorter than the window)",
      args.window))

  # A multi-SECOND frame is asset load (bake / prefilter / shader JIT), not a rendering
  # hitch, and one of them drags the mean and DEFINES the min-sustained window. Say so
  # LOUDLY with the warm-up that clears it rather than quietly reporting a fiction — the
  # samples are never dropped silently, the reader decides.
  stall_ms = max(200.0, 20.0 * percentile(srt, 50))
  last_stall = None
  acc = 0.0
  for i, p in enumerate(periods):
    acc += p
    if p > stall_ms:
      last_stall = (i, p, acc)
  if last_stall is not None:
    idx, worst, at_ms = last_stall
    print("WARNING         : the scored window still contains LOAD-SCALE frames (>%.0fms; "
          "last is %.0fms at +%.1fs). Re-run the analyzer with --warmup %.0f to score "
          "steady state only." % (stall_ms, worst, at_ms / 1000.0,
                                  args.warmup + at_ms / 1000.0 + 1.0))

  if args.align is None:
    return 0
  if fit is None:
    print("\nork.bench.frametimes: %s has too few usable correlation samples — location "
          "scoring skipped" % args.align, file=sys.stderr)
    return 2
  return report_tour(args, scored, cumtime, fit, stamped)


def report_tour(args, scored, cumtime, fit, stamped):
  from ork.bench import walk_path as WP

  rate, off, npts, rms = fit
  if stamped:
    print("\npath knobs      : %s (from the align log)" % " ".join(stamped))
  print("\ncorrelation     : game_time = %.5f * frame_clock %+.3f  (%d bracketed flushes, "
        "residual %.3fs)" % (rate, off, npts, rms))
  print("tour            : %s" % WP.describe())
  if abs(rate - 1.0) > 0.05:
    print("WARNING         : the sim clock ran at %.3fx the frame clock — a position "
          "attribution error of that scale is baked into the tables below." % rate)

  # attribute every scored frame to a location and a direction
  frames = []       # (period_ms, state)
  for idx, p in scored:
    gt = rate * (cumtime[idx] - 0.5 * p / 1000.0) + off
    frames.append((p, WP.state(gt)))
  gt_lo = frames[0][1]["t"]
  gt_hi = frames[-1][1]["t"]
  print("walk covered    : game time %.1f..%.1fs (%.2f laps of the tour)" % (
      gt_lo, gt_hi, (gt_hi - gt_lo) / WP.TOUR_SECONDS))

  kept = [(p, s) for p, s in frames if s["t_seg"] >= args.settle]
  print("teleport settle : %.1fs after each segment start dropped (%d of %d frames)" % (
      args.settle, len(frames) - len(kept), len(frames)))
  if not kept:
    print("ork.bench.frametimes: every frame fell inside a teleport settle window",
          file=sys.stderr)
    return 2

  # ---- per segment ---------------------------------------------------------
  by_seg = {}
  for p, s in kept:
    by_seg.setdefault(s["seg"], []).append((p, s))
  print("\nPER-SEGMENT (worst rolling %.0fs window inside each segment, and where the eye "
        "was for it)" % args.window)
  hdr = "%-4s %-15s %-16s %6s %7s %7s %7s %7s %8s %8s" % (
      "seg", "name", "class", "frames", "secs", "p50ms", "p95ms", "maxms", "meanFPS",
      "minsust")
  print(hdr)
  print("-" * len(hdr))
  for si in sorted(by_seg):
    rows = by_seg[si]
    st = block_stats([p for p, _ in rows])
    seg = WP.SEGMENTS[si]
    print("%-4d %-15s %-16s %6d %7.1f %7.2f %7.2f %7.2f %8.2f %8s" % (
        si, seg["name"], seg["cls"], st["n"], st["secs"], st["p50"], st["p95"], st["mx"],
        st["fps"], ("%.2f" % st["msf"]) if st["msf"] is not None else "n/a"))
    w = worst_window([p for p, _ in rows], args.window)
    if w is not None:
      _, i0, i1 = w
      mid = rows[(i0 + i1) // 2][1]
      print("     worst %.0fs: %.2f FPS at eye (%.1f, %.1f, %.1f) heading %.0f deg (%s), "
            "t_seg %.1fs lap %d" % (
                args.window, w[0], mid["eye"][0], mid["eye"][1], mid["eye"][2],
                mid["heading"], WP.heading_label(WP.heading_bin(mid["heading"])),
                mid["t_seg"], mid["lap"]))

  # ---- per segment x direction --------------------------------------------
  pairs = {}
  runs = {}
  prev_key = None
  for p, s in kept:
    key = (s["seg"], WP.heading_bin(s["heading"]))
    pairs.setdefault(key, []).append(p)
    if key != prev_key:
      runs.setdefault(key, []).append([])
      prev_key = key
    runs[key][-1].append(p)
  rank = []
  for key, ps in pairs.items():
    if len(ps) < 4:
      continue
    srt = sorted(ps)
    secs = sum(ps) / 1000.0
    worst_run = None
    for r in runs[key]:
      m = min_sustained_fps(r, args.window)
      if m is not None and (worst_run is None or m < worst_run):
        worst_run = m
    rank.append((sum(ps) / len(ps), key, len(ps), secs, percentile(srt, 50),
                 percentile(srt, 95), worst_run))
  rank.sort(reverse=True)
  print("\nWORST LOCATION x DIRECTION (ranked by mean frame time; heading bins of %.0f deg, "
        "+Z called N)" % (360.0 / WP.HEADINGS))
  hdr = "%-4s %-15s %-5s %6s %7s %7s %7s %8s %8s" % (
      "seg", "name", "dir", "frames", "secs", "meanms", "p95ms", "meanFPS", "minsust")
  print(hdr)
  print("-" * len(hdr))
  show = rank[:args.pairs]
  for mean_ms, key, n, secs, p50, p95, msf in show:
    si, hb = key
    print("%-4d %-15s %-5s %6d %7.1f %7.2f %7.2f %8.2f %8s" % (
        si, WP.SEGMENTS[si]["name"], WP.heading_label(hb), n, secs, mean_ms, p95,
        1000.0 / mean_ms, ("%.2f" % msf) if msf is not None else "n/a"))
  if len(rank) > args.pairs:
    print("... %d more pairs; the FASTEST is:" % (len(rank) - args.pairs))
    mean_ms, key, n, secs, p50, p95, msf = rank[-1]
    si, hb = key
    print("%-4d %-15s %-5s %6d %7.1f %7.2f %7.2f %8.2f %8s" % (
        si, WP.SEGMENTS[si]["name"], WP.heading_label(hb), n, secs, mean_ms, p95,
        1000.0 / mean_ms, ("%.2f" % msf) if msf is not None else "n/a"))
  return 0


if __name__ == "__main__":
  sys.exit(main())
