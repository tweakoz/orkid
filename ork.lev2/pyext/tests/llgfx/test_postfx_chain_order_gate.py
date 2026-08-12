#!/usr/bin/env ork.python
###############################################################################
# POST CHAIN ORDER + SETTLE PROBE GATE.
#
# Two observables that were lost together, and hid each other:
#
#  1. CHAIN ORDER IS A STAGE ORDER, NOT A DECLARATION ORDER. A scene declares
#     its sky in the first line of __init__, so the sky's ACES tone stage is the
#     FIRST name in postfx_order; a subsurface material auto-attaches the SSSS
#     effect much later, landing BEHIND it. SSSS reads the forward node's second
#     color attachment, the tone stage publishes a single display-referred
#     buffer, and an effect handed a one-buffer input can only refuse — leaving
#     its never-rendered rtgroup as the chain's output. Whole frame black. This
#     gate declares exactly that shape (sky tonemap=True + a subsurface
#     material), asserts the DECLARED order is still tone-first (so the gate is
#     testing the runtime sort and not a lucky authoring order), and asserts the
#     rendered frame is lit.
#
#  2. THE SETTLE PROBE MUST NOT CALL A BLACK FRAME LIT. The frame the ordering
#     bug produced was not zero — it carried the output dither's least
#     significant bit on 13% of its pixels, which cleared a probe that counted
#     any non-zero channel. PASS on a black PNG is the defect that lets every
#     black-screen bug hide, so leg 2 forces a black frame (an authored grade
#     with value=0, downstream of everything) and requires the player to FAIL
#     loud and exit non-zero.
#
# Both legs run the SHIPPED path: Scene -> .ecs -> ork.ecs.player.exe -S.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import json
import re
import shutil
import subprocess
import sys
import tempfile

from orkengine import core  # core before lev2
from orkengine import lev2
from orkengine import ecs

from orkengine.core import lev2_pyexdir
lev2_pyexdir.addToSysPath()

from ork.hypergraph.ecs.scene import Scene

WIDTH, HEIGHT = 640, 360
SETTLE_TIMEOUT = 45.0   # leg 2's settled-black verdict lands in ~10s of drain


class ChainOrderScene(Scene):
  """The minimal reproduction shape: a tone stage declared with the sky (i.e.
  FIRST), and an effect the material stack attaches afterwards."""

  def __init__(self):
    super().__init__()
    SG = self.sky(
      skybox_intensity   = 1.0,
      diffuse_intensity  = 1.0,
      specular_intensity = 1.0,
      ambient_light      = vec3(0),
      msaa               = 1,
      ssaa               = 1,
      latitude_deg  = 36.0,
      day_of_year   = 223.0,
      time_of_day   = 8.5,
      time_scale    = 0.0,
      celestial     = True,
      moon          = False,
      stars         = False,
      sun_intensity = 3.0,
      sun_params    = {"shadow_caster": False},
      tonemap       = True)

    # has_subsurface on any material is what auto-attaches the SSSS post-fx
    # node, and it happens in Scene.build — long after sky() named the tone
    # stage.
    mat = self.asset.PbrMaterial(
      "mat_skin",
      base_color        = vec3(0.85, 0.65, 0.55),
      metallic          = 0.0,
      roughness         = 0.8,
      subsurface_color  = vec3(0.90, 0.35, 0.25),
      subsurface_radius = vec3(0.50, 0.20, 0.10),
      subsurface_factor = 0.9)
    drw = self.asset.IcoSphere("drw_skin", radius=2.0, subdivisions=3, material=mat)
    self.entity(
      "ent_skin",
      transform={"translation": vec3(0, 0, 0)},
      components=[SG.component(nodes={"n": {"drawable": drw}})])


def _compose(path):
  ez = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ez.mainThreadBegin()
  ctx = ez.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  scene = ChainOrderScene()
  sd = ecs.SceneData()
  scene.build(sd)
  js = sd.serializeJson()
  ez.mainThreadEnd()
  with open(path, "w") as f:
    f.write(js)
  return js


def _postfx_order(js):
  doc = json.loads(js)

  def walk(node):
    if isinstance(node, dict):
      if "postfx_order" in node:
        return node["postfx_order"]
      for v in node.values():
        r = walk(v)
        if r is not None:
          return r
    elif isinstance(node, list):
      for v in node:
        r = walk(v)
        if r is not None:
          return r
    return None

  return walk(doc)


