#!/usr/bin/env python3
###############################################################################
# phasetimes_report.py — unit tests for ork.bench.phasetimes.py, the analyzer
# that turns an ORKID_PROFILER_DUMP into per-phase percentiles.
#
# Runs STANDALONE with plain CPython — no engine, no GPU, no staging:
#     python3 obt.project/unittests/phasetimes_report.py
#
# WHAT THIS IS: synthetic dumps whose answer is known by construction, so the
# aggregation is checked against arithmetic rather than against a previous run
# of itself. The dump FORMAT is the contract under test as much as the math —
# a column added or reordered in ProfilerChannel::frameEnd breaks these.
#
# The properties that make the report trustworthy:
#   * percentiles are read off a known distribution (0..99 ms, one per frame);
#   * a phase that runs INTERMITTENTLY is scored on the frames it ran (share)
#     AND spread over every scored frame (amort) — conflating the two is how a
#     once-every-third-frame cost gets reported as three times its real weight;
#   * warm-up discards by the dump's own clock, and by frame index;
#   * channels are independent (the GPU channel's phases never mix with a CPU
#     channel's), and a channel with no share reference still reports times;
#   * junk lines are skipped and counted, never parsed into fake samples.
###############################################################################

import importlib.util
import io
import os
import sys
import tempfile
import unittest
from contextlib import redirect_stdout

_BIN = os.path.join(
    os.path.dirname(os.path.dirname(os.path.realpath(__file__))), "bin",
    "ork.bench.phasetimes.py")


def _load_analyzer():
  spec = importlib.util.spec_from_file_location("ork_bench_phasetimes", _BIN)
  mod = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(mod)
  return mod


PT = _load_analyzer()

HEADER = "# ORKID_PROFILER_DUMP v1: t_s channel frame series total_ms isolated_ms count\n"


def line(t_s, channel, frame, series, total_ms, iso_ms=None, count=1):
  if iso_ms is None:
    iso_ms = total_ms
  return "%.4f %s %d %s %.4f %.4f %d\n" % (t_s, channel, frame, series, total_ms,
                                           iso_ms, count)


def write_dump(text):
  fd, path = tempfile.mkstemp(suffix=".phases.log")
  with os.fdopen(fd, "w") as f:
    f.write(text)
  return path


def run_report(path, **kwargs):
  """the analyzer's main() on a dump; returns (rc, stdout)."""
  argv = [_BIN, path]
  for k, v in kwargs.items():
    argv += ["--" + k.replace("_", "-"), str(v)]
  old = sys.argv
  buf = io.StringIO()
  try:
    sys.argv = argv
    with redirect_stdout(buf):
      rc = PT.main()
  finally:
    sys.argv = old
  return rc, buf.getvalue()


def row(out, series):
  """the report's row for a series, as a list of fields."""
  for ln in out.splitlines():
    parts = ln.split()
    if parts and parts[0] == series:
      return parts
  return None


class TestPercentiles(unittest.TestCase):

  def test_known_distribution(self):
    """0..99 ms, one frame each: the percentiles are arithmetic, not opinion."""
    srt = [float(i) for i in range(100)]
    self.assertAlmostEqual(PT.percentile(srt, 50), 49.5)
    self.assertAlmostEqual(PT.percentile(srt, 95), 94.05)
    self.assertAlmostEqual(PT.percentile(srt, 99), 98.01)
    self.assertAlmostEqual(PT.percentile(srt, 0), 0.0)
    self.assertAlmostEqual(PT.percentile(srt, 100), 99.0)

  def test_empty_series_is_nan(self):
    self.assertNotEqual(PT.percentile([], 50), PT.percentile([], 50))  # nan


class TestParse(unittest.TestCase):

  def test_columns_and_junk(self):
    text = HEADER
    text += line(0.5, "GPU:main", 7, "fwd:total", 10.25, 2.0, 1)
    text += "this is not a sample line\n"
    text += "0.6 GPU:main NOTANINT fwd:total 1 1 1\n"
    path = write_dump(text)
    rows, bad = PT.parse_dump(path)
    os.unlink(path)
    self.assertEqual(len(rows), 1)
    self.assertEqual(bad, 2)
    t_s, ch, frame, series, total_ms, iso_ms, count = rows[0]
    self.assertAlmostEqual(t_s, 0.5)
    self.assertEqual(ch, "GPU:main")
    self.assertEqual(frame, 7)
    self.assertEqual(series, "fwd:total")
    self.assertAlmostEqual(total_ms, 10.25)
    self.assertAlmostEqual(iso_ms, 2.0)
    self.assertEqual(count, 1)

  def test_empty_dump_is_a_loud_failure(self):
    path = write_dump(HEADER)
    rc, out = run_report(path)
    os.unlink(path)
    self.assertEqual(rc, 2)


