#!/usr/bin/env ork.python
"""
SKYLIGHT lane B slice B1 — Hillaire LUT plausibility gate.

Drives the REAL engine path (pbr::HillaireSky + orkshader://sky + skytools.i2),
not a python re-implementation, and asserts the three LUTs are physically
plausible rather than merely nonblack:

  transmittance (256x64)  * in [0,1] everywhere
                          * STRICTLY DECREASING along u at fixed altitude —
                            u=0 is the zenith ray (shortest path through the
                            medium) and u=1 is horizon-grazing (longest), so a
                            LUT that rises toward the horizon has its Bruneton
                            parameterization inverted.
                          * blue extinguishes faster than red (Rayleigh ~1/l^4)
  multi-scatter  (32x32)  * finite, non-negative, nonblack (a black MS LUT means
                            the transmittance LUT never reached the raymarch)
  sky-view      (192x108) * nonblack AND sun-angle sensitive: two bakes at
                            different sun elevations must differ materially

Also asserts the bake's change-stamp behaviour: a re-bake with an unchanged
medium is skipped, and a medium edit forces one (that skip is what keeps the
per-frame cost to the sky-view LUT alone).

FRAME CONTRACT: the LUT passes record into the caller's command buffer, so each
bake is bracketed by ctx.beginFrame()/endFrame() — the same standalone-frame
pattern LightProbe::exportEquirectangular uses.
"""
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys; sys.stdout.reconfigure(line_buffering=True)

# repo root = five levels up; prepend THIS checkout's scripts dir so ork.testing
# resolves from the same tree as this test (worktree-shadowing idiom).
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine.core import vec3
from ork.testing import headless_app, verdict

import math
import numpy

TRANSMITTANCE_W, TRANSMITTANCE_H = 256, 64
MULTISCATTER_W, MULTISCATTER_H   = 32, 32
SKYVIEW_W, SKYVIEW_H             = 192, 108

VIEW_ALTITUDE_KM = 0.0005   # eye on the ground


