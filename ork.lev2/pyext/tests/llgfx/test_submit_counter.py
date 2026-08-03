#!/usr/bin/env ork.python
"""
Per-frame GPU submit counter oracle.

Context.submitCount() publishes, once per frame boundary, how many vkQueueSubmit calls the
engine issued during the previous frame (every queue: graphics, compute, offscreen capture,
external composite — they all funnel through VkThreadedQueue::queueSubmit). A frame-time
comparison between two render strategies is only honest if their submit counts are visible
instead of assumed, so the number has to be readable from an IN-PROCESS python gate, not
scraped out of a spawned player's stdout.

Two failure modes this guards:
  * counter never moves        -> the property reads a dead stub and every comparison is blind
  * counter accumulates        -> the reset is not at a true frame boundary; the value keeps
                                  climbing and looks plausible while measuring nothing

So: step frames, sample ctx.submitCount each time, and require (a) at least one non-zero
sample and (b) the tail is NOT strictly increasing (strict growth across every frame of a
steady-state loop is the accumulation signature).
"""
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys; sys.stdout.reconfigure(line_buffering=True)

# repo root = five levels up; prepend THIS checkout's scripts dir so ork.testing resolves
# from the same tree as this test (mirrors the sibling worktree-shadowing idiom).
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from orkengine import core   # core before lev2
from orkengine import lev2
from ork.testing import headless_app, verdict

tokens = core.CrcStringProxy()

NUM_FRAMES = 12
W, H = 256, 192


def main():
  rc = 1
  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2']) as app:
    ctx = app.ctx

    rtg = lev2.RtGroup(ctx, W, H)
    rtb = rtg.createBuffer(tokens.RGBA8, tokens.color)
    rtb.clearColor = core.vec4(0.1, 0.2, 0.3, 1)
    capbuf = lev2.CaptureBuffer()

    samples = []
    for i in range(NUM_FRAMES):
      # a real frame: clear the RTG and read it back — the capture forces a submit,
      # so a frame that renders is a frame that submits.
      ctx.beginFrame()
      ctx.FBI.rtGroupPush(rtg)
      ctx.FBI.rtGroupClear(rtg)
      ctx.FBI.captureAsFormat(rtb, capbuf, "RGBA8")
      ctx.FBI.rtGroupPop()
      ctx.endFrame()
      samples.append(int(ctx.submitCount))
      print("  frame %2d submitCount %d" % (i, samples[-1]), flush=True)

    tail = samples[2:]                      # skip boot frames (device/asset submits)
    any_nonzero = any(s > 0 for s in samples)
    strictly_increasing = all(b > a for a, b in zip(tail, tail[1:])) and len(tail) > 2

    ok = any_nonzero and not strictly_increasing
    detail = "samples=%s nonzero=%d accumulating=%d" % (
        ",".join(str(s) for s in samples), int(any_nonzero), int(strictly_increasing))
    print("=== submit counter oracle %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    rc = verdict(ok, detail)   # verdict BEFORE teardown

  sys.exit(rc)


main()
