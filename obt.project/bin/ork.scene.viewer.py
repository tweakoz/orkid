#!/usr/bin/env ork.python

###############################################################################
# ork.scene.viewer.py — thin wrapper: tojson → ork.ecs.player.exe.
#
# ONE PLAYBACK PATH (owner-ratified, E.2 era): viewing a Scene class is
#   1. author + serialize the scene to an .ecs (the tojson phase, in-process)
#   2. exec ork.ecs.player.exe on it (the C++ host — the SAME loader/wire/render
#      path serialized scenes ship with; no separate Python playback to drift)
# `-e` still opens ork.ecsedit.py (the editor is Python by design).
#
# Usage:
#   ork.scene.viewer.py hello                          # bare name
#   ork.scene.viewer.py hello --camdist 220            # player camera args pass through
#   ork.scene.viewer.py hello -e                       # ecsedit instead of the player
#   ork.scene.viewer.py --class MyScene scene.py       # multi-class file
#   ork.scene.viewer.py --list                         # list scenes
###############################################################################

import argparse, os, shutil, sys, tempfile

from orkengine import core  # core MUST be imported before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.ecs.scene.resolve import (
  resolve_scene_file, load_scene_class, list_scene_files,
)


def _apply_scene_wrap(scene_class, args):
  """Generic scene-class wrap hook — orkid ships only the MECHANISM.

  When ORKEXP_SCENE_WRAP names a `<module>:<func>` (module = a .py file path OR
  an importable module name), import that callable and let it return a wrapped
  Scene subclass to author in place of the loaded one. The host uses this to
  inject a presentation/behavior overlay (a different render preset, extra
  systems, ...) onto an ARBITRARY scene without that scene knowing — the overlay
  is entirely host-supplied, so no specifics live in the engine. The callable
  receives `(scene_class, args)` (args = this viewer's parsed argparse namespace,
  opaque/best-effort) and returns a Scene subclass. No-op when the env var is
  unset — returns the class unchanged."""
  spec = os.environ.get("ORKEXP_SCENE_WRAP")
  if not spec:
    return scene_class
  target, sep, func = spec.partition(":")
  if not sep or not func:
    raise ValueError(f"ORKEXP_SCENE_WRAP must be '<module>:<func>'; got {spec!r}")
  import importlib
  if target.endswith(".py") or os.sep in target:
    # If the loaded scene already imported this EXACT file as a module (e.g. a
    # host base class it subclasses), REUSE that module. Loading a second copy
    # via spec_from_file_location would mint duplicate class objects, silently
    # breaking the identity checks (isinstance/issubclass) the wrap relies on.
    real = os.path.realpath(target)
    mod = next((m for m in list(sys.modules.values())
                if getattr(m, "__file__", None)
                and os.path.realpath(m.__file__) == real), None)
    if mod is None:
      import importlib.util
      ms = importlib.util.spec_from_file_location("_orkexp_scene_wrap_mod", target)
      if ms is None or ms.loader is None:
        raise ImportError(f"cannot load scene-wrap module from {target!r}")
      mod = importlib.util.module_from_spec(ms)
      ms.loader.exec_module(mod)
  else:
    mod = importlib.import_module(target)
  wrapped = getattr(mod, func)(scene_class, args)
  print(f"ork.scene.viewer: scene wrap {spec} → {wrapped.__name__}", file=sys.stderr)
  return wrapped


