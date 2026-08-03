#!/usr/bin/env ork.python
################################################################################
# lev2.RtGroup(ctx, w, h, msaa) — the pyext hardware-MSAA knob.
#
# The argument is a literal sample COUNT (1/2/4/8/16), not the --msaa LEVEL the scene
# params use. Two contract points this gate exists to hold:
#
#   DEFAULT IS 1x. Every existing caller passes three arguments and must keep getting a
#     single-sample target.
#   REFUSAL IS LOUD. A count that is not a hw sample count, or one above the device
#     ceiling, THROWS. It must never round down silently: a caller measuring per-sample
#     cost that is quietly handed a quieter target publishes a wrong number and never
#     finds out (that is the whole reason the knob exists — see the MSAA surcharge leg
#     in test_terrain_meshshader_ab.py).
#
# Also asserts a multisampled group is usable, not merely constructible: it renders a
# clear + capture round trip, which exercises the multisample attachment and its resolve
# to the sampled image.
#
# Fully synthetic — no assets, no window.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

import numpy

from orkengine import core   # core before lev2
from orkengine import lev2
from ork.testing import headless_app, verdict

tokens = core.CrcStringProxy()

W, H = 64, 64
CLEAR = core.vec4(0.25, 0.5, 0.75, 1.0)

################################################################################

def build(ctx, msaa=None):
  rtg = lev2.RtGroup(ctx, W, H) if msaa is None else lev2.RtGroup(ctx, W, H, msaa)
  rtb = rtg.createBuffer(tokens.RGBA8, tokens.color)
  rtb.clearColor = CLEAR
  rtg.createDepthBuffer(tokens.Z32F, True)
  rtg.autoclear = True
  return rtg, rtb


def clear_and_capture(app, rtg, rtb):
  """One clear-only frame + readback. On a multisampled group this goes through the
  multisample attachment and its resolve, so a broken resolve shows up as wrong pixels."""
  ctx = app.ctx
  capbuf = lev2.CaptureBuffer()
  ctx.beginFrame()
  ctx.FBI.rtGroupPush(rtg)
  ctx.FBI.rtGroupClear(rtg)
  future = ctx.FBI.captureAsFormat(rtb, capbuf, "RGBA8")
  ctx.FBI.rtGroupPop()
  ctx.endFrame()
  spins = 0
  while not future.is_ready:
    app.run_frames(1)
    spins += 1
    assert spins < 600, "msaa capture never became ready"
  return numpy.array(capbuf, dtype=numpy.uint8).reshape(capbuf.height, capbuf.width, 4)

################################################################################

def main():
  fails = []
  code = 1
  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2']) as app:
    ctx = app.ctx
    devmax = int(ctx.msaa_max_samples)
    print("device msaa max = %dx" % devmax)

    # ---- default stays 1x (existing callers must not change behavior) ----------
    rtg, rtb = build(ctx)
    if rtg.msaa_samples != 1:
      fails.append("3-arg RtGroup default is %dx, must be 1x" % rtg.msaa_samples)

    # ---- every supported count is honored EXACTLY (no quiet round-down) -------
    tested = []
    for n in (1, 2, 4, 8, 16):
      if n > devmax:
        continue
      rtg_n, rtb_n = build(ctx, n)
      tested.append(n)
      if rtg_n.msaa_samples != n:
        fails.append("asked for %dx, got %dx" % (n, rtg_n.msaa_samples))
      img = clear_and_capture(app, rtg_n, rtb_n)
      want = (int(CLEAR.x * 255.0 + 0.5), int(CLEAR.y * 255.0 + 0.5), int(CLEAR.z * 255.0 + 0.5))
      got = tuple(int(v) for v in img[H // 2, W // 2, :3])
      if max(abs(a - b) for a, b in zip(want, got)) > 2:
        fails.append("%dx clear+resolve produced %s, expected ~%s" % (n, got, want))
      print("  msaa=%dx honored, clear+resolve center=%s" % (n, got))
    if not tested:
      fails.append("device reports msaa max %dx — not even 1x was testable" % devmax)

    # ---- refusal is loud ------------------------------------------------------
    for bad, why in ((3, "not a hw sample count"), (0, "not a hw sample count"),
                     (devmax * 2, "above the device ceiling")):
      try:
        lev2.RtGroup(ctx, W, H, bad)
        fails.append("msaa=%d (%s) was ACCEPTED — the knob must throw, never round down"
                     % (bad, why))
      except Exception as ex:
        print("  msaa=%d refused: %s" % (bad, ex))

    detail = "RtGroup msaa knob | devmax=%dx honored=%s default=1x" % (
        devmax, ",".join("%dx" % n for n in tested))
    code = verdict(not fails, detail if not fails else detail + " | " + "; ".join(fails))

  sys.exit(code)


main()