def _sun_dir(elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up)."""
  e = math.radians(elevation_deg)
  return vec3(math.cos(e), math.sin(e), 0.0)


def _grab(ctx, rtg, w, h):
  """captureAsFormat must be issued INSIDE the frame; the caller ends the frame
  before waiting. Returns (future, capturebuffer)."""
  buf = lev2.CaptureBuffer()
  fut = ctx.FBI.captureAsFormat(rtg.buffer(0), buf, "RGBA32F")
  return fut, buf


def _readback(fut, buf, w, h):
  fut.wait(buf)
  assert buf.width == w and buf.height == h, f"capture extent {buf.width}x{buf.height} != {w}x{h}"
  return numpy.array(buf, dtype=numpy.float32).reshape(h, w, 4)


def main():
  failures = []

  def check(label, ok, detail=""):
    print(f"  {'PASS' if ok else 'FAIL'} {label} {detail}", flush=True)
    if not ok:
      failures.append(label)
    return ok

  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2']) as app:
    ctx  = app.ctx
    rcfd = lev2.RenderContextFrameData(ctx)

    atmo = lev2.SkyAtmosphereData()
    sky  = lev2.HillaireSky(ctx)

    print("=== bake: static LUTs + sky-view @ sun elevation 60 deg ===", flush=True)
    ctx.beginFrame()
    baked_first = sky.bakeStaticLuts(ctx, rcfd, atmo)
    baked_again = sky.bakeStaticLuts(ctx, rcfd, atmo)   # same medium -> skipped
    sky.updateSkyView(ctx, rcfd, atmo, _sun_dir(60.0), VIEW_ALTITUDE_KM)
    fut_t, buf_t = _grab(ctx, sky.transmittanceRtGroup, TRANSMITTANCE_W, TRANSMITTANCE_H)
    fut_m, buf_m = _grab(ctx, sky.multiScatterRtGroup, MULTISCATTER_W, MULTISCATTER_H)
    fut_s, buf_s = _grab(ctx, sky.skyViewRtGroup, SKYVIEW_W, SKYVIEW_H)
    ctx.endFrame()

    tra  = _readback(fut_t, buf_t, TRANSMITTANCE_W, TRANSMITTANCE_H)[..., :3]
    msc  = _readback(fut_m, buf_m, MULTISCATTER_W, MULTISCATTER_H)[..., :3]
    sky0 = _readback(fut_s, buf_s, SKYVIEW_W, SKYVIEW_H)[..., :3]

    print("=== bake: sky-view @ sun elevation 5 deg (same medium) ===", flush=True)
    ctx.beginFrame()
    sky.updateSkyView(ctx, rcfd, atmo, _sun_dir(5.0), VIEW_ALTITUDE_KM)
    fut_s2, buf_s2 = _grab(ctx, sky.skyViewRtGroup, SKYVIEW_W, SKYVIEW_H)
    ctx.endFrame()
    sky1 = _readback(fut_s2, buf_s2, SKYVIEW_W, SKYVIEW_H)[..., :3]

    print("=== bake: medium edit forces a re-bake ===", flush=True)
    hash_before = atmo.medium_hash
    atmo.mie_scattering = atmo.mie_scattering * 3.0
    hash_after = atmo.medium_hash
    ctx.beginFrame()
    baked_after_edit = sky.bakeStaticLuts(ctx, rcfd, atmo)
    ctx.endFrame()

    ############################################################
    # change-stamp behaviour
    ############################################################
    print("=== bake cadence ===", flush=True)
    check("bake_ran_first_call", bool(baked_first))
    check("bake_skipped_when_medium_unchanged", not bool(baked_again))
    check("medium_hash_moves_on_edit", hash_before != hash_after,
          f"{hash_before} -> {hash_after}")
    check("bake_ran_after_medium_edit", bool(baked_after_edit))

    ############################################################
    # transmittance LUT
    ############################################################
    print("=== transmittance LUT ===", flush=True)
    check("transmittance_finite", bool(numpy.isfinite(tra).all()))
    check("transmittance_in_unit_range",
          float(tra.min()) >= -1.0e-3 and float(tra.max()) <= 1.0 + 1.0e-3,
          f"min={float(tra.min()):.5f} max={float(tra.max()):.5f}")
    check("transmittance_nonblack", float(tra.max()) > 0.5, f"max={float(tra.max()):.5f}")

    # u = 0 is the zenith ray, u = 1 horizon-grazing -> monotone decrease.
    green = tra[..., 1]
    diffs = numpy.diff(green, axis=1)
    worst_rise = float(diffs.max())
    check("transmittance_decreasing_toward_horizon", worst_rise <= 1.0e-3,
          f"worst_per_texel_rise={worst_rise:.6f}")

    zenith_col  = float(green[:, 0].mean())
    horizon_col = float(green[:, -1].mean())
    check("transmittance_zenith_vs_horizon_spread", (zenith_col - horizon_col) > 0.3,
          f"zenith={zenith_col:.5f} horizon={horizon_col:.5f}")

    # ground-level, mid-path column: Rayleigh extinguishes blue hardest.
    mid = tra[0, TRANSMITTANCE_W // 2]
    check("transmittance_rayleigh_ordering", mid[0] > mid[1] > mid[2],
          f"rgb=({mid[0]:.5f},{mid[1]:.5f},{mid[2]:.5f})")

    ############################################################
    # multi-scatter LUT
    ############################################################
    print("=== multi-scatter LUT ===", flush=True)
    check("multiscatter_finite", bool(numpy.isfinite(msc).all()))
    check("multiscatter_nonnegative", float(msc.min()) >= -1.0e-6, f"min={float(msc.min()):.8f}")
    check("multiscatter_nonblack", float(msc.max()) > 1.0e-5,
          f"max={float(msc.max()):.8f} mean={float(msc.mean()):.8f}")

    ############################################################
    # sky-view LUT
    ############################################################
    print("=== sky-view LUT ===", flush=True)
    check("skyview_finite", bool(numpy.isfinite(sky0).all()))
    check("skyview_nonblack", float(sky0.max()) > 1.0e-4,
          f"max={float(sky0.max()):.6f} mean={float(sky0.mean()):.6f}")
    check("skyview_nonblack_lowsun", float(sky1.max()) > 1.0e-4,
          f"max={float(sky1.max()):.6f} mean={float(sky1.mean()):.6f}")

    # sun-angle sensitivity: a LUT that ignores the sun is the failure this catches
    # (e.g. an unbound SkySunDirection param falling back to zero).
    denom = max(float(numpy.abs(sky0).mean()), 1.0e-8)
    rel_delta = float(numpy.abs(sky1 - sky0).mean()) / denom
    check("skyview_sun_angle_sensitive", rel_delta > 0.05, f"rel_mean_delta={rel_delta:.5f}")

    ok = (len(failures) == 0)
    detail = ("tra_max=%.4f ms_max=%.6f sky_mean=%.5f sun_rel_delta=%.4f" %
              (float(tra.max()), float(msc.max()), float(sky0.mean()), rel_delta))
    if failures:
      detail += " failed=" + ",".join(failures)
    # VERDICT BEFORE TEARDOWN (#57)
    verdict(ok, detail)

  sys.exit(0 if ok else 1)


main()
