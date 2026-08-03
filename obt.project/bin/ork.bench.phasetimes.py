#!/usr/bin/env python3
###############################################################################
# ork.bench.phasetimes.py — per-PHASE GPU percentile report for an
# ORKID_PROFILER_DUMP file (the headless twin of the Shift+~ ProfilerView).
#
# The dump is one whitespace-separated line per (frame, series) that RAN:
#
#   <t_s> <channel> <frame> <series> <total_ms> <isolated_ms> <count>
#
# written by ork::ProfilerChannel::frameEnd in a --profiler build. Series that
# did not run in a frame emit no line, so "frames" below is how many of the
# scored frames the phase actually appeared in — a phase that runs every third
# frame is scored on the frames it ran, and its share-of-frame is amortized
# separately (share_amort) so both readings are visible.
#
#   ork.bench.phasetimes.py /path/to/phases.log
#   ork.bench.phasetimes.py phases.log --warmup 90 --channel GPU
#
# TOTAL vs ISOLATED: these series NEST (fwd:total brackets the passes inside
# it). total_ms is the whole bracket including children; isolated_ms excludes
# them. The share column is against --total (default fwd:total), so the shares
# of the phases nested inside it are the ones that add up.
#
# WARM-UP: the first --warmup seconds are DISCARDED, as in ork.bench.frametimes.
# The dump's clock starts at the FIRST profiled frame — which on a big scene is
# still deep in loading — so a scene with a long load needs a --warmup past it
# (or --skip-frames). Load-scale frames surviving into the scored window are
# called out LOUDLY rather than quietly averaged in.
###############################################################################

import argparse
import sys


def parse_dump(path):
  """[(t_s, channel, frame, series, total_ms, iso_ms, count)] in file order."""
  out = []
  bad = 0
  with open(path) as f:
    for line in f:
      if line.startswith("#"):
        continue
      parts = line.split()
      if len(parts) != 7:
        bad += 1
        continue
      try:
        out.append((float(parts[0]), parts[1], int(parts[2]), parts[3],
                    float(parts[4]), float(parts[5]), int(parts[6])))
      except ValueError:
        bad += 1
        continue
  return out, bad


def percentile(sorted_vals, pct):
  if not sorted_vals:
    return float("nan")
  k = (len(sorted_vals) - 1) * (pct / 100.0)
  lo = int(k)
  hi = min(lo + 1, len(sorted_vals) - 1)
  return sorted_vals[lo] + (sorted_vals[hi] - sorted_vals[lo]) * (k - lo)