def _run_player(ecs_path, png_path):
  exe = shutil.which("ork.ecs.player.exe")
  assert exe, "ork.ecs.player.exe not on PATH"
  env = dict(os.environ)
  # the offscreen player must not find a display to scan out on
  env.pop("DISPLAY", None)
  env.pop("WAYLAND_DISPLAY", None)
  cmd = [exe, ecs_path, "-S", png_path,
         "--settle-timeout", str(SETTLE_TIMEOUT),
         "-W", str(WIDTH), "-H", str(HEIGHT)]
  proc = subprocess.run(cmd, env=env, capture_output=True, text=True)
  out = proc.stdout + proc.stderr
  verdict = None
  m = re.search(r"SNAPSHOT_RESULT=(\w+)", out)
  if m:
    verdict = m.group(1)
  rgbmax = 0
  for m in re.finditer(r"SNAPSHOT probe .*rgbmax<(\d+)>", out):
    rgbmax = max(rgbmax, int(m.group(1)))
  return proc.returncode, verdict, rgbmax, out


def _blacken(js):
  """Authored grade at value=0 — a black frame produced by a node the chain
  sorter puts LAST, so nothing downstream can rescue it. The forcing device for
  leg 2: the player must refuse to call this settled-lit."""
  doc = json.loads(js)

  def walk(node):
    if isinstance(node, dict):
      if "postfx_nodes" in node and "postfx_order" in node:
        node["postfx_nodes"]["hsvg"] = {
          "object": {
            "class": "PostFxNodeHSVG",
            "uuid": "6f1b4c22-0d2e-4a7a-9f10-3c5e7a1b9d44",
            "properties": {"gamma": 1.0, "hue": 0.0, "saturation": 1.0, "value": 0.0},
          }
        }
        node["postfx_order"] = node["postfx_order"] + ",hsvg"
        return True
      for v in node.values():
        if walk(v):
          return True
    elif isinstance(node, list):
      for v in node:
        if walk(v):
          return True
    return False

  assert walk(doc), "no SceneGraphSystemData postfx block in the composed scene"
  return json.dumps(doc)


def main():
  workdir = tempfile.mkdtemp(prefix="postfx_order_")
  lit_ecs = os.path.join(workdir, "chain_order.ecs")
  lit_png = os.path.join(workdir, "chain_order.png")
  blk_ecs = os.path.join(workdir, "chain_black.ecs")
  blk_png = os.path.join(workdir, "chain_black.png")

  js = _compose(lit_ecs)
  order = _postfx_order(js)
  print(f"[chain] declared postfx_order = {order!r}", flush=True)

  names = [n.strip() for n in (order or "").split(",") if n.strip()]
  assert "aces" in names and "ssss" in names, \
      f"repro shape lost — expected both aces and ssss in {names}"
  # The DECLARED order must still be the bad one, else this gate proves nothing
  # about the runtime sort.
  declared_bad = names.index("aces") < names.index("ssss")
  assert declared_bad, \
      "declaration order now puts the effect first — re-point this gate at the " \
      "shape it exists to cover (tone stage declared BEFORE an HDR effect)"

  # LEG 1 — the scene-declared tone stage renders.
  rc, verdict, rgbmax, out = _run_player(lit_ecs, lit_png)
  print(f"[leg1] rc={rc} verdict={verdict} rgbmax={rgbmax}", flush=True)
  leg1 = (rc == 0) and (verdict == "PASS") and (rgbmax >= 32) and os.path.exists(lit_png)
  if not leg1:
    sys.stderr.write(out[-4000:])

  # LEG 2 — a black frame is refused, loudly and non-zero.
  with open(blk_ecs, "w") as f:
    f.write(_blacken(js))
  rc2, verdict2, rgbmax2, out2 = _run_player(blk_ecs, blk_png)
  print(f"[leg2] rc={rc2} verdict={verdict2} rgbmax={rgbmax2}", flush=True)
  leg2 = (rc2 != 0) and (verdict2 == "FAIL") and (rgbmax2 < 32)
  if not leg2:
    sys.stderr.write(out2[-4000:])

  ecs.headless_exit()
  ok = leg1 and leg2
  print(f"POSTFX_CHAIN_ORDER_GATE: {'PASS' if ok else 'FAIL'} "
        f"lit<rc={rc},{verdict},rgbmax={rgbmax}> black<rc={rc2},{verdict2},rgbmax={rgbmax2}>",
        flush=True)
  return 0 if ok else 1


if __name__ == "__main__":
  sys.exit(main())
