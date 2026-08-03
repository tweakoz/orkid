#!/usr/bin/env ork.python
###############################################################################
# section_mip_minify.py — O3 GATE: the MINIFICATION observable for baked
# section texture-array MIP CHAINS. Sibling to section_bake_player.py (which
# proves the bake + per-gid distinctness); this one proves the mips actually
# ANTI-ALIAS a minified (far) view — an honest instrument, not "didn't crash".
#
# ONE process, ONE gid-partitioned cube (SdfBaked -> section_unwrap), the STORED
# SectionArray sampler (surface_stored sampling one sampler2DArray at ctx.layer),
# skybox DISABLED so the lit cube sits on a dark background (trivial masking).
# The camera is placed FAR so each 256px section texture minifies hard. We bake
# the SAME section content into the array TWICE, flipping ONE knob via the
# bake_section_array(mips=...) parameter (the same C++ TXI reserve/upload/finalize
# machinery the shipping player path uses):
#   leg OFF: num_mips=1 -> the sampler undersamples mip-0 -> the section pattern
#            ALIASES (high-frequency shimmer/moire).
#   leg ON : a CPU trilinear mip chain -> the sampler reads a coarser mip -> smooth.
#
# OBSERVABLE: interior high-frequency ENERGY of the rendered cube (gray minus its
# 3x3 box blur, mean-abs over the eroded lit mask — the silhouette is eroded out so
# we measure TEXTURE aliasing, not the geometry edge). PASS iff both legs render a
# comparable lit footprint AND hf_on < hf_off * RATIO_MAX (mips dropped the
# minification aliasing).
#
# The mip flag is not in the content cache key (mips regenerate from mip-0), so the
# two legs share byte-identical mip-0 content — only the chain differs.
#
#   run: MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS=1 ork.python section_mip_minify.py
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
os.environ.pop("ORKID_SECTION_MIPS", None)   # the bake(mips=) PARAM controls each leg (no env override)
import sys
import numpy as np
from orkengine.core import vec3, CrcStringProxy, asyncWorkPending, asyncWorkSummary   # core before lev2
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.hypergraph.dflow.hypermesh import make_drawable
from ork.hypergraph.assets.hypermesh.sdf_baked import SdfBaked
from ork.hypergraph.assets.materials.hypermesh.section_array import SectionArray
from ork.hypergraph.ptex3d.section_bake import bake_section_array

os.environ.setdefault("MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS", "1")

tokens    = CrcStringProxy()
DIM       = 512
BAKE_RES  = 256
SETTLE    = 320               # first capture: async IBL envmap (.xir) must load before lit-PBR albedo reads
SETTLE2   = 64                # second capture: only the array rebind needs to propagate
EYE       = vec3(18, 14, 27)  # FAR -> the 256px section textures minify hard on the small cube
RATIO_MAX = float(os.environ.get("SECMIP_RATIO", "0.85"))   # mips must cut interior HF energy by >= 15%
OUT_OFF   = os.environ.get("SECMIP_OUT_OFF", "/tmp/section_mip_off.png")
OUT_ON    = os.environ.get("SECMIP_OUT_ON",  "/tmp/section_mip_on.png")


def _box_blur3(g):
  acc = np.zeros_like(g)
  for dy in (-1, 0, 1):
    for dx in (-1, 0, 1):
      acc += np.roll(np.roll(g, dy, 0), dx, 1)
  return acc / 9.0


def _erode(mask, n):
  m = mask.copy()
  for _ in range(int(n)):
    m = (m & np.roll(m, 1, 0) & np.roll(m, -1, 0) & np.roll(m, 1, 1) & np.roll(m, -1, 1))
  return m


def _hf_interior(rgb):
  """Interior HF energy + lit/interior px. rgb is HxWx3 float."""
  gray = rgb.mean(axis=2)
  lit = gray > 8.0
  interior = _erode(lit, 2)
  hp = np.abs(gray - _box_blur3(gray))
  vals = hp[interior]
  hf = float(vals.mean()) if vals.size else 0.0
  return hf, int(interior.sum()), int(lit.sum())