def parse_args():
  p = argparse.ArgumentParser(
    description="Tier 3 Scene viewer — tojson(scene) → ork.ecs.player.exe")
  p.add_argument("scene_file", nargs="?",
                 help="Scene bare name (resolved via ORK_SCENES_SEARCH_PATH) "
                      "or explicit path to a .py file")
  p.add_argument("--class", dest="class_name", default=None,
                 help="explicit Scene subclass to load (auto-find used if omitted)")
  p.add_argument("-e", "--edit", action="store_true",
                 help="launch ork.ecsedit.py instead of the C++ player")
  p.add_argument("--camdist", type=float, default=None,
                 help="player camera distance (passed through)")
  p.add_argument("--camheight", type=float, default=None,
                 help="player camera height (passed through)")
  p.add_argument("--roundtrip", type=float, default=None,
                 help="player: fire the live serdes round-trip at T+N seconds")
  p.add_argument("-t", "--ssaa", type=int, default=0,
                 help="SSAA multiplier (passed through to the player / ecsedit)")
  p.add_argument("-f", "--fullscreen", action="store_true",
                 help="fullscreen (passed through to the player / ecsedit)")
  p.add_argument("-W", "--width", type=int, default=0,
                 help="initial window width when not fullscreen (0=default)")
  p.add_argument("-H", "--height", type=int, default=0,
                 help="initial window height when not fullscreen (0=default)")
  p.add_argument("--hidpi", action="store_true",
                 help="render at the display's backing (Retina) scale; default is LoDPI to save fillrate")
  p.add_argument("--vr", action="store_true",
                 help="VR: present on the HMD through the active XR runtime (player only; needs ORKID_VR_DRIVER=openxr + a live runtime, else NoVR desktop fallback)")
  p.add_argument("--devkeys", action="store_true",
                 help="player dev keys [E/G/T/H/M/B/R] + key legend (thin passthrough; DEFAULT ON when not in VR — owner ruling jul30)")
  p.add_argument("--no-devkeys", dest="no_devkeys", action="store_true",
                 help="suppress the default-on dev keys")
  p.add_argument("--physics-debug", dest="physics_debug", action="store_true",
                 help="Bullet debug wireframe ON from startup (thin passthrough; the VR/offscreen path — no keyboard needed)")
  p.add_argument("--offscreen", action="store_true",
                 help="headless (no window): with --movie records a movie, else renders forever as fast as it can (player only)")
  p.add_argument("--movie", default=None, metavar="PATH",
                 help="record an offscreen movie to PATH (mp4; implies --offscreen)")
  p.add_argument("--movieframes", type=int, default=None,
                 help="movie frame count (with --movie; player default 300)")
  p.add_argument("--moviefps", type=float, default=None,
                 help="movie fps (with --movie; player default 60)")
  p.add_argument("--list", "-l", action="store_true",
                 help="list all scenes found in ORK_SCENES_SEARCH_PATH and exit")
  p.add_argument("--keep-json", action="store_true",
                 help="leave the intermediate .ecs file on disk (default: tmpfile)")
  return p.parse_args()


