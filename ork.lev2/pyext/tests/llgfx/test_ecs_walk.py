#!/usr/bin/env python3
###############################################################################
# E.2-walk gate — walk-on-terrain through the REAL pipeline: scn_scatter (terrain +
# scatter + terrain_collider + walker library calls) → tojson → ork.ecs.player.exe.
# NOTE: this gate launches the PLAYER (a transient window, self-terminated) — the
# walk path is host+physics+camera, not headless-renderable.
#   1. the player detects WALK MODE from the scene,
#   2. the capsule FALLS and SETTLES on the bullet heightfield at the height the
#      baked EXR says (same artifact the chunks render — the alignment probe),
#   3. --autowalk drives 'W' through the CONTROLLER-MESSAGE input channel for 6s —
#      the character must displace horizontally,
#   4. after release it settles again (stable y), with frame evidence throughout,
#   5. E2A4 — the SHOOT case (ORK_WALK_SELFTEST=1): the input script's selftest
#      fires the '/' path (CameraRay request → spawner handle → spawn with
#      velocity+spin); the spawn must land, NO ghost ball may exist before it
#      (the AutoSpawn-reflection regression), and the player must stay alive
#      well past the ball's 8s lifetime despawn (slot free + zero).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import re, subprocess, sys, time, shutil

REPO   = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
         os.path.dirname(os.path.abspath(__file__))))))
TMP    = os.path.join(REPO, ".tmp")
ECS    = os.path.join(TMP, "walk_gate.ecs")

# scale constants come FROM THE SCENE SOURCE (they were once duplicated here
# and silently went stale when the scene was rescaled 512/60 -> 2048/250)
_SCENE_SRC = open(os.path.join(REPO, "ork.data", "scenes", "scn_scatter.py")).read()
HEIGHT_M = float(re.search(r"^HEIGHT_M\s*=\s*([\d.]+)", _SCENE_SRC, re.M).group(1))
EXTENT_M = float(re.search(r"^EXTENT_M\s*=\s*([\d.]+)", _SCENE_SRC, re.M).group(1))


def sample_height_exr(x, z, extent_m=EXTENT_M):
  """Nearest-texel sample of the baked terra height EXR at world (x,z) -> meters.
  Runs in a SUBPROCESS with the proper headless lifecycle (orkid imports without
  appinit/teardown abort on exit — the known trap)."""
  snippet = (
      "import os; os.environ['PYTHONUNBUFFERED']='1'\n"
      "import sys\n"
      "from orkengine import core, lev2, ecs\n"
      "import numpy as np\n"
      "ezapp = ecs.headless_appinit(use_subsystems=['opq','core','gpu','lev2'])\n"
      "ezapp.mainThreadBegin()\n"
      "ezapp.bindGfxToCurrentThread()\n"
      "from orkengine.lev2 import Image\n"
      "p = core.Path.expandPathString('<assetcache>/terrain/terra/height.exr')\n"
      "a = np.array(Image.createFromFile(str(p)).numpy, dtype=np.float32)\n"
      "a = a[...,0] if a.ndim==3 else a\n"
      "H,W = a.shape\n"
      "u = min(max(%g/%g + 0.5, 0.0), 1.0)\n"
      "v = min(max(%g/%g + 0.5, 0.0), 1.0)\n"
      "xi = min(int(u*W), W-1); yi = min(int(v*H), H-1)\n"
      "print('HSAMPLE %%g' %% (float(a[yi,xi]) * %g), flush=True)\n"
      "ezapp.mainThreadEnd()\n"
      "ecs.headless_exit()\n"
      "sys.exit(0)\n" % (x, extent_m, z, extent_m, HEIGHT_M))
  r = subprocess.run(["ork.python", "-c", snippet], capture_output=True, text=True, timeout=120)
  m = re.search(r"HSAMPLE ([-\d.]+)", r.stdout + r.stderr)
  assert m, "height sampler subprocess produced no sample"
  return float(m.group(1))