class App(ComponentizedApplication):
  def __init__(self):
    super().__init__()
    self._frame = 0
    self._state = "init"
    self._fut = None
    self._cap = None
    self._want_exit = False
    self._done = False
    self.off = None
    self.on = None
    self._arr = None
    self.result = None
    self.SGC = self.addComponent("std_scenegraph", StandardSceneGraphComponent,
                                 eye=EYE, tgt=vec3(0, 0, 0), up=vec3(0, 1, 0),
                                 grid_variant=None)
    self.createEzApp(enable_lockstep_ups=True, enable_lockstep_fps=True,
                     enable_freerun_ups=True, enable_freerun_fps=True,
                     freerun=False, target_ups=60, target_fps=60,
                     width=DIM, height=DIM, use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  def _bake(self, ctx, mips):
    arr, _, _ = bake_section_array(ctx, key=self._key, num_layers=self._num_layers,
                                   bake_res=BAKE_RES, mips=mips)
    self._gmtl.bindParam(SectionArray.ARRAY_SAMPLER, arr)
    return arr

  def _onGpuInit(self, ctx):
    self._ctx = ctx
    self.ezapp.topWidget.enableUiDraw()
    asset = SdfBaked()
    self._live = asset.materialize_live(ctx)
    cdd, gmtl = make_drawable(self._live, ctx, animated=False, material_cls=SectionArray)
    self._gmtl = gmtl
    layer_gids = list(lev2.hypermesh.sectionLayerGids(self._live, ctx))
    if len(layer_gids) < 1:
      raise RuntimeError("section_mip_minify: SectionUnwrap produced no layer->gid table (empty mesh?)")
    self._num_layers = len(layer_gids)
    self._key = "section_mip_minify::SdfBaked::%d" % self._num_layers
    self._arr = self._bake(ctx, mips=False)         # leg OFF first (num_mips=1)
    self.node = self.SGC.layer_fwd.createDrawableNodeFromData("secmip", cdd)
    self.SGC.pbr_common.enable_skybox = False        # lit cube on a dark bg -> trivial cube mask
    self._state = "settle_off"
    print("section_mip_minify: materialized verts=%d faces=%d layers=%d (FAR eye=%.1f,%.1f,%.1f)"
          % (self._live.mesh.num_verts, self._live.mesh.num_faces, self._num_layers, EYE.x, EYE.y, EYE.z), flush=True)

  def _onUpdate(self, updinfo):
    if not self._done:
      self.SGC.scenegraph.updateScene(self.SGC.cameralut)

  def _issue_capture(self, ctx):
    rtg = getattr(self.SGC.SGVPW, "rtgroup", None)
    if rtg is None or rtg.numBuffers < 1:
      return False
    # the async IBL envmap (.xir) GPU upload must COMPLETE before the lit-PBR albedo reads —
    # a fixed frame count RACES it on fast GPUs (RADV) and captures BLACK. Gate on the
    # async-tracker registry (asyncWorkPending, what the offscreen player settles on).
    pend = asyncWorkPending()
    if pend > 0:
      if self._frame > 4000:
        raise RuntimeError("section_mip_minify: async work never drained after %d frames (pending=%d %s)"
                           % (self._frame, pend, asyncWorkSummary()))
      return False
    self._cap = lev2.CaptureBuffer()
    self._fut = ctx.FBI.captureAsFormat(rtg.buffer(0), self._cap, "RGBA8")
    return True

  def _grab_rgb(self):
    a = np.array(self._cap, dtype=np.uint8).reshape(self._cap.height, self._cap.width, 4)
    return a[..., :3].astype(np.float32)

  def _save(self, rgb, path):
    try:
      from PIL import Image
      Image.fromarray(rgb.astype(np.uint8)).transpose(Image.FLIP_TOP_BOTTOM).save(path)
    except Exception as e:
      print("PNG write error: %r" % e, flush=True)

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    if self._want_exit:
      self._want_exit = False
      self.ezapp.signalExit()
      return
    if self._done:
      return
    self._frame += 1
    if self._state == "settle_off":
      if self._fut is None:
        if self._frame >= SETTLE:
          self._issue_capture(ctx)
        return
      if not bool(self._fut.is_ready):
        return
      self.off = self._grab_rgb()
      self._save(self.off, OUT_OFF)
      self._fut = None
      self._arr = self._bake(ctx, mips=True)          # leg ON (trilinear chain, SAME mip-0 content)
      self._state = "settle_on"
      self._frame = 0
      return
    if self._state == "settle_on":
      if self._fut is None:
        if self._frame >= SETTLE2:
          self._issue_capture(ctx)
        return
      if not bool(self._fut.is_ready):
        return
      self.on = self._grab_rgb()
      self._save(self.on, OUT_ON)
      self._fut = None
      self._finish()

  def _finish(self):
    hf_off, ipx_off, lpx_off = _hf_interior(self.off)
    hf_on,  ipx_on,  lpx_on  = _hf_interior(self.on)
    self.result = dict(hf_off=hf_off, hf_on=hf_on, ipx_off=ipx_off, ipx_on=ipx_on,
                       lpx_off=lpx_off, lpx_on=lpx_on)
    print("section_mip_minify: OFF hf=%.4f interior_px=%d lit_px=%d -> %s"
          % (hf_off, ipx_off, lpx_off, OUT_OFF), flush=True)
    print("section_mip_minify: ON  hf=%.4f interior_px=%d lit_px=%d -> %s"
          % (hf_on, ipx_on, lpx_on, OUT_ON), flush=True)
    self._done = True
    self._want_exit = True


def main():
  app = App()
  app.ezapp.mainThreadLoop()
  r = app.result or {}
  hf_off = r.get("hf_off", 0.0)
  hf_on  = r.get("hf_on", 0.0)
  lpx_off = r.get("lpx_off", 0)
  lpx_on  = r.get("lpx_on", 0)
  ipx_off = r.get("ipx_off", 0)
  ipx_on  = r.get("ipx_on", 0)
  ratio = (hf_on / hf_off) if hf_off > 1e-6 else 9.99
  lit_ok = lpx_off > 500 and lpx_on > 500
  footprint_ok = (0.7 < (ipx_on / max(1, ipx_off)) < 1.4) if ipx_off > 0 else False
  drop_ok = (hf_off > hf_on) and (ratio < RATIO_MAX)
  passed = bool(r) and lit_ok and footprint_ok and drop_ok
  print("[secmip] ratio hf_on/hf_off = %.3f (need < %.2f)" % (ratio, RATIO_MAX), flush=True)
  print("SECTION_MIP_MINIFY_RESULT=%s lit=%d footprint=%d drop=%d hf_off=%.4f hf_on=%.4f ratio=%.3f"
        % ("PASS" if passed else "FAIL", int(lit_ok), int(footprint_ok), int(drop_ok),
           hf_off, hf_on, ratio), flush=True)
  sys.exit(0 if passed else 1)


if __name__ == "__main__":
  main()