def report_channel(channel, rows, args):
  """rows: the channel's scored rows. Returns nothing; prints one table."""
  frames = sorted({r[2] for r in rows})
  by_series = {}
  for _, _, frame, series, total_ms, iso_ms, count in rows:
    s = by_series.setdefault(series, dict(total=[], iso=[], calls=0))
    s["total"].append(total_ms)
    s["iso"].append(iso_ms)
    s["calls"] += count

  stats = {}
  for name, s in by_series.items():
    srt = sorted(s["total"])
    stats[name] = dict(
        n=len(srt), calls=s["calls"],
        p50=percentile(srt, 50), p95=percentile(srt, 95),
        p99=percentile(srt, 99), mx=srt[-1],
        p50iso=percentile(sorted(s["iso"]), 50),
        sum=sum(s["total"]))

  # the denominator for share-of-frame. Explicit, with the whole-frame series as the
  # documented fallback — a share against a phase that was not measured is a fiction.
  ref = None
  for cand in [args.total, "gfx:all"]:
    if cand in stats:
      ref = cand
      break

  print("\nchannel         : %s" % channel)
  print("frames (scored) : %d  (frame index %d..%d, %.1fs of dump clock)" % (
      len(frames), frames[0], frames[-1], rows[-1][0] - rows[0][0]))
  if ref is None:
    print("share reference : NONE — neither '%s' nor 'gfx:all' present in this channel"
          % args.total)
  else:
    print("share reference : %s (p50 %.3f ms)%s" % (
        ref, stats[ref]["p50"], "" if ref == args.total else
        "  [--total '%s' absent, fell back]" % args.total))
    print("columns         : share = phase p50 / %s p50 (cost on the frames it runs);"
          " amort = phase mean per SCORED frame / %s p50" % (ref, ref))

  hdr = "%-22s %7s %7s %8s %8s %8s %8s %8s %8s %8s" % (
      "series", "frames", "calls", "p50ms", "p95ms", "p99ms", "maxms", "p50iso",
      "share", "amort")
  print(hdr)
  print("-" * len(hdr))
  order = sorted(stats.items(), key=lambda kv: -kv[1]["p50"])
  for name, st in order:
    if ref is None or stats[ref]["p50"] <= 0.0:
      share = amort = float("nan")
    else:
      # share    : the cost of the phase ON THE FRAMES IT RUNS, vs a median frame.
      # amort    : the same cost spread over ALL scored frames — what removing the
      #            phase entirely would give back on average.
      share = 100.0 * st["p50"] / stats[ref]["p50"]
      amort = 100.0 * (st["sum"] / len(frames)) / stats[ref]["p50"]
    print("%-22s %7d %7d %8.3f %8.3f %8.3f %8.3f %8.3f %7.1f%% %7.1f%%" % (
        name, st["n"], st["calls"], st["p50"], st["p95"], st["p99"], st["mx"],
        st["p50iso"], share, amort))

  # A load-scale frame (shader JIT, prefilter bake) inside the scored window defines the
  # max column and skews p99. Say so with the --warmup that clears it; never drop silently.
  if ref is not None:
    stall_ms = max(200.0, 20.0 * stats[ref]["p50"])
    worst = None
    for t_s, _, _, series, total_ms, _, _ in rows:
      if series == ref and total_ms > stall_ms:
        worst = (t_s, total_ms)
    if worst is not None:
      print("WARNING         : the scored window still contains LOAD-SCALE frames "
            "(%s >%.0fms; last is %.0fms at dump t=%.1fs). Re-run with --warmup %.0f "
            "to score steady state only." % (
                ref, stall_ms, worst[1], worst[0], worst[0] + 1.0))


def main():
  ap = argparse.ArgumentParser(
      description="per-phase GPU percentiles for an ORKID_PROFILER_DUMP file")
  ap.add_argument("dump", help="ORKID_PROFILER_DUMP path")
  ap.add_argument("--warmup", type=float, default=4.0,
                  help="seconds discarded from the head of the dump (default 4)")
  ap.add_argument("--skip-frames", type=int, default=0,
                  help="frames with an index below this are discarded BEFORE the warm-up "
                       "window (default 0)")
  ap.add_argument("--channel", default=None,
                  help="report only channels whose name contains this (default: all)")
  ap.add_argument("--total", default="fwd:total",
                  help="series used as the share-of-frame denominator (default fwd:total)")
  args = ap.parse_args()

  rows, bad = parse_dump(args.dump)
  if not rows:
    print("ork.bench.phasetimes: %s has no samples (%d unparsable lines)" % (
        args.dump, bad), file=sys.stderr)
    return 2

  t0 = rows[0][0]
  kept = [r for r in rows
          if r[2] >= args.skip_frames and (r[0] - t0) >= args.warmup]

  print("dump            : %s" % args.dump)
  print("samples (raw)   : %d over %.1fs%s" % (
      len(rows), rows[-1][0] - t0,
      "" if bad == 0 else "  (%d unparsable lines skipped)" % bad))
  print("warm-up discard : %.1fs + frames below %d (%d samples dropped)" % (
      args.warmup, args.skip_frames, len(rows) - len(kept)))

  if not kept:
    print("ork.bench.phasetimes: %s has no samples after a %.1fs warm-up discard — "
          "run longer or lower --warmup" % (args.dump, args.warmup), file=sys.stderr)
    return 2

  channels = []
  for r in kept:
    if r[1] not in channels:
      channels.append(r[1])
  if args.channel is not None:
    channels = [c for c in channels if args.channel in c]
    if not channels:
      print("ork.bench.phasetimes: no channel matches '%s'" % args.channel,
            file=sys.stderr)
      return 2

  for ch in channels:
    report_channel(ch, [r for r in kept if r[1] == ch], args)
  return 0


if __name__ == "__main__":
  sys.exit(main())