def main():
  # The ECS sub-interpreter machinery is GIL-OFF by design; under PYTHON_GIL=1 it
  # deadlocks (see test_gil_ecs_regression.py). The wrapper sets this too — this is
  # the belt for the runner we exec below. setdefault: an explicit caller value wins.
  os.environ.setdefault("PYTHON_GIL", "0")

  args = parse_args()

  if args.list or not args.scene_file:
    # list_scene_files prints to stdout itself and returns None.
    # Bare invocation (no scene_file) defaults to listing — most useful
    # default when the user forgot the name.
    list_scene_files()
    return 0

  try:
    scene_path  = resolve_scene_file(args.scene_file)
    scene_class = load_scene_class(scene_path, args.class_name)
  except (FileNotFoundError, ValueError) as e:
    print(f"ork.scene.viewer: {e}", file=sys.stderr)
    return 2

  # Generic scene-wrap hook — when ORKEXP_SCENE_WRAP names a host wrap, let it
  # overlay the loaded scene (e.g. force an alternate render preset). Fail loud
  # rather than silently authoring the un-wrapped scene.
  try:
    scene_class = _apply_scene_wrap(scene_class, args)
  except Exception as e:
    print(f"ork.scene.viewer: scene wrap failed: {e}", file=sys.stderr)
    return 2

  print(f"ork.scene.viewer: {scene_class.__name__} ← {scene_path}",
        file=sys.stderr)

  # tojson phase — exactly what ork.scene.tojson.py does. Subsystem-mode
  # lev2 init + bindGfxToCurrentThread so the Scene's eager asset
  # composition (HdriToXir bake, etc.) can drive GPU work inline.
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "ezapp.bindGfxToCurrentThread() returned null"

  scene = scene_class()
  sd    = ecs.SceneData()
  scene.build(sd)
  js    = sd.serializeJson()
  ezapp.mainThreadEnd()

  # Park the .ecs in a tmpfile (or named, if --keep-json).
  if args.keep_json:
    out_path = f"/tmp/{scene_class.__name__}.ecs"
    with open(out_path, "w") as f:
      f.write(js)
  else:
    tf = tempfile.NamedTemporaryFile(suffix=".ecs", delete=False, mode="w")
    tf.write(js)
    tf.close()
    out_path = tf.name

  if args.edit and (args.offscreen or args.movie):
    print("ork.scene.viewer: --offscreen/--movie are player-only; ignored in --edit mode",
          file=sys.stderr)

  if args.edit:
    # the EDITOR stays Python (ecsedit) — same -s CLI as before.
    runner = shutil.which("ork.ecsedit.py")
    if runner is None:
      print("ork.scene.viewer: ork.ecsedit.py not found on PATH", file=sys.stderr)
      return 2
    cmd = [runner, "-s", out_path, "--ssaa", str(args.ssaa)]
    if args.fullscreen:
      cmd.append("-f")
    if args.width > 0:
      cmd += ["--width", str(args.width)]
    if args.height > 0:
      cmd += ["--height", str(args.height)]
    if args.hidpi:
      cmd.append("--hidpi")
  else:
    # PLAYBACK is the C++ player — the one true loader/wire/render path.
    runner = shutil.which("ork.ecs.player.exe")
    if runner is None:
      print("ork.scene.viewer: ork.ecs.player.exe not found on PATH", file=sys.stderr)
      return 2
    cmd = [runner, out_path]
    if args.camdist is not None:
      cmd += ["--camdist", str(args.camdist)]
    if args.camheight is not None:
      cmd += ["--camheight", str(args.camheight)]
    if args.roundtrip is not None:
      cmd += ["--roundtrip", str(args.roundtrip)]
    if args.fullscreen:
      cmd += ["--fullscreen"]
    if args.width > 0:
      cmd += ["--width", str(args.width)]
    if args.height > 0:
      cmd += ["--height", str(args.height)]
    if args.hidpi:
      cmd += ["--hidpi"]
    if args.ssaa and args.ssaa > 1:
      cmd += ["--ssaa", str(args.ssaa)]
    # VR (player only): route the scene onto the HMD via the active XR runtime. Thin
    # passthrough — the player owns the runtime check + NoVR fallback. One playback path.
    if args.vr:
      cmd += ["--vr"]
    # physics-debug instrumentation (player only): both thin passthroughs.
    # dev keys default ON outside VR (owner ruling jul30); --no-devkeys opts out
    if (args.devkeys or not args.vr) and not args.no_devkeys:
      cmd += ["--devkeys"]
    if args.physics_debug:
      cmd += ["--physics-debug"]
    # OFFSCREEN (headless, player only): with --movie record a clip; without, render
    # forever as fast as it can (perf/soak). --movie implies offscreen in the player.
    if args.movie:
      cmd += ["--movie", args.movie]
      if args.moviefps is not None:
        cmd += ["--moviefps", str(args.moviefps)]
      if args.movieframes is not None:
        cmd += ["--movieframes", str(args.movieframes)]
    elif args.offscreen:
      cmd += ["--offscreen-forever"]

  print(f"ork.scene.viewer: wrote {out_path} ({len(js)} bytes); launching {os.path.basename(runner)}",
        file=sys.stderr)

  # exec the chosen runner — replaces this process.
  os.execvp(runner, cmd)


if __name__ == "__main__":
  ret = main()
  ecs.headless_exit()
  sys.exit(ret)