def main():
  os.makedirs(TMP, exist_ok=True)
  # 1. author + serialize (subprocess — its own lifecycle)
  rc = subprocess.call(["ork.scene.tojson.py", "-i", "scn_scatter", "-o", ECS])
  assert rc == 0, "tojson failed"

  # 2. run the player with scripted walking; kill after the window.
  # ORK_WALK_SELFTEST=1 arms the input script's shoot selftest (fires the
  # whole '/' path ~2s in: CameraRay request -> spawner handle -> spawn).
  exe = shutil.which("ork.ecs.player.exe")
  assert exe, "ork.ecs.player.exe not on PATH"
  env = dict(os.environ, ORK_WALK_SELFTEST="1")
  proc = subprocess.Popen([exe, ECS, "--autowalk", "4"],  # input flows host->PythonSystem script->semantic actions
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                          env=env)
  t0 = time.time()
  out_lines = []
  while time.time() - t0 < 28:   # ≤30s iterations (owner rule); rely on warm caches
    line = proc.stdout.readline()
    if not line:
      break
    out_lines.append(line)
  proc.kill()
  out = "".join(out_lines)
  open(os.path.join(TMP, "walk_gate_player.log"), "w").write(out)

  ok = False
  try:
    assert "WALK MODE" in out, "player did not detect the CharacterControllerSystem"
    assert "AUTOWALK begin" in out and "AUTOWALK end" in out, "scripted input never fired"
    assert re.search(r"FPS<\d", out), "no frame evidence"

    pos = [(float(m[0]), float(m[1]), float(m[2]))
           for m in re.findall(r"charctl pos<([-\d.]+) ([-\d.]+) ([-\d.]+)>", out)]
    assert len(pos) >= 5, "too few charctl pose samples (%d)" % len(pos)

    # settle: the last two samples' y stable (walking ended well before the tail)
    assert abs(pos[-1][1] - pos[-2][1]) < 0.5, "character never settled (y still changing)"

    # horizontal displacement from the walk (first settled sample vs final)
    dx = pos[-1][0] - pos[0][0]
    dz = pos[-1][2] - pos[0][2]
    dist = (dx * dx + dz * dz) ** 0.5
    assert dist > 5.0, "autowalk produced only %.2fm displacement (input channel dead?)" % dist

    # ALIGNMENT PROBE: rest height ≈ baked terrain height at the final (x,z).
    # capsule rest offset = extent/2 + radius ≈ 1.4 (scn_scatter walker dims);
    # tolerance covers nearest-texel + slope within a texel.
    x, y, z = pos[-1]
    th = sample_height_exr(x, z)
    err = abs(y - (th + 1.4))
    print("WALK probe: rest y=%.2f terrain=%.2f (+1.4 capsule) err=%.2f disp=%.1fm" % (y, th, err, dist),
          flush=True)
    assert err < 3.0, "rest height off by %.2fm — physics/render terrain misaligned" % err

    # 5. SHOOT case (E2A4): the selftest must have fired AND spawned.
    shoot_at = out.find("[walk_input] SELFTEST shoot")
    assert shoot_at >= 0, "shoot selftest never fired (script selftest broken?)"
    assert "shoot ent<" in out[shoot_at:], "selftest fired but spawn produced no entity"
    # NO ghost ball: before the first deliberate shot there must be no
    # ball collision (an autospawned ball at origin = the unreflected
    # AutoSpawn regression signature).
    assert not re.search(r"Collision.*ball_spawner", out[:shoot_at]), \
        "ball collision BEFORE the first shot — ghost autospawn is back"
    # alive well past the 8s lifetime despawn (slot free+zero path): the
    # 2s-cadence bullet telemetry keeps printing AFTER the walk ends (the
    # shot at ~2s despawns at ~10s ≈ 5s past AUTOWALK end). Counted relative
    # to AUTOWALK end so a COLD terrain rebake eating the capture window
    # doesn't false-fail (absolute tick counts did).
    tail = out[out.find("AUTOWALK end"):]
    assert tail.count("bullet timing") >= 3, \
        "player died early (%d telemetry ticks after walk) — despawn-path crash?" % tail.count("bullet timing")
    ok = True
  except AssertionError:
    import traceback
    traceback.print_exc()
  print("=== ecs walk gate %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
  sys.exit(0 if ok else 1)


main()