class TestReport(unittest.TestCase):

  def _dump(self, nframes=100):
    """fwd:total = 10ms every frame; color_pass = 6ms every frame;
       sky_ibl = 3ms on every THIRD frame. Shares: 60% and 30%; sky_ibl amortizes
       to 10% of a frame. Frames 0..9 sit inside a 1.0s warm-up."""
    text = HEADER
    t = 0.0
    for frame in range(nframes):
      text += line(t, "GPU:main", frame, "fwd:total", 10.0, 1.0)
      text += line(t, "GPU:main", frame, "fwd:color_pass", 6.0)
      if 0 == (frame % 3):
        text += line(t, "GPU:main", frame, "fwd:sky_ibl", 3.0)
      text += line(t, "Main:main", frame, "cpu:draw", 4.0)
      t += 0.1
    return write_dump(text)

  def test_shares_and_intermittent_amortization(self):
    path = self._dump()
    rc, out = run_report(path, warmup=0.0)
    os.unlink(path)
    self.assertEqual(rc, 0)

    total = row(out, "fwd:total")
    self.assertEqual(int(total[1]), 100)          # frames
    self.assertAlmostEqual(float(total[3]), 10.0)  # p50
    self.assertEqual(total[8], "100.0%")           # share of itself

    color = row(out, "fwd:color_pass")
    self.assertEqual(int(color[1]), 100)
    self.assertEqual(color[8], "60.0%")
    self.assertEqual(color[9], "60.0%")            # runs every frame: share==amort

    # 3ms on the 34 frames it runs = 30% of one of THOSE frames, but only 10.2%
    # once spread over all 100 scored frames — both readings, never conflated.
    ibl = row(out, "fwd:sky_ibl")
    self.assertEqual(int(ibl[1]), 34)
    self.assertEqual(ibl[8], "30.0%")
    self.assertEqual(ibl[9], "10.2%")

  def test_sorted_by_p50_share_descending(self):
    path = self._dump()
    rc, out = run_report(path, warmup=0.0)
    os.unlink(path)
    body = out.split("channel         : GPU:main")[1]
    order = [ln.split()[0] for ln in body.splitlines()
             if ln.split() and ln.split()[0].startswith("fwd:")]
    self.assertEqual(order, ["fwd:total", "fwd:color_pass", "fwd:sky_ibl"])

  def test_warmup_discards_by_the_dumps_own_clock(self):
    path = self._dump()
    rc, out = run_report(path, warmup=1.0)
    os.unlink(path)
    self.assertEqual(rc, 0)
    self.assertEqual(int(row(out, "fwd:total")[1]), 90)
    self.assertIn("frame index 10..99", out)

  def test_skip_frames_discards_by_index(self):
    path = self._dump()
    rc, out = run_report(path, warmup=0.0, skip_frames=40)
    os.unlink(path)
    self.assertEqual(int(row(out, "fwd:total")[1]), 60)

  def test_channels_are_independent(self):
    path = self._dump()
    rc, out = run_report(path, warmup=0.0)
    os.unlink(path)
    self.assertIn("channel         : GPU:main", out)
    self.assertIn("channel         : Main:main", out)
    # the CPU channel has no fwd:total, so it reports times but refuses a share
    cpu = out.split("channel         : Main:main")[1]
    self.assertIn("share reference : NONE", cpu)
    self.assertIn("cpu:draw", cpu)
    self.assertNotIn("fwd:color_pass", cpu)

  def test_channel_filter(self):
    path = self._dump()
    rc, out = run_report(path, warmup=0.0, channel="GPU")
    os.unlink(path)
    self.assertIn("GPU:main", out)
    self.assertNotIn("Main:main", out)

  def test_load_scale_frame_is_called_out(self):
    text = HEADER
    t = 0.0
    for frame in range(50):
      ms = 900.0 if frame == 30 else 10.0
      text += line(t, "GPU:main", frame, "fwd:total", ms, 1.0)
      t += 0.1
    path = write_dump(text)
    rc, out = run_report(path, warmup=0.0)
    os.unlink(path)
    self.assertIn("LOAD-SCALE", out)
    self.assertIn("900ms", out)


if __name__ == "__main__":
  unittest.main(verbosity=2)
